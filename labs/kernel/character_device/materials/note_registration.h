/* SPDX-License-Identifier: GPL-2.0 */
#ifndef NOTE_REGISTRATION_H
#define NOTE_REGISTRATION_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/module.h>

/* 只管理整组创建、整组销毁的教学设备，不承担热拔插。 */
struct note_registration {
	dev_t first;
	unsigned int count;
	unsigned int created_nodes;
	struct class *class;
	struct cdev *cdev;
};

static void note_destroy_nodes(struct note_registration *registration)
{
	while (registration->created_nodes) {
		--registration->created_nodes;
		device_destroy(registration->class,
			       registration->first + registration->created_nodes);
	}
	if (registration->class) {
		class_destroy(registration->class);
		registration->class = NULL;
	}
}

/* 调用前，所有实例和锁必须已就绪；成功发布后不再执行可失败的步骤。 */
static int note_register(struct note_registration *registration,
			const struct file_operations *operations, const char *name,
			unsigned int major_number, unsigned int count,
			bool auto_create_node)
{
	struct device *device;
	int error;

	if (!count || count > 16 ||
	    MAJOR(MKDEV(major_number, 0)) != major_number)
		return -EINVAL;
	registration->count = count;
	if (major_number) {
		registration->first = MKDEV(major_number, 0);
		error = register_chrdev_region(registration->first, count, name);
	} else {
		error = alloc_chrdev_region(&registration->first, 0, count, name);
	}
	if (error)
		return error;

	if (auto_create_node) {
		registration->class = class_create(name);
		if (IS_ERR(registration->class)) {
			error = PTR_ERR(registration->class);
			registration->class = NULL;
			goto fail_region;
		}
		while (registration->created_nodes < count) {
			device = device_create(registration->class, NULL,
				registration->first + registration->created_nodes,
				NULL, "%s%u", name, registration->created_nodes);
			if (IS_ERR(device)) {
				error = PTR_ERR(device);
				goto fail_nodes;
			}
			++registration->created_nodes;
		}
	}

	/* 此时节点可能已可见，但还不能通过设备号进入本驱动。 */
	registration->cdev = cdev_alloc();
	if (!registration->cdev) {
		error = -ENOMEM;
		goto fail_nodes;
	}
	registration->cdev->ops = operations;
	registration->cdev->owner = operations->owner;
	error = cdev_add(registration->cdev, registration->first, count);
	if (error) {
		/* 尚未建立映射；只放弃 cdev_alloc 获得的对象引用。 */
		kobject_put(&registration->cdev->kobj);
		registration->cdev = NULL;
		goto fail_nodes;
	}
	return 0;

fail_nodes:
	note_destroy_nodes(registration);
fail_region:
	unregister_chrdev_region(registration->first, count);
	return error;
}

/* 只对成功注册的整组调用；普通模块卸载时不存在仍持有模块的打开文件。 */
static void note_unregister(struct note_registration *registration)
{
	cdev_del(registration->cdev);
	registration->cdev = NULL;
	note_destroy_nodes(registration);
	unregister_chrdev_region(registration->first, registration->count);
}

#endif
