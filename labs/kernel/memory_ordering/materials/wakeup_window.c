#include <stdbool.h>
#include <stdio.h>

struct model {
    bool event;
    bool registered;
    bool runnable;
    bool observed;
    bool parked;
};

struct result {
    unsigned schedules;
    unsigned lost;
};

static void waiter_step(struct model *m, unsigned step, bool register_first)
{
    const unsigned register_step = register_first ? 0U : 1U;
    const unsigned check_step = register_first ? 1U : 0U;
    if (step == register_step) {
        // 条件已成立时，后续动作只是空步骤，保留固定枚举长度。
        if (!m->observed) {
            m->registered = true;
            m->runnable = false;
        }
    } else if (step == check_step) {
        m->observed = m->event;
    } else if (!m->observed) {
        // 唤醒先于调度时，任务已可运行，不能再把它无条件睡下。
        m->parked = !m->runnable;
    }
}

static void producer_step(struct model *m, unsigned step)
{
    if (step == 0) {
        m->event = true; // 单次持久条件，本模型不复位。
    } else if (m->registered) {
        m->runnable = true;
        m->parked = false;
    }
}

static void enumerate(struct model m, unsigned waiter_pc, unsigned producer_pc,
                      bool register_first, struct result *result)
{
    if (waiter_pc == 3 && producer_pc == 2) {
        ++result->schedules;
        if (m.event && m.parked && !m.runnable)
            ++result->lost;
        return;
    }
    if (waiter_pc < 3) {
        struct model next = m;
        waiter_step(&next, waiter_pc, register_first);
        enumerate(next, waiter_pc + 1, producer_pc, register_first, result);
    }
    if (producer_pc < 2) {
        struct model next = m;
        producer_step(&next, producer_pc);
        enumerate(next, waiter_pc, producer_pc + 1, register_first, result);
    }
}

int main(void)
{
    const struct model initial = {.runnable = true};
    struct result bad = {0}, good = {0};
    enumerate(initial, 0, 0, false, &bad);
    enumerate(initial, 0, 0, true, &good);
    printf("check_first: schedules=%u lost=%u\n", bad.schedules, bad.lost);
    printf("register_first: schedules=%u lost=%u\n", good.schedules, good.lost);
    // 单线程枚举只检查登记空窗，不模拟弱内存或真实调度器。
    return bad.schedules == 10 && bad.lost == 1 &&
                   good.schedules == 10 && good.lost == 0 ? 0 : 1;
}
