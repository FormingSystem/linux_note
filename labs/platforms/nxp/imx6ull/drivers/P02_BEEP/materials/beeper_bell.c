/* 目标Linux程序：确认SND_BELL能力，短暂请求发声，再明确关闭。 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int send_bell(int fd, int on)
{
    struct input_event event = {0};
    ssize_t written;
    event.type = EV_SND;
    event.code = SND_BELL;
    event.value = on;
    do {
        written = write(fd, &event, sizeof(event));
    } while (written < 0 && errno == EINTR);
    if (written != (ssize_t)sizeof(event)) {
        if (written >= 0)
            errno = EIO; /* 不把不足一个事件的结果误当成功。 */
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    unsigned char sounds[(SND_MAX + 8) / 8] = {0};
    char name[128] = {0};
    struct timespec delay = {0, 200000000L};
    struct sigaction action = {0};
    int fd;
    int result = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s /dev/input/eventN\n", argv[0]);
        return 2;
    }
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL)) {
        perror("sigaction");
        return 1;
    }
    fd = open(argv[1], O_WRONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    if (ioctl(fd, EVIOCGBIT(EV_SND, sizeof(sounds)), sounds) < 0 ||
        ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
        perror("query input device");
        goto out;
    }
    name[sizeof(name) - 1] = '\0';
    printf("device: %s\n", name);
    if (!(sounds[SND_BELL / 8] & (1U << (SND_BELL % 8)))) {
        fprintf(stderr, "device does not advertise SND_BELL\n");
        goto out;
    }
    if (stop_requested)
        goto out;
    if (send_bell(fd, 1) < 0) {
        perror("bell on");
        goto stop;
    }
    /* 200ms是用户程序等待时间，不是硬件实际发声时长的保证。 */
    while (!stop_requested && nanosleep(&delay, &delay) < 0) {
        if (errno != EINTR) {
            perror("nanosleep");
            goto stop;
        }
    }
    result = 0;
stop:
    if (send_bell(fd, 0) < 0) {
        perror("bell off");
        result = 1;
    }
out:
    if (close(fd) < 0) {
        perror("close");
        result = 1;
    }
    return result;
}
