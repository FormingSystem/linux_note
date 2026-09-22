/* 只读观察路径号码、已打开对象与一次读取，不创建节点。 */
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

int main(void)
{
    struct stat before, opened;
    int fd = -1, result = 1;
    char byte;
    ssize_t count;

    if (stat("/dev/null", &before) < 0) {
        perror("stat");
        return 1;
    }
    if (!S_ISCHR(before.st_mode)) {
        fputs("/dev/null 不是字符设备\n", stderr);
        return 1;
    }
    printf("path number: %u %u\n", major(before.st_rdev), minor(before.st_rdev));
    fd = open("/dev/null", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    if (fstat(fd, &opened) < 0) {
        perror("fstat");
        goto out;
    }
    if (!S_ISCHR(opened.st_mode) || opened.st_rdev != before.st_rdev) {
        fputs("打开对象的类型或设备号与先前观察不符\n", stderr);
        goto out;
    }
    printf("opened number: %u %u\n", major(opened.st_rdev), minor(opened.st_rdev));
    count = read(fd, &byte, 1);
    if (count < 0) {
        perror("read");
        goto out;
    }
    printf("one read: %zd bytes\n", count);
    result = count == 0 ? 0 : 1;
out:
    if (close(fd) < 0) {
        perror("close");
        result = 1;
    }
    return result;
}
