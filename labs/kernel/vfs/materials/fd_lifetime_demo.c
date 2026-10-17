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
    /* Linux 已经撤销槽位后，不能因关闭错误重试可能被复用的号码。 */
    return number < 0 ? 0 : close(number);
}

static int expect_byte(int fd, char expected)
{
    char value;
    ssize_t count;
    do {
        count = read(fd, &value, 1);
    } while (count < 0 && errno == EINTR);
    if (count == 1 && value == expected)
        return 0;
    if (count >= 0)
        errno = EIO;
    return -1;
}

int main(void)
{
    char workspace[] = "/tmp/vfs-fd-demo-XXXXXX";
    const char text[] = "ABC";
    int dir_fd = -1, first_fd = -1, alias_fd = -1, independent_fd = -1;
    int reused_fd = -1, old_number = -1, flags, alias_flags, other_flags;
    int result = EXIT_FAILURE;
    bool have_workspace = false, have_file = false;
    size_t written = 0;

    if (!mkdtemp(workspace))
        goto error;
    have_workspace = true;
    dir_fd = open(workspace, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0)
        goto error;
    first_fd = openat(dir_fd, "value", O_RDWR | O_CREAT | O_EXCL, 0600);
    if (first_fd < 0)
        goto error;
    have_file = true;
    while (written < sizeof(text) - 1) {
        ssize_t count = write(first_fd, text + written, sizeof(text) - 1 - written);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            if (count == 0)
                errno = EIO;
            goto error;
        }
        written += (size_t)count;
    }
    if (lseek(first_fd, 0, SEEK_SET) < 0)
        goto error;
    alias_fd = dup(first_fd);
    independent_fd = openat(dir_fd, "value", O_RDONLY | O_CLOEXEC);
    if (alias_fd < 0 || independent_fd < 0)
        goto error;
    if (expect_byte(first_fd, 'A') || expect_byte(alias_fd, 'B') ||
        expect_byte(independent_fd, 'A'))
        goto error;

    /* 描述符标志属于各自槽位，状态标志属于共享的打开关系。 */
    if (fcntl(first_fd, F_SETFD, FD_CLOEXEC) < 0)
        goto error;
    alias_flags = fcntl(alias_fd, F_GETFD);
    if (alias_flags < 0)
        goto error;
    if (alias_flags & FD_CLOEXEC) {
        errno = EIO;
        goto error;
    }
    flags = fcntl(alias_fd, F_GETFL);
    if (flags < 0 || fcntl(alias_fd, F_SETFL, flags | O_APPEND) < 0)
        goto error;
    flags = fcntl(first_fd, F_GETFL);
    other_flags = fcntl(independent_fd, F_GETFL);
    if (flags < 0 || other_flags < 0)
        goto error;
    if (!(flags & O_APPEND) || (other_flags & O_APPEND)) {
        errno = EIO;
        goto error;
    }

    old_number = first_fd;
    if (close_owned(&first_fd) < 0 || expect_byte(alias_fd, 'C'))
        goto error;
    /* 在这个单线程程序中定向复用旧槽位，不假定下次 open 的号码。 */
    reused_fd = dup2(independent_fd, old_number);
    if (reused_fd < 0 || expect_byte(reused_fd, 'B'))
        goto error;
    puts("dup共享位置与状态标志；CLOEXEC独立；关闭一项后别名仍有效；旧号码已改指另一打开关系");
    result = EXIT_SUCCESS;
    goto out;
error:
    perror("描述符生命周期实验");
out:
    if (close_owned(&reused_fd) < 0)
        result = EXIT_FAILURE;
    if (close_owned(&first_fd) < 0)
        result = EXIT_FAILURE;
    if (close_owned(&alias_fd) < 0)
        result = EXIT_FAILURE;
    if (close_owned(&independent_fd) < 0)
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
