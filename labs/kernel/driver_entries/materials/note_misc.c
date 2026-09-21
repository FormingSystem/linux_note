// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

static const char greeting[] = "hello from misc\n";

static int note_open(struct inode *inode, struct file *file)
{
    int ret = nonseekable_open(inode, file);

    if (ret)
        return ret;
    /* 同一个打开文件被共享时，串行化常规 read 的位置更新。 */
    file->f_mode |= FMODE_ATOMIC_POS;
    return 0;
}

static ssize_t note_read(struct file *file, char __user *buf,
                         size_t count, loff_t *pos)
{
    return simple_read_from_buffer(buf, count, pos,
                                   greeting, sizeof(greeting) - 1);
}

static const struct file_operations note_fops = {
    .owner = THIS_MODULE,
    .open = note_open,
    .read = note_read,
};

static struct miscdevice note_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "note_misc",
    .fops = &note_fops,
    .mode = 0600,
};

static int __init note_misc_init(void)
{
    return misc_register(&note_device);
}

static void __exit note_misc_exit(void)
{
    misc_deregister(&note_device);
}

module_init(note_misc_init);
module_exit(note_misc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("固定只读内容的 misc 教学设备");
