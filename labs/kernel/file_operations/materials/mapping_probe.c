#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

int main(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    int fd;
    void *area;

    if (page_size <= 0)
        return 1;
    fd = open("/dev/note_mapping", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    area = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (area == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    if (close(fd)) { /* 成功后不再保留数字描述符。 */
        perror("close");
        munmap(area, (size_t)page_size);
        return 1;
    }
    fwrite(area, 1, 18, stdout);
    puts("fd 已关闭；完成另一个终端的普通卸载观察后按回车。");
    (void)getchar();
    if (munmap(area, (size_t)page_size)) {
        perror("munmap");
        return 1;
    }
    return 0;
}
