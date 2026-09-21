// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/kstrtox.h>

static const char note_message[] = "hello class\n";

struct note_state {
    struct mutex lock;
    int enabled;
    dev_t devt;
    struct cdev cdev;
    struct class *class;
    struct device *dev;
};
static struct note_state note_state;

static ssize_t enabled_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    struct note_state *state = dev_get_drvdata(dev);
    int value;

    mutex_lock(&state->lock);
    value = state->enabled;
    mutex_unlock(&state->lock);
    return sysfs_emit(buf, "%d\n", value);
}

static ssize_t enabled_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf, size_t count)
{
    struct note_state *state = dev_get_drvdata(dev);
    int value;
    int ret;

    /* 内核在 count 之外补终止零；拒绝请求内部的零字节。 */
    if (memchr(buf, '\0', count))
        return -EINVAL;
    ret = kstrtoint(buf, 10, &value);
    if (ret)
        return ret;
    if (value != 0 && value != 1)
        return -EINVAL;

    /* 解析只改局部量，完整验证成功后才提交共享状态。 */
    mutex_lock(&state->lock);
    state->enabled = value;
    mutex_unlock(&state->lock);
    return count;
}
static DEVICE_ATTR_RW(enabled);

static struct attribute *note_attrs[] = {
    &dev_attr_enabled.attr,
    NULL,
};
ATTRIBUTE_GROUPS(note);

static int note_open(struct inode *inode, struct file *file)
{
    file->private_data = &note_state;
    /* 普通共享文件位置由 VFS 串行更新，禁止定位式读取。 */
    file->f_mode |= FMODE_ATOMIC_POS;
    return nonseekable_open(inode, file);
}

static ssize_t note_read(struct file *file, char __user *buf,
                         size_t count, loff_t *pos)
{
    struct note_state *state = file->private_data;
    ssize_t ret;

    if (!count)
        return 0;
    mutex_lock(&state->lock);
    if (!state->enabled)
        ret = -EACCES;
    else
        ret = simple_read_from_buffer(buf, count, pos, note_message,
                                      sizeof(note_message) - 1);
    mutex_unlock(&state->lock);
    return ret;
}

static const struct file_operations note_fops = {
    .owner = THIS_MODULE,
    .open = note_open,
    .read = note_read,
};

static int __init note_control_init(void)
{
    struct note_state *state = &note_state;
    int ret;

    mutex_init(&state->lock);
    state->enabled = 1;
    ret = alloc_chrdev_region(&state->devt, 0, 1, "note_control");
    if (ret)
        return ret;
    state->class = class_create("note_control");
    if (IS_ERR(state->class)) {
        ret = PTR_ERR(state->class);
        goto undo_number;
    }
    cdev_init(&state->cdev, &note_fops);
    state->cdev.owner = THIS_MODULE;
    state->dev = device_create_with_groups(state->class, NULL,
                                          state->devt, state,
                                          note_groups, "note_control0");
    if (IS_ERR(state->dev)) {
        ret = PTR_ERR(state->dev);
        goto undo_class;
    }
    /* 最后才发布字符分派；成功后没有另一个可失败的初始化步骤。 */
    ret = cdev_add(&state->cdev, state->devt, 1);
    if (ret)
        goto undo_device;
    return 0;

undo_device:
    device_unregister(state->dev);
undo_class:
    class_destroy(state->class);
undo_number:
    unregister_chrdev_region(state->devt, 1);
    return ret;
}

static void __exit note_control_exit(void)
{
    struct note_state *state = &note_state;

    cdev_del(&state->cdev);
    /* 不持业务锁等待 sysfs 回调排空。 */
    device_unregister(state->dev);
    class_destroy(state->class);
    unregister_chrdev_region(state->devt, 1);
}

module_init(note_control_init);
module_exit(note_control_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("用 sysfs 属性控制字符消息的读取");
