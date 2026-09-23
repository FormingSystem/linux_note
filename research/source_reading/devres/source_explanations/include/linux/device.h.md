---
id: research.source_reading.devres.impl.device_header
title: "devres action失败回滚包装"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_devres\_action失败回滚包装

上游位置`include/linux/device.h`，NXP Linux 6.12.20固定dfaf2136提交；[完整原文](../../../../linux/include/linux/device.h)。先读[模块责任选择](../../../navigation/P02_记录与分组清理导读.md#2.3_选择接口先确定责任是否保留)。

## 1.1\_reset包装失败直接执行

仓库补充Doxygen阅读说明，非上游原注释：

```c
/**
 * @brief S0登记失败时立即履行action；成功留给后续S4清理。
 * @note action可能在此调用者上下文执行，失败后不得重复清理同一责任。
 */
```

```c
static inline int __devm_add_action_or_reset(struct device *dev, void (*action)(void *),
					     void *data, const char *name)
{
	/* 仓库阅读注释：失败直接回调，没有留下可再次释放的登记。 */
	int ret;

	ret = __devm_add_action(dev, action, data, name);
	if (ret)
		action(data);

	return ret;
}
```

本版本公开`devm_add_action_or_reset(dev, action, data)`宏把action名称作为额外name传入本函数；普通`devm_add_action`宏直接转到[普通登记实现](../../drivers/base/devres.c.md#1.1_普通action登记成功才转交责任)。名称字符串用于记录，不改变返回值或资源责任。

ret为零时不调用action；非零时调用后仍返回原错误，调用者必须停止把data当成成功保留的资源。这个包装不负责一般资源状态检查，也不使回调可以在不合适的上下文睡眠。返回[总索引](../../../navigation/P01_Linux_6.12_devres源码阅读索引.md#1.2_按问题进入实现)。
