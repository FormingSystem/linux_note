// SPDX-License-Identifier: GPL-2.0
#include <linux/err.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/slab.h>

struct template_object {
    struct kref ref;
    int id;
    char *buffer;
};
static int fail_stage;
module_param(fail_stage, int, 0444);
MODULE_PARM_DESC(fail_stage, "模拟失败阶段：0正常，1外壳，2缓冲区，3业务校验");
static unsigned int release_calls;

static void object_release(struct kref *ref)
{
    struct template_object *obj = container_of(ref, struct template_object, ref);
    kfree(obj->buffer); /* NULL表示该子资源尚未建立。 */
    ++release_calls; /* 记录保存在对象外，不在free后读取对象。 */
    kfree(obj);
}

static void object_put(struct template_object *obj)
{
    /* 只接受持有一份的有效对象，不接受NULL或ERR_PTR。 */
    kref_put(&obj->ref, object_release);
}

static struct template_object *object_get(struct template_object *obj)
{
    /* 调用者已有一份，新增的一份可交给另一责任槽位。 */
    kref_get(&obj->ref);
    return obj;
}

static struct template_object *object_alloc(int id)
{
    struct template_object *obj;
    if (fail_stage == 1)
        return NULL; /* 在取得任何分配前模拟失败。 */
    obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    kref_init(&obj->ref);
    obj->id = id;
    return obj; /* 仅交给内部创建路径，尚不能发布给业务用户。 */
}

static int object_prepare(struct template_object *obj)
{
    /* 前提：创建者独占、尚未发布、只调用一次。 */
    if (fail_stage == 2)
        return -ENOMEM;
    obj->buffer = kstrdup("ready", GFP_KERNEL);
    if (!obj->buffer)
        return -ENOMEM;
    if (fail_stage == 3)
        return -EINVAL; /* 已有子资源时模拟业务校验失败。 */
    return 0;
}

static struct template_object *object_create(int id)
{
    struct template_object *obj = object_alloc(id);
    int ret;
    if (!obj)
        return ERR_PTR(-ENOMEM);
    ret = object_prepare(obj);
    if (ret) {
        object_put(obj); /* 唯一回滚入口，处理半初始化对象。 */
        return ERR_PTR(ret);
    }
    return obj; /* 成功交付完整对象及初始一份；不负责发布。 */
}

static int __init note_template_init(void)
{
    struct template_object *creator, *observer;
    if (fail_stage < 0 || fail_stage > 3)
        return -EINVAL;
    creator = object_create(7);
    if (IS_ERR(creator)) {
        pr_info("note_template: stage=%d error=%ld releases=%u\n",
                fail_stage, PTR_ERR(creator), release_calls);
        return PTR_ERR(creator); /* 无对象交付，调用者不能再put。 */
    }
    observer = object_get(creator);
    object_put(creator);
    creator = NULL;
    pr_info("note_template: id=%d buffer=%s releases=%u\n",
            observer->id, observer->buffer, release_calls);
    object_put(observer);
    observer = NULL;
    pr_info("note_template: finished releases=%u\n", release_calls);
    return 0;
}

static void __exit note_template_exit(void)
{
    /* 使用均在init同步结束，无外部入口、异步工作或卸载等待。 */
}
module_init(note_template_init);
module_exit(note_template_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("分层创建与统一失败回滚模板");
