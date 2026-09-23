// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/timer.h>

struct deadline_request {
    struct kref ref;
    struct timer_list timer;
    spinlock_t gate;
    bool closing;
    bool issued;
    bool ticket;
    unsigned int fired;
};
static struct deadline_request *managed_request;
static unsigned int delay_ms = 60000;
module_param(delay_ms, uint, 0444);
MODULE_PARM_DESC(delay_ms, "一次到期的等待毫秒数；卸载时同步关闭");
static unsigned int release_calls;

static void deadline_release(struct kref *ref)
{
    struct deadline_request *request = container_of(ref, struct deadline_request, ref);
    ++release_calls;
    kfree(request);
}
static void deadline_put(struct deadline_request *request)
{
    kref_put(&request->ref, deadline_release);
}
static void deadline_expired(struct timer_list *timer)
{
    struct deadline_request *request = from_timer(request, timer, timer);
    unsigned long flags;
    spin_lock_irqsave(&request->gate, flags);
    request->ticket = false; /* 正在执行者接管原有票据，并非责任已经结束。 */
    ++request->fired;
    spin_unlock_irqrestore(&request->gate, flags);
    deadline_put(request); /* 回调最后一次访问后归还该份额。 */
}
/* 调用者已有一份；本协议每个对象最多接受一次启动，不允许改期或重初始化。 */
static int deadline_start(struct deadline_request *request, unsigned long expires)
{
    unsigned long flags;
    int result = 0;
    spin_lock_irqsave(&request->gate, flags);
    if (request->closing)
        result = -ESHUTDOWN;
    else if (request->issued)
        result = -EALREADY;
    else {
        request->issued = true;
        request->ticket = true;
        kref_get(&request->ref);
        /* 首次安排且尚未shutdown；返回零表示此前不pending，不是失败。 */
        mod_timer(&request->timer, expires);
    }
    spin_unlock_irqrestore(&request->gate, flags);
    return result;
}
/* 只有管理者调用，持有初始份额；顺序重复关闭允许，不能并发关闭。 */
static void deadline_close(struct deadline_request *request)
{
    unsigned long flags;
    spin_lock_irqsave(&request->gate, flags);
    request->closing = true;
    spin_unlock_irqrestore(&request->gate, flags);
    /* 必须在gate外等待，且从可执行同步退出的进程上下文调用。 */
    if (timer_shutdown_sync(&request->timer)) {
        spin_lock_irqsave(&request->gate, flags);
        request->ticket = false;
        spin_unlock_irqrestore(&request->gate, flags);
        deadline_put(request); /* 取消者接管未执行的唯一票据。 */
    }
    /* 初始份额仍归管理者；本函数不消费调用者份额。 */
}
static int __init note_deadline_init(void)
{
    managed_request = kzalloc(sizeof(*managed_request), GFP_KERNEL);
    if (!managed_request)
        return -ENOMEM;
    kref_init(&managed_request->ref);
    spin_lock_init(&managed_request->gate);
    timer_setup(&managed_request->timer, deadline_expired, 0);
    /* 全新对象的首次启动符合全部前提，不能被外部关闭者插入。 */
    deadline_start(managed_request, jiffies + msecs_to_jiffies(delay_ms));
    return 0;
}
static void __exit note_deadline_exit(void)
{
    deadline_close(managed_request);
    /* shutdown已等回调退出，且不再有生产者；这里读取稳定统计。 */
    pr_info("note_deadline: fired=%u ticket=%d\n",
            managed_request->fired, managed_request->ticket);
    deadline_put(managed_request);
    managed_request = NULL;
    pr_info("note_deadline: releases=%u\n", release_calls);
}
module_init(note_deadline_init);
module_exit(note_deadline_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("单次定时器份额的执行与关闭对照");
