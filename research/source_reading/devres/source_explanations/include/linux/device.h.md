---
id: research.source_reading.devres.impl.device_header
title: "devres action失败回滚包装"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_devres\_action失败回滚包装

资源已经取得，保存清理责任的记录却可能分配失败。普通登记把这个错误交回调用者；reset 包装要解决的是：能否把同一个清理动作也用于登记失败的当场回滚？下面检查它怎样保持错误返回，以及为什么调用者不能再清理一次。

上游位置 `include/linux/device.h`，NXP Linux 6.12.20，固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`；[完整原文](../../../../linux/include/linux/device.h)。前置责任模型见[模块责任选择](../../../navigation/P02_记录与分组清理导读.md#2.3_选择接口先确定责任是否保留)。对象关系复用[核心记录图](../../drivers/base/devres.c.md#第1章_devres核心登记与分组实现)：本包装没有新增一种记录，只改变普通 action 登记失败后的调用路径。

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

ret 为零时不调用 action；非零时，先同步执行 action，再返回原错误。因此调用者不能把失败当作登记成功，也不能再履行一次已由 action 消费的责任。不过，`data` 是否仍能使用要看 action 的契约：如果它释放对象，指针随之失效；如果它只停止时钟，对象存储可能仍存在。不能从“执行过回调”直接推导所有 data 都已释放。

修改这个包装时必须保持“成功只登记，失败只执行一次回滚，并保留原错误”的配对关系；把错误分支改成成功返回，会让调用者误以为资源状态仍保持且有后续清理记录。它也不负责一般资源状态检查，不使回调可以在不合适的上下文睡眠。其成功路径对应[核心对象图与清理过程](../../drivers/base/devres.c.md#1.6_devres_release_all交出待清理记录)中的登记后清理；失败路径在 S0 直接返回给调用者，不进入设备链或 todo。返回[总索引](../../../navigation/P01_Linux_6.12_devres源码阅读索引.md#1.2_按问题进入实现)。
