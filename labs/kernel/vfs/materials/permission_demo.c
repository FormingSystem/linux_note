#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    char directory[] = "/tmp/vfs_permission_XXXXXX";
    char path[sizeof(directory) + 16];
    int fd = -1, other_fd = -1, created = 0, result = EXIT_FAILURE;
    unsigned char value;
    struct stat state;
    /* 特权用户可能绕过本实验要观察的传统DAC拒绝。 */
    if (geteuid() == 0) {
        puts("跳过：请使用普通非特权用户观察mode权限检查");
        return 77;
    }
    if (!mkdtemp(directory)) { perror("mkdtemp"); return EXIT_FAILURE; }
    snprintf(path, sizeof(path), "%s/data", directory);
    fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) { perror("create"); goto done; }
    created = 1;
    if (fchmod(fd, 0600) < 0) { perror("initialize mode"); goto done; }
    if (pwrite(fd, "K", 1, 0) != 1) {
        fprintf(stderr, "初始化单字节写入失败\n"); goto done;
    }
    other_fd = open(path, O_RDONLY);
    if (other_fd < 0) { perror("baseline open"); goto done; }
    if (close(other_fd) < 0) { other_fd = -1; perror("close baseline"); goto done; }
    other_fd = -1;
    /* 改变inode的mode，不改变已经取得的file的打开模式。 */
    if (fchmod(fd, 0000) < 0 || fstat(fd, &state) < 0) {
        perror("chmod/stat"); goto done;
    }
    if ((state.st_mode & 0777) != 0) {
        fprintf(stderr, "文件系统没有呈现预期的mode，跳过\n");
        result = 77; goto done;
    }
    if (pread(fd, &value, 1, 0) != 1 || value != 'K') {
        fprintf(stderr, "旧fd读取未通过；需检查后端或额外安全策略\n");
        goto done;
    }
    other_fd = open(path, O_RDONLY);
    if (other_fd >= 0) {
        puts("跳过：mode为零仍可新打开，当前身份或后端不满足非特权DAC前提");
        result = 77; goto done;
    }
    if (errno != EACCES) { perror("new open"); goto done; }
    puts("旧fd仍能读取K；新open因权限不足被拒绝");
    result = EXIT_SUCCESS;
done:
    /* 只恢复和清理本次创建的对象；不改动其他路径的权限。 */
    if (other_fd >= 0 && close(other_fd) < 0) { perror("close other"); result = EXIT_FAILURE; }
    if (fd >= 0) {
        if (fchmod(fd, 0600) < 0) { perror("restore mode"); result = EXIT_FAILURE; }
        if (close(fd) < 0) { perror("close"); result = EXIT_FAILURE; }
    }
    if (created && unlink(path) < 0) { perror("unlink"); result = EXIT_FAILURE; }
    if (rmdir(directory) < 0) { perror("rmdir"); result = EXIT_FAILURE; }
    return result;
}
