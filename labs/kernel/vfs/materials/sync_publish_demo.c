#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int close_owned(int *fd)
{
    int number = *fd;
    *fd = -1;
    return number < 0 ? 0 : close(number);
}

static int sync_retry(int fd)
{
    int result;
    do {
        result = fsync(fd);
    } while (result < 0 && errno == EINTR);
    return result;
}

int main(void)
{
    char workspace[] = "/tmp/vfs-sync-demo-XXXXXX";
    const char content[] = "version=1\n";
    size_t written = 0;
    int dir_fd = -1, file_fd = -1, result = EXIT_FAILURE;
    bool have_workspace = false, have_temp = false, have_final = false;
    if (!mkdtemp(workspace))
        goto error;
    have_workspace = true;
    dir_fd = open(workspace, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0)
        goto error;
    file_fd = openat(dir_fd, "pending", O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    if (file_fd < 0)
        goto error;
    have_temp = true;
    while (written < sizeof(content) - 1) {
        ssize_t count = write(file_fd, content + written, sizeof(content) - 1 - written);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            if (count == 0)
                errno = EIO;
            goto error;
        }
        written += (size_t)count;
    }
    /* 先检查文件数据与必要元数据的同步结果。EIO等错误不能循环到成功后忽略。 */
    if (sync_retry(file_fd) < 0)
        goto error;
    if (renameat(dir_fd, "pending", dir_fd, "published") < 0)
        goto error;
    have_temp = false;
    have_final = true;
    /* 名称变化属于目录，不能由前一次文件fsync代替。 */
    if (sync_retry(dir_fd) < 0)
        goto error;
    puts("文件fsync、名称发布和目录fsync均返回成功；本程序没有模拟掉电");
    result = EXIT_SUCCESS;
    goto out;
error:
    perror("同步与名称发布实验");
out:
    if (close_owned(&file_fd) < 0)
        result = EXIT_FAILURE;
    if (have_temp && unlinkat(dir_fd, "pending", 0) < 0)
        result = EXIT_FAILURE;
    if (have_final && unlinkat(dir_fd, "published", 0) < 0)
        result = EXIT_FAILURE;
    if (close_owned(&dir_fd) < 0)
        result = EXIT_FAILURE;
    if (have_workspace && rmdir(workspace) < 0)
        result = EXIT_FAILURE;
    if (result != EXIT_SUCCESS)
        fprintf(stderr, "实验或清理失败；实验目录：%s\n", workspace);
    return result;
}
