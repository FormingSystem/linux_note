#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static int close_owned(int *fd)
{
    int number = *fd;
    *fd = -1;
    /* Linux close 失败也不重试原号码，避免误关已复用的槽位。 */
    return number < 0 ? 0 : close(number);
}

int main(void)
{
    char workspace[] = "/tmp/vfs-open-demo-XXXXXX";
    int dir_fd = -1, original_fd = -1, probe_fd = -1;
    int result = EXIT_FAILURE;
    bool have_workspace = false, have_file = false;
    char value = 'A';
    ssize_t count;
    struct stat original_stat, truncated_stat;

    if (!mkdtemp(workspace))
        goto error;
    have_workspace = true;
    dir_fd = open(workspace, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0)
        goto error;
    original_fd = openat(dir_fd, "value", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (original_fd < 0)
        goto error;
    have_file = true;
    do {
        count = write(original_fd, &value, 1);
    } while (count < 0 && errno == EINTR);
    if (count != 1) {
        if (count >= 0)
            errno = EIO;
        goto error;
    }

    /* 排他创建遇到现存名称应拒绝，不能先截断后再报告 EEXIST。 */
    probe_fd = openat(dir_fd, "value",
                      O_WRONLY | O_CREAT | O_EXCL | O_TRUNC | O_CLOEXEC, 0600);
    if (probe_fd >= 0 || errno != EEXIST) {
        fprintf(stderr, "排他创建未产生预期的 EEXIST\n");
        goto out;
    }
    do {
        count = pread(original_fd, &value, 1, 0);
    } while (count < 0 && errno == EINTR);
    if (count != 1 || value != 'A') {
        fprintf(stderr, "拒绝排他创建后原数据未保留\n");
        goto out;
    }

    /* O_PATH 取得路径句柄，不建立可读写文件数据的普通打开关系。 */
    probe_fd = openat(dir_fd, "value", O_PATH | O_CLOEXEC);
    if (probe_fd < 0)
        goto error;
    errno = 0;
    if (read(probe_fd, &value, 1) != -1 || errno != EBADF) {
        fprintf(stderr, "O_PATH 读取未产生预期的 EBADF\n");
        goto out;
    }
    if (close_owned(&probe_fd) < 0)
        goto error;

    probe_fd = openat(dir_fd, "value", O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (probe_fd < 0)
        goto error;
    if (fstat(original_fd, &original_stat) < 0 ||
        fstat(probe_fd, &truncated_stat) < 0)
        goto error;
    if (original_stat.st_dev != truncated_stat.st_dev ||
        original_stat.st_ino != truncated_stat.st_ino ||
        original_stat.st_size != 0 || truncated_stat.st_size != 0) {
        fprintf(stderr, "截断后的对象身份或长度不符合预期\n");
        goto out;
    }
    puts("排他创建被拒绝且A保留；O_PATH不能read；普通O_TRUNC使同一对象长度变0");
    result = EXIT_SUCCESS;
    goto out;

error:
    perror("打开意图实验");
out:
    if (close_owned(&probe_fd) < 0)
        result = EXIT_FAILURE;
    if (close_owned(&original_fd) < 0)
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
