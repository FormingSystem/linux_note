// SPDX-License-Identifier: GPL-2.0
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/slab.h>

struct basic_object {
    struct kref ref;
    unsigned int id;
};
static unsigned int holders = 1;
module_param(holders, uint, 0444);
MODULE_PARM_DESC(holders, "顺序模拟的独立持有者数量：1或2");
static unsigned int release_calls;

static void basic_release(struct kref *ref)
{
    struct basic_object *obj = container_of(ref, struct basic_object, ref);
    pr_info("note_basics: release id=%u\n", obj->id);
    ++release_calls; /* 记录放在对象外，归还以后只读此计数。 */
    kfree(obj);
}
static int __init note_basics_init(void)
{
    struct basic_object *slots[2] = { NULL, NULL };
    struct basic_object *creator;
    unsigned int index;

    if (holders < 1 || holders > 2)
        return -EINVAL;
    creator = kzalloc(sizeof(*creator), GFP_KERNEL);
    if (!creator)
        return -ENOMEM;
    kref_init(&creator->ref);
    creator->id = 7;
    slots[0] = creator; /* 初始份额交给第一个责任槽位。 */
    pr_info("note_basics: initialized ref=%u\n", kref_read(&creator->ref));
    if (holders == 2) {
        kref_get(&creator->ref); /* 原份额有效时，为第二个持有者追加一份。 */
        slots[1] = creator;
        pr_info("note_basics: shared ref=%u\n", kref_read(&creator->ref));
    }
    creator = NULL; /* 辅助别名不再参与后续访问。 */
    for (index = 0; index < holders; ++index) {
        struct basic_object *owned = slots[index];
        int final;
        slots[index] = NULL;
        pr_info("note_basics: holder=%u use id=%u\n", index + 1, owned->id);
        final = kref_put(&owned->ref, basic_release);
        /* 最后put也不再读取owned，只使用局部返回值和外部记录。 */
        pr_info("note_basics: holder=%u put_final=%d releases=%u\n",
                index + 1, final, release_calls);
    }
    return 0;
}
static void __exit note_basics_exit(void)
{
    /* 两个槽位在init同步结束，不代表两个线程。没有异步或外部入口。 */
    pr_info("note_basics: unloaded releases=%u\n", release_calls);
}
module_init(note_basics_init);
module_exit(note_basics_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("一份与两份引用的完整基础观察实验");
