/* 单键evdev观察器：先取当前状态，失步后重新取状态，不伪造丢失的点击。 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define WORD_BITS (8U * sizeof(unsigned long))
#define KEY_WORDS ((KEY_CNT + WORD_BITS - 1U) / WORD_BITS)
static volatile sig_atomic_t stop_requested;

struct key_reader {
    unsigned int code;
    bool dropping;
    bool pressed;
};

static void request_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static bool bitmap_has(const unsigned long *bits, unsigned int code)
{
    return !!(bits[code / WORD_BITS] & (1UL << (code % WORD_BITS)));
}

static int snapshot_key(int fd, struct key_reader *reader, const char *reason)
{
    unsigned long bits[KEY_WORDS] = {0};
    int bytes;

    do {
        bytes = ioctl(fd, EVIOCGKEY(sizeof(bits)), bits);
    } while (bytes < 0 && errno == EINTR && !stop_requested);
    if (bytes < 0)
        return -1;
    if ((unsigned int)bytes * 8U <= reader->code) {
        errno = EPROTO;
        return -1;
    }
    reader->pressed = bitmap_has(bits, reader->code);
    printf("%s: code=%u current=%s (not a new click)\n",
           reason, reader->code, reader->pressed ? "pressed" : "released");
    return 0;
}

static int consume_event(int fd, struct key_reader *reader,
                         const struct input_event *event)
{
    if (event->type == EV_SYN && event->code == SYN_DROPPED) {
        reader->dropping = true;
        puts("SYN_DROPPED: event history lost; ignore until next SYN_REPORT");
        return 0;
    }
    if (reader->dropping) {
        if (event->type == EV_SYN && event->code == SYN_REPORT) {
            if (snapshot_key(fd, reader, "resync") < 0)
                return -1;
            reader->dropping = false;
        }
        return 0;
    }
    if (event->type == EV_SYN && event->code == SYN_REPORT) {
        puts("-- SYN_REPORT --");
        return 0;
    }
    if (event->type != EV_KEY || event->code != reader->code)
        return 0;
    switch (event->value) {
    case 0:
    case 1:
        reader->pressed = event->value == 1;
        printf("EV_KEY code=%u value=%d %s\n", reader->code, event->value,
               reader->pressed ? "pressed" : "released");
        break;
    case 2:
        printf("EV_KEY code=%u value=2 repeat (not a new press)\n", reader->code);
        break;
    default:
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct key_reader reader = {.code = KEY_F13};
    struct sigaction action = {0};
    unsigned long capabilities[KEY_WORDS] = {0};
    struct pollfd wait_fd;
    char name[256] = {0};
    int fd, result = EXIT_FAILURE, bytes;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s /dev/input/eventX [keycode]\n", argv[0]);
        return 2;
    }
    if (argc == 3) {
        char *end;
        unsigned long code;
        errno = 0;
        code = strtoul(argv[2], &end, 10);
        if (errno || end == argv[2] || *end || !code || code > KEY_MAX) {
            fputs("keycode must be 1..KEY_MAX\n", stderr);
            return 2;
        }
        reader.code = (unsigned int)code;
    }
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL)) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }
    if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 0) {
        perror("EVIOCGNAME");
        goto out;
    }
    bytes = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(capabilities)), capabilities);
    if (bytes < 0) {
        perror("EVIOCGBIT");
        goto out;
    }
    if ((unsigned int)bytes * 8U <= reader.code ||
        !bitmap_has(capabilities, reader.code)) {
        fputs("device does not advertise selected key\n", stderr);
        goto out;
    }
    printf("device=%s code=%u\n", name, reader.code);
    if (snapshot_key(fd, &reader, "initial") < 0) {
        perror("EVIOCGKEY initial");
        goto out;
    }
    wait_fd.fd = fd;
    wait_fd.events = POLLIN;
    while (!stop_requested) {
        struct input_event event;
        ssize_t size;
        int ready;

        wait_fd.revents = 0;
        /* 有限超时让停止标志与poll之间的窄窗口也能退出。 */
        ready = poll(&wait_fd, 1, 1000);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            goto out;
        }
        if (!ready)
            continue; /* 超时只是没有可读帧，不能据此宣布设备失效。 */
        if (wait_fd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fputs("event device disconnected or poll failed\n", stderr);
            goto out;
        }
        if (!(wait_fd.revents & POLLIN))
            continue;
        /* 一次只取一条，重同步后没有用户缓冲中的旧批次尾巴需要重放。 */
        size = read(fd, &event, sizeof(event));
        if (size < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            perror("read");
            goto out;
        }
        if (size != (ssize_t)sizeof(event)) {
            fputs("unexpected EOF or incomplete input_event\n", stderr);
            goto out;
        }
        if (consume_event(fd, &reader, &event) < 0) {
            perror("event or resync");
            goto out;
        }
    }
    result = EXIT_SUCCESS;
out:
    if (close(fd) < 0) {
        perror("close");
        result = EXIT_FAILURE;
    }
    return result;
}
