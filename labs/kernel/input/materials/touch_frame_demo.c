/* 教学控制器协议与完整槽规划；不是任何真实芯片的寄存器驱动。 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEMO_SLOTS 2U
#define DEMO_MAX_PACKET (4U + 5U * DEMO_SLOTS)
#define DEMO_MAX_X 1023U
#define DEMO_MAX_Y 767U

struct demo_point { uint8_t key; uint16_t x; uint16_t y; };
struct demo_frame {
    uint8_t sequence;
    unsigned int count;
    struct demo_point points[DEMO_SLOTS];
};
struct demo_slot { bool active; struct demo_point point; };
struct demo_state {
    bool have_sequence;
    uint8_t sequence;
    struct demo_slot slots[DEMO_SLOTS];
};
struct demo_plan {
    struct demo_state next;
    bool release[DEMO_SLOTS];
    bool start[DEMO_SLOTS];
    bool identity_reset;
};
enum demo_result {
    demo_ok, demo_duplicate, demo_bad_length, demo_bad_magic,
    demo_bad_checksum, demo_bad_coordinate, demo_repeated_key
};

static uint16_t demo_read_le16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8));
}

static enum demo_result demo_parse(const uint8_t *data, size_t size,
                                   struct demo_frame *output)
{
    struct demo_frame frame = {0};
    uint8_t checksum = 0;

    /* 先校验总长，绝不按未验证的点数访问记录。 */
    if (size < 4U)
        return demo_bad_length;
    if (data[0] != 0xa5U)
        return demo_bad_magic;
    frame.sequence = data[1];
    frame.count = data[2];
    if (frame.count > DEMO_SLOTS || size != 4U + 5U * frame.count)
        return demo_bad_length;
    for (size_t i = 0; i + 1U < size; ++i)
        checksum ^= data[i];
    if (checksum != data[size - 1U])
        return demo_bad_checksum;

    for (unsigned int i = 0; i < frame.count; ++i) {
        const uint8_t *record = data + 3U + 5U * i;
        struct demo_point *point = &frame.points[i];
        point->key = record[0];
        point->x = demo_read_le16(record + 1);
        point->y = demo_read_le16(record + 3);
        if (point->x > DEMO_MAX_X || point->y > DEMO_MAX_Y)
            return demo_bad_coordinate;
        for (unsigned int j = 0; j < i; ++j)
            if (frame.points[j].key == point->key)
                return demo_repeated_key;
    }
    *output = frame; /* 所有检查通过以后才公开解析结果。 */
    return demo_ok;
}

static int demo_find_key(const struct demo_frame *frame, uint8_t key)
{
    for (unsigned int i = 0; i < frame->count; ++i)
        if (frame->points[i].key == key)
            return (int)i;
    return -1;
}

static enum demo_result demo_prepare(const uint8_t *data, size_t size,
                                     const struct demo_state *current,
                                     bool force_reset, struct demo_plan *output)
{
    struct demo_frame frame;
    struct demo_plan plan = {0};
    bool assigned[DEMO_SLOTS] = {false};
    enum demo_result result = demo_parse(data, size, &frame);

    if (result != demo_ok)
        return result;
    if (current->have_sequence && frame.sequence == current->sequence &&
        !force_reset)
        return demo_duplicate;

    /* 跳号后不再相信旧身份连续性；这是保守教学策略。 */
    plan.identity_reset = force_reset ||
        (current->have_sequence &&
         frame.sequence != (uint8_t)(current->sequence + 1U));
    plan.next.have_sequence = true;
    plan.next.sequence = frame.sequence;

    /* 第一遍只保留交集，先把离开者的槽从候选映射中腾出来。 */
    for (unsigned int slot = 0; slot < DEMO_SLOTS; ++slot) {
        int point_index = -1;
        if (current->slots[slot].active && !plan.identity_reset)
            point_index = demo_find_key(&frame, current->slots[slot].point.key);
        if (point_index >= 0) {
            unsigned int index = (unsigned int)point_index;
            plan.next.slots[slot].active = true;
            plan.next.slots[slot].point = frame.points[index];
            assigned[index] = true;
        } else {
            plan.release[slot] = current->slots[slot].active;
        }
    }

    /* 第二遍分配新接触；完整点集已验证不超过容量。 */
    for (unsigned int i = 0; i < frame.count; ++i) {
        if (assigned[i])
            continue;
        for (unsigned int slot = 0; slot < DEMO_SLOTS; ++slot) {
            if (!plan.next.slots[slot].active) {
                plan.next.slots[slot].active = true;
                plan.next.slots[slot].point = frame.points[i];
                plan.start[slot] = true;
                assigned[i] = true;
                break;
            }
        }
    }
    *output = plan; /* current始终只读，坏包和重复帧不修改它。 */
    return demo_ok;
}

