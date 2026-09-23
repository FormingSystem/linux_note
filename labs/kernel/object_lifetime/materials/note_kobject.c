// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/slab.h>

struct named_object {
    struct kobject kobj;
    int value; /* 发布前固定，本例不提供属性读写入口。 */
};
static unsigned int release_calls;

static void named_release(struct kobject *kobj)
{
    struct named_object *obj = container_of(kobj, struct named_object, kobj);
    ++release_calls;
    kfree(obj); /* 名称和父引用由kobject core按自己的协议清理。 */
}
static const struct kobj_type named_type = { .release = named_release };

static int __init note_kobject_init(void)
{
    struct named_object *creator, *reader;
    struct kobject *held;
    int result;

    /* 此同步演示不实现延迟调试释放时模块代码的异步退出协议。 */
    if (!IS_ENABLED(CONFIG_SYSFS) || IS_ENABLED(CONFIG_DEBUG_KOBJECT_RELEASE))
        return -EOPNOTSUPP;
    creator = kzalloc(sizeof(*creator), GFP_KERNEL);
    if (!creator)
        return -ENOMEM;
    creator->value = 7;
    result = kobject_init_and_add(&creator->kobj, &named_type,
                                 kernel_kobj, "note_kref_lifetime");
    if (result) {
        kobject_put(&creator->kobj); /* 初始化已完成，失败也经类型回调。 */
        return result;
    }
    held = kobject_get(&creator->kobj); /* 已拥有正引用，追加观察者的一份。 */
    reader = container_of(held, struct named_object, kobj);
    pr_info("note_kobject: added value=%d release=%u\n", reader->value, release_calls);
    kobject_del(&creator->kobj); /* 撤下层次与sysfs入口，不归还本对象初始一份。 */
    kobject_put(&creator->kobj);
    creator = NULL;
    pr_info("note_kobject: removed value=%d release=%u\n", reader->value, release_calls);
    kobject_put(held); /* 最后观察者归还，core再调用named_type.release。 */
    return 0;
}

static void __exit note_kobject_exit(void)
{
    /* 无外部入口、无异步持有者；支持配置下init已完成所有归还。 */
    pr_info("note_kobject: release=%u\n", release_calls);
}
module_init(note_kobject_init);
module_exit(note_kobject_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("kobject撤下与最后引用分离的同步演示");
