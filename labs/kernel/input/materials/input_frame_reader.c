#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* 教学对象：只演示逻辑记录，不是Linux input_event二进制布局。 */
enum demo_type { DEMO_SYN = 0, DEMO_ABS = 3 };
enum demo_code { DEMO_X = 0, DEMO_Y = 1, DEMO_REPORT = 0 };

struct demo_event {
    unsigned int type;
    unsigned int code;
    int value;
};

struct reader_state {
    int x;
    int y;
    bool have_x;
    bool have_y;
    bool dirty;
    unsigned int frames;
};

static int feed_events(struct reader_state *reader,
                       const struct demo_event *events, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        const struct demo_event *event = &events[i];
        if (event->type == DEMO_ABS && event->code == DEMO_X) {
            reader->x = event->value;
            reader->have_x = true;
            reader->dirty = true;
        } else if (event->type == DEMO_ABS && event->code == DEMO_Y) {
            reader->y = event->value;
            reader->have_y = true;
            reader->dirty = true;
        } else if (event->type == DEMO_SYN && event->code == DEMO_REPORT) {
            /* 本例约定第一帧显式提供X/Y；之后保留未变化轴。 */
            if (!reader->have_x || !reader->have_y)
                return -1;
            if (reader->dirty) {
                ++reader->frames;
                printf("frame %u: x=%d y=%d\n",
                       reader->frames, reader->x, reader->y);
                reader->dirty = false;
            }
        } else {
            return -1; /* 只接受本实验已经解释的两种记录。 */
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    const struct demo_event events[] = {
        { DEMO_ABS, DEMO_X, 10 }, { DEMO_ABS, DEMO_Y, 20 },
        { DEMO_SYN, DEMO_REPORT, 0 },
        { DEMO_ABS, DEMO_X, 15 }, { DEMO_SYN, DEMO_REPORT, 0 },
        { DEMO_ABS, DEMO_Y, 25 }, { DEMO_SYN, DEMO_REPORT, 0 }
    };
    const size_t total = sizeof(events) / sizeof(events[0]);
    size_t chunk = 1;
    struct reader_state reader = { 0 };

    if (argc > 2)
        return 2;
    if (argc == 2) {
        char *end;
        errno = 0;
        unsigned long value = strtoul(argv[1], &end, 10);
        if (errno || end == argv[1] || *end || value == 0 || value > total) {
            fprintf(stderr, "chunk must be in 1..%zu\n", total);
            return 2;
        }
        chunk = (size_t)value;
    }

    for (size_t offset = 0; offset < total; offset += chunk) {
        size_t count = total - offset < chunk ? total - offset : chunk;
        printf("block [%zu,%zu)\n", offset, offset + count);
        if (feed_events(&reader, events + offset, count)) {
            fprintf(stderr, "invalid teaching event stream\n");
            return 1;
        }
    }
    if (reader.dirty) {
        fprintf(stderr, "incomplete final frame\n");
        return 1;
    }
    return 0;
}
