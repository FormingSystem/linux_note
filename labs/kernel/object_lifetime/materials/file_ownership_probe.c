/* Linux用户态：两个独立open各持一份，dup只共享已有文件实例。 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "note_kref_file_protocol.h"

static int close_one(int *fd)
{
    int owned = *fd;
    *fd = -1; /* Linux close错误也不对可能复用的号码盲目重试。 */
    if (owned >= 0 && close(owned) < 0) {
        perror("close");
        return 0;
    }
    return 1;
}
int main(void)
{
    int first = -1, alias = -1, separate = -1, rejected = -1, result = 1;
    first = open("/dev/note_kref_file", O_RDWR);
    if (first < 0) { perror("open first"); goto out; }
    alias = dup(first);
    if (alias < 0) { perror("dup"); goto out; }
    separate = open("/dev/note_kref_file", O_RDWR);
    if (separate < 0) { perror("open separate"); goto out; }
    if (!close_one(&first)) goto out;
    if (ioctl(alias, NOTE_FILE_STEP, 0) < 0) { perror("step alias"); goto out; }
    if (ioctl(separate, NOTE_FILE_CLOSE, 0) < 0) { perror("close business"); goto out; }
    errno = 0;
    if (ioctl(alias, NOTE_FILE_STEP, 0) != -1 || errno != ESHUTDOWN) {
        fputs("关闭后仍接纳业务或返回了其他错误\n", stderr);
        goto out;
    }
    errno = 0;
    rejected = open("/dev/note_kref_file", O_RDWR);
    if (rejected >= 0 || errno != ESHUTDOWN) {
        fputs("关闭后open没有按契约拒绝\n", stderr);
        goto out;
    }
    puts("共享文件仍有效，业务门关闭后拒绝请求和新open");
    result = 0;
out:
    if (!close_one(&first)) result = 1;
    if (!close_one(&alias)) result = 1;
    if (!close_one(&separate)) result = 1;
    if (!close_one(&rejected)) result = 1;
    return result;
}
