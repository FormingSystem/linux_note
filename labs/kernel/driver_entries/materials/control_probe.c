/* 只操作 note_control 教学模块，任何退出路径都尝试恢复 enabled=1。 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int set_enabled(const char *value)
{
    const char *path = "/sys/class/note_control/note_control0/enabled";
    int fd = open(path, O_WRONLY);
    int result = 0;
    ssize_t count;
    if (fd < 0) { perror("open enabled"); return 0; }
    count = write(fd, value, 2);
    if (count < 0)
        perror("write enabled");
    else if (count != 2)
        fputs("enabled 写入未完整完成\n", stderr);
    else
        result = 1;
    if (close(fd) < 0) { perror("close enabled"); result = 0; }
    return result;
}

int main(void)
{
    int fd = open("/dev/note_control0", O_RDONLY), result = 1;
    char data[64];
    ssize_t count;
    if (fd < 0) { perror("open device"); return 1; }
    count = read(fd, data, 5);
    if (count < 0) { perror("read first"); goto out; }
    if (count != 5 || memcmp(data, "hello", 5)) goto mismatch;
    puts("first=hello");
    if (!set_enabled("0\n")) goto out;
    count = read(fd, data, 3);
    if (count != -1 || errno != EACCES) goto mismatch;
    puts("禁用：EACCES");
    if (!set_enabled("1\n")) goto out;
    count = read(fd, data, sizeof(data));
    if (count < 0) { perror("read rest"); goto out; }
    if (count != 7 || memcmp(data, " class\n", 7)) goto mismatch;
    puts("rest: 7 bytes，内容为剩余的 class 与换行");
    count = read(fd, data, 1);
    if (count < 0) { perror("read end"); goto out; }
    if (count != 0) goto mismatch;
    puts("end=0");
    result = 0;
    goto out;
mismatch:
    fputs("返回值、字节或位置与预测不符\n", stderr);
out:
    /* 恢复失败必须报告，不能把测试输出成功当作现场已经恢复。 */
    if (!set_enabled("1\n")) result = 1;
    if (close(fd) < 0) { perror("close device"); result = 1; }
    return result;
}
