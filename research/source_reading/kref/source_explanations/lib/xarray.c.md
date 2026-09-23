---
id: research.kref.impl.xarray_c
title: "xarray.c查询与删除锁边界"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_xarray.c查询与删除锁边界

上游相对路径 lib/xarray.c；NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20，blob 32d4bac8c94ca13e11f350c6bcfcacc2040d0359。以下函数体按该 Git 对象核对，中文 Doxygen 是仓库补充；底层节点算法不在本页展开。

## 1.1\_查询内部读侧窗口在返回前结束

```c
/** @brief 仓库补充阅读说明：在内部RCU窗口读取索引；返回地址时该窗口已经结束。 */
void *xa_load(struct xarray *xa, unsigned long index)
{
	XA_STATE(xas, xa, index);
	void *entry;

	rcu_read_lock();
	do {
		entry = xas_load(&xas);
		if (xa_is_zero(entry))
			entry = NULL;
	} while (xas_retry(&xas, entry));
	rcu_read_unlock();

	return entry;
}
```

xa_load 的内部 RCU 用于其数据结构访问，没有替对象取得 kref。应用若需要在返回后 get，仍须按协议用外层 xa_lock 保持条目拥有的一份，或采用另外完整的对象回收方案。

## 1.2\_删除包装与已持锁入口

```c
/** @brief 仓库补充阅读说明：调用者已经持xa_lock；将该索引条目替换为NULL并返回旧结果。 */
void *__xa_erase(struct xarray *xa, unsigned long index)
{
	XA_STATE(xas, xa, index);
	return xas_result(&xas, xas_store(&xas, NULL));
}
```

```c
/** @brief 仓库补充阅读说明：自行取得和释放xa_lock，把旧条目交还调用者。 */
void *xa_erase(struct xarray *xa, unsigned long index)
{
	void *entry;

	xa_lock(xa);
	entry = __xa_erase(xa, index);
	xa_unlock(xa);

	return entry;
}
```

应用已有 xa_lock 时应调用 __xa_erase；调用 xa_erase 则由它自行加锁。本例直接用后者，返回后只对非空旧对象归还成员份额。底层 xas_store/xas_result 负责索引结构和标记结果，本页不扩展为节点分配算法。

返回[整数索引模块](../../navigation/P06_整数索引与拥有型查找导读.md#6.2_把容器动作接到引用周期)或[总阅读索引](../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
[期待对象删除](../../navigation/P06_整数索引与拥有型查找导读.md#6.4_删除当前条目与删除期待对象)在同一外层xa_lock窗口比较身份再进入__xa_erase，不重复套xa_erase公开锁包装。
