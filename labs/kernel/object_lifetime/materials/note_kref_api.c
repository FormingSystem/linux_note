// SPDX-License-Identifier: GPL-2.0
#include <linux/err.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/slab.h>

struct api_object {
    struct kref ref;
    unsigned int id; /* 初始化后不变，拥有引用时可读取。 */
};
static bool trace_events = true;
module_param(trace_events, bool, 0444);
MODULE_PARM_DESC(trace_events, "打印引用操作请求，不改变引用动作");
static unsigned int get_requests, put_requests, release_calls, argument_calls;

static void object_release(struct kref *ref)
{
    struct api_object *obj = container_of(ref, struct api_object, ref);
    ++release_calls;
    if (trace_events)
        pr_info("note_api: release id=%u\n", obj->id);
    kfree(obj); /* 最后一次对象字段读取已经结束。 */
}
/* 输入须有合法地址和正计数保护；返回值代表新增的一份。 */
static struct api_object *object_get_at(struct api_object *obj,
                                      const char *function, unsigned int line)
{
    ++get_requests;
    if (trace_events)
        pr_info("note_api: get request id=%u at %s:%u\n", obj->id, function, line);
    kref_get(&obj->ref);
    return obj;
}
/* 归还调用者的一份；不接受NULL、错误指针或借用份额。 */
static void object_put_at(struct api_object *obj,
                          const char *function, unsigned int line)
{
    ++put_requests;
    if (trace_events)
        pr_info("note_api: put request id=%u at %s:%u\n", obj->id, function, line);
    kref_put(&obj->ref, object_release);
    /* 此后不读取obj；统计放在外部静态存储中。 */
}
#define object_get(obj) object_get_at((obj), __func__, __LINE__)
#define object_put(obj) object_put_at((obj), __func__, __LINE__)

/* 接受空的责任槽位；非空必须恰好拥有一份，不能放ERR_PTR。 */
static void object_put_slot(struct api_object **slot)
{
    struct api_object *owned = *slot;
    *slot = NULL; /* 先撤销本地责任记录，再触发可能的最终清理。 */
    if (owned)
        object_put(owned);
}
static struct api_object *object_create(unsigned int id)
{
    struct api_object *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return ERR_PTR(-ENOMEM);
    kref_init(&obj->ref);
    obj->id = id;
    return obj;
}
/* 仅用于观察宏参数求值次数，返回借用地址，不增加责任。 */
static struct api_object *argument_once(struct api_object *obj)
{
    ++argument_calls;
    return obj;
}
static int __init note_api_init(void)
{
    struct api_object *creator, *observer = NULL, *recipient = NULL;
    creator = object_create(7);
    if (IS_ERR(creator))
        return PTR_ERR(creator); /* 工厂没有交付对象，不调用put_slot。 */
    observer = object_get(argument_once(creator));
    recipient = observer; /* 显式转交：引用计数不变。 */
    observer = NULL;
    object_put_slot(&observer); /* 空槽没有一份可还。 */
    object_put_slot(&creator);
    pr_info("note_api: recipient id=%u release_calls=%u\n", recipient->id, release_calls);
    object_put_slot(&recipient);
    object_put_slot(&recipient); /* 同一局部槽已为空，不是再次归还旧份额。 */
    pr_info("note_api: get_requests=%u put_requests=%u releases=%u argument_calls=%u\n",
            get_requests, put_requests, release_calls, argument_calls);
    return 0;
}
static void __exit note_api_exit(void)
{
    /* 所有动作均在init同步完成，无外部入口、共享槽位或异步来源。 */
}
module_init(note_api_init);
module_exit(note_api_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("引用封装、单次求值与清空责任槽位实验");
