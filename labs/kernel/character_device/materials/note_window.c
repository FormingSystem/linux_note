// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(format) KBUILD_MODNAME ": " format

#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "note_registration.h"

/* 参数在加载时确定，运行时只读，避免缓冲区与清理边界漂移。 */
static unsigned int major_number;
static unsigned int instance_count = 1;
static unsigned int buffer_size = 4096;
static bool auto_create_node = true;
module_param(major_number, uint, 0444);
module_param(instance_count, uint, 0444);
module_param(buffer_size, uint, 0444);
module_param(auto_create_node, bool, 0444);

struct note_window {
	char *buffer;
	size_t length;
	struct mutex lock;
};

static struct note_window *windows;
static struct note_registration registration;

static int note_window_open(struct inode *inode, struct file *file)
{
	unsigned int index = iminor(inode) - MINOR(registration.first);

	if (index >= instance_count)
		return -ENODEV;
	file->private_data = &windows[index];
	/* 让 VFS 为共享打开实例的普通 read/write/lseek 串行维护位置。 */
	file->f_mode |= FMODE_ATOMIC_POS;
	return 0;
}

static ssize_t note_window_read(struct file *file, char __user *destination,
			       size_t count, loff_t *position)
{
	struct note_window *window = file->private_data;
	ssize_t result;

	if (!count)
		return 0;
	if (mutex_lock_interruptible(&window->lock))
		return -ERESTARTSYS;
	result = simple_read_from_buffer(destination, count, position,
					window->buffer, window->length);
	mutex_unlock(&window->lock);
	return result;
}

static ssize_t note_window_write(struct file *file, const char __user *source,
				size_t count, loff_t *position)
{
	struct note_window *window = file->private_data;
	char *temporary;
	loff_t start;
	size_t requested, copied;
	ssize_t result;

	if (!count)
		return 0;
	temporary = kmalloc(min_t(size_t, count, buffer_size), GFP_KERNEL);
	if (!temporary)
		return -ENOMEM;
	if (mutex_lock_interruptible(&window->lock)) {
		result = -ERESTARTSYS;
		goto free_temporary;
	}
	start = *position;
	if (file->f_flags & O_APPEND)
		start = window->length;
	if (start < 0) {
		result = -EINVAL;
		goto unlock;
	}
	if (start >= buffer_size) {
		result = -ENOSPC;
		goto unlock;
	}
	requested = min_t(size_t, count, buffer_size - (size_t)start);
	/* 失败清零只能碰到私有临时区，不能破坏未提交的旧内容。 */
	copied = requested - copy_from_user(temporary, source, requested);
	if (!copied) {
		result = -EFAULT;
		goto unlock;
	}
	if ((size_t)start > window->length)
		memset(window->buffer + window->length, 0, start - window->length);
	memcpy(window->buffer + start, temporary, copied);
	*position = start + copied;
	window->length = max_t(size_t, window->length, *position);
	result = copied;
unlock:
	mutex_unlock(&window->lock);
free_temporary:
	kfree(temporary);
	return result;
}

static loff_t note_window_llseek(struct file *file, loff_t offset, int whence)
{
	struct note_window *window = file->private_data;
	loff_t result;

	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
		return -EINVAL;
	if (mutex_lock_interruptible(&window->lock))
		return -ERESTARTSYS;
	result = generic_file_llseek_size(file, offset, whence,
					 buffer_size, window->length);
	mutex_unlock(&window->lock);
	return result;
}

static const struct file_operations note_window_operations = {
	.owner = THIS_MODULE,
	.open = note_window_open,
	.read = note_window_read,
	.write = note_window_write,
	.llseek = note_window_llseek,
};

static void note_window_free(void)
{
	unsigned int index;

	for (index = 0; index < instance_count; ++index)
		kfree(windows[index].buffer);
	kfree(windows);
}

static int __init note_window_init(void)
{
	unsigned int index;
	int error = -ENOMEM;

	if (!instance_count || instance_count > 16 ||
	    buffer_size < 8 || buffer_size > 65536)
		return -EINVAL;
	windows = kcalloc(instance_count, sizeof(*windows), GFP_KERNEL);
	if (!windows)
		return -ENOMEM;
	for (index = 0; index < instance_count; ++index) {
		mutex_init(&windows[index].lock);
		windows[index].buffer = kzalloc(buffer_size, GFP_KERNEL);
		if (!windows[index].buffer)
			goto fail;
	}
	error = note_register(&registration, &note_window_operations,
		"note_window", major_number, instance_count, auto_create_node);
	if (error)
		goto fail;
	pr_info("major=%u minors=0..%u capacity=%u\n",
		MAJOR(registration.first), instance_count - 1, buffer_size);
	return 0;
fail:
	note_window_free();
	return error;
}

static void __exit note_window_exit(void)
{
	note_unregister(&registration);
	note_window_free();
}

module_init(note_window_init);
module_exit(note_window_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("教学用有限内存窗口字符设备");
