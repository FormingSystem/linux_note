---
id: research.kref.impl.bus_c
title: "bus.c注册描述与内部退出"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_bus.c注册描述与内部退出

上游相对路径drivers/base/bus.c；NXP官方固定提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux6.12.20，blob 657c93c38b0dc2a2247e5f482fadd3a9376a58e8。中文Doxygen为仓库补充阅读说明；公共描述与内部状态位置另核对drivers/base/base.h及include/linux/device相应头文件。

## 1.1\_注销内部目录与登记份额

bus_to_subsys返回带临时引用的内部对象。注销处理可选根设备、属性与内部drivers/devices kset，再注销subsys并归还临时份额；这不是给任意外部设备或驱动做完整退出的替代。顺序应由所属子系统在停止使用者后调用。

```c
/** @brief 仓库补充阅读说明：bus_unregister 的对象身份与责任见上文。 */
void bus_unregister(const struct bus_type *bus)
{
	struct subsys_private *sp = bus_to_subsys(bus);
	struct kobject *bus_kobj;

	if (!sp)
		return;

	pr_debug("bus: '%s': unregistering\n", bus->name);
	if (sp->dev_root)
		device_unregister(sp->dev_root);

	bus_kobj = &sp->subsys.kobj;
	sysfs_remove_groups(bus_kobj, bus->bus_groups);
	remove_probe_files(bus);
	bus_remove_file(bus, &bus_attr_uevent);

	kset_unregister(sp->drivers_kset);
	kset_unregister(sp->devices_kset);
	kset_unregister(&sp->subsys);
	subsys_put(sp);
}
```

## 1.2\_内部release不释放公共bus\_type描述

to_subsys_private把内嵌kobject恢复到core内部对象，回收锁类登记与私有分配。这里没有kfree(bus_type)，也没有对设备实例调用dev_release。公共描述及其中回调由其所属代码维持有效期限；普通驱动不手动操纵内部kset引用。

```c
/** @brief 仓库补充阅读说明：bus_release 的对象身份与责任见上文。 */
static void bus_release(struct kobject *kobj)
{
	struct subsys_private *priv = to_subsys_private(kobj);

	lockdep_unregister_key(&priv->lock_key);
	kfree(priv);
}
```

本页仅完成固定源码和调用责任核对，未执行class/bus注册、真实sysfs、事件、设备解绑或并发测试。不得把代码块解析或链接检查称为运行验收。

回到[分类与总线模块入口](../../../navigation/P08_device引用与资源退出导读.md#8.5_分类与总线的公共描述及内部份额)和[总阅读索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
