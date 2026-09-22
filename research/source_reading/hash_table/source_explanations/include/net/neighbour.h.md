---
id: research.source_reading.hash_table.neighbour_header_implementation
title: "neighbour.h的桶链与设备身份"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_neighbour.h的桶链与设备身份

上游位置 include/net/neighbour.h，[固定原文](../../../../linux/include/net/neighbour.h)保存完整布局。版本见[总索引](../../../navigation/P01_Linux_6.12_哈希计算源码阅读索引.md)，调用顺序见[邻居导读](../../../navigation/P05_子系统索引身份与寿命导读.md#5.4_邻居身份与地址解析分开阅读)。中文 Doxygen 为仓库补充。

## 1.1\_桶链与完整身份

neigh_table 的 nht 指向当前 neigh_hash_table，后者保存 hash_buckets、hash_shift 与 hash_rnd。每个 neighbour 的 next 串起桶链，dev 保存设备，primary_key 保存协议地址；refcnt 与 nud_state 分别管理引用和可达性状态。这些字段是多组不同职责的状态，不是一个 hash 成功位。

```c
/**
 * ___neigh_lookup_noref - 仓库补充阅读说明：调用者已持读侧保护；以设备和协议键匹配，无引用返回候选。
 */
static inline struct neighbour *___neigh_lookup_noref(
	struct neigh_table *tbl,
	bool (*key_eq)(const struct neighbour *n, const void *pkey),
	__u32 (*hash)(const void *pkey,
		      const struct net_device *dev,
		      __u32 *hash_rnd),
	const void *pkey,
	struct net_device *dev)
{
	struct neigh_hash_table *nht = rcu_dereference(tbl->nht);
	struct neighbour *n;
	u32 hash_val;
	hash_val = hash(pkey, dev, nht->hash_rnd) >> (32 - nht->hash_shift);
	for (n = rcu_dereference(nht->hash_buckets[hash_val]);
	     n != NULL;
	     n = rcu_dereference(n->next)) {
		if (n->dev == dev && key_eq(n, pkey))
			return n;
	}
	return NULL;
}

/**
 * __neigh_lookup_noref - 仓库补充阅读说明：使用表自身的 hash 与 key_eq 回调，不改变无引用契约。
 */
static inline struct neighbour *__neigh_lookup_noref(struct neigh_table *tbl,
						     const void *pkey,
						     struct net_device *dev)
{
	return ___neigh_lookup_noref(tbl, tbl->key_eq, tbl->hash, pkey, dev);
}
```

hash 先混合 pkey、dev 和当前种子，再按 hash_shift 取桶；key_eq 比较协议键，n->dev 则独立排除另一设备。相同地址在不同设备上对应不同邻居，不能只看协议地址字节就合并对象。

这些函数没有 rcu_read_lock，也没有增加 refcnt。调用者必须保证访问时的保护；需要跨出临界区使用时，从[neigh_lookup 包装](../../net/core/neighbour.c.md#1.1_设备和协议地址共同匹配)追踪实际引用取得。找到条目只说明索引身份匹配，不承诺 ha 中已有可用链路地址。
