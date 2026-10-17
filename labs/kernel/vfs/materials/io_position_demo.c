#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

static int close_owned(int *fd)
{
    int number = *fd;
    *fd = -1;
    return number < 0 ? 0 : close(number);
}

static ssize_t read_retry(int fd, void *buffer, size_t length)
{
    ssize_t count;
    do {
        count = read(fd, buffer, length);
    } while (count < 0 && errno == EINTR);
    return count;
}

int main(void)
{
    char workspace[] = "/tmp/vfs-io-demo-XXXXXX";
    const char initial[] = "ABC";
    char value = 0, first[2] = {0}, second[2] = {0}, final[3];
    struct iovec vectors[] = {
        {.iov_base = first, .iov_len = sizeof(first)},
        {.iov_base = second, .iov_len = sizeof(second)}
    };
    int dir_fd = -1, file_fd = -1, result = EXIT_FAILURE;
    bool have_workspace = false, have_file = false;
    size_t written = 0;
    ssize_t count;

    if (!mkdtemp(workspace))
        goto error;
    have_workspace = true;
    dir_fd = open(workspace, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0)
        goto error;
    /* 本实验没有O_APPEND，显式位置不会受到追加意图影响。 */
    file_fd = openat(dir_fd, "value", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (file_fd < 0)
        goto error;
    have_file = true;
    while (written < sizeof(initial) - 1) {
        count = write(file_fd, initial + written, sizeof(initial) - 1 - written);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            if (count == 0)
                errno = EIO;
            goto error;
        }
        written += (size_t)count;
    }
    if (lseek(file_fd, 0, SEEK_SET) < 0)
        goto error;
    if (read_retry(file_fd, &value, 1) != 1 || value != 'A')
        goto mismatch;
    do {
        count = pread(file_fd, &value, 1, 2);
    } while (count < 0 && errno == EINTR);
    if (count != 1 || value != 'C' || lseek(file_fd, 0, SEEK_CUR) != 1)
        goto mismatch;
    if (read_retry(file_fd, &value, 1) != 1 || value != 'B')
        goto mismatch;

    /* 总缓冲容量为4，但当前位置到EOF只剩1字节。 */
    do {
        count = readv(file_fd, vectors, 2);
    } while (count < 0 && errno == EINTR);
    if (count != 1 || first[0] != 'C' || second[0] != 0 ||
        lseek(file_fd, 0, SEEK_CUR) != 3)
        goto mismatch;
    if (read_retry(file_fd, &value, 1) != 0)
        goto mismatch;

    do {
        count = pwrite(file_fd, "X", 1, 0);
    } while (count < 0 && errno == EINTR);
    if (count != 1 || lseek(file_fd, 0, SEEK_CUR) != 3)
        goto mismatch;
    do {
        count = pread(file_fd, final, sizeof(final), 0);
    } while (count < 0 && errno == EINTR);
    if (count != (ssize_t)sizeof(final) || memcmp(final, "XBC", sizeof(final)))
        goto mismatch;
    puts("普通读推进位置；显式读写不改共享位置；向量读只返回剩余1字节；再次读得到EOF");
    result = EXIT_SUCCESS;
    goto out;
mismatch:
    fprintf(stderr, "观察结果未满足本实验预期\n");
    goto out;
error:
    perror("读写位置实验");
out:
    if (close_owned(&file_fd) < 0)
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
