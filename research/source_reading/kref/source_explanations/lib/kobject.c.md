---
id: research.kref.impl.kobject_c
title: "kobject.c初始化撤下与类型清理"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_kobject.c初始化撤下与类型清理

上游相对路径 lib/kobject.c；NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20，blob 72fa20f405f1520a63dd50d9aa37f6609306eb3e。下面中文 Doxygen 均为仓库补充阅读说明，函数体及类型字段以固定 Git 对象核对。

## 1.1\_初始化后失败仍有初始责任

K0 的内部初始化建立 kref=1 和初始化状态，K1 的添加失败不会自动归还这一份。kobject_init_and_add 先初始化，再把命名/添加结果返回；调用者失败仍应 put。正常参数的类型描述必须一直有效；重复初始化的诊断不表示任意重用已初始化对象合法。kobject_add_varg 和其目录创建在本页作为下一层边界，不展开 sysfs 节点算法。

```c
/** @brief 仓库补充阅读说明：kobject_init_internal 对应本节阶段，外层责任及配置见上文。 */
static void kobject_init_internal(struct kobject *kobj)
{
	if (!kobj)
		return;
	kref_init(&kobj->kref);
	INIT_LIST_HEAD(&kobj->entry);
	kobj->state_in_sysfs = 0;
	kobj->state_add_uevent_sent = 0;
	kobj->state_remove_uevent_sent = 0;
	kobj->state_initialized = 1;
}
```

```c
/** @brief 仓库补充阅读说明：kobject_init 对应本节阶段，外层责任及配置见上文。 */
void kobject_init(struct kobject *kobj, const struct kobj_type *ktype)
{
	char *err_str;

	if (!kobj) {
		err_str = "invalid kobject pointer!";
		goto error;
	}
	if (!ktype) {
		err_str = "must have a ktype to be initialized properly!\n";
		goto error;
	}
	if (kobj->state_initialized) {
		/* do not error out as sometimes we can recover */
		pr_err("kobject (%p): tried to init an initialized object, something is seriously wrong.\n",
		       kobj);
		dump_stack_lvl(KERN_ERR);
	}

	kobject_init_internal(kobj);
	kobj->ktype = ktype;
	return;

error:
	pr_err("kobject (%p): %s\n", kobj, err_str);
	dump_stack_lvl(KERN_ERR);
}
```

```c
/** @brief 仓库补充阅读说明：kobject_init_and_add 对应本节阶段，外层责任及配置见上文。 */
int kobject_init_and_add(struct kobject *kobj, const struct kobj_type *ktype,
			 struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int retval;

	kobject_init(kobj, ktype);

	va_start(args, fmt);
	retval = kobject_add_varg(kobj, parent, fmt, args);
	va_end(args);

	return retval;
}
```

## 1.2\_取得返回同一个框架对象

K2 以有效正引用为前提追加对象自身一份，返回原地址；NULL 输入保持 NULL。state_initialized 检查提供诊断，不验证任意悬空地址。它不恢复已撤下的 sysfs 入口，也不自动发送事件。

```c
/** @brief 仓库补充阅读说明：kobject_get 对应本节阶段，外层责任及配置见上文。 */
struct kobject *kobject_get(struct kobject *kobj)
{
	if (kobj) {
		if (!kobj->state_initialized)
			WARN(1, KERN_WARNING
				"kobject: '%s' (%p): is not initialized, yet kobject_get() is being called.\n",
			     kobject_name(kobj), kobj);
		kref_get(&kobj->kref);
	}
	return kobj;
}
```

## 1.3\_撤下层次不消费本对象引用

K3 的内部清理移除属性组、在已发送ADD且未发送REMOVE时补REMOVE、删除目录并释放对应sysfs持有，退出kset并清parent。外层先保存parent，完成撤下后归还父对象引用；没有归还kobj自己的那一份。正常调用必须针对成功添加且尚未撤下的对象，不把NULL处理解释成任意重复删除安全。

```c
/** @brief 仓库补充阅读说明：__kobject_del 对应本节阶段，外层责任及配置见上文。 */
static void __kobject_del(struct kobject *kobj)
{
	struct kernfs_node *sd;
	const struct kobj_type *ktype;

	sd = kobj->sd;
	ktype = get_ktype(kobj);

	if (ktype)
		sysfs_remove_groups(kobj, ktype->default_groups);

	/* send "remove" if the caller did not do it but sent "add" */
	if (kobj->state_add_uevent_sent && !kobj->state_remove_uevent_sent) {
		pr_debug("'%s' (%p): auto cleanup 'remove' event\n",
			 kobject_name(kobj), kobj);
		kobject_uevent(kobj, KOBJ_REMOVE);
	}

	sysfs_remove_dir(kobj);
	sysfs_put(sd);

	kobj->state_in_sysfs = 0;
	kobj_kset_leave(kobj);
	kobj->parent = NULL;
}
```

