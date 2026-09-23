// SPDX-License-Identifier: GPL-2.0
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

/* 对象外的顺序观察账本，不是 struct timer_list 或真实引用计数器。 */
struct timer_model {
    bool pending;
    bool running;
    bool shutdown;
    bool work_pending;
    unsigned int refs;
    unsigned int callbacks;
};

static int model_mod(struct timer_model *timer)
{
    if (timer->shutdown)
        return 0; /* 固定接口在 shutdown 后丢弃启动，也返回零。 */
    int was_pending = timer->pending;
    timer->pending = true; /* 改期仍然只有一个 pending，不新增票据。 */
    return was_pending;
}

static void model_begin(struct timer_model *timer)
{
    assert(timer->pending && !timer->running);
    timer->pending = false;
    timer->running = true;
    ++timer->callbacks;
}

static void model_end(struct timer_model *timer)
{
    assert(timer->running);
    timer->running = false;
}

static int model_delete_sync(struct timer_model *timer, bool shutdown)
{
    if (shutdown)
        timer->shutdown = true;
    /* 显式完成已执行实例，表示等待之后的结果，不实现线程等待。 */
    if (timer->running)
        model_end(timer);
    int was_pending = timer->pending;
    timer->pending = false;
    return was_pending;
}

static void model_work(struct timer_model *timer)
{
    assert(timer->work_pending);
    timer->work_pending = false;
    (void)model_mod(timer); /* 借用 worker 尝试重新启动 timer。 */
}

static void owner_exit(struct timer_model *timer)
{
    assert(!timer->pending && !timer->running && !timer->work_pending);
    assert(timer->refs == 1);
    --timer->refs;
}

int main(void)
{
    struct timer_model bad = { .refs = 1 };
    ++bad.refs; assert(model_mod(&bad) == 0);
    ++bad.refs; assert(model_mod(&bad) == 1);
    model_begin(&bad); model_end(&bad); --bad.refs; /* 仅一次回调归还。 */
    --bad.refs; /* 管理者退出，错误地留下无人认领的一份。 */
    assert(bad.refs == 1 && bad.callbacks == 1);
    puts("two gets, one callback: leaked responsibility=1");

    struct timer_model owned = { .refs = 1 };
    assert(model_mod(&owned) == 0 && model_mod(&owned) == 1);
    model_begin(&owned);
    assert(!owned.pending && owned.running); /* pending 为假并非已退出。 */
    assert(model_delete_sync(&owned, true) == 0);
    owner_exit(&owned);
    assert(owned.refs == 0 && owned.callbacks == 1);
    puts("owner retained through running callback: refs=0");

    struct timer_model reopened = { .refs = 1 };
    assert(model_mod(&reopened) == 0);
    assert(model_delete_sync(&reopened, false) == 1);
    assert(model_mod(&reopened) == 0 && reopened.pending);
    assert(model_delete_sync(&reopened, true) == 1);
    owner_exit(&reopened);
    puts("delete allowed a later restart; shutdown closed it");

    struct timer_model cycle = { .refs = 1, .work_pending = true };
    assert(model_delete_sync(&cycle, true) == 0);
    model_work(&cycle);
    assert(!cycle.pending && !cycle.work_pending);
    assert(model_mod(&cycle) == 0 && !cycle.pending);
    owner_exit(&cycle);
    puts("worker rearm after shutdown: discarded, refs=0");
    return 0;
}
