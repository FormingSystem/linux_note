---
id: research.kref.impl.kobject_h
title: "kobject.h身份状态与类型描述"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_kobject.h身份状态与类型描述

上游相对路径 include/linux/kobject.h；NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20，blob c8219505a79f98bc370e52997efc8af51833cfda。下面中文 Doxygen 均为仓库补充阅读说明，函数体及类型字段以固定 Git 对象核对。

## 1.1\_对象身份与独立状态

name是名字存储，parent/kset表达不同关系，sd对应sysfs目录节点，kref记录本对象份额。初始化、sysfs登记和事件发送状态是不同位，不能从一个计数值推出其余状态。调试配置额外内嵌延迟工作，不代表每个普通构建都存在异步释放路径。

```c
/** @brief 仓库补充阅读说明：K0至K5共同涉及的身份、关系和状态载体。 */
struct kobject {
	const char		*name;
	struct list_head	entry;
	struct kobject		*parent;
	struct kset		*kset;
	const struct kobj_type	*ktype;
	struct kernfs_node	*sd; /* sysfs directory entry */
	struct kref		kref;

	unsigned int state_initialized:1;
	unsigned int state_in_sysfs:1;
	unsigned int state_add_uevent_sent:1;
	unsigned int state_remove_uevent_sent:1;
	unsigned int uevent_suppress:1;

#ifdef CONFIG_DEBUG_KOBJECT_RELEASE
	struct delayed_work	release;
#endif
};
```

## 1.2\_类型回调不存放在内嵌kref里

kobj_type给出这一类对象的release和可选属性、命名空间、所有权规则；实例通过ktype指针关联它。release收到struct kobject地址，外层类型再container_of回到自己的分配。没有属性需求的示例可以只提供release，不必捏造属性组；类型描述和回调代码必须覆盖所有实例的最终清理。

```c
/** @brief 仓库补充阅读说明：类型级描述，由各实例保存指针，须保持有效。 */
struct kobj_type {
	void (*release)(struct kobject *kobj);
	const struct sysfs_ops *sysfs_ops;
	const struct attribute_group **default_groups;
	const struct kobj_ns_type_operations *(*child_ns_type)(const struct kobject *kobj);
	const void *(*namespace)(const struct kobject *kobj);
	void (*get_ownership)(const struct kobject *kobj, kuid_t *uid, kgid_t *gid);
};
```

回到[框架模块](../../../navigation/P07_kobject身份与类型清理导读.md#7.2_从K0到K5连接状态与回调)和[总阅读索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
