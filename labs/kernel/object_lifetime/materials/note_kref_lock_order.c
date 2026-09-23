// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

struct lock_order_object {
    struct kref ref;
};
static DEFINE_MUTEX(lock_a);
static DEFINE_MUTEX(lock_b);
static unsigned int mode;
module_param(mode, uint, 0444);
MODULE_PARM_DESC(mode, "0在A外最后归还，1故意在A内最后归还，2在A内非最后归还");
static unsigned int release_calls;

static void lock_order_release(struct kref *ref)
{
    struct lock_order_object *obj = container_of(ref, struct lock_order_object, ref);
    mutex_lock(&lock_b); /* 最后put同步到达此处，继承调用者已有的持锁上下文。 */
    ++release_calls;
    mutex_unlock(&lock_b);
    kfree(obj);
}
static int __init note_lock_order_init(void)
{
    struct lock_order_object *obj;
    if (mode > 2)
        return -EINVAL;
    if (mode == 1 && !IS_ENABLED(CONFIG_PROVE_LOCKING))
        return -EOPNOTSUPP;
    obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return -ENOMEM;
    kref_init(&obj->ref);
    if (mode == 2)
        kref_get(&obj->ref); /* 另留一份，保证A内的这次put不是最后一次。 */
    mutex_lock(&lock_a);
    if (mode != 0)
        kref_put(&obj->ref, lock_order_release);
    mutex_unlock(&lock_a);
    if (mode != 1)
        kref_put(&obj->ref, lock_order_release);
    obj = NULL; /* 三种模式到这里都已释放，不再通过旧地址观察。 */

    /* 先前的锁都已归还；此路径检查历史顺序，不安排真实双任务互等。 */
    mutex_lock(&lock_b);
    mutex_lock(&lock_a);
    mutex_unlock(&lock_a);
    mutex_unlock(&lock_b);
    pr_info("note_lock_order: mode=%u release=%u\n", mode, release_calls);
    return 0;
}
static void __exit note_lock_order_exit(void)
{
    pr_info("note_lock_order: exit release=%u\n", release_calls);
}
module_init(note_lock_order_init);
module_exit(note_lock_order_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("最后引用回调与历史锁依赖的顺序实验");
