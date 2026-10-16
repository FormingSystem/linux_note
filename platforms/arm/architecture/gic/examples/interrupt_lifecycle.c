/* 普通中断的离散教学模型；状态在进程内，不读写设备寄存器。 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum trigger_kind { TRIGGER_LEVEL, TRIGGER_EDGE };
enum { DATA_CAPACITY = 16 };

struct source {
    enum trigger_kind trigger;
    char data[DATA_CAPACITY + 1];
    size_t used;
    bool edge_pending;
    bool active;
    bool priority_busy;
};

static void require(bool condition, const char *message)
{
    /* 普通函数始终执行，不受 NDEBUG 影响；失败时报告动作条件。 */
    if (!condition) {
        fprintf(stderr, "模型检查失败: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static bool pending(const struct source *source)
{
    return source->trigger == TRIGGER_LEVEL
        ? source->used != 0 : source->edge_pending;
}

static void receive(struct source *source, char value)
{
    /* 电平模型以缓冲非空维持请求；边沿模型每个字节产生一次边沿。 */
    require(source->used < DATA_CAPACITY, "教学缓冲已满");
    source->data[source->used++] = value;
    source->data[source->used] = '\0';
    if (source->trigger == TRIGGER_EDGE)
        source->edge_pending = true;
}

static bool acknowledge(struct source *source)
{
    /* 只保留一个来源，其他交付条件假定满足。 */
    if (source->active || !pending(source))
        return false;
    source->edge_pending = false;
    source->active = true;
    source->priority_busy = true;
    return true;
}

static void drain(struct source *source, char result[DATA_CAPACITY + 1])
{
    /* 保留本次取出的字符供检查，再清空设备数据；不清边沿历史。 */
    memcpy(result, source->data, source->used + 1);
    source->used = 0;
    source->data[0] = '\0';
}

static void eoi(struct source *source, bool split)
{
    require(source->priority_busy, "没有尚待优先级下降的确认");
    source->priority_busy = false;
    if (!split)
        source->active = false;
}

static void deactivate(struct source *source)
{
    require(source->active && !source->priority_busy,
            "本模型要求先确认、再下降优先级、最后停用");
    source->active = false;
}

static void show(const char *label, const struct source *source)
{
    printf("%s: pending=%d, active=%d, priority_busy=%d, data=%s\n",
           label, pending(source), source->active, source->priority_busy,
           source->used != 0 ? source->data : "-");
}

int main(void)
{
    struct source normal = { .trigger = TRIGGER_LEVEL };
    struct source uncleared = { .trigger = TRIGGER_LEVEL };
    struct source unfinished = { .trigger = TRIGGER_LEVEL };
    struct source merged = { .trigger = TRIGGER_EDGE };
    struct source late = { .trigger = TRIGGER_EDGE };
    char drained_data[DATA_CAPACITY + 1];
    bool was_acknowledged;

    receive(&normal, 'A');
    was_acknowledged = acknowledge(&normal);
    require(was_acknowledged, "正常路径应当能够确认");
    drain(&normal, drained_data);
    require(strcmp(drained_data, "A") == 0, "正常路径应读出 A");
    eoi(&normal, false);
    show("正常", &normal);

    receive(&uncleared, 'A');
    was_acknowledged = acknowledge(&uncleared);
    require(was_acknowledged, "漏清源场景应先确认");
    eoi(&uncleared, false);  /* 故意不取走设备数据。 */
    show("漏清源", &uncleared);
    was_acknowledged = acknowledge(&uncleared);
    require(was_acknowledged, "电平条件仍在，应能再次确认");
    drain(&uncleared, drained_data);
    eoi(&uncleared, false);

    receive(&unfinished, 'A');
    was_acknowledged = acknowledge(&unfinished);
    require(was_acknowledged, "漏停用场景应先确认");
    drain(&unfinished, drained_data);
    eoi(&unfinished, true);  /* 分离模式，暂不执行停用。 */
    receive(&unfinished, 'B');
    show("漏停用", &unfinished);
    was_acknowledged = acknowledge(&unfinished);
    require(!was_acknowledged, "旧 Active 尚在，新请求不能再次确认");
    deactivate(&unfinished);
    was_acknowledged = acknowledge(&unfinished);
    require(was_acknowledged, "停用旧交接后应能确认 B");
    drain(&unfinished, drained_data);
    eoi(&unfinished, false);

    receive(&merged, 'A');
    receive(&merged, 'B');
    show("确认前两次边沿", &merged);
    was_acknowledged = acknowledge(&merged);
    require(was_acknowledged, "合并的边沿应提供一次确认");
    drain(&merged, drained_data);
    require(strcmp(drained_data, "AB") == 0, "一次通知可以对应两个字节");
    eoi(&merged, false);

    receive(&late, 'A');
    was_acknowledged = acknowledge(&late);
    require(was_acknowledged, "先确认 A 对应的请求");
    receive(&late, 'B');  /* 确认后再来的边沿锁存为新通知。 */
    drain(&late, drained_data);
    require(strcmp(drained_data, "AB") == 0, "本轮可以一并读走 A 和 B");
    eoi(&late, false);
    show("本轮已读走后来数据", &late);
    was_acknowledged = acknowledge(&late);
    require(was_acknowledged, "数据已读走，新边沿通知仍可确认");
    drain(&late, drained_data);
    require(strcmp(drained_data, "") == 0, "后一轮可以没有剩余设备数据");
    eoi(&late, false);
    return EXIT_SUCCESS;
}
