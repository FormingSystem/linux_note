#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

int main(void)
{
    char directory[] = "/tmp/vfs_notify_XXXXXX";
    char before[sizeof(directory) + 16], after[sizeof(directory) + 16];
    unsigned char buffer[4096];
    int notify_fd = -1, data_fd = -1, wd = -1, location = 0;
    int result = EXIT_FAILURE, seen_create = 0, seen_modify = 0;
    int seen_close = 0, seen_from = 0, seen_to = 0, seen_delete = 0;
    uint32_t from_cookie = 0, to_cookie = 0;

    if (!mkdtemp(directory)) { perror("mkdtemp"); return EXIT_FAILURE; }
    snprintf(before, sizeof(before), "%s/before", directory);
    snprintf(after, sizeof(after), "%s/after", directory);
    notify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (notify_fd < 0) { perror("inotify_init1"); goto done; }
    /* 先建立观察，再制造变化；目录观察只覆盖这里的直接子项。 */
    wd = inotify_add_watch(notify_fd, directory,
                          IN_CREATE | IN_MODIFY | IN_CLOSE_WRITE |
                          IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE | IN_ONLYDIR);
    if (wd < 0) { perror("inotify_add_watch"); goto done; }
    data_fd = open(before, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (data_fd < 0) { perror("create"); goto done; }
    location = 1;
    if (write(data_fd, "N", 1) != 1) {
        fputs("写入单字节失败\n", stderr); goto done;
    }
    if (close(data_fd) < 0) { data_fd = -1; perror("close data"); goto done; }
    data_fd = -1;
    if (rename(before, after) < 0) { perror("rename"); goto done; }
    location = 2;
    if (unlink(after) < 0) { perror("unlink"); goto done; }
    location = 0;

    /* 操作已经结束，接下来读到的是历史提示，文件现在已经不存在。 */
    struct pollfd ready = { .fd = notify_fd, .events = POLLIN };
    int poll_result;
    do { poll_result = poll(&ready, 1, 5000); } while (poll_result < 0 && errno == EINTR);
    if (poll_result < 0) { perror("poll"); goto done; }
    if (poll_result == 0 || !(ready.revents & POLLIN) ||
        (ready.revents & (POLLERR | POLLHUP | POLLNVAL))) {
        fputs("未取得可读取的事件队列\n", stderr); goto done;
    }
    for (;;) {
        ssize_t count = read(notify_fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && errno == EAGAIN) break;
        if (count <= 0) { fputs("读取事件失败\n", stderr); goto done; }
        size_t offset = 0;
        while (offset < (size_t)count) {
            struct inotify_event event;
            size_t remaining = (size_t)count - offset;
            /* memcpy读取固定头，避免把任意字节地址直接当作对齐的结构体。 */
            if (remaining < sizeof(event)) { fputs("事件头不完整\n", stderr); goto done; }
            memcpy(&event, buffer + offset, sizeof(event));
            if (event.len > remaining - sizeof(event)) {
                fputs("事件名称区越界\n", stderr); goto done;
            }
            const char *name = (const char *)buffer + offset + sizeof(event);
            if (event.len && !memchr(name, '\0', event.len)) {
                fputs("事件名称没有结束符\n", stderr); goto done;
            }
            if (event.mask & IN_Q_OVERFLOW) {
                fputs("队列溢出，事件历史不完整，应重新扫描\n", stderr); goto done;
            }
            if (event.wd != wd || (event.mask & (IN_IGNORED | IN_UNMOUNT))) {
                fputs("观察关系已变化，本次对照无法完成\n", stderr); goto done;
            }
            printf("wd=%d mask=0x%08" PRIx32 " cookie=%" PRIu32 " name=%s\n",
                   event.wd, event.mask, event.cookie, event.len ? name : "(自身)");
            if (event.len && strcmp(name, "before") == 0) {
                if (event.mask & IN_CREATE) seen_create = 1;
                if (event.mask & IN_MODIFY) seen_modify = 1;
                if (event.mask & IN_CLOSE_WRITE) seen_close = 1;
                if (event.mask & IN_MOVED_FROM) { seen_from = 1; from_cookie = event.cookie; }
            }
            if (event.len && strcmp(name, "after") == 0) {
                if (event.mask & IN_MOVED_TO) { seen_to = 1; to_cookie = event.cookie; }
                if (event.mask & IN_DELETE) seen_delete = 1;
            }
            offset += sizeof(event) + event.len;
        }
    }
    if (!(seen_create && seen_modify && seen_close && seen_from && seen_to && seen_delete) ||
        from_cookie != to_cookie) {
        fputs("事件集合或rename配对不符合本次受控实验\n", stderr); goto done;
    }
    puts("观察到创建、修改、关闭、改名和删除；名称来自过去事件，不代表文件仍存在");
    result = EXIT_SUCCESS;
done:
    /* 每个路径只在本程序仍拥有相应文件时清理。 */
    if (data_fd >= 0 && close(data_fd) < 0) { perror("close cleanup"); result = EXIT_FAILURE; }
    if (location && unlink(location == 1 ? before : after) < 0) {
        perror("unlink cleanup"); result = EXIT_FAILURE;
    }
    if (wd >= 0 && inotify_rm_watch(notify_fd, wd) < 0 && errno != EINVAL) {
        perror("inotify_rm_watch"); result = EXIT_FAILURE;
    }
    if (notify_fd >= 0 && close(notify_fd) < 0) { perror("close notify"); result = EXIT_FAILURE; }
    if (rmdir(directory) < 0) { perror("rmdir"); result = EXIT_FAILURE; }
    return result;
}
