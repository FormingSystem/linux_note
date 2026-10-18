/* 在目标Linux运行；另一个终端按正文完成解绑和重新绑定。 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int expect_write(int fd, const char *command, size_t count,
                        ssize_t expected, int expected_errno)
{
    ssize_t result;
    int saved_errno;
    errno = 0;
    result = write(fd, command, count);
    saved_errno = errno;
    printf("write(%zu): result=%ld errno=%d\n", count, (long)result, saved_errno);
    return result == expected && (result >= 0 || saved_errno == expected_errno);
}

static int wait_for_operator(const char *message)
{
    int ch;
    puts(message);
    fflush(stdout);
    do {
        ch = getchar();
    } while (ch != '\n' && ch != EOF);
    return ch != EOF;
}

int main(int argc, char **argv)
{
    int old_fd;
    int new_fd = -1;
    int result = 1;
    const char invalid[] = {'0', ' ', 'o', 'n', 0};

    if (argc != 2) {
        fprintf(stderr, "usage: %s /dev/note_led\n", argv[0]);
        return 2;
    }
    old_fd = open(argv[1], O_WRONLY);
    if (old_fd < 0) {
        perror("open before unbind");
        return 1;
    }
    if (!expect_write(old_fd, "0 off\n", 6, 6, 0) ||
        !expect_write(old_fd, invalid, sizeof(invalid), -1, EINVAL) ||
        !expect_write(old_fd, "", 0, 0, 0))
        goto out;
    if (!wait_for_operator("在另一终端完成unbind后，按回车；本程序保留旧fd。") ||
        !expect_write(old_fd, "0 on\n", 5, -1, ENODEV))
        goto out;
    if (!wait_for_operator("在另一终端完成bind后，按回车；旧fd仍不应连接新设备。") ||
        !expect_write(old_fd, "0 on\n", 5, -1, ENODEV))
        goto out;
    new_fd = open(argv[1], O_WRONLY);
    if (new_fd < 0) {
        perror("open after bind");
        goto out;
    }
    if (!expect_write(new_fd, "0 off\n", 6, 6, 0))
        goto out;
    puts("旧文件被隔离，新文件访问重新绑定的实例；没有证明LED电气状态。");
    result = 0;
out:
    if (new_fd >= 0)
        close(new_fd);
    close(old_fd);
    return result;
}
