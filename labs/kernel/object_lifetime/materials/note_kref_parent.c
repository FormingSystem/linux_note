// SPDX-License-Identifier: GPL-2.0
#include <linux/err.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

struct parent_object {
    struct kref ref;
    struct mutex gate;
    struct list_head children; /* 非拥有索引，不为child增加一份。 */
    bool closing;
};
struct child_object {
    struct kref ref;
    struct parent_object *parent; /* 每个child合计持有parent的一份。 */
    struct list_head node;
    unsigned int completed;
};
static unsigned int parent_releases, child_releases;

static void parent_release(struct kref *ref)
{
    struct parent_object *parent = container_of(ref, struct parent_object, ref);
    WARN_ON(!list_empty(&parent->children));
    ++parent_releases;
    kfree(parent);
}
static void parent_put(struct parent_object *parent)
{
    kref_put(&parent->ref, parent_release);
}
static struct parent_object *parent_create(void)
{
    struct parent_object *parent = kzalloc(sizeof(*parent), GFP_KERNEL);
    if (!parent)
        return NULL;
    kref_init(&parent->ref);
    mutex_init(&parent->gate);
    INIT_LIST_HEAD(&parent->children);
    return parent;
}
/* 调用者持有child；只改索引，不消费child或parent的任何份额。 */
static void child_detach(struct child_object *child)
{
    struct parent_object *parent = child->parent;
    mutex_lock(&parent->gate);
    if (!list_empty(&child->node))
        list_del_init(&child->node);
    mutex_unlock(&parent->gate);
}
static void child_release(struct kref *ref)
{
    struct child_object *child = container_of(ref, struct child_object, ref);
    struct parent_object *parent = child->parent;
    /* 内部最终清理可以在零计数时操作自己的存储，父桥接份额仍然存在。 */
    child_detach(child);
    ++child_releases;
    kfree(child);
    parent_put(parent); /* 此后不能再访问child或parent。 */
}
static void child_put(struct child_object *child)
{
    kref_put(&child->ref, child_release);
}
/* 调用者已有parent份额，覆盖锁外分配；只在锁内最终检查后建立桥接并发布。 */
static struct child_object *child_create(struct parent_object *parent)
{
    struct child_object *child = kzalloc(sizeof(*child), GFP_KERNEL);
    if (!child)
        return ERR_PTR(-ENOMEM);
    INIT_LIST_HEAD(&child->node);
    mutex_lock(&parent->gate);
    if (parent->closing) {
        mutex_unlock(&parent->gate);
        kfree(child); /* 尚未建立child引用协议和父桥接，只回滚私有分配。 */
        return ERR_PTR(-ESHUTDOWN);
    }
    kref_get(&parent->ref);
    child->parent = parent;
    kref_init(&child->ref);
    list_add_tail(&child->node, &parent->children);
    mutex_unlock(&parent->gate);
    return child;
}
static int child_request(struct child_object *child)
{
    struct parent_object *parent = child->parent;
    int result = -ESHUTDOWN;
    mutex_lock(&parent->gate);
    if (!parent->closing) {
        ++child->completed;
        result = 0;
    }
    mutex_unlock(&parent->gate);
    return result;
}
/* 仅关闭创建与本例同步业务，不等待所有child消失，不消费调用者份额。 */
static void parent_close(struct parent_object *parent)
{
    mutex_lock(&parent->gate);
    parent->closing = true;
    mutex_unlock(&parent->gate);
}
static int __init note_parent_init(void)
{
    struct parent_object *parent = parent_create();
    struct child_object *child;
    int before, after;
    if (!parent)
        return -ENOMEM;
    child = child_create(parent);
    if (IS_ERR(child)) {
        parent_put(parent);
        return PTR_ERR(child);
    }
    kref_get(&child->ref); /* 第二个child拥有者，不另给parent加一份。 */
    before = child_request(child);
    parent_close(parent);
    parent_put(parent); /* 结束管理者，child的桥接仍保留父对象。 */
    child_detach(child);
    child_detach(child); /* 重复摘链不归还任何调用者份额。 */
    child_put(child); /* 第一个child拥有者结束，另一个仍持有。 */
    after = child_request(child);
    pr_info("note_parent: before=%d after=%d completed=%u parent_free=%u\n",
            before, after, child->completed, parent_releases);
    child_put(child); /* child最终清理后，父桥接份额才归还。 */
    return 0;
}
static void __exit note_parent_exit(void)
{
    /* 无外部入口或异步回调，init已经结束全部责任。 */
    pr_info("note_parent: children=%u parents=%u\n", child_releases, parent_releases);
}
module_init(note_parent_init);
module_exit(note_parent_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("子对象桥接父份额与非拥有链表的完整退出实验");
