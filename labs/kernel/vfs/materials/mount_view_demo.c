#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

static int join_path(char *out, size_t size, const char *base,
                     const char *suffix)
{
    int count = snprintf(out, size, "%s/%s", base, suffix);
    if (count < 0 || (size_t)count >= size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

static int write_all(int fd, const char *data, size_t size)
{
    while (size != 0) {
        ssize_t count = write(fd, data, size);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (count == 0) {
            errno = EIO;
            return -1;
        }
        data += count;
        size -= (size_t)count;
    }
    return 0;
}

static int read_exact(int fd, char *data, size_t size)
{
    while (size != 0) {
        ssize_t count = read(fd, data, size);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (count == 0) {
            errno = EIO;
            return -1;
        }
        data += count;
        size -= (size_t)count;
    }
    return 0;
}

int main(void)
{
    char workspace[] = "/tmp/vfs-mount-demo-XXXXXX";
    char tree[256], view[256], subtree[256], source_file[256], view_file[256];
    char buffer[32];
    struct stat source_stat, view_stat;
    bool have_workspace = false, have_tree = false, have_view = false;
    bool tree_mounted = false, view_mounted = false;
    int fd = -1, held_fd = -1, result = EXIT_FAILURE;

    /* 先隔离挂载拓扑并切断传播，再创建实验对象。 */
    if (unshare(CLONE_NEWNS) < 0 ||
        mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
        perror("隔离挂载视图");
        goto out;
    }
    if (!mkdtemp(workspace)) {
        perror("创建实验目录");
        goto out;
    }
    have_workspace = true;
    if (join_path(tree, sizeof(tree), workspace, "tree") < 0 ||
        join_path(view, sizeof(view), workspace, "view") < 0 ||
        join_path(subtree, sizeof(subtree), tree, "sub") < 0 ||
        join_path(source_file, sizeof(source_file), subtree, "value") < 0 ||
        join_path(view_file, sizeof(view_file), view, "value") < 0) {
        perror("生成实验路径");
        goto out;
    }
    if (mkdir(tree, 0700) < 0) {
        perror("创建原树挂载点");
        goto out;
    }
    have_tree = true;
    if (mkdir(view, 0700) < 0) {
        perror("创建别名挂载点");
        goto out;
    }
    have_view = true;
    if (mount("tmpfs", tree, "tmpfs", MS_NODEV | MS_NOSUID | MS_NOEXEC,
              "size=1m") < 0) {
        perror("挂载临时文件系统");
        goto out;
    }
    tree_mounted = true;
    if (mkdir(subtree, 0700) < 0) {
        perror("建立子目录");
        goto out;
    }
    fd = open(source_file, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    if (fd < 0 || write_all(fd, "alpha\n", 6) < 0) {
        perror("建立源文件");
        goto out;
    }
    if (close(fd) < 0) {
        fd = -1;
        perror("关闭源文件");
        goto out;
    }
    fd = -1;
    if (mount(subtree, view, NULL, MS_BIND, NULL) < 0) {
        perror("绑定子树");
        goto out;
    }
    view_mounted = true;
    held_fd = open(view_file, O_RDONLY | O_CLOEXEC);
    if (held_fd < 0 || stat(source_file, &source_stat) < 0 ||
        fstat(held_fd, &view_stat) < 0) {
        perror("核对两条访问路径");
        goto out;
    }
    if (source_stat.st_dev != view_stat.st_dev ||
        source_stat.st_ino != view_stat.st_ino) {
        fprintf(stderr, "绑定视图没有指向预期的同一文件\n");
        goto out;
    }
    puts("两条路径到达同一文件，视图根为原树的 sub");

    errno = 0;
    if (umount2(view, 0) == 0) {
        view_mounted = false;
        fprintf(stderr, "持有视图文件时普通卸载意外成功\n");
        goto out;
    }
    if (errno != EBUSY) {
        perror("普通卸载不是预期的 EBUSY");
        goto out;
    }
    puts("普通卸载返回 EBUSY");
    if (umount2(view, MNT_DETACH) < 0) {
        perror("分离绑定视图");
        goto out;
    }
    view_mounted = false;

    /* 新路径回到下层空目录；旧 fd 仍持有已经分离的视图。 */
    fd = open(view_file, O_RDONLY | O_CLOEXEC);
    if (fd >= 0 || errno != ENOENT) {
        fprintf(stderr, "分离后新路径未返回预期的 ENOENT\n");
        goto out;
    }
    if (read_exact(held_fd, buffer, 6) < 0 ||
        memcmp(buffer, "alpha\n", 6) != 0) {
        fprintf(stderr, "旧 fd 没有读到预期内容\n");
        goto out;
    }
    puts("新路径已不见文件，旧 fd 仍读到 alpha");
    result = EXIT_SUCCESS;

out:
    /* 只处理本程序成功取得的资源，不使用递归目录删除。 */
    if (fd >= 0 && close(fd) < 0)
        result = EXIT_FAILURE;
    if (held_fd >= 0 && close(held_fd) < 0)
        result = EXIT_FAILURE;
    if (view_mounted && umount2(view, MNT_DETACH) < 0) {
        perror("清理绑定视图");
        result = EXIT_FAILURE;
    }
    if (tree_mounted && umount2(tree, MNT_DETACH) < 0) {
        perror("清理临时树");
        result = EXIT_FAILURE;
    }
    if (have_view && rmdir(view) < 0) {
        perror("清理视图目录");
        result = EXIT_FAILURE;
    }
    if (have_tree && rmdir(tree) < 0) {
        perror("清理原树目录");
        result = EXIT_FAILURE;
    }
    if (have_workspace && rmdir(workspace) < 0) {
        perror("清理实验目录");
        result = EXIT_FAILURE;
    }
    return result;
}
