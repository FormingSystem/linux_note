// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/errno.h>

static struct kobject *note_kobj;

/* 属性只返回不变的内容，不保存用户缓冲区地址。 */
static ssize_t status_show(struct kobject *kobj,
                           struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "ready\n");
}

static struct kobj_attribute status_attr = __ATTR_RO(status);

static int __init note_sysfs_init(void)
{
    int ret;

    note_kobj = kobject_create_and_add("note_sysfs", kernel_kobj);
    if (!note_kobj)
        return -ENOMEM;

    ret = sysfs_create_file(note_kobj, &status_attr.attr);
    if (ret) {
        kobject_put(note_kobj);
        note_kobj = NULL;
        return ret;
    }
    return 0;
}

static void __exit note_sysfs_exit(void)
{
    /* 先撤销属性并等待活动回调结束，再交还对象引用。 */
    sysfs_remove_file(note_kobj, &status_attr.attr);
    kobject_put(note_kobj);
    note_kobj = NULL;
}

module_init(note_sysfs_init);
module_exit(note_sysfs_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("只读 sysfs 属性教学示例");
