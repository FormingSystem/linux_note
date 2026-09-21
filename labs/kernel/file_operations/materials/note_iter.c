// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uio.h>

static const char message[] = "hello from iter\n";

static int iter_open(struct inode *inode, struct file *file)
{
    /* 保留定位能力；共享打开文件的常规读取协调 f_pos。 */
    file->f_mode |= FMODE_ATOMIC_POS;
    return 0;
}

static loff_t iter_seek(struct file *file, loff_t offset, int whence)
{
    return fixed_size_llseek(file, offset, whence, sizeof(message) - 1);
}

static ssize_t iter_read(struct kiocb *request, struct iov_iter *to)
{
    loff_t pos = request->ki_pos;
    size_t want, done;

    if (pos < 0)
        return -EINVAL;
    if (!iov_iter_count(to) || pos >= sizeof(message) - 1)
        return 0;

    want = min_t(size_t, iov_iter_count(to), sizeof(message) - 1 - pos);
    done = copy_to_iter(message + pos, want, to);
    if (!done)
        return -EFAULT;
    /* 请求位置只提交实际完成量，不直接改 file->f_pos。 */
    request->ki_pos = pos + done;
    return done;
}

static const struct file_operations iter_fops = {
    .owner = THIS_MODULE,
    .open = iter_open,
    .llseek = iter_seek,
    .read_iter = iter_read,
};

static struct miscdevice iter_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "note_iter",
    .fops = &iter_fops,
    .mode = 0400,
};

static int __init iter_init(void)
{
    return misc_register(&iter_device);
}

static void __exit iter_exit(void)
{
    misc_deregister(&iter_device);
}

module_init(iter_init);
module_exit(iter_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("同步迭代读取与独立请求位置");
