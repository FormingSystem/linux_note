---
id: research.kref.impl.idr_c
title: "idr.c编号发布与查找边界"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_idr.c编号发布与查找边界

上游相对路径 lib/idr.c；NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20，blob da36054c3ca02058dcfa3338c712ba664b79b13a。以下函数体按该 Git 对象核对，中文 Doxygen 是仓库补充；底层节点算法不在本页展开。

## 1.1\_返回编号与对象字段初始化

```c
/** @brief 仓库补充阅读说明：用局部id接收分配编号；不会自行写应用对象的id字段。 */
int idr_alloc(struct idr *idr, void *ptr, int start, int end, gfp_t gfp)
{
	u32 id = start;
	int ret;

	if (WARN_ON_ONCE(start < 0))
		return -EINVAL;

	ret = idr_alloc_u32(idr, ptr, &id, end > 0 ? end - 1 : INT_MAX, gfp);
	if (ret)
		return ret;

	return id;
}
```

```c
/** @brief 仓库补充阅读说明：先写nextid指向的位置，再把ptr发布到索引。 */
int idr_alloc_u32(struct idr *idr, void *ptr, u32 *nextid,
			unsigned long max, gfp_t gfp)
{
	struct radix_tree_iter iter;
	void __rcu **slot;
	unsigned int base = idr->idr_base;
	unsigned int id = *nextid;

	if (WARN_ON_ONCE(!(idr->idr_rt.xa_flags & ROOT_IS_IDR)))
		idr->idr_rt.xa_flags |= IDR_RT_MARKER;

	id = (id < base) ? 0 : id - base;
	radix_tree_iter_init(&iter, id);
	slot = idr_get_free(&idr->idr_rt, &iter, gfp, max - base);
	if (IS_ERR(slot))
		return PTR_ERR(slot);

	*nextid = iter.index + base;
	/* 替换辅助内部包含发布所需屏障。 */
	radix_tree_iter_replace(&idr->idr_rt, &iter, slot, ptr);
	radix_tree_iter_tag_clear(&idr->idr_rt, &iter, IDR_FREE);

	return 0;
}
```

idr_alloc 使用的是局部 u32 id，所以调用返回后应用仍须设置 obj->id。本章以外层 mutex 排斥全部查找/增删，成功写字段以后才解锁。若使用 idr_alloc_u32 并让 nextid 指向对象的 u32 字段，内层本来就先写编号再发布；两种方案不能混称为“任意 idr_alloc 自动初始化对象”。该分配接口需要外部写方同步。

## 1.2\_查询与移除不管理对象引用

```c
/** @brief 仓库补充阅读说明：返回编号关联指针，未增加对象引用。 */
void *idr_find(const struct idr *idr, unsigned long id)
{
	return radix_tree_lookup(&idr->idr_rt, id - idr->idr_base);
}
```

```c
/** @brief 仓库补充阅读说明：删除编号映射并交回旧指针，未归还对象引用。 */
void *idr_remove(struct idr *idr, unsigned long id)
{
	return radix_tree_delete_item(&idr->idr_rt, id - idr->idr_base, NULL);
}
```

本章外层同一 mutex 覆盖 idr_find 与 get；remove 取得旧指针、解锁后才 put。未注册编号返回 NULL，不产生可归还份额。索引的 radix tree 实现不是业务对象生命周期管理者。

返回[整数索引模块](../../navigation/P06_整数索引与拥有型查找导读.md#6.2_把容器动作接到引用周期)或[总阅读索引](../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
