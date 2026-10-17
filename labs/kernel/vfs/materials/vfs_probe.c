#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int show_file(const char *stage, int fd)
{
    struct stat metadata;
    char path[64], target[512], buffer[512];
    ssize_t length;
    FILE *info;
    if (fstat(fd, &metadata) < 0) { perror("fstat"); return -1; }
    int size = snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    if (size < 0 || (size_t)size >= sizeof(path)) return -1;
    length = readlink(path, target, sizeof(target) - 1);
    if (length < 0) { perror("readlink"); return -1; }
    target[length] = '\0';
    printf("%s pid=%jd fd=%d dev=%ju inode=%ju links=%ju\n显示路径=%s\n",
           stage, (intmax_t)getpid(), fd, (uintmax_t)metadata.st_dev,
           (uintmax_t)metadata.st_ino, (uintmax_t)metadata.st_nlink, target);
    size = snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", fd);
    if (size < 0 || (size_t)size >= sizeof(path)) return -1;
    info = fopen(path, "r");
    if (!info) { perror("open fdinfo"); return -1; }
    while (fgets(buffer, sizeof(buffer), info)) fputs(buffer, stdout);
    int failed = ferror(info);
    if (fclose(info) != 0) failed = 1;
    if (failed) fputs("读取fdinfo失败\n", stderr);
    return failed ? -1 : 0;
}

int main(void)
{
    char path[] = "/tmp/vfs_probe.XXXXXX";
    const char payload[] = "hello";
    char received[sizeof(payload)];
    int fd = -1, have_name = 0, result = EXIT_FAILURE;
    size_t done = 0;
    fd = mkstemp(path);
    if (fd < 0) { perror("mkstemp"); goto cleanup; }
    have_name = 1;
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) { perror("fcntl"); goto cleanup; }
    while (done < sizeof(payload)) {
        ssize_t count = write(fd, payload + done, sizeof(payload) - done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { fputs("写入失败\n", stderr); goto cleanup; }
        done += (size_t)count;
    }
    if (show_file("删除前", fd) < 0) goto cleanup;
    if (unlink(path) < 0) { perror("unlink"); goto cleanup; }
    have_name = 0;
    /* 目录名查找与已有file的使用分开观察。 */
    int fresh_fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fresh_fd >= 0) {
        close(fresh_fd); fputs("名字被重新建立，停止本次观察\n", stderr); goto cleanup;
    }
    if (errno != ENOENT) { perror("再次open的错误不是ENOENT"); goto cleanup; }
    puts("再次open原名字：ENOENT");
    if (lseek(fd, 0, SEEK_SET) < 0) { perror("lseek"); goto cleanup; }
    done = 0;
    while (done < sizeof(received)) {
        ssize_t count = read(fd, received + done, sizeof(received) - done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { fputs("旧fd读取失败\n", stderr); goto cleanup; }
        done += (size_t)count;
    }
    if (memcmp(received, payload, sizeof(payload))) { fputs("内容不符\n", stderr); goto cleanup; }
    puts("旧fd仍读到hello");
    if (show_file("删除后", fd) < 0) goto cleanup;
    result = EXIT_SUCCESS;
cleanup:
    if (fd >= 0 && close(fd) < 0) { perror("close"); result = EXIT_FAILURE; }
    if (have_name && unlink(path) < 0) { perror("cleanup unlink"); result = EXIT_FAILURE; }
    return result;
}
