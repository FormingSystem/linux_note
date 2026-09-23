// SPDX-License-Identifier: GPL-2.0
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/slab.h>

struct my_refobj {
    int id;
    struct kref ref;
    int state;
    char *data;
};
static unsigned int release_calls;

static void my_refobj_release(struct kref *ref)
{
    struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

    kfree(refobj->data); /* kzalloc 使尚未申请成功的 data 为 NULL。 */
    ++release_calls; /* 完成记录放在即将释放的对象之外。 */
    kfree(refobj);
}

static void my_refobj_put(struct my_refobj *refobj)
{
    if (refobj) /* 本封装允许归还空槽，非空时仍须持有一份。 */
        kref_put(&refobj->ref, my_refobj_release);
}

static struct my_refobj *my_refobj_alloc(int id)
{
    struct my_refobj *refobj = kzalloc(sizeof(*refobj), GFP_KERNEL);
    if (!refobj)
        return NULL;

    kref_init(&refobj->ref); /* 创建者先持初始一份，尚未发布。 */
    refobj->id = id;
    refobj->data = kstrdup("note", GFP_KERNEL);
    if (!refobj->data) {
        my_refobj_put(refobj); /* 失败也经相同的清理出口。 */
        return NULL;
    }
    refobj->state = 1;
    return refobj; /* 成功才将完整可用对象交给调用者。 */
}

static struct my_refobj *my_refobj_get(struct my_refobj *refobj)
{
    /* 前提：非空且当前路径已有有效引用；不做查找或有效性探测。 */
    kref_get(&refobj->ref);
    return refobj;
}

static int __init note_object_init(void)
{
    struct my_refobj *creator = my_refobj_alloc(7);
    struct my_refobj *consumer;

    if (!creator)
        return -ENOMEM;
    consumer = my_refobj_get(creator);
    my_refobj_put(creator);
    creator = NULL; /* 结束此槽的使用，其他别名不会被自动清空。 */

    pr_info("note_object: id=%d state=%d data=%s\n",
            consumer->id, consumer->state, consumer->data);
    my_refobj_put(consumer);
    consumer = NULL;
    return 0;
}

static void __exit note_object_exit(void)
{
    /* 本例所有使用在 init 内同步完成，没有外部用户。 */
    pr_info("note_object: release_calls=%u\n", release_calls);
}

module_init(note_object_init);
module_exit(note_object_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("类型封装与部分初始化清理实验");
