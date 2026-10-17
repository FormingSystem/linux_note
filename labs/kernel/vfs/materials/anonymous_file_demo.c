#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <unistd.h>

static int show_identity(const char *label, int fd)
{
    char path[64], target[256];
    struct stat metadata;
    int size = snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    if (size < 0 || (size_t)size >= sizeof(path)) return -1;
    if (fstat(fd, &metadata) < 0) { perror("fstat"); return -1; }
    ssize_t count = readlink(path, target, sizeof(target) - 1);
    if (count < 0) { perror("readlink"); return -1; }
    target[count] = '\0';
    printf("%s fd=%d inode=%ju 显示名称=%s\n", label, fd,
           (uintmax_t)metadata.st_ino, target);
    return 0;
}

static int read_count(int fd, uint64_t expected)
{
    uint64_t value = 0;
    ssize_t count;
    do { count = read(fd, &value, sizeof(value)); } while (count < 0 && errno == EINTR);
    if (count != (ssize_t)sizeof(value)) { fputs("读取计数失败\n", stderr); return -1; }
    if (value != expected) { fputs("读取计数不符\n", stderr); return -1; }
    printf("本次取走计数=%" PRIu64 "\n", value);
    return 0;
}

int main(void)
{
    int first_fd = -1, second_fd = -1, duplicate_fd = -1;
    int result = EXIT_FAILURE;
    uint64_t value;
    ssize_t count;

    /* 两次创建建立两份计数对象，非阻塞方式使空对象立即报告EAGAIN。 */
    first_fd = eventfd(2, EFD_NONBLOCK | EFD_CLOEXEC);
    if (first_fd < 0) { perror("eventfd first"); goto cleanup; }
    second_fd = eventfd(7, EFD_NONBLOCK | EFD_CLOEXEC);
    if (second_fd < 0) { perror("eventfd second"); goto cleanup; }
    duplicate_fd = fcntl(first_fd, F_DUPFD_CLOEXEC, 0);
    if (duplicate_fd < 0) { perror("duplicate"); goto cleanup; }
    if (show_identity("第一次创建", first_fd) < 0 ||
        show_identity("第二次创建", second_fd) < 0 ||
        show_identity("第一次的复制", duplicate_fd) < 0) goto cleanup;

    /* 复制描述符共享原file和计数，读取复制者会消费第一次创建的对象。 */
    if (read_count(duplicate_fd, 2) < 0) goto cleanup;
    do { count = read(first_fd, &value, sizeof(value)); } while (count < 0 && errno == EINTR);
    if (count != -1 || errno != EAGAIN) {
        fputs("原描述符没有观察到共享对象已空\n", stderr); goto cleanup;
    }
    puts("原描述符返回EAGAIN；第二次创建的对象仍应保有7");
    if (read_count(second_fd, 7) < 0) goto cleanup;
    result = EXIT_SUCCESS;
cleanup:
    if (duplicate_fd >= 0 && close(duplicate_fd) < 0) { perror("close duplicate"); result = EXIT_FAILURE; }
    if (second_fd >= 0 && close(second_fd) < 0) { perror("close second"); result = EXIT_FAILURE; }
    if (first_fd >= 0 && close(first_fd) < 0) { perror("close first"); result = EXIT_FAILURE; }
    return result;
}
