// SPDX-License-Identifier: GPL-2.0
#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/hash.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#define OBJECT_HASH_BITS 2
#define OBJECT_HASH_SIZE (1U << OBJECT_HASH_BITS)
enum hash_state { HASH_NEW, HASH_LIVE, HASH_DYING };
struct hash_object {
    struct kref ref;
    struct hlist_node node;
    u32 id; /* 创建后固定，不能在已发布时换桶。 */
    enum hash_state state;
    unsigned int completed;
};
static struct hlist_head object_table[OBJECT_HASH_SIZE];
/* 本例用同一锁保护桶、业务状态与短统计操作。 */
static DEFINE_SPINLOCK(table_lock);
static atomic_t release_calls = ATOMIC_INIT(0);

static void hash_release(struct kref *ref)
{
    struct hash_object *obj = container_of(ref, struct hash_object, ref);
    WARN_ON(!hlist_unhashed(&obj->node));
    WARN_ON(obj->state == HASH_LIVE);
    atomic_inc(&release_calls);
    kfree(obj); /* 无睡眠清理；此时不再持table_lock。 */
}
static void hash_put(struct hash_object *obj)
{
    if (obj)
        kref_put(&obj->ref, hash_release);
}
static struct hash_object *hash_create(u32 id)
{
    struct hash_object *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    kref_init(&obj->ref);
    INIT_HLIST_NODE(&obj->node);
    obj->id = id;
    obj->state = HASH_NEW;
    return obj;
}
/* 输入已有一份；只发布一次，成功另给表一份，失败保留原份额。 */
static int hash_publish(struct hash_object *obj)
{
    struct hash_object *candidate;
    unsigned long flags;
    unsigned int bucket = hash_32(obj->id, OBJECT_HASH_BITS);
    int result = -EINVAL;
    spin_lock_irqsave(&table_lock, flags);
    if (obj->state != HASH_NEW || !hlist_unhashed(&obj->node))
        goto out;
    hlist_for_each_entry(candidate, &object_table[bucket], node) {
        if (candidate->id == obj->id) {
            result = -EEXIST;
            goto out;
        }
    }
    kref_get(&obj->ref);
    obj->state = HASH_LIVE;
    hlist_add_head(&obj->node, &object_table[bucket]);
    result = 0;
out:
    spin_unlock_irqrestore(&table_lock, flags);
    return result;
}
static struct hash_object *hash_lookup(u32 id)
{
    struct hash_object *obj, *found = NULL;
    unsigned long flags;
    unsigned int bucket = hash_32(id, OBJECT_HASH_BITS);
    spin_lock_irqsave(&table_lock, flags);
    hlist_for_each_entry(obj, &object_table[bucket], node) {
        if (obj->id == id && obj->state == HASH_LIVE) {
            kref_get(&obj->ref); /* 表份额保证可见对象仍为正计数。 */
            found = obj;
            break;
        }
    }
    spin_unlock_irqrestore(&table_lock, flags);
    return found;
}
static int hash_request(struct hash_object *obj, unsigned int *completed)
{
    unsigned long flags;
    int result = -ESHUTDOWN;
    spin_lock_irqsave(&table_lock, flags);
    if (obj->state == HASH_LIVE) {
        *completed = ++obj->completed;
        result = 0;
    }
    spin_unlock_irqrestore(&table_lock, flags);
    return result;
}
/* 调用者另有一份；重复撤下只在有效输入上成立。 */
static void hash_unpublish(struct hash_object *obj)
{
    unsigned long flags;
    bool removed = false;
    spin_lock_irqsave(&table_lock, flags);
    if (!hlist_unhashed(&obj->node)) {
        obj->state = HASH_DYING;
        hlist_del_init(&obj->node);
        removed = true;
    }
    spin_unlock_irqrestore(&table_lock, flags);
    if (removed)
        hash_put(obj);
}
static int __init note_hash_init(void)
{
    struct hash_object *creator, *reader;
    unsigned int completed = 0;
    int result;
    for (unsigned int i = 0; i < OBJECT_HASH_SIZE; ++i)
        INIT_HLIST_HEAD(&object_table[i]);
    creator = hash_create(7);
    if (!creator)
        return -ENOMEM;
    result = hash_publish(creator);
    if (result) {
        hash_put(creator);
        return result;
    }
    reader = hash_lookup(7);
    if (!reader) {
        hash_unpublish(creator);
        hash_put(creator);
        return -ENOENT;
    }
    hash_put(creator);
    result = hash_request(reader, &completed);
    pr_info("note_hash: before=%d completed=%u\n", result, completed);
    hash_unpublish(reader);
    hash_unpublish(reader);
    result = hash_request(reader, &completed);
    pr_info("note_hash: after=%d completed=%u\n", result, completed);
    hash_put(reader);
    return 0;
}
static void __exit note_hash_exit(void)
{
    /* 本模块没有外部IRQ或用户入口，全部使用在init内结束。 */
    pr_info("note_hash: release=%d\n", atomic_read(&release_calls));
}
module_init(note_hash_init);
module_exit(note_hash_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("拥有型哈希与IRQ保存锁模板");
