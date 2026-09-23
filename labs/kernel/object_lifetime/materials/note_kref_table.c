// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

enum object_state { OBJECT_NEW, OBJECT_LIVE, OBJECT_DYING };
struct table_object {
    int id; /* 发布前固定；编号在当前表内唯一。 */
    struct list_head node; /* table_lock保护。 */
    struct mutex lock; /* 保护state与completed。 */
    enum object_state state;
    unsigned int completed;
    struct kref ref;
};
static LIST_HEAD(object_table);
static DEFINE_MUTEX(table_lock);
static unsigned int release_calls; /* 本模块无外部入口，初始化内顺序演示。 */

static void table_release(struct kref *ref)
{
    struct table_object *obj = container_of(ref, struct table_object, ref);
    WARN_ON(!list_empty(&obj->node));
    WARN_ON(obj->state == OBJECT_LIVE);
    ++release_calls;
    kfree(obj); /* 正常协议已无其他使用者，所有对象锁操作已经结束。 */
}

static void table_put(struct table_object *obj)
{
    if (obj)
        kref_put(&obj->ref, table_release);
}

static struct table_object *table_create(int id)
{
    struct table_object *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    obj->id = id;
    INIT_LIST_HEAD(&obj->node);
    mutex_init(&obj->lock);
    obj->state = OBJECT_NEW;
    kref_init(&obj->ref);
    return obj;
}

/* 调用者持有一份；成功另给表一份，失败不消费。只发布一次。 */
static int table_publish(struct table_object *obj)
{
    struct table_object *candidate;
    int result = -EINVAL;
    mutex_lock(&table_lock);
    mutex_lock(&obj->lock);
    if (obj->state != OBJECT_NEW || !list_empty(&obj->node))
        goto out;
    list_for_each_entry(candidate, &object_table, node) {
        if (candidate->id == obj->id) {
            result = -EEXIST;
            goto out;
        }
    }
    kref_get(&obj->ref);
    obj->state = OBJECT_LIVE;
    list_add_tail(&obj->node, &object_table);
    result = 0;
out:
    mutex_unlock(&obj->lock);
    mutex_unlock(&table_lock);
    return result;
}

static struct table_object *table_lookup(int id)
{
    struct table_object *obj, *found = NULL;
    mutex_lock(&table_lock);
    list_for_each_entry(obj, &object_table, node) {
        if (obj->id != id)
            continue;
        mutex_lock(&obj->lock);
        if (obj->state == OBJECT_LIVE) {
            kref_get(&obj->ref);
            found = obj;
        }
        mutex_unlock(&obj->lock);
        break;
    }
    mutex_unlock(&table_lock);
    return found;
}

/* 仅同步增加计数；不启动硬件或异步工作。调用者持有一份。 */
static int table_request(struct table_object *obj, unsigned int *completed)
{
    int result = -ESHUTDOWN;
    mutex_lock(&obj->lock);
    if (obj->state == OBJECT_LIVE) {
        *completed = ++obj->completed;
        result = 0;
    }
    mutex_unlock(&obj->lock);
    return result;
}

/* 调用者独立持有一份；只归还本次取回的表份额，支持有效参数上重复调用。 */
static void table_unpublish(struct table_object *obj)
{
    bool removed = false;
    mutex_lock(&table_lock);
    mutex_lock(&obj->lock);
    if (!list_empty(&obj->node)) {
        obj->state = OBJECT_DYING;
        list_del_init(&obj->node);
        removed = true;
    }
    mutex_unlock(&obj->lock);
    mutex_unlock(&table_lock);
    if (removed)
        table_put(obj);
}

static int __init note_table_init(void)
{
    struct table_object *creator = table_create(7), *reader;
    unsigned int completed = 0;
    int result;
    if (!creator)
        return -ENOMEM;
    result = table_publish(creator);
    if (result) {
        table_put(creator);
        return result;
    }
    reader = table_lookup(7);
    if (!reader) {
        table_unpublish(creator);
        table_put(creator);
        return -ENOENT;
    }
    table_put(creator);
    result = table_request(reader, &completed);
    pr_info("note_table: before=%d completed=%u\n", result, completed);
    table_unpublish(reader);
    table_unpublish(reader);
    result = table_request(reader, &completed);
    pr_info("note_table: after=%d completed=%u\n", result, completed);
    table_put(reader);
    return 0;
}

static void __exit note_table_exit(void)
{
    pr_info("note_table: release=%u empty=%d\n", release_calls, list_empty(&object_table));
}

module_init(note_table_init);
module_exit(note_table_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("集合锁与对象锁配对的完整同步服务实验");