static void demo_emit_and_commit(struct demo_state *state,
                                 const struct demo_plan *plan)
{
    printf("sequence=%u reset=%u\n", (unsigned int)plan->next.sequence,
           plan->identity_reset ? 1U : 0U);
    /* 所有旧身份先结束，再报告新点；对接Input时仍在同一串行帧内。 */
    for (unsigned int slot = 0; slot < DEMO_SLOTS; ++slot)
        if (plan->release[slot])
            printf(" release slot%u key%u\n", slot,
                   (unsigned int)state->slots[slot].point.key);
    for (unsigned int slot = 0; slot < DEMO_SLOTS; ++slot) {
        const struct demo_slot *next = &plan->next.slots[slot];
        if (next->active)
            printf(" %s slot%u key%u @(%u,%u)\n",
                   plan->start[slot] ? "start" : "keep", slot,
                   (unsigned int)next->point.key,
                   (unsigned int)next->point.x, (unsigned int)next->point.y);
    }
    puts(" sync");
    *state = plan->next;
}

/* 以下仅为演示输入生成器；真实驱动的字节应来自控制器。 */
static size_t demo_encode(uint8_t sequence, const struct demo_point *points,
                          unsigned int count, uint8_t data[DEMO_MAX_PACKET])
{
    size_t size = 4U + 5U * count;
    data[0] = 0xa5U; data[1] = sequence; data[2] = (uint8_t)count;
    for (unsigned int i = 0; i < count; ++i) {
        uint8_t *record = data + 3U + 5U * i;
        record[0] = points[i].key;
        record[1] = (uint8_t)points[i].x;
        record[2] = (uint8_t)(points[i].x >> 8);
        record[3] = (uint8_t)points[i].y;
        record[4] = (uint8_t)(points[i].y >> 8);
    }
    data[size - 1U] = 0;
    for (size_t i = 0; i + 1U < size; ++i)
        data[size - 1U] ^= data[i];
    return size;
}

int main(void)
{
    const struct demo_point first[] = {{7, 10, 20}, {9, 50, 60}};
    const struct demo_point turnover[] = {{11, 70, 80}, {9, 55, 60}};
    const struct demo_point repeated[] = {{9, 1, 2}, {9, 3, 4}};
    struct demo_state state = {0};
    struct demo_plan plan;
    uint8_t data[DEMO_MAX_PACKET];
    size_t size = demo_encode(254, first, 2, data);

    if (demo_prepare(data, size, &state, false, &plan) != demo_ok)
        return 1;
    demo_emit_and_commit(&state, &plan);
    size = demo_encode(255, turnover, 2, data);
    if (demo_prepare(data, size, &state, false, &plan) != demo_ok ||
        !plan.release[0] || !plan.start[0] || plan.start[1] ||
        plan.next.slots[1].point.key != 9)
        return 1;
    demo_emit_and_commit(&state, &plan);

    /* 校验错误和重复key不得生成可提交的计划。 */
    data[size - 1U] ^= 1U;
    if (demo_prepare(data, size, &state, false, &plan) != demo_bad_checksum)
        return 1;
    puts("bad checksum: no report, state retained");
    size = demo_encode(0, repeated, 2, data);
    if (demo_prepare(data, size, &state, false, &plan) != demo_repeated_key)
        return 1;
    puts("repeated key: no report, state retained");

    size = demo_encode(0, turnover, 2, data);
    if (demo_prepare(data, size, &state, false, &plan) != demo_ok ||
        plan.identity_reset)
        return 1;
    demo_emit_and_commit(&state, &plan); /* 255到0是正常回绕。 */
    if (demo_prepare(data, size, &state, false, &plan) != demo_duplicate)
        return 1;
    puts("duplicate sequence: no report");

    size = demo_encode(2, turnover, 2, data);
    if (demo_prepare(data, size, &state, false, &plan) != demo_ok ||
        !plan.identity_reset || !plan.release[0] || !plan.release[1])
        return 1;
    demo_emit_and_commit(&state, &plan); /* 跳号，明确结束旧身份。 */
    size = demo_encode(3, NULL, 0, data);
    if (demo_prepare(data, size, &state, false, &plan) != demo_ok ||
        plan.next.slots[0].active || plan.next.slots[1].active)
        return 1;
    demo_emit_and_commit(&state, &plan);
    return 0;
}
