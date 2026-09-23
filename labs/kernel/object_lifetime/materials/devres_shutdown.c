/* 教学状态模型：显式插入事件，不模拟Linux IRQ、锁或真实分配器。 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

enum resource_kind { memory_record, clock_record, irq_record };

struct model {
    enum resource_kind records[3];
    unsigned int count;
    bool memory_live;
    bool clock_on;
    bool irq_registered;
    bool source_running;
    unsigned int invalid_access;
    unsigned int order_errors;
    unsigned int accepted;
};

static void record_resource(struct model *state, enum resource_kind resource)
{
    assert(state->count < 3);
    state->records[state->count++] = resource;
}

static void deliver_interrupt(struct model *state)
{
    /* 已撤销的入口不再接受调用；已登记不意味着所需状态已准备好。 */
    if (!state->irq_registered)
        return;
    if (!state->memory_live || !state->clock_on) {
        ++state->invalid_access;
        return; /* 只记录反例，不真的访问失效内存或硬件。 */
    }
    ++state->accepted;
}

static void release_records(struct model *state)
{
    while (state->count != 0) {
        switch (state->records[--state->count]) {
        case irq_record:
            /* 本模型要求先停止本设备事件源；不声称free_irq会替代该动作。 */
            if (state->source_running)
                ++state->order_errors;
            state->irq_registered = false;
            putchar('I');
            break;
        case clock_record:
            if (state->irq_registered)
                ++state->order_errors;
            state->clock_on = false;
            putchar('C');
            break;
        case memory_record:
            if (state->irq_registered)
                ++state->order_errors;
            state->memory_live = false;
            putchar('M');
            break;
        }
    }
}

static void run_case(unsigned int scenario)
{
    struct model state = {0};
    printf("case %u release=", scenario);
    if (scenario == 0) /* 内存取得失败，尚无已登记责任。 */
        goto finish;
    state.memory_live = true;
    record_resource(&state, memory_record);
    if (scenario == 1) /* 时钟取得或准备失败。 */
        goto finish;

    if (scenario == 5) {
        /* 反例一：时钟尚未准备，就允许handler被调用。 */
        state.irq_registered = true;
        deliver_interrupt(&state);
        state.irq_registered = false;
    }
    state.clock_on = true;
    record_resource(&state, clock_record);
    if (scenario == 2) /* IRQ登记失败，时钟责任仍已成立。 */
        goto finish;
    state.irq_registered = true;
    record_resource(&state, irq_record);
    deliver_interrupt(&state); /* 允许登记后立即到来的事件。 */
    if (scenario == 3) /* 启动失败的本模型保证事件源没有运行。 */
        goto finish;
    state.source_running = true;
    deliver_interrupt(&state);
    state.source_running = false; /* 先停止本设备源；实际驱动还要排空活动。 */
    if (scenario == 6) {
        /* 反例二：入口尚在就关时钟，已有或迟到的handler仍可能进入。 */
        state.clock_on = false;
        deliver_interrupt(&state);
        state.clock_on = true; /* 恢复模型，随后按正确顺序收束。 */
    }

finish:
    release_records(&state);
    deliver_interrupt(&state); /* 入口撤销后应拒绝，不读取已结束的资源。 */
    assert(!state.memory_live && !state.clock_on && !state.irq_registered);
    assert(!state.source_running && state.count == 0);
    assert(state.order_errors == 0);
    assert(state.invalid_access == (scenario >= 5 ? 1U : 0U));
    assert(state.accepted == (scenario < 3 ? 0U : scenario == 3 ? 1U : 2U));
    printf(" accepted=%u invalid=%u\n", state.accepted, state.invalid_access);
}

int main(void)
{
    for (unsigned int scenario = 0; scenario < 7; ++scenario)
        run_case(scenario);
    puts("7 explicit event traces passed; no kernel or hardware execution");
    return 0;
}
