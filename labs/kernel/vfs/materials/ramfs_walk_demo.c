#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/magic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    char directory[PATH_MAX];
    const char payload[] = "ramfs";
    char result_data[sizeof(payload)];
    struct statfs filesystem;
    struct stat metadata;
    int base_fd = -1, dir_fd = -1, file_fd = -1;
    int have_directory = 0, have_a = 0, have_b = 0, have_c = 0;
    int result = EXIT_FAILURE;
    size_t done = 0;

    if (argc != 2) { fprintf(stderr, "用法：%s ramfs挂载目录\n", argv[0]); return EXIT_FAILURE; }
    base_fd = open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (base_fd < 0) { perror("open base"); goto cleanup; }
    if (fstatfs(base_fd, &filesystem) < 0) { perror("fstatfs"); goto cleanup; }
    if ((unsigned long)filesystem.f_type != RAMFS_MAGIC) {
        fputs("目标不是ramfs，停止实验\n", stderr); goto cleanup;
    }
    int size = snprintf(directory, sizeof(directory), "%s/demo_XXXXXX", argv[1]);
    if (size < 0 || (size_t)size >= sizeof(directory)) { fputs("路径过长\n", stderr); goto cleanup; }
    if (!mkdtemp(directory)) { perror("mkdtemp"); goto cleanup; }
    have_directory = 1;
    dir_fd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0) { perror("open directory"); goto cleanup; }
    file_fd = openat(dir_fd, "a", O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
    if (file_fd < 0) { perror("openat a"); goto cleanup; }
    have_a = 1;
    while (done < sizeof(payload)) {
        ssize_t count = write(file_fd, payload + done, sizeof(payload) - done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { fputs("写入失败\n", stderr); goto cleanup; }
        done += (size_t)count;
    }
    if (fsync(file_fd) < 0) { perror("fsync"); goto cleanup; }
    puts("fsync返回成功；ramfs没有因此获得磁盘副本");

    /* 两个名称共享inode；改名只改变其中一个名称，随后删除原名。 */
    if (linkat(dir_fd, "a", dir_fd, "b", 0) < 0) { perror("linkat"); goto cleanup; }
    have_b = 1;
    if (renameat(dir_fd, "b", dir_fd, "c") < 0) { perror("renameat"); goto cleanup; }
    have_b = 0; have_c = 1;
    if (unlinkat(dir_fd, "a", 0) < 0) { perror("unlinkat a"); goto cleanup; }
    have_a = 0;
    if (fstat(file_fd, &metadata) < 0) { perror("fstat"); goto cleanup; }
    if (metadata.st_nlink != 1) { fputs("剩余链接数不符\n", stderr); goto cleanup; }
    printf("删除a后仍有c，inode链接数=%ju\n", (uintmax_t)metadata.st_nlink);
    if (close(file_fd) < 0) { file_fd = -1; perror("close a"); goto cleanup; }
    file_fd = -1;

    /* 此时没有打开的文件fd；正名称及数据仍由文件系统的驻留责任保留。 */
    file_fd = openat(dir_fd, "c", O_RDONLY | O_CLOEXEC);
    if (file_fd < 0) { perror("reopen c"); goto cleanup; }
    done = 0;
    while (done < sizeof(payload)) {
        ssize_t count = read(file_fd, result_data + done, sizeof(payload) - done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { fputs("重新读取失败\n", stderr); goto cleanup; }
        done += (size_t)count;
    }
    if (memcmp(result_data, payload, sizeof(payload))) { fputs("内容不符\n", stderr); goto cleanup; }
    puts("全部文件fd关闭以后，重新打开c仍读到ramfs");
    result = EXIT_SUCCESS;

cleanup:
    if (file_fd >= 0 && close(file_fd) < 0) { perror("close file"); result = EXIT_FAILURE; }
    /* 只清理这个新建私有目录中由本程序成功创建的名称。 */
    if (have_a && unlinkat(dir_fd, "a", 0) < 0) { perror("unlink a"); result = EXIT_FAILURE; }
    if (have_b && unlinkat(dir_fd, "b", 0) < 0) { perror("unlink b"); result = EXIT_FAILURE; }
    if (have_c && unlinkat(dir_fd, "c", 0) < 0) { perror("unlink c"); result = EXIT_FAILURE; }
    if (dir_fd >= 0 && close(dir_fd) < 0) { perror("close directory"); result = EXIT_FAILURE; }
    if (have_directory && rmdir(directory) < 0) { perror("rmdir"); result = EXIT_FAILURE; }
    if (base_fd >= 0 && close(base_fd) < 0) { perror("close base"); result = EXIT_FAILURE; }
    return result;
}
