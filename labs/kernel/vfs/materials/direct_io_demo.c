#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int close_owned(int *fd)
{
    int number = *fd;
    *fd = -1;
    return number < 0 ? 0 : close(number);
}

int main(int argc, char **argv)
{
    char *workspace = NULL;
    void *out_buffer = NULL, *in_buffer = NULL;
    struct statx info = {0};
    size_t alignment, length;
    int dir_fd = -1, file_fd = -1, result = EXIT_FAILURE;
    int have_dir = 0, have_file = 0, error;
    ssize_t count;
    if (argc != 2) {
        fprintf(stderr, "用法：%s <可写测试目录>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (asprintf(&workspace, "%s/vfs-dio-XXXXXX", argv[1]) < 0)
        goto error;
    if (!mkdtemp(workspace))
        goto error;
    have_dir = 1;
    dir_fd = open(workspace, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0)
        goto error;
    /* 先建立独占文件，确保之后不支持O_DIRECT时也知道应清理哪个文件。 */
    file_fd = openat(dir_fd, "data", O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
    if (file_fd < 0)
        goto error;
    have_file = 1;
    if (statx(file_fd, "", AT_EMPTY_PATH, STATX_DIOALIGN, &info) < 0)
        goto error;
    if (!(info.stx_mask & STATX_DIOALIGN) || !info.stx_dio_mem_align ||
        !info.stx_dio_offset_align) {
        puts("跳过：此文件未提供可用的直接I/O对齐要求");
        result = 77;
        goto out;
    }
    alignment = info.stx_dio_mem_align;
    length = info.stx_dio_offset_align;
    if ((alignment & (alignment - 1)) || length > 8U * 1024U * 1024U) {
        puts("跳过：对齐或长度超出这个小实验的分配方案");
        result = 77;
        goto out;
    }
    if (alignment < sizeof(void *))
        alignment = sizeof(void *);
    error = posix_memalign(&out_buffer, alignment, length);
    if (error) {
        errno = error;
        goto error;
    }
    error = posix_memalign(&in_buffer, alignment, length);
    if (error) {
        errno = error;
        goto error;
    }
    memset(out_buffer, 0x5a, length);
    memset(in_buffer, 0, length);
    if (close_owned(&file_fd) < 0)
        goto error;
    file_fd = openat(dir_fd, "data", O_RDWR | O_DIRECT | O_CLOEXEC);
    if (file_fd < 0)
        goto error;
    /* 同步调用返回前缓冲区保持不变；短写不盲目拆成可能失去对齐的尾请求。 */
    do {
        count = pwrite(file_fd, out_buffer, length, 0);
    } while (count < 0 && errno == EINTR);
    if (count != (ssize_t)length) {
        if (count >= 0)
            errno = EIO;
        goto error;
    }
    do {
        count = pread(file_fd, in_buffer, length, 0);
    } while (count < 0 && errno == EINTR);
    if (count != (ssize_t)length || memcmp(out_buffer, in_buffer, length)) {
        if (count >= 0)
            errno = EIO;
        goto error;
    }
    /* O_DIRECT本身不替代同步要求，独立检查fsync。 */
    do {
        error = fsync(file_fd);
    } while (error < 0 && errno == EINTR);
    if (error < 0)
        goto error;
    printf("对齐内存=%u 偏移及长度=%u；读回一致，fsync成功\n",
           info.stx_dio_mem_align, info.stx_dio_offset_align);
    result = EXIT_SUCCESS;
    goto out;
error:
    perror("直接I/O实验");
out:
    if (close_owned(&file_fd) < 0)
        result = EXIT_FAILURE;
    free(in_buffer);
    free(out_buffer);
    if (have_file && unlinkat(dir_fd, "data", 0) < 0)
        result = EXIT_FAILURE;
    if (close_owned(&dir_fd) < 0)
        result = EXIT_FAILURE;
    if (have_dir && rmdir(workspace) < 0)
        result = EXIT_FAILURE;
    if (result == EXIT_FAILURE && workspace)
        fprintf(stderr, "实验或清理失败，请检查：%s\n", workspace);
    free(workspace);
    return result;
}
