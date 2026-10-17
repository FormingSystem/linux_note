#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static int create_value(int dir_fd, const char *name, char value, bool *created)
{
    int fd = openat(dir_fd, name, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    ssize_t count;
    if (fd < 0)
        return -1;
    *created = true;
    do {
        count = write(fd, &value, 1);
    } while (count < 0 && errno == EINTR);
    if (count != 1) {
        int saved_errno = count < 0 ? errno : EIO;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    /* Linux 上不因 close 失败而重试同一个数字。 */
    return close(fd);
}

static int read_value(int dir_fd, const char *name, char *value)
{
    int fd = openat(dir_fd, name, O_RDONLY | O_CLOEXEC);
    ssize_t count;
    if (fd < 0)
        return -1;
    do {
        count = read(fd, value, 1);
    } while (count < 0 && errno == EINTR);
    if (count != 1) {
        int saved_errno = count < 0 ? errno : EIO;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    return close(fd);
}

int main(void)
{
    char workspace[] = "/tmp/vfs-path-demo-XXXXXX";
    char walked, simplified;
    int dir_fd = -1, result = EXIT_FAILURE;
    bool have_workspace = false, have_other = false, have_inner = false;
    bool have_root_file = false, have_other_file = false, have_link = false;

    if (!mkdtemp(workspace)) {
        perror("创建实验目录");
        goto out;
    }
    have_workspace = true;
    dir_fd = open(workspace, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0) {
        perror("打开实验起点");
        goto out;
    }
    if (mkdirat(dir_fd, "other", 0700) < 0) {
        perror("创建 other");
        goto out;
    }
    have_other = true;
    if (mkdirat(dir_fd, "other/inner", 0700) < 0) {
        perror("创建 inner");
        goto out;
    }
    have_inner = true;
    if (create_value(dir_fd, "target", 'R', &have_root_file) < 0 ||
        create_value(dir_fd, "other/target", 'N', &have_other_file) < 0) {
        perror("建立两个不同文件");
        goto out;
    }
    /* 相对链接目标以链接所在目录为起点解释。 */
    if (symlinkat("other/inner", dir_fd, "a") < 0) {
        perror("创建符号链接");
        goto out;
    }
    have_link = true;
    if (read_value(dir_fd, "a/../target", &walked) < 0 ||
        read_value(dir_fd, "target", &simplified) < 0) {
        perror("读取两条路径");
        goto out;
    }
    if (walked != 'N' || simplified != 'R') {
        fprintf(stderr, "路径结果不符合已建立的目录关系\n");
        goto out;
    }
    printf("a/../target -> %c，直接 target -> %c\n", walked, simplified);
    result = EXIT_SUCCESS;

out:
    /* 仅删除本次成功创建的对象，从子项向父目录配对清理。 */
    if (have_link && unlinkat(dir_fd, "a", 0) < 0)
        result = EXIT_FAILURE;
    if (have_other_file && unlinkat(dir_fd, "other/target", 0) < 0)
        result = EXIT_FAILURE;
    if (have_root_file && unlinkat(dir_fd, "target", 0) < 0)
        result = EXIT_FAILURE;
    if (have_inner && unlinkat(dir_fd, "other/inner", AT_REMOVEDIR) < 0)
        result = EXIT_FAILURE;
    if (have_other && unlinkat(dir_fd, "other", AT_REMOVEDIR) < 0)
        result = EXIT_FAILURE;
    if (dir_fd >= 0 && close(dir_fd) < 0)
        result = EXIT_FAILURE;
    if (have_workspace && rmdir(workspace) < 0)
        result = EXIT_FAILURE;
    if (result != EXIT_SUCCESS)
        fprintf(stderr, "实验或配对清理失败，请核对先前错误及实验目录\n");
    return result;
}
