#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int close_owned(int *fd)
{
    int number = *fd;
    *fd = -1;
    return number < 0 ? 0 : close(number);
}

static int write_at(int fd, const void *buffer, size_t length, off_t pos)
{
    const char *bytes = buffer;
    size_t done = 0;
    while (done < length) {
        ssize_t count = pwrite(fd, bytes + done, length - done, pos + (off_t)done);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            if (count == 0)
                errno = EIO;
            return -1;
        }
        done += (size_t)count;
    }
    return 0;
}

static ssize_t read_at(int fd, void *buffer, size_t length, off_t pos)
{
    ssize_t count;
    do {
        count = pread(fd, buffer, length, pos);
    } while (count < 0 && errno == EINTR);
    return count;
}

int main(void)
{
    char workspace[] = "/tmp/vfs-cache-demo-XXXXXX";
    char buffer[3], value = 0;
    struct stat first_stat, second_stat;
    int dir_fd = -1, first_fd = -1, second_fd = -1, result = EXIT_FAILURE;
    bool have_workspace = false, have_file = false;
    if (!mkdtemp(workspace))
        goto error;
    have_workspace = true;
    dir_fd = open(workspace, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0)
        goto error;
    first_fd = openat(dir_fd, "value", O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
    if (first_fd < 0)
        goto error;
    have_file = true;
    second_fd = openat(dir_fd, "value", O_RDONLY | O_CLOEXEC);
    if (second_fd < 0 || fstat(first_fd, &first_stat) < 0 ||
        fstat(second_fd, &second_stat) < 0)
        goto error;
    if (first_stat.st_dev != second_stat.st_dev || first_stat.st_ino != second_stat.st_ino)
        goto mismatch;
    /* 独立打开对象观察同一文件；本程序不统计缓存命中。 */
    if (write_at(first_fd, "ABC", 3, 0) < 0)
        goto error;
    if (read_at(second_fd, buffer, sizeof(buffer), 0) != 3 || memcmp(buffer, "ABC", 3))
        goto mismatch;
    if (ftruncate(first_fd, 1) < 0)
        goto error;
    if (read_at(second_fd, &value, 1, 1) != 0)
        goto mismatch;
    /* 4096只是选定的文件偏移，程序不假设系统页大小为4096。 */
    if (write_at(first_fd, "Z", 1, 4096) < 0)
        goto error;
    if (read_at(second_fd, &value, 1, 2048) != 1 || value != 0)
        goto mismatch;
    if (read_at(second_fd, &value, 1, 4096) != 1 || value != 'Z')
        goto mismatch;
    if (fstat(second_fd, &second_stat) < 0)
        goto error;
    if (second_stat.st_size != 4097)
        goto mismatch;
    puts("独立打开可见ABC；缩短后偏移1为EOF；扩展后的间隙为零，偏移4096为Z");
    result = EXIT_SUCCESS;
    goto out;
mismatch:
    fprintf(stderr, "观察结果未满足本实验预期\n");
    goto out;
error:
    perror("共享文件内容实验");
out:
    if (close_owned(&second_fd) < 0)
        result = EXIT_FAILURE;
    if (close_owned(&first_fd) < 0)
        result = EXIT_FAILURE;
    if (have_file && unlinkat(dir_fd, "value", 0) < 0)
        result = EXIT_FAILURE;
    if (close_owned(&dir_fd) < 0)
        result = EXIT_FAILURE;
    if (have_workspace && rmdir(workspace) < 0)
        result = EXIT_FAILURE;
    if (result != EXIT_SUCCESS)
        fprintf(stderr, "实验或清理失败；实验目录：%s\n", workspace);
    return result;
}
