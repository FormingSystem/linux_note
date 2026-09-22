---
id: research.source_reading.hash_table.neighbour_implementation
title: "neighbour.c的引用与桶存储"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_neighbour.c的引用与桶存储

上游位置 net/core/neighbour.c，[固定原文](../../../../linux/net/core/neighbour.c)是本页证据。身份回到[总索引](../../../navigation/P01_Linux_6.12_哈希计算源码阅读索引.md)，场景回到[邻居导读](../../../navigation/P05_子系统索引身份与寿命导读.md#5.4_邻居身份与地址解析分开阅读)。中文 Doxygen 为仓库阅读说明。

## 1.1\_设备和协议地址共同匹配

NEIGH_CACHE_STAT_INC 是邻居表诊断计数宏，分别记录查询和候选命中事件；它与下面真正取得寿命的 refcount_inc_not_zero 不是同一类操作。

```c
/**
 * neigh_lookup - 仓库补充阅读说明：读侧内查完整设备与地址身份，再尝试取得非零引用。
 */
struct neighbour *neigh_lookup(struct neigh_table *tbl, const void *pkey,
			       struct net_device *dev)
{
	struct neighbour *n;
	NEIGH_CACHE_STAT_INC(tbl, lookups);
	rcu_read_lock();
	n = __neigh_lookup_noref(tbl, pkey, dev);
	if (n) {
		if (!refcount_inc_not_zero(&n->refcnt))
			n = NULL;
		NEIGH_CACHE_STAT_INC(tbl, hits);
	}
	rcu_read_unlock();
	return n;
}
```

内部无引用扫描的唯一展开见[neighbour.h](../../include/net/neighbour.h.md#1.1_桶链与完整身份)。这里在退出 RCU 之前增加 refcnt，失败便返回 NULL，成功后调用者用 neigh_release 释放。不要把这份实现与 conntrack 的复核协议互换：不同对象的分配、身份更新和销毁约束不相同。

统计也有边界：lookups 计本次尝试；hits 位于“曾找到候选”分支，即使非零引用取得失败也会增加。单看 hits 不能证明成功返回了对象，更不能证明已经获得对端 MAC 地址。发送路径还要检查可达性状态及输出函数。

## 1.2\_随机因子与回收预算

PAGE_SIZE 是当前构建的一页字节数，NEIGH_NUM_HASH_RND 是种子数组项数，本版本为 4。GFP_ATOMIC 是非睡眠分配标志，__GFP_ZERO 请求清零新页；这些选择说明分配上下文和初值，不能推出内存总能取得。

```c
/**
 * neigh_get_hash_rnd - 仓库补充阅读说明：产生一个最低位置一的随机因子。
 */
static void neigh_get_hash_rnd(u32 *x)
{
	*x = get_random_u32() | 1;
}

/**
 * neigh_hash_alloc - 仓库补充阅读说明：分配新的桶存储，按大小选择分配方式并填充该表种子。
 */
static struct neigh_hash_table *neigh_hash_alloc(unsigned int shift)
{
	size_t size = (1 << shift) * sizeof(struct neighbour *);
	struct neigh_hash_table *ret;
	struct neighbour __rcu **buckets;
	int i;
	ret = kmalloc(sizeof(*ret), GFP_ATOMIC);
	if (!ret)
		return NULL;
	if (size <= PAGE_SIZE) {
		buckets = kzalloc(size, GFP_ATOMIC);
	} else {
		buckets = (struct neighbour __rcu **)
			  __get_free_pages(GFP_ATOMIC | __GFP_ZERO,
					   get_order(size));
		kmemleak_alloc(buckets, size, 1, GFP_ATOMIC);
	}
	if (!buckets) {
		kfree(ret);
		return NULL;
	}
	ret->hash_buckets = buckets;
	ret->hash_shift = shift;
	for (i = 0; i < NEIGH_NUM_HASH_RND; i++)
		neigh_get_hash_rnd(&ret->hash_rnd[i]);
	return ret;
}
```

这份实现是在创建新的 neigh_hash_table 时填写 hash_rnd，并非教材旧稿的统一周期刷新。小桶数组用 kzalloc，大数组按页分配；任一步失败都会释放先前已取得的存储。GFP_ATOMIC 表明这条路径不能依赖可睡眠回收，不保证一定分配成功。

原文的 neigh_hash_grow 在表锁保护下重新计算桶并发布 tbl->nht，旧桶经 RCU 延后释放；本页未逐句展开迁移算法，不能把它当成 rhashtable 的 future_tbl 协议副本。正文只需要理解种子跟随具体桶存储以及换表需要配套重排。

容量压力还涉及业务对象，而不只是桶数组。neigh_alloc 根据 gc_entries、gc_thresh2/3 和 last_flush 决定是否尝试强制回收；neigh_forced_gc 只考虑引用、邻居状态与时间条件允许的项，并设有工作预算。到上界且没有回收到足够空间时仍可能报告 table full。阈值不是“内核一定腾出空间”的保证；本批不声称已验证真实网络压力或全部垃圾回收状态。
