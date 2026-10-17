#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static int make_file(int dir_fd, const char *name, char value, bool *created)
{
    int fd = openat(dir_fd, name, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
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
    return fd;
}

static int read_at_start(int fd, char *value)
{
    ssize_t count;
    /* 使用显式偏移，避免前面的写入偏移干扰观察。 */
    do {
        count = pread(fd, value, 1, 0);
    } while (count < 0 && errno == EINTR);
    if (count == 1)
        return 0;
    if (count >= 0)
        errno = EIO;
    return -1;
}

int main(void)
{
    char workspace[] = "/tmp/vfs-replace-demo-XXXXXX";
    int dir_fd = -1, source_fd = -1, victim_fd = -1;
    int result = EXIT_FAILURE;
    bool have_workspace = false, have_source = false;
    bool have_target = false, have_alias = false;
    struct stat source_stat, target_stat, victim_stat;
    char source_value, victim_value;

    if (!mkdtemp(workspace))
        goto error;
    have_workspace = true;
    dir_fd = open(workspace, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0)
        goto error;
    source_fd = make_file(dir_fd, "source", 'A', &have_source);
    if (source_fd < 0)
        goto error;
    victim_fd = make_file(dir_fd, "target", 'B', &have_target);
    if (victim_fd < 0)
        goto error;
    if (linkat(dir_fd, "source", dir_fd, "alias", 0) < 0)
        goto error;
    have_alias = true;

    /* 替换名称入口，两个已经打开的 fd 均保持原来的对象身份。 */
    if (renameat(dir_fd, "source", dir_fd, "target") < 0)
        goto error;
    have_source = false;
    if (fstat(source_fd, &source_stat) < 0 ||
        fstatat(dir_fd, "target", &target_stat, 0) < 0 ||
        fstat(victim_fd, &victim_stat) < 0)
        goto error;
    if (source_stat.st_dev != target_stat.st_dev ||
        source_stat.st_ino != target_stat.st_ino ||
        source_stat.st_nlink != 2 || victim_stat.st_nlink != 0) {
        fprintf(stderr, "重命名后的身份或链接数不符合实验前提\n");
        goto out;
    }
    if (unlinkat(dir_fd, "target", 0) < 0)
        goto error;
    have_target = false;
    if (unlinkat(dir_fd, "alias", 0) < 0)
        goto error;
    have_alias = false;
    if (fstat(source_fd, &source_stat) < 0 ||
        read_at_start(source_fd, &source_value) < 0 ||
        read_at_start(victim_fd, &victim_value) < 0)
        goto error;
    if (source_stat.st_nlink != 0 || source_value != 'A' || victim_value != 'B') {
        fprintf(stderr, "名称退出后旧打开关系未产生预期结果\n");
        goto out;
    }
    printf("两个对象的名称链接数均为0，旧fd仍读到 %c 和 %c\n",
           source_value, victim_value);
    result = EXIT_SUCCESS;
    goto out;

error:
    perror("名称替换实验");
out:
    /* 只清理本次建立且尚未成功移除的名称；不重试 Linux close。 */
    if (have_alias && unlinkat(dir_fd, "alias", 0) < 0)
        result = EXIT_FAILURE;
    if (have_source && unlinkat(dir_fd, "source", 0) < 0)
        result = EXIT_FAILURE;
    if (have_target && unlinkat(dir_fd, "target", 0) < 0)
        result = EXIT_FAILURE;
    if (source_fd >= 0 && close(source_fd) < 0)
        result = EXIT_FAILURE;
    if (victim_fd >= 0 && close(victim_fd) < 0)
        result = EXIT_FAILURE;
    if (dir_fd >= 0 && close(dir_fd) < 0)
        result = EXIT_FAILURE;
    if (have_workspace && rmdir(workspace) < 0)
        result = EXIT_FAILURE;
    if (result != EXIT_SUCCESS)
        fprintf(stderr, "实验或清理失败；实验目录：%s\n", workspace);
    return result;
}
