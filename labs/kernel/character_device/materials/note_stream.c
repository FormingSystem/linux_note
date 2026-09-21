// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(format) KBUILD_MODNAME ": " format

#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include "note_registration.h"

static unsigned int major_number;
static unsigned int instance_count = 1;
static unsigned int buffer_size = 4096;
static bool auto_create_node = true;
module_param(major_number, uint, 0444);
module_param(instance_count, uint, 0444);
module_param(buffer_size, uint, 0444);
module_param(auto_create_node, bool, 0444);

struct note_stream {
	char *buffer;
	size_t head;                 /* 下一次写入的位置。 */
	size_t tail;                 /* 下一次读取的位置。 */
	size_t used;                 /* 已提交且未消费的字节数。 */
	struct mutex lock;
	wait_queue_head_t readers;
	wait_queue_head_t writers;
};

static struct note_stream *streams;
static struct note_registration registration;

static int note_stream_open(struct inode *inode, struct file *file)
{
	unsigned int index = iminor(inode) - MINOR(registration.first);

	if (index >= instance_count)
		return -ENODEV;
	file->private_data = &streams[index];
	return stream_open(inode, file);
}

static ssize_t note_stream_read(struct file *file, char __user *destination,
			       size_t count, loff_t *position)
{
	struct note_stream *stream = file->private_data;
	size_t requested, copied;
	int error;

	/* 流式打开不使用 position，它可能是 NULL。 */
	if (!count)
		return 0;
	for (;;) {
		if (mutex_lock_interruptible(&stream->lock))
			return -ERESTARTSYS;
		if (stream->used)
			break;
		mutex_unlock(&stream->lock);
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		error = wait_event_interruptible(stream->readers,
					 READ_ONCE(stream->used) != 0);
		if (error)
			return error;
		/* 醒来只得到竞争机会；必须加锁重新检查。 */
	}
	requested = min(count, stream->used);
	requested = min(requested, (size_t)buffer_size - stream->tail);
	copied = requested - copy_to_user(destination,
					 stream->buffer + stream->tail, requested);
	stream->tail = (stream->tail + copied) % buffer_size;
	WRITE_ONCE(stream->used, stream->used - copied);
	mutex_unlock(&stream->lock);
	if (copied)
		wake_up_interruptible(&stream->writers);
	return copied ? (ssize_t)copied : -EFAULT;
}

static ssize_t note_stream_write(struct file *file, const char __user *source,
				size_t count, loff_t *position)
{
	struct note_stream *stream = file->private_data;
	size_t requested, copied;
	int error;

	if (!count)
		return 0;
	for (;;) {
		if (mutex_lock_interruptible(&stream->lock))
			return -ERESTARTSYS;
		if (stream->used < buffer_size)
			break;
		mutex_unlock(&stream->lock);
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		error = wait_event_interruptible(stream->writers,
					 READ_ONCE(stream->used) < buffer_size);
		if (error)
			return error;
	}
	requested = min(count, (size_t)buffer_size - stream->used);
	requested = min(requested, (size_t)buffer_size - stream->head);
	/* 目标完全位于空闲区，复制失败清零不会破坏尚未读取的数据。 */
	copied = requested - copy_from_user(stream->buffer + stream->head,
					   source, requested);
	stream->head = (stream->head + copied) % buffer_size;
	WRITE_ONCE(stream->used, stream->used + copied);
	mutex_unlock(&stream->lock);
	if (copied)
		wake_up_interruptible(&stream->readers);
	return copied ? (ssize_t)copied : -EFAULT;
}

static __poll_t note_stream_poll(struct file *file, poll_table *wait)
{
	struct note_stream *stream = file->private_data;
	__poll_t mask = 0;

	/* 先登记再检查，覆盖状态在二者之间改变的情况。 */
	poll_wait(file, &stream->readers, wait);
	poll_wait(file, &stream->writers, wait);
	mutex_lock(&stream->lock);
	if (stream->used)
		mask |= EPOLLIN | EPOLLRDNORM;
	if (stream->used < buffer_size)
		mask |= EPOLLOUT | EPOLLWRNORM;
	mutex_unlock(&stream->lock);
	return mask;
}

static const struct file_operations note_stream_operations = {
	.owner = THIS_MODULE,
	.open = note_stream_open,
	.read = note_stream_read,
	.write = note_stream_write,
	.poll = note_stream_poll,
};

static void note_stream_free(void)
{
	unsigned int index;

	for (index = 0; index < instance_count; ++index)
		kfree(streams[index].buffer);
	kfree(streams);
}

static int __init note_stream_init(void)
{
	unsigned int index;
	int error = -ENOMEM;

	if (!instance_count || instance_count > 16 ||
	    buffer_size < 8 || buffer_size > 65536)
		return -EINVAL;
	streams = kcalloc(instance_count, sizeof(*streams), GFP_KERNEL);
	if (!streams)
		return -ENOMEM;
	for (index = 0; index < instance_count; ++index) {
		mutex_init(&streams[index].lock);
		init_waitqueue_head(&streams[index].readers);
		init_waitqueue_head(&streams[index].writers);
		streams[index].buffer = kzalloc(buffer_size, GFP_KERNEL);
		if (!streams[index].buffer)
			goto fail;
	}
	error = note_register(&registration, &note_stream_operations,
		"note_stream", major_number, instance_count, auto_create_node);
	if (error)
		goto fail;
	pr_info("major=%u minors=0..%u capacity=%u\n",
		MAJOR(registration.first), instance_count - 1, buffer_size);
	return 0;
fail:
	note_stream_free();
	return error;
}

static void __exit note_stream_exit(void)
{
	note_unregister(&registration);
	note_stream_free();
}

module_init(note_stream_init);
module_exit(note_stream_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("教学用等待通知环形流字符设备");
