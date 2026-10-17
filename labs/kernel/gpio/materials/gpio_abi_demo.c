// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/gpio.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested;

static void stop_handler(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int open_simulator(const char *path)
{
    struct gpiochip_info info = {0};
    int chip_fd = open(path, O_RDONLY | O_CLOEXEC);

    if (chip_fd < 0)
        return -1;
    if (ioctl(chip_fd, GPIO_GET_CHIPINFO_IOCTL, &info) < 0) {
        int saved_errno = errno;
        close(chip_fd);
        errno = saved_errno;
        return -1;
    }
    /* 本程序只允许操作本章建立的模拟bank。 */
    if (strncmp(info.label, "linux-note-abi-sim", sizeof(info.label)) ||
        info.lines != 3) {
        close(chip_fd);
        errno = ENODEV;
        return -1;
    }
    return chip_fd;
}

static int request_lines(int chip_fd, struct gpio_v2_line_request *request)
{
    memset(request, 0, sizeof(*request));
    request->offsets[0] = 0;  /* 槽位0：高有效enable输出。 */
    request->offsets[1] = 2;  /* 槽位1：低有效alert输入。 */
    request->num_lines = 2;
    snprintf(request->consumer, sizeof(request->consumer), "gpio-abi-demo");
    request->config.flags = GPIO_V2_LINE_FLAG_INPUT |
        GPIO_V2_LINE_FLAG_ACTIVE_LOW | GPIO_V2_LINE_FLAG_EDGE_RISING |
        GPIO_V2_LINE_FLAG_EDGE_FALLING;
    request->config.num_attrs = 2;
    /* FLAGS属性替换槽位0的默认flags，不是向默认值再加OUTPUT。 */
    request->config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS;
    request->config.attrs[0].attr.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    request->config.attrs[0].mask = UINT64_C(1);
    request->config.attrs[1].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    request->config.attrs[1].attr.values = 0;
    request->config.attrs[1].mask = UINT64_C(1);
    return ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, request);
}

static int set_enable(int request_fd, int enabled)
{
    struct gpio_v2_line_values values = {
        .bits = enabled ? UINT64_C(1) : 0,
        .mask = UINT64_C(1)
    };

    return ioctl(request_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values);
}

int main(int argc, char **argv)
{
    struct sigaction action = {0};
    struct gpio_v2_line_request request, competing = {0};
    struct gpio_v2_line_values values = {.mask = UINT64_C(3)};
    int chip_fd = -1, request_fd = -1, held_fd = -1;
    int result = EXIT_FAILURE, event_count = 0, live_fd;

    if (argc != 2) {
        fprintf(stderr, "usage: %s /dev/gpiochipN\n", argv[0]);
        return EXIT_FAILURE;
    }
    action.sa_handler = stop_handler;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    chip_fd = open_simulator(argv[1]);
    if (chip_fd < 0) { perror("open simulator"); goto out; }
    if (request_lines(chip_fd, &request) < 0) {
        perror("request lines"); goto out;
    }
    request_fd = request.fd;
    held_fd = fcntl(request_fd, F_DUPFD_CLOEXEC, 0);
    if (held_fd < 0) { perror("duplicate request"); goto out; }
    /* Linux close出错也不重试，避免误关被复用的fd号码。 */
    if (close(request_fd) < 0) {
        request_fd = -1; perror("close original request"); goto out;
    }
    request_fd = -1;

    competing.offsets[0] = 0;
    competing.num_lines = 1;
    competing.config.flags = GPIO_V2_LINE_FLAG_INPUT;
    if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &competing) == 0) {
        close(competing.fd);
        fprintf(stderr, "unexpected second owner\n"); goto out;
    }
    if (errno != EBUSY) { perror("expected EBUSY"); goto out; }
    puts("EBUSY: duplicated request still owns offset 0");
    if (close(chip_fd) < 0) {
        chip_fd = -1; perror("close chip"); goto out;
    }
    chip_fd = -1;
    if (set_enable(held_fd, 1) < 0 ||
        ioctl(held_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0) {
        perror("request values"); goto out;
    }
    printf("chip fd closed; enable=%u alert=%u (logical)\n",
           (unsigned)(values.bits & 1), (unsigned)((values.bits >> 1) & 1));
    int status_flags = fcntl(held_fd, F_GETFL);
    if (status_flags < 0 || fcntl(held_fd, F_SETFL, status_flags | O_NONBLOCK) < 0) {
        perror("nonblocking request"); goto out;
    }
    puts("ready: inject pull-down; after event 1 inject pull-up");
    while (!stop_requested && event_count < 2) {
        struct pollfd waiter = {.fd = held_fd, .events = POLLIN};
        struct gpio_v2_line_event event;
        int ready = poll(&waiter, 1, 60000);
        ssize_t count;

        if (ready < 0 && errno == EINTR)
            continue;
        if (ready < 0) { perror("poll"); goto out; }
        if (!ready) { fprintf(stderr, "event wait timed out\n"); goto out; }
        if (waiter.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "request no longer usable\n"); goto out;
        }
        if (!(waiter.revents & POLLIN))
            continue;
        count = read(held_fd, &event, sizeof(event));
        if (count < 0 && (errno == EAGAIN || errno == EINTR))
            continue;
        if (count < 0) { perror("read event"); goto out; }
        if (count != (ssize_t)sizeof(event)) {
            fprintf(stderr, "unexpected event size\n"); goto out;
        }
        printf("event %d: offset=%" PRIu32 " id=%" PRIu32
               " seq=%" PRIu32 " line_seq=%" PRIu32 " ns=%" PRIu64 "\n",
               ++event_count, (uint32_t)event.offset, (uint32_t)event.id,
               (uint32_t)event.seqno, (uint32_t)event.line_seqno,
               (uint64_t)event.timestamp_ns);
    }
    if (!stop_requested)
        result = EXIT_SUCCESS;

out:
    /* 先请求安全输出，再关闭最后持有引用；这不是掉电安全保证。 */
    live_fd = held_fd >= 0 ? held_fd : request_fd;
    if (live_fd >= 0 && set_enable(live_fd, 0) < 0) {
        perror("disable enable"); result = EXIT_FAILURE;
    }
    if (held_fd >= 0 && close(held_fd) < 0) {
        perror("close held request"); result = EXIT_FAILURE;
    }
    if (request_fd >= 0 && close(request_fd) < 0) {
        perror("close request"); result = EXIT_FAILURE;
    }
    if (chip_fd >= 0)
        close(chip_fd);
    if (result != EXIT_SUCCESS)
        return result;

    chip_fd = open_simulator(argv[1]);
    if (chip_fd < 0) { perror("reopen simulator"); return EXIT_FAILURE; }
    if (request_lines(chip_fd, &request) < 0) {
        perror("request again"); close(chip_fd); return EXIT_FAILURE;
    }
    puts("last request reference closed: both lines can be requested again");
    result = close(request.fd) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    close(chip_fd);
    return result;
}
