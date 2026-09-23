---
id: research.kref.impl.device_core
title: "core.c设备引用与最终清理"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_core.c设备引用与最终清理

上游相对路径 drivers/base/core.c；NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0、Linux 6.12.20，blob ec0ef6a0de942742215862206ea2aee8a65199b7。中文Doxygen为仓库补充，函数体按固定Git对象核对。

## 1.1\_注册包装建立初始份额

D0的device_initialize先建立内嵌kobject的初始引用和设备内部状态，D1再调用device_add；添加失败不会把初始份额自动收走，调用者仍须put_device。此页只展开包装，初始化、设备添加和回滚的完整子系统步骤不等于本宿主夹具已运行。

```c
/** @brief 仓库补充阅读说明：device_register 执行本节职责，外层状态前提见上文。 */
int device_register(struct device *dev)
{
	device_initialize(dev);
	return device_add(dev);
}
```

## 1.2\_设备取得与归还进入kobject

D2/D4通过设备类型接口进入kobject core，NULL按包装处理。get_device不是查找或任意地址验证；先有有效正引用依据才能追加。put_device不接收任意应用回调，core的类型描述负责后续分派。

```c
/** @brief 仓库补充阅读说明：get_device 执行本节职责，外层状态前提见上文。 */
struct device *get_device(struct device *dev)
{
	return dev ? kobj_to_dev(kobject_get(&dev->kobj)) : NULL;
}
```

```c
/** @brief 仓库补充阅读说明：put_device 执行本节职责，外层状态前提见上文。 */
void put_device(struct device *dev)
{
	/* might_sleep(); */
	if (dev)
		kobject_put(&dev->kobj);
}
```

## 1.3\_注销同时归还初始化份额

D3先device_del撤下相关设备模型关系，随后put_device归还原有初始化份额。若仍有独立拥有者，存储继续存在；不能在unregister后再盲目补一次同份put，也不能直接free。device_del只在已成功添加且尚未撤下的协议中使用。

```c
/** @brief 仓库补充阅读说明：device_unregister 执行本节职责，外层状态前提见上文。 */
void device_unregister(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);
	device_del(dev);
	put_device(dev);
}
```

## 1.4\_最终release按对象类型选择

D5保存内部私有指针p，清理devres与dma_range_map，再按dev->release、dev->type->release、dev->class->dev_release的先后选择一个回调；没有bus release兜底。回调可能释放外壳，随后core只释放先前保存的p。这里的devres清理是最终兜底，不表示驱动解绑时资源必定还在。

```c
/** @brief 仓库补充阅读说明：device_release 执行本节职责，外层状态前提见上文。 */
static void device_release(struct kobject *kobj)
{
	struct device *dev = kobj_to_dev(kobj);
	struct device_private *p = dev->p;

	/*
	 * Some platform devices are driven without driver attached
	 * and managed resources may have been acquired.  Make sure
	 * all resources are released.
	 *
	 * Drivers still can add resources into device after device
	 * is deleted but alive, so release devres here to avoid
	 * possible memory leak.
	 */
	devres_release_all(dev);

	kfree(dev->dma_range_map);

	if (dev->release)
		dev->release(dev);
	else if (dev->type && dev->type->release)
		dev->type->release(dev);
	else if (dev->class && dev->class->dev_release)
		dev->class->dev_release(dev);
	else
		WARN(1, KERN_ERR "Device '%s' does not have a release() function, it is broken and must be fixed. See Documentation/core-api/kobject.rst.\n",
			dev_name(dev));
	kfree(p);
}
```

宿主保留这五个主体与既有九个kobject函数，设备初始化/添加/撤下以及devres等底层行为是明确替身，验证应用分支、引用结算和三个release优先级；未执行完整driver core、真实资源释放、并发或目标装卸。

回到[设备模块](../../../navigation/P08_device引用与资源退出导读.md#8.2_从D0到D5区分登记与存储)和[总阅读索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。

同一组取得、归还与最终分派在[独立会话桥接导读](../../../navigation/P08_device引用与资源退出导读.md#8.6_私有会话连接设备份额)中由S1、S3～S5串起；私有kref只调用公开device接口，不直接接管内部计数。
[设备层次错误定位](../../../navigation/P08_device引用与资源退出导读.md#8.7_设备与私有引用的错误定位)沿公开接口、初始化份额和私有桥接检查调用契约，不把get_device当作悬空指针测试。

[最终框架验收](../../../navigation/P08_device引用与资源退出导读.md#8.8_最终验收中的框架责任)复用本文件注销与最后回调实现，观察者份额仅保持设备存储，不恢复已结束的驱动资源。
