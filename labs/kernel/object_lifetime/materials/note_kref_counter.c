// SPDX-License-Identifier: GPL-2.0
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct counter_object;
struct counter_work {
    struct work_struct work;
    struct counter_object *obj;
};
struct counter_object {
    struct kref ref;
    struct mutex lock;
    struct completion start;
    struct counter_work workers[2];
    unsigned int plain;
    atomic_t atomic;
};
static unsigned int mode;
module_param(mode, uint, 0444);
MODULE_PARM_DESC(mode, "0互斥更新，1原子增加，2仅在KCSAN构建中故意无锁竞争");
static unsigned int release_calls;

static void counter_release(struct kref *ref)
{
    struct counter_object *obj = container_of(ref, struct counter_object, ref);
    ++release_calls;
    kfree(obj);
}
static void counter_put(struct counter_object *obj)
{
    kref_put(&obj->ref, counter_release);
}
static void counter_worker(struct work_struct *work)
{
    struct counter_work *item = container_of(work, struct counter_work, work);
    struct counter_object *obj = item->obj;
    unsigned int step;
    wait_for_completion(&obj->start);
    for (step = 0; step < 100000; ++step) {
        if (mode == 0) {
            mutex_lock(&obj->lock);
            ++obj->plain;
            mutex_unlock(&obj->lock);
        } else if (mode == 1) {
            atomic_inc(&obj->atomic);
        } else {
            ++obj->plain; /* 故意的数据竞争，不以此结果推导正确计数。 */
        }
        if ((step & 255u) == 0)
            cond_resched(); /* 提供调度机会，不作为两执行者必定交错的证明。 */
    }
    counter_put(obj); /* 消费本次已接纳的工作份额。 */
}
static int __init note_counter_init(void)
{
    struct workqueue_struct *queue;
    struct counter_object *obj;
    unsigned int index, result_count;
    int result = 0;
    if (mode > 2)
        return -EINVAL;
    if (mode == 2 && !IS_ENABLED(CONFIG_KCSAN))
        return -EOPNOTSUPP;
    queue = alloc_workqueue("note_counter", WQ_UNBOUND, 2);
    if (!queue)
        return -ENOMEM;
    obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj) {
        destroy_workqueue(queue);
        return -ENOMEM;
    }
    kref_init(&obj->ref); /* 管理者一直持有到队列已销毁且统计结束。 */
    mutex_init(&obj->lock);
    init_completion(&obj->start);
    atomic_set(&obj->atomic, 0);
    for (index = 0; index < 2; ++index) {
        obj->workers[index].obj = obj;
        INIT_WORK(&obj->workers[index].work, counter_worker);
        kref_get(&obj->ref);
        if (!queue_work(queue, &obj->workers[index].work)) {
            counter_put(obj); /* 拒绝只收回本次候选，之前接纳者仍由worker归还。 */
            result = -EIO;
            break;
        }
    }
    complete_all(&obj->start); /* 成功或部分失败都先放行已接纳者，再等待。 */
    destroy_workqueue(queue);
    result_count = mode == 1 ? (unsigned int)atomic_read(&obj->atomic) : obj->plain;
    pr_info("note_counter: mode=%u result=%d count=%u\n", mode, result, result_count);
    counter_put(obj);
    return result;
}
static void __exit note_counter_exit(void)
{
    pr_info("note_counter: release=%u\n", release_calls);
}
module_init(note_counter_init);
module_exit(note_counter_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("对象份额与两工作线程字段更新的独立责任实验");
