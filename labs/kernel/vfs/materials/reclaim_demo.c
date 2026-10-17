#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    char directory[] = "/tmp/vfs_reclaim_XXXXXX";
    char pathname[sizeof(directory) + 16];
    const char payload[] = "alive";
    char observed[sizeof(payload)];
    const size_t length = sizeof(payload);
    struct stat status;
    int first_fd = -1, second_fd = -1, named = 0;
    int result = EXIT_FAILURE;
    void *mapping = MAP_FAILED;
    size_t done = 0;

    if (!mkdtemp(directory)) { perror("mkdtemp"); return EXIT_FAILURE; }
    snprintf(pathname, sizeof(pathname), "%s/object", directory);
    first_fd = open(pathname, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (first_fd < 0) { perror("open"); goto cleanup; }
    named = 1;
    while (done < length) {
        ssize_t count = write(first_fd, payload + done, length - done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { fputs("写入失败\n", stderr); goto cleanup; }
        done += (size_t)count;
    }
    second_fd = dup(first_fd);
    if (second_fd < 0) { perror("dup"); goto cleanup; }
    mapping = mmap(NULL, length, PROT_READ, MAP_SHARED, first_fd, 0);
    if (mapping == MAP_FAILED) { perror("mmap"); goto cleanup; }

    /* 名称撤销后，用已有fd检查链接数；不以路径存在性判断对象存活。 */
    if (unlink(pathname) < 0) { perror("unlink"); goto cleanup; }
    named = 0;
    if (fstat(first_fd, &status) < 0) { perror("fstat"); goto cleanup; }
    if (status.st_nlink != 0) { fputs("链接数并非零\n", stderr); goto cleanup; }
    errno = 0;
    if (stat(pathname, &status) == 0 || errno != ENOENT) {
        fputs("原名称没有按预期消失\n", stderr); goto cleanup;
    }
    puts("R1：名称已经消失，但已有对象的链接数仍可通过fd观察为零");

    if (close(first_fd) < 0) { first_fd = -1; perror("close first"); goto cleanup; }
    first_fd = -1;
    /* dup共享同一个打开对象；pread避免依赖此前write留下的共享文件位置。 */
    done = 0;
    while (done < length) {
        ssize_t count = pread(second_fd, observed + done, length - done, (off_t)done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { fputs("旧fd读取失败\n", stderr); goto cleanup; }
        done += (size_t)count;
    }
    if (memcmp(observed, payload, length)) { fputs("fd内容不符\n", stderr); goto cleanup; }
    if (close(second_fd) < 0) { second_fd = -1; perror("close second"); goto cleanup; }
    second_fd = -1;

    /* 两个fd都已关闭，映射仍独立持有file；只读文件实际长度范围内的字节。 */
    if (memcmp(mapping, payload, length)) { fputs("映射内容不符\n", stderr); goto cleanup; }
    puts("R2：两个fd都已关闭，先前映射仍能读取alive");
    if (munmap(mapping, length) < 0) { perror("munmap"); goto cleanup; }
    mapping = MAP_FAILED;
    puts("已归还本程序最后一份映射；此输出不能证明内核内存或磁盘空间已经最终回收");
    result = EXIT_SUCCESS;

cleanup:
    if (mapping != MAP_FAILED && munmap(mapping, length) < 0) {
        perror("munmap cleanup"); result = EXIT_FAILURE;
    }
    if (first_fd >= 0 && close(first_fd) < 0) { perror("close cleanup"); result = EXIT_FAILURE; }
    if (second_fd >= 0 && close(second_fd) < 0) { perror("close dup cleanup"); result = EXIT_FAILURE; }
    if (named && unlink(pathname) < 0) { perror("unlink cleanup"); result = EXIT_FAILURE; }
    if (rmdir(directory) < 0) { perror("rmdir"); result = EXIT_FAILURE; }
    return result;
}
