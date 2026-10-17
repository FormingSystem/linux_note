/* 教学事件解释器：不读取 evdev 二进制，也不模拟内核锁或丢帧恢复。 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum demo_code { demo_slot, demo_tracking, demo_x, demo_y, demo_sync };
struct demo_event { enum demo_code code; int value; };
struct demo_contact { int tracking; int x; int y; };

static const struct demo_event stream[] = {
    {demo_slot, 0}, {demo_tracking, 45}, {demo_x, 10}, {demo_y, 20},
    {demo_slot, 1}, {demo_tracking, 46}, {demo_x, 50}, {demo_y, 60},
    {demo_sync, 0},
    {demo_x, 55}, {demo_sync, 0}, /* 选择器仍指向槽 1，Y 沿用 60。 */
    {demo_slot, 0}, {demo_tracking, -1}, {demo_sync, 0},
    {demo_tracking, 47}, {demo_x, 20}, {demo_y, 30}, {demo_sync, 0},
    {demo_tracking, -1}, {demo_slot, 1}, {demo_tracking, -1}, {demo_sync, 0}
};

int main(int argc, char **argv)
{
    struct demo_contact slots[2] = {{-1, 0, 0}, {-1, 0, 0}};
    unsigned int selected = 0, frame = 0;
    bool omit_release = false;

    if (argc == 2 && strcmp(argv[1], "omit-release") == 0)
        omit_release = true;
    else if (argc != 1) {
        fprintf(stderr, "usage: %s [omit-release]\n", argv[0]);
        return 2;
    }

    for (size_t i = 0; i < sizeof(stream) / sizeof(stream[0]); ++i) {
        const struct demo_event event = stream[i];
        /* 删除第三帧的释放记录，观察幽灵接触；不修改其余输入。 */
        if (omit_release && frame == 2 && event.code == demo_tracking)
            continue;
        switch (event.code) {
        case demo_slot:
            if (event.value < 0 || event.value >= 2) {
                fprintf(stderr, "invalid slot\n");
                return 1;
            }
            selected = (unsigned int)event.value;
            break;
        case demo_tracking:
            if (event.value < -1) {
                fprintf(stderr, "invalid tracking id\n");
                return 1;
            }
            /* 接触身份变化不清空轴缓存；未报告的轴继续沿用槽内值。 */
            slots[selected].tracking = event.value;
            break;
        case demo_x:
            slots[selected].x = event.value;
            break;
        case demo_y:
            slots[selected].y = event.value;
            break;
        case demo_sync:
            printf("frame %u:", ++frame);
            for (unsigned int slot = 0; slot < 2; ++slot) {
                if (slots[slot].tracking >= 0)
                    printf(" slot%u=id%d@(%d,%d)", slot,
                           slots[slot].tracking, slots[slot].x, slots[slot].y);
            }
            putchar('\n');
            break;
        }
    }
    return 0;
}
