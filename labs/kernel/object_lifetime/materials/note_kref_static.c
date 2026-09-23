// SPDX-License-Identifier: GPL-2.0
#include <linux/kref.h>
#include <linux/module.h>

struct note_static_object {
    int value;
    struct kref ref;
};

/* 初始一份属于模块；静态存储不由 kfree 管理。 */
static struct note_static_object note_object = {
    .value = 42,
    .ref = KREF_INIT(1),
};
static unsigned int release_calls;

static void note_static_release(struct kref *ref)
{
    struct note_static_object *object =
        container_of(ref, struct note_static_object, ref);
    pr_info("note_static: final value=%d\n", object->value);
    ++release_calls; /* 对象外的完成记录；回调不释放静态对象。 */
}

static int __init note_static_init(void)
{
    kref_get(&note_object.ref); /* 在未归还的初始引用下增加临时独立份额。 */
    kref_put(&note_object.ref, note_static_release);
    return 0; /* 模块仍持初始一份。 */
}

static void __exit note_static_exit(void)
{
    /* 没有发布入口、异步 work 或外部使用者，可以归还最后一份。 */
    kref_put(&note_object.ref, note_static_release);
    pr_info("note_static: release_calls=%u\n", release_calls);
}

module_init(note_static_init);
module_exit(note_static_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("静态存储与引用归零分工实验");
