/* 每次打开各自维护读取位置，错误时也关闭已有描述符。 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int read_part(int fd, size_t requested, const char *expected)
{
    char data[64];
    size_t length = strlen(expected);
    ssize_t count = read(fd, data, requested);
    if (count < 0) { perror("read"); return 0; }
    if ((size_t)count != length || memcmp(data, expected, length)) {
        fputs("读取内容或长度不符\n", stderr);
        return 0;
    }
    printf("read: %zd bytes\n", count);
    return 1;
}

int main(void)
{
    int fd = open("/dev/note_misc", O_RDONLY), result = 1;
    if (fd < 0) { perror("open"); return 1; }
    if (!read_part(fd, 5, "hello") || !read_part(fd, 64, " from misc\n") ||
        !read_part(fd, 64, ""))
        goto out;
    if (close(fd) < 0) { perror("close"); return 1; }
    fd = open("/dev/note_misc", O_RDONLY);
    if (fd < 0) { perror("reopen"); return 1; }
    if (read_part(fd, 64, "hello from misc\n"))
        result = 0;
out:
    if (close(fd) < 0) { perror("close"); result = 1; }
    return result;
}
