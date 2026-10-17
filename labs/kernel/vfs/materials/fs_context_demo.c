#define _GNU_SOURCE
#include <errno.h>
#include <linux/magic.h>
#include <linux/mount.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <unistd.h>

#if !defined(SYS_fsopen) || !defined(SYS_fsconfig) || !defined(SYS_fsmount)
#error "需要提供 fsopen、fsconfig 和 fsmount 编号的 Linux 用户态头文件"
#endif

static int configure(int fd, unsigned int command, const char *key,
                     const char *value)
{
    return (int)syscall(SYS_fsconfig, fd, command, key, value, 0);
}

int main(void)
{
    int context_fd = -1;
    int mount_fd = -1;
    int result = EXIT_FAILURE;
    struct statfs info;

    context_fd = (int)syscall(SYS_fsopen, "tmpfs", FSOPEN_CLOEXEC);
    if (context_fd < 0) {
        perror("fsopen tmpfs");
        goto out;
    }

    /* 只改变本次新配置；未知参数应被拒绝，随后仍可继续设置。 */
    errno = 0;
    if (configure(context_fd, FSCONFIG_SET_FLAG,
                  "vfs_demo_unknown_option", NULL) != -1 || errno != EINVAL) {
        fprintf(stderr, "未知参数未返回预期的 EINVAL\n");
        goto out;
    }
    puts("未知参数被拒绝，继续配置");
    if (configure(context_fd, FSCONFIG_SET_STRING, "size", "1m") < 0) {
        perror("设置 tmpfs 大小");
        goto out;
    }
    if (configure(context_fd, FSCONFIG_CMD_CREATE, NULL, NULL) < 0) {
        perror("建立可挂载的根");
        goto out;
    }

    /* 建树完成后，创建参数阶段已经结束。 */
    errno = 0;
    if (configure(context_fd, FSCONFIG_SET_STRING, "size", "2m") != -1 ||
        errno != EBUSY) {
        fprintf(stderr, "建树后的参数写入未返回预期的 EBUSY\n");
        goto out;
    }
    mount_fd = (int)syscall(SYS_fsmount, context_fd, FSMOUNT_CLOEXEC,
                           MOUNT_ATTR_NODEV | MOUNT_ATTR_NOSUID |
                           MOUNT_ATTR_NOEXEC);
    if (mount_fd < 0) {
        perror("建立分离挂载");
        goto out;
    }

    /* 没有调用 move_mount：这个挂载没有接入调用者的目录树。 */
    if (close(context_fd) < 0) {
        context_fd = -1; /* Linux 的 close 错误不以重试旧 fd 处理。 */
        perror("关闭配置上下文");
        goto out;
    }
    context_fd = -1;
    if (fstatfs(mount_fd, &info) < 0) {
        perror("读取分离挂载的文件系统信息");
        goto out;
    }
    if ((unsigned long)info.f_type != TMPFS_MAGIC) {
        fprintf(stderr, "返回对象不是预期的 tmpfs\n");
        goto out;
    }
    puts("配置上下文已关闭，挂载 fd 仍持有 tmpfs");
    result = EXIT_SUCCESS;

out:
    if (mount_fd >= 0 && close(mount_fd) < 0) {
        perror("关闭挂载 fd");
        result = EXIT_FAILURE;
    }
    if (context_fd >= 0 && close(context_fd) < 0) {
        perror("关闭配置 fd");
        result = EXIT_FAILURE;
    }
    return result;
}
