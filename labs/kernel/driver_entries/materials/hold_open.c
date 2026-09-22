/* 保留打开文件，供另一个终端观察普通模块卸载的引用限制。 */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    int fd = open("/dev/note_misc", O_RDONLY);
    int result = 0;
    if (fd < 0) { perror("open"); return 1; }
    puts("设备保持打开；现在可在终端 B 尝试普通卸载。");
    puts("按回车关闭设备：");
    if (getchar() == EOF && ferror(stdin)) {
        perror("stdin");
        result = 1;
    }
    if (close(fd) < 0) { perror("close"); result = 1; }
    return result;
}
