---
id: research.kref.impl.class_c
title: "class.c分类描述与内部引用"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_class.c分类描述与内部引用

上游相对路径drivers/base/class.c；NXP官方固定提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux6.12.20，blob ce460e1ab1376d785d5386477ae3c91e47df4686。中文Doxygen为仓库补充阅读说明；公共描述与内部状态位置另核对drivers/base/base.h及include/linux/device相应头文件。

## 1.1\_查找内部对象会取得临时份额

输入是公共class描述地址，返回的是core内部subsys_private。函数持class_kset列表锁定位，在退出锁前subsys_get取得内部对象份额；调用者结束须subsys_put。它不是为任意设备实例取得引用，也不自动维持调用者保留的公共描述指针。

```c
/** @brief 仓库补充阅读说明：class_to_subsys 的对象身份与责任见上文。 */
struct subsys_private *class_to_subsys(const struct class *class)
{
	struct subsys_private *sp = NULL;
	struct kobject *kobj;

	if (!class || !class_kset)
		return NULL;

	spin_lock(&class_kset->list_lock);

	if (list_empty(&class_kset->list))
		goto done;

	list_for_each_entry(kobj, &class_kset->list, entry) {
		struct kset *kset = container_of(kobj, struct kset, kobj);

		sp = container_of_const(kset, struct subsys_private, subsys);
		if (sp->class == class)
			goto done;
	}
	sp = NULL;
done:
	sp = subsys_get(sp);
	spin_unlock(&class_kset->list_lock);
	return sp;
}
```

## 1.2\_注销配对登记与临时查找份额

class_unregister先取得查找份额，移除分类属性并kset_unregister，最后subsys_put归还临时份额。class_destroy仅对class_create产生的描述使用，过滤ERR/NULL后走unregister；不是遍历并销毁全部设备实例的接口。须先按框架退出设备和其他使用者，再结束分类注册。

```c
/** @brief 仓库补充阅读说明：class_unregister 的对象身份与责任见上文。 */
void class_unregister(const struct class *cls)
{
	struct subsys_private *sp = class_to_subsys(cls);

	if (!sp)
		return;

	pr_debug("device class '%s': unregistering\n", cls->name);

	sysfs_remove_groups(&sp->subsys.kobj, cls->class_groups);
	kset_unregister(&sp->subsys);
	subsys_put(sp);
}
```

```c
/** @brief 仓库补充阅读说明：class_destroy 的对象身份与责任见上文。 */
void class_destroy(const struct class *cls)
{
	if (IS_ERR_OR_NULL(cls))
		return;

	class_unregister(cls);
}
```

## 1.3\_动态描述与内部外壳各有清理者

class_create动态分配公共描述，设置class_release回调后注册；失败清理该新分配。最终内部class_release先调用公共描述的class_release，再注销内部锁类并释放subsys_private。class_create_release负责释放动态公共描述，两次free针对两个不同分配；手工注册的静态描述不能照抄动态描述释放策略。

```c
/** @brief 仓库补充阅读说明：class_create 的对象身份与责任见上文。 */
struct class *class_create(const char *name)
{
	struct class *cls;
	int retval;

	cls = kzalloc(sizeof(*cls), GFP_KERNEL);
	if (!cls) {
		retval = -ENOMEM;
		goto error;
	}

	cls->name = name;
	cls->class_release = class_create_release;

	retval = class_register(cls);
	if (retval)
		goto error;

	return cls;

error:
	kfree(cls);
	return ERR_PTR(retval);
}
```

```c
/** @brief 仓库补充阅读说明：class_create_release 的对象身份与责任见上文。 */
static void class_create_release(const struct class *cls)
{
	pr_debug("%s called for %s\n", __func__, cls->name);
	kfree(cls);
}
```

```c
/** @brief 仓库补充阅读说明：class_release 的对象身份与责任见上文。 */
static void class_release(struct kobject *kobj)
{
	struct subsys_private *cp = to_subsys_private(kobj);
	const struct class *class = cp->class;

	pr_debug("class '%s': release.\n", class->name);

	if (class->class_release)
		class->class_release(class);
	else
		pr_debug("class '%s' does not have a release() function, "
			 "be careful\n", class->name);

	lockdep_unregister_key(&cp->lock_key);
	kfree(cp);
}
```

本页仅完成固定源码和调用责任核对，未执行class/bus注册、真实sysfs、事件、设备解绑或并发测试。不得把代码块解析或链接检查称为运行验收。

回到[分类与总线模块入口](../../../navigation/P08_device引用与资源退出导读.md#8.5_分类与总线的公共描述及内部份额)和[总阅读索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
