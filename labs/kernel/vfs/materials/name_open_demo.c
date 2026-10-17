/* Linux/POSIX 用户态实验：名称、独立打开和 dup 共享的进度。 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static int read_one(int fd, const char *label)
{
    char value;
    ssize_t count;

    do {
        count = read(fd, &value, 1);
    } while (count < 0 && errno == EINTR);
    if (count < 0) {
        perror("read");
        return -1;
    }
    if (count != 1) {
        fprintf(stderr, "%s: unexpected EOF\n", label);
        return -1;
    }
    printf("%s: %c\n", label, value);
    return 0;
}

int main(void)
{
    char directory[] = "/tmp/vfs-demo-XXXXXX";
    char first[128], alias[128];
    int initial = -1, duplicate = -1, independent = -1;
    int probe = -1, status = EXIT_FAILURE;
    int first_exists = 0, alias_exists = 0;
    struct stat initial_stat, other_stat;
    const char text[] = "ABCDE";
    size_t done = 0;

    /* 只操作本次创建的私有目录，失败时按已取得的资源逆序清理。 */
    if (!mkdtemp(directory)) { perror("mkdtemp"); return status; }
    snprintf(first, sizeof(first), "%s/first", directory);
    snprintf(alias, sizeof(alias), "%s/alias", directory);
    initial = open(first, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (initial < 0) { perror("open first"); goto out; }
    first_exists = 1;
    while (done < sizeof(text) - 1) {
        ssize_t count = write(initial, text + done, sizeof(text) - 1 - done);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { perror("write"); goto out; }
        if (count == 0) { fputs("write made no progress\n", stderr); goto out; }
        done += (size_t)count;
    }
    if (lseek(initial, 0, SEEK_SET) < 0) { perror("lseek"); goto out; }
    if (link(first, alias) < 0) { perror("link"); goto out; }
    alias_exists = 1;
    duplicate = dup(initial);
    if (duplicate < 0) { perror("dup"); goto out; }
    independent = open(alias, O_RDONLY);
    if (independent < 0) { perror("open alias"); goto out; }
    if (fstat(initial, &initial_stat) < 0 ||
        fstat(independent, &other_stat) < 0) { perror("fstat"); goto out; }
    printf("same object: %s; names: %lu\n",
           initial_stat.st_dev == other_stat.st_dev &&
           initial_stat.st_ino == other_stat.st_ino ? "yes" : "no",
           (unsigned long)initial_stat.st_nlink);
    if (read_one(initial, "first open") < 0 ||
        read_one(duplicate, "dup of first") < 0 ||
        read_one(independent, "independent open") < 0) goto out;
    if (unlink(first) < 0) { perror("unlink first"); goto out; }
    first_exists = 0;
    if (unlink(alias) < 0) { perror("unlink alias"); goto out; }
    alias_exists = 0;
    if (fstat(initial, &initial_stat) < 0) { perror("fstat after unlink"); goto out; }
    printf("names after unlink: %lu\n", (unsigned long)initial_stat.st_nlink);
    if (read_one(initial, "old open after unlink") < 0) goto out;
    errno = 0;
    probe = open(first, O_RDONLY);
    if (probe >= 0 || errno != ENOENT) {
        fprintf(stderr, "new open did not fail with ENOENT\n");
        goto out;
    }
    puts("new open: ENOENT");
    status = EXIT_SUCCESS;
out:
    /* Linux close 出错也不盲目重试同一个数字，防止误关已复用描述符。 */
    if (probe >= 0 && close(probe) < 0) { perror("close probe"); status = EXIT_FAILURE; }
    if (independent >= 0 && close(independent) < 0) { perror("close independent"); status = EXIT_FAILURE; }
    if (duplicate >= 0 && close(duplicate) < 0) { perror("close duplicate"); status = EXIT_FAILURE; }
    if (initial >= 0 && close(initial) < 0) { perror("close initial"); status = EXIT_FAILURE; }
    if (alias_exists && unlink(alias) < 0) { perror("cleanup alias"); status = EXIT_FAILURE; }
    if (first_exists && unlink(first) < 0) { perror("cleanup first"); status = EXIT_FAILURE; }
    if (rmdir(directory) < 0) { perror("rmdir"); status = EXIT_FAILURE; }
    return status;
}
