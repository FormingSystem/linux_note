/* 条件：八字节空队列，只有本程序读写；失败后停止，不自动重放。 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int readable(int fd, int expected)
{
    struct pollfd item = { .fd = fd, .events = POLLIN, .revents = 0 };
    int count = poll(&item, 1, 0);
    if (count < 0) {
        perror("poll");
        return 0;
    }
    if ((item.revents & (POLLERR | POLLHUP | POLLNVAL)) ||
        !!(item.revents & POLLIN) != expected) {
        fprintf(stderr, "就绪条件不符: revents=%x\n", (unsigned int)item.revents);
        return 0;
    }
    return 1;
}

static int write_part(int fd, const char *data, size_t length, ssize_t expected)
{
    ssize_t count = write(fd, data, length);
    if (count == expected)
        return 1;
    if (count < 0)
        perror("write");
    else
        fprintf(stderr, "write: 预期 %zd，实际 %zd\n", expected, count);
    return 0;
}

static int read_part(int fd, size_t requested, const char *expected)
{
    char data[8];
    size_t length = strlen(expected);
    ssize_t count = read(fd, data, requested);
    if (count < 0) {
        perror("read");
        return 0;
    }
    if ((size_t)count != length || memcmp(data, expected, length) != 0) {
        fputs("读取长度或内容不符\n", stderr);
        return 0;
    }
    return 1;
}

int main(void)
{
    int reader = -1, writer = -1, result = 1;
    struct stat left, right;
    char byte;
    ssize_t count;

    reader = open("/dev/note_stream0", O_RDONLY | O_NONBLOCK);
    if (reader < 0) { perror("open reader"); goto out; }
    writer = open("/dev/note_stream0", O_WRONLY | O_NONBLOCK);
    if (writer < 0) { perror("open writer"); goto out; }
    if (fstat(reader, &left) < 0 || fstat(writer, &right) < 0) {
        perror("fstat"); goto out;
    }
    if (!S_ISCHR(left.st_mode) || !S_ISCHR(right.st_mode) ||
        left.st_rdev != right.st_rdev) {
        fputs("两个入口不是同号字符设备\n", stderr); goto out;
    }
    if (!readable(reader, 0))
        goto out;
    count = read(reader, &byte, 1);
    if (count != -1 || errno != EAGAIN) {
        fputs("空流没有返回 EAGAIN\n", stderr); goto out;
    }
    if (!write_part(writer, "ABCDEF", 6, 6) || !readable(reader, 1) ||
        !read_part(reader, 4, "ABCD") ||
        !write_part(writer, "wxyz", 4, 2) || /* 本次只到数组末尾。 */
        !write_part(writer, "yz", 2, 2) ||
        !read_part(reader, 6, "EFwx") ||    /* 读取同样在末尾短返回。 */
        !read_part(reader, 2, "yz") || !readable(reader, 0))
        goto out;
    puts("回绕、短传输和可读条件符合预期");
    result = 0;
out:
    if (writer >= 0 && close(writer) < 0) { perror("close writer"); result = 1; }
    if (reader >= 0 && close(reader) < 0) { perror("close reader"); result = 1; }
    return result;
}
