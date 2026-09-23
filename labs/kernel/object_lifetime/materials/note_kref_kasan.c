// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/slab.h>

struct diagnostic_object {
    struct kref ref;
    int id;
};
static bool fault;
module_param(fault, bool, 0444);
MODULE_PARM_DESC(fault, "默认正确路径；true仅在Generic KASAN实验内核触发故意UAF");
static unsigned int release_calls;

static void diagnostic_release(struct kref *ref)
{
    struct diagnostic_object *obj = container_of(ref, struct diagnostic_object, ref);
    ++release_calls; /* 统计位于对象外，释放后不读对象。 */
    kfree(obj);
}

static noinline int observe_released_object(struct diagnostic_object *obj)
{
    /* 故意错误的实验点：READ_ONCE只要求读取，并不能恢复存储期限。 */
    return READ_ONCE(obj->id);
}

static int __init note_kasan_init(void)
{
    struct diagnostic_object *obj;
    int saved_id;
    /* 构建条件不满足时拒绝故障路径，避免将普通内核崩溃当成KASAN结果。 */
    if (fault && !IS_ENABLED(CONFIG_KASAN_GENERIC))
        return -EOPNOTSUPP;
    obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return -ENOMEM;
    kref_init(&obj->ref);
    obj->id = 9;
    saved_id = obj->id;
    kref_put(&obj->ref, diagnostic_release);
    if (fault)
        pr_info("note_kasan: invalid read=%d\n", observe_released_object(obj));
    else
        pr_info("note_kasan: saved=%d releases=%u\n", saved_id, release_calls);
    return 0;
}

static void __exit note_kasan_exit(void)
{
    /* 初始化中唯一对象已经归还；本例没有线程、队列或外部入口。 */
    pr_info("note_kasan: unloaded releases=%u\n", release_calls);
}
module_init(note_kasan_init);
module_exit(note_kasan_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Generic KASAN下最后归还后的故意读取与标量副本对照");
