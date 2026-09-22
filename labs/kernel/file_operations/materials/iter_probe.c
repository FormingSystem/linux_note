/* 比较 readv、显式位置读取和共享位置，不测复制性能。 */
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

int main(void)
{
    char first[5] = { 0 }, rest[11] = { 0 }, data[5];
    struct iovec parts[2] = {
        { .iov_base = first, .iov_len = sizeof(first) },
        { .iov_base = rest, .iov_len = sizeof(rest) }
    };
    int fd = open("/dev/note_iter", O_RDONLY);
    int result = 1;
    ssize_t count;
    if (fd < 0) { perror("open"); return 1; }
    count = readv(fd, parts, 2);
    if (count < 0) { perror("readv"); goto out; }
    if (count != 16 || memcmp(first, "hello", 5) ||
        memcmp(rest, " from iter\n", 11))
        goto mismatch;
    printf("readv=%zd first=%.*s rest_bytes=%zu\n", count, 5, first, sizeof(rest));
    count = read(fd, data, 1);
    if (count < 0) { perror("read end"); goto out; }
    if (count != 0) goto mismatch;
    puts("end=0");
    count = pread(fd, data, sizeof(data), 0);
    if (count < 0) { perror("pread"); goto out; }
    if (count != 5 || memcmp(data, "hello", 5)) goto mismatch;
    printf("positioned=%.*s\n", 5, data);
    count = read(fd, data, 1);
    if (count < 0) { perror("read still end"); goto out; }
    if (count != 0) goto mismatch;
    puts("still end=0");
    if (lseek(fd, 0, SEEK_SET) < 0) { perror("lseek"); goto out; }
    count = read(fd, data, sizeof(data));
    if (count < 0) { perror("read rewound"); goto out; }
    if (count != 5 || memcmp(data, "hello", 5)) goto mismatch;
    printf("rewound=%.*s\n", 5, data);
    result = 0;
    goto out;
mismatch:
    fputs("字节或位置结果与预测不符\n", stderr);
out:
    if (close(fd) < 0) { perror("close"); result = 1; }
    return result;
}
