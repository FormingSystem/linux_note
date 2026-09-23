---
id: research.kref.impl.device_dd
title: "dd.c解绑资源与设备存储边界"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_dd.c解绑资源与设备存储边界

上游相对路径 drivers/base/dd.c；NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0、Linux 6.12.20，blob bcc1f28b71f4f554ec8cf279934d13bdc6acce9c。中文Doxygen为仓库补充，函数体按固定Git对象核对。

## 1.1\_解绑清理不等待设备引用归零

此函数位于驱动绑定/解绑管理实现，在对应失败或解绑收尾路径清理资源与驱动关联。它调用devres_release_all并清理DMA配置、driver和drvdata等，不先检查设备kref是否归零。因此额外get_device只能延长设备对象存储，不会阻止这些资源清理。这里不把所有probe重试与解绑调用时序展开成第二套驱动教程。

```c
/** @brief 仓库补充阅读说明：清理绑定周期资源，独立于设备最终引用回收。 */
static void device_unbind_cleanup(struct device *dev)
{
	devres_release_all(dev);
	arch_teardown_dma_ops(dev);
	kfree(dev->dma_range_map);
	dev->dma_range_map = NULL;
	dev->driver = NULL;
	dev_set_drvdata(dev, NULL);
	if (dev->pm_domain && dev->pm_domain->dismiss)
		dev->pm_domain->dismiss(dev);
	pm_runtime_reinit(dev);
	dev_pm_set_driver_flags(dev, 0);
}
```

调用者必须按完整驱动退出协议先停止对这些资源的使用。保存devm_kzalloc返回值的session若活过解绑，不能只靠额外设备引用继续解引用旧资源；应停止并排空访问，或为确有独立寿命的数据另建所有权。最终device_release里再次清理devres是兜底，不会恢复已经释放的旧资源。

本函数只完成固定源码和调用位置核对，没有在宿主执行实际解绑、DMA或PM行为；相关应用夹具的devres是顺序替身。回到[设备资源模块](../../../navigation/P08_device引用与资源退出导读.md#8.3_设备引用不保留驱动受管资源)和[总阅读索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
