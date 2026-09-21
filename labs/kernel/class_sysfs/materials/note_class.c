// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sysfs.h>

struct note_instance {
    unsigned int instance_id;
    struct device *dev;
};

static struct class *note_class;
static struct note_instance note_instances[2];

static ssize_t instance_id_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    const struct note_instance *instance = dev_get_drvdata(dev);

    /* 每个设备保存自己的实例地址，共享同一段回调。 */
    return sysfs_emit(buf, "%u\n", instance->instance_id);
}
static DEVICE_ATTR_RO(instance_id);

static struct attribute *note_attrs[] = {
    &dev_attr_instance_id.attr,
    NULL,
};
ATTRIBUTE_GROUPS(note);

static int __init note_class_init(void)
{
    int index;
    int ret;

    note_class = class_create("note_class");
    if (IS_ERR(note_class))
        return PTR_ERR(note_class);

    for (index = 0; index < ARRAY_SIZE(note_instances); ++index) {
        note_instances[index].instance_id = index;
        /* devt 为零：创建分类对象及属性，不申请字符设备号码。 */
        note_instances[index].dev = device_create_with_groups(
            note_class, NULL, 0, &note_instances[index],
            note_groups, "note_class%d", index);
        if (IS_ERR(note_instances[index].dev)) {
            ret = PTR_ERR(note_instances[index].dev);
            goto undo_devices;
        }
    }
    return 0;

undo_devices:
    /* 只注销已经成功创建的对象，零设备号不能区分两个实例。 */
    while (index-- > 0)
        device_unregister(note_instances[index].dev);
    class_destroy(note_class);
    return ret;
}

static void __exit note_class_exit(void)
{
    int index = ARRAY_SIZE(note_instances);

    while (index-- > 0)
        device_unregister(note_instances[index].dev);
    class_destroy(note_class);
}

module_init(note_class_init);
module_exit(note_class_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("两个只有 sysfs 属性的分类实例");