```c
/** @brief 仓库补充阅读说明：kobject_del 对应本节阶段，外层责任及配置见上文。 */
void kobject_del(struct kobject *kobj)
{
	struct kobject *parent;

	if (!kobj)
		return;

	parent = kobj->parent;
	__kobject_del(kobj);
	kobject_put(parent);
}
```

## 1.4\_最后归还进入类型清理

K4 的 kobject_put 把私有 kobject_release 传给 kref；普通配置下立即进入 K5 cleanup，调试配置会排延迟工作。cleanup 在可能释放外壳前保存parent、type和name；仍登记时补内部撤下，类型release后仅使用保存的name/parent完成清理，不能再访问已释放kobj。类型回调释放外壳，不擅自重复释放由core管理的name或parent引用。CONFIG_DEBUG_KOBJECT_RELEASE 的异步分支使回调代码与类型描述需要更长寿命；本章模块明确不支持该配置，未验证该延迟分支。

```c
/** @brief 仓库补充阅读说明：kobject_cleanup 对应本节阶段，外层责任及配置见上文。 */
static void kobject_cleanup(struct kobject *kobj)
{
	struct kobject *parent = kobj->parent;
	const struct kobj_type *t = get_ktype(kobj);
	const char *name = kobj->name;

	pr_debug("'%s' (%p): %s, parent %p\n",
		 kobject_name(kobj), kobj, __func__, kobj->parent);

	if (t && !t->release)
		pr_debug("'%s' (%p): does not have a release() function, it is broken and must be fixed. See Documentation/core-api/kobject.rst.\n",
			 kobject_name(kobj), kobj);

	/* remove from sysfs if the caller did not do it */
	if (kobj->state_in_sysfs) {
		pr_debug("'%s' (%p): auto cleanup kobject_del\n",
			 kobject_name(kobj), kobj);
		__kobject_del(kobj);
	} else {
		/* avoid dropping the parent reference unnecessarily */
		parent = NULL;
	}

	if (t && t->release) {
		pr_debug("'%s' (%p): calling ktype release\n",
			 kobject_name(kobj), kobj);
		t->release(kobj);
	}

	/* free name if we allocated it */
	if (name) {
		pr_debug("'%s': free name\n", name);
		kfree_const(name);
	}

	kobject_put(parent);
}
```

```c
/** @brief 仓库补充阅读说明：kobject_release 对应本节阶段，外层责任及配置见上文。 */
static void kobject_release(struct kref *kref)
{
	struct kobject *kobj = container_of(kref, struct kobject, kref);
#ifdef CONFIG_DEBUG_KOBJECT_RELEASE
	unsigned long delay = HZ + HZ * get_random_u32_below(4);
	pr_info("'%s' (%p): %s, parent %p (delayed %ld)\n",
		kobject_name(kobj), kobj, __func__, kobj->parent, delay);
	INIT_DELAYED_WORK(&kobj->release, kobject_delayed_cleanup);

	schedule_delayed_work(&kobj->release, delay);
#else
	kobject_cleanup(kobj);
#endif
}
```

```c
/** @brief 仓库补充阅读说明：kobject_put 对应本节阶段，外层责任及配置见上文。 */
void kobject_put(struct kobject *kobj)
{
	if (kobj) {
		if (!kobj->state_initialized)
			WARN(1, KERN_WARNING
				"kobject: '%s' (%p): is not initialized, yet kobject_put() is being called.\n",
			     kobject_name(kobj), kobj);
		kref_put(&kobj->kref, kobject_release);
	}
}
```

固定九函数在宿主夹具保留主体，普通计数链也取自固定源码；命名/添加、sysfs、父环境、诊断与分配为明确替身。七组检查核对支持配置下的初始化失败清理、显式/隐式撤下与最终归还；没有真实sysfs操作、并发、事件或延迟调试释放证明。

回到[框架模块](../../navigation/P07_kobject身份与类型清理导读.md#7.2_从K0到K5连接状态与回调)和[总阅读索引](../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
