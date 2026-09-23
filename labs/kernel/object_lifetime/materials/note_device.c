// SPDX-License-Identifier: GPL-2.0
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/slab.h>

struct note_device {
    struct device dev;
    int value; /* 本例发布前固定，不表示硬件仍可操作。 */
};
static unsigned int release_calls;

static void note_device_release(struct device *dev)
{
    struct note_device *obj = container_of(dev, struct note_device, dev);
    ++release_calls;
    kfree(obj);
}

static int __init note_device_init(void)
{
    struct note_device *creator, *reader;
    struct device *held;
    int result;
    if (!IS_ENABLED(CONFIG_SYSFS) || IS_ENABLED(CONFIG_DEBUG_KOBJECT_RELEASE))
        return -EOPNOTSUPP; /* 同步演示不实现调试延迟释放的代码退出。 */
    creator = kzalloc(sizeof(*creator), GFP_KERNEL);
    if (!creator)
        return -ENOMEM;
    creator->value = 7;
    device_initialize(&creator->dev);
    creator->dev.release = note_device_release;
    result = dev_set_name(&creator->dev, "note_device_lifetime");
    if (result)
        goto put_creator;
    result = device_add(&creator->dev);
    if (result)
        goto put_creator;
    held = get_device(&creator->dev); /* 已有正引用，取得观察者一份。 */
    reader = container_of(held, struct note_device, dev);
    pr_info("note_device: added value=%d release=%u\n", reader->value, release_calls);
    device_unregister(&creator->dev); /* 已包含归还初始化份额，不再额外put它。 */
    creator = NULL;
    pr_info("note_device: removed value=%d release=%u\n", reader->value, release_calls);
    put_device(held);
    return 0;
put_creator:
    put_device(&creator->dev); /* 初始化后，即使命名或添加失败也由框架清理。 */
    return result;
}

static void __exit note_device_exit(void)
{
    pr_info("note_device: release=%u\n", release_calls);
}
module_init(note_device_init);
module_exit(note_device_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("设备注销与最终引用回收的同步演示");
