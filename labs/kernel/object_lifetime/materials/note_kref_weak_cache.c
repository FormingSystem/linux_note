// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

struct cached_object {
    struct kref ref;
    struct rcu_head rcu;
    int value; /* 发布前固定，取得对象后只读。 */
};
static struct cached_object __rcu *cached;
static DEFINE_SPINLOCK(cache_lock);
static atomic_t free_calls = ATOMIC_INIT(0);

static void cached_free(struct rcu_head *head)
{
    struct cached_object *obj = container_of(head, struct cached_object, rcu);
    atomic_inc(&free_calls);
    kfree(obj);
}
static void cached_release(struct kref *ref)
{
    struct cached_object *obj = container_of(ref, struct cached_object, ref);
    unsigned long flags;
    spin_lock_irqsave(&cache_lock, flags);
    /* 只撤销指向自己的槽，不能抹掉并发替换进去的另一个对象。 */
    if (rcu_access_pointer(cached) == obj)
        rcu_assign_pointer(cached, NULL);
    spin_unlock_irqrestore(&cache_lock, flags);
    call_rcu(&obj->rcu, cached_free); /* 撤销入口后，才开始延迟回收。 */
}
static void cached_put(struct cached_object *obj)
{
    kref_put(&obj->ref, cached_release);
}
static struct cached_object *cached_create(int value)
{
    struct cached_object *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    kref_init(&obj->ref);
    obj->value = value;
    return obj;
}
/* 非NULL参数必须由调用者持有一份；槽不取得引用，也不消费参数份额。 */
static void cache_set(struct cached_object *obj)
{
    unsigned long flags;
    spin_lock_irqsave(&cache_lock, flags);
    rcu_assign_pointer(cached, obj);
    spin_unlock_irqrestore(&cache_lock, flags);
}
static struct cached_object *cache_get(void)
{
    struct cached_object *obj;
    rcu_read_lock();
    obj = rcu_dereference(cached);
    if (obj && !kref_get_unless_zero(&obj->ref))
        obj = NULL;
    rcu_read_unlock();
    return obj; /* 成功交付一份，失败不交付。 */
}
static int __init note_cache_init(void)
{
    struct cached_object *creator = cached_create(42), *reader;
    if (!creator)
        return -ENOMEM;
    cache_set(creator);
    reader = cache_get();
    if (!reader) {
        cached_put(creator);
        rcu_barrier();
        return -ENOENT;
    }
    cached_put(creator);
    cache_set(NULL); /* 清缓存不put；槽从未拥有一份。 */
    pr_info("note_cache: value=%d\n", reader->value);
    cache_set(reader); /* 当前有独立份额，允许再次缓存同一有效对象。 */
    cached_put(reader); /* 最后一份归还，release先清槽再安排回收。 */
    pr_info("note_cache: empty=%d\n", rcu_access_pointer(cached) == NULL);
    return 0;
}
static void __exit note_cache_exit(void)
{
    /* 无外部入口；init已归还全部份额，来源停止后等模块回调退出。 */
    rcu_barrier();
    pr_info("note_cache: free=%d\n", atomic_read(&free_calls));
}
module_init(note_cache_init);
module_exit(note_cache_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("单个弱缓存槽在最后归还时撤销并延迟回收");
