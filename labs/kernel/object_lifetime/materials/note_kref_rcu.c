// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rculist.h>
#include <linux/slab.h>

struct rcu_object {
    struct kref ref;
    struct rcu_head rcu;
    struct list_head node;
    struct mutex lock; /* 保护dying与completed；读区外使用。 */
    int id; /* 发布前固定，旧读者可以在读区内比较。 */
    bool linked, ever_published; /* 仅由update_lock保护。 */
    bool dying;
    unsigned int completed;
};
static LIST_HEAD(rcu_table);
static DEFINE_MUTEX(update_lock);
static atomic_t free_calls = ATOMIC_INIT(0);

static void object_rcu_free(struct rcu_head *head)
{
    struct rcu_object *obj = container_of(head, struct rcu_object, rcu);
    atomic_inc(&free_calls);
    kfree(obj); /* 回调不睡眠；旧临时读者已经越过所需边界。 */
}

static void object_release(struct kref *ref)
{
    struct rcu_object *obj = container_of(ref, struct rcu_object, ref);
    WARN_ON(obj->linked);
    call_rcu(&obj->rcu, object_rcu_free); /* 私有失败对象也走同一回收出口。 */
}

static void object_put(struct rcu_object *obj)
{
    if (obj)
        kref_put(&obj->ref, object_release);
}

static struct rcu_object *object_create(int id)
{
    struct rcu_object *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    obj->id = id;
    obj->dying = true; /* 私有对象尚不接纳请求。 */
    INIT_LIST_HEAD(&obj->node);
    mutex_init(&obj->lock);
    kref_init(&obj->ref);
    return obj;
}

/* 调用者保留自己的份额；成功另外建立表份额，失败不消费。 */
static int object_publish(struct rcu_object *obj)
{
    struct rcu_object *candidate;
    int result = -EINVAL;
    mutex_lock(&update_lock);
    if (obj->ever_published)
        goto out;
    list_for_each_entry(candidate, &rcu_table, node) {
        if (candidate->id == obj->id) {
            result = -EEXIST;
            goto out;
        }
    }
    mutex_lock(&obj->lock);
    obj->dying = false;
    kref_get(&obj->ref);
    obj->linked = obj->ever_published = true;
    list_add_rcu(&obj->node, &rcu_table);
    mutex_unlock(&obj->lock);
    result = 0;
out:
    mutex_unlock(&update_lock);
    return result;
}

/* 只取得存储的长期份额；不承诺后续业务请求仍会被接纳。 */
static struct rcu_object *object_lookup(int id)
{
    struct rcu_object *obj, *found = NULL;
    rcu_read_lock();
    list_for_each_entry_rcu(obj, &rcu_table, node) {
        if (obj->id == id) {
            if (kref_get_unless_zero(&obj->ref))
                found = obj;
            break;
        }
    }
    rcu_read_unlock();
    return found;
}

static int object_request(struct rcu_object *obj, unsigned int *completed)
{
    int result = -ESHUTDOWN;
    mutex_lock(&obj->lock); /* 调用者持有引用，且已经离开普通RCU读区。 */
    if (!obj->dying) {
        *completed = ++obj->completed;
        result = 0;
    }
    mutex_unlock(&obj->lock);
    return result;
}

/* 调用者另持一份；先update_lock再对象锁，只有本次摘下才归还表份额。 */
static void object_unpublish(struct rcu_object *obj)
{
    bool removed = false;
    mutex_lock(&update_lock);
    mutex_lock(&obj->lock);
    if (obj->linked) {
        obj->dying = true;
        list_del_rcu(&obj->node); /* 不清空next，旧读者可能仍沿它遍历。 */
        obj->linked = false;
        removed = true;
    }
    mutex_unlock(&obj->lock);
    mutex_unlock(&update_lock);
    if (removed)
        object_put(obj);
}

static int __init note_rcu_init(void)
{
    struct rcu_object *creator = object_create(7), *reader;
    unsigned int completed = 0;
    int result;
    if (!creator)
        return -ENOMEM;
    result = object_publish(creator);
    if (result) {
        object_put(creator);
        rcu_barrier(); /* 初始化失败也不能留下指向本模块代码的回调。 */
        return result;
    }
    reader = object_lookup(7);
    if (!reader) {
        object_unpublish(creator);
        object_put(creator);
        rcu_barrier();
        return -ENOENT;
    }
    object_put(creator);
    result = object_request(reader, &completed);
    pr_info("note_rcu: before=%d completed=%u\n", result, completed);
    object_unpublish(reader);
    object_unpublish(reader);
    result = object_request(reader, &completed);
    pr_info("note_rcu: after=%d completed=%u\n", result, completed);
    object_put(reader);
    return 0;
}

static void __exit note_rcu_exit(void)
{
    /* 本演示无外部入口，初始化已停止所有来源并归还全部份额。 */
    rcu_barrier();
    pr_info("note_rcu: free=%d empty=%d\n", atomic_read(&free_calls), list_empty(&rcu_table));
}
module_init(note_rcu_init);
module_exit(note_rcu_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RCU临时读取转长期引用与模块回调退出实验");
