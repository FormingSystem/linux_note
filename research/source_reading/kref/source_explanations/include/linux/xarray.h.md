---
id: research.kref.impl.xarray_h
title: "xarray.h插入锁包装"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_xarray.h插入锁包装

上游相对路径 include/linux/xarray.h；NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20，blob 0b618ec04115fc3993bf33a7c358632bef170fc9。以下函数体按该 Git 对象核对，中文 Doxygen 是仓库补充；底层节点算法不在本页展开。

## 1.1\_插入包装自行管理锁

```c
/** @brief 仓库补充阅读说明：检查分配上下文，取锁调用底层插入，解锁后返回结果。 */
static inline int __must_check xa_insert(struct xarray *xa,
		unsigned long index, void *entry, gfp_t gfp)
{
	int err;

	might_alloc(gfp);
	xa_lock(xa);
	err = __xa_insert(xa, index, entry, gfp);
	xa_unlock(xa);

	return err;
}
```

本包装未读取 kref。调用者须在发布前预留条目责任，失败自行归还。__xa_insert 在允许分配时可暂时释放再取得 xa_lock，因此不能把这个包装理解为“任意附属应用状态跨分配始终被锁保护”。本例 id 在发布前固定，全部对象初始化在调用之前完成。

返回[整数索引模块](../../../navigation/P06_整数索引与拥有型查找导读.md#6.2_把容器动作接到引用周期)或[总阅读索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
