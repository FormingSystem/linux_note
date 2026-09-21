// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/seq_file.h>

struct note_session {
    atomic_t flush_count;
};

static int session_open(struct inode *inode, struct file *file)
{
    struct note_session *session;

    session = kzalloc(sizeof(*session), GFP_KERNEL);
    if (!session)
        return -ENOMEM;
    atomic_set(&session->flush_count, 0);
    /* 每次成功打开持有一个上下文；dup 不会再调用这里。 */
    file->private_data = session;
    return nonseekable_open(inode, file);
}

static int session_flush(struct file *file, fl_owner_t owner)
{
    struct note_session *session = file->private_data;

    atomic_inc(&session->flush_count);
    return 0;
}

static void session_fdinfo(struct seq_file *seq, struct file *file)
{
    struct note_session *session = file->private_data;

    seq_printf(seq, "note_flushes:\t%d\n",
               atomic_read(&session->flush_count));
}

static int session_release(struct inode *inode, struct file *file)
{
    struct note_session *session = file->private_data;

    pr_info("note_session: release after %d flushes\n",
            atomic_read(&session->flush_count));
    kfree(session);
    return 0;
}

static const struct file_operations session_fops = {
    .owner = THIS_MODULE,
    .open = session_open,
    .flush = session_flush,
    .release = session_release,
    .show_fdinfo = session_fdinfo,
};

static struct miscdevice session_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "note_session",
    .fops = &session_fops,
    .mode = 0600,
};

static int __init session_init(void)
{
    return misc_register(&session_device);
}

static void __exit session_exit(void)
{
    misc_deregister(&session_device);
}

module_init(session_init);
module_exit(session_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("每次打开与关闭引用教学示例");
