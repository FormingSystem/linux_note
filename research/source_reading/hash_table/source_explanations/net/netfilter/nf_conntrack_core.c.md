---
id: research.source_reading.hash_table.conntrack_implementation
title: "nf_conntrack_core.c的候选与身份复核"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_nf\_conntrack\_core.c的候选与身份复核

上游位置 net/netfilter/nf_conntrack_core.c，证据为[固定原文](../../../../linux/net/netfilter/nf_conntrack_core.c)。[总索引](../../../navigation/P01_Linux_6.12_哈希计算源码阅读索引.md)固定版本，[C0～C4 导读](../../../navigation/P05_子系统索引身份与寿命导读.md#5.3_连接跟踪的一轮取得流程)组织状态。以下中文 Doxygen 为仓库补充，删去上游长注释而保留函数体。

## 1.1\_从上下文到候选桶

本页承接教材的 TCP、NAT 和 RCU 前提。NF_CT_DIRECTION 从候选 tuplehash 取得方向，用于按方向检查 zone；NF_CT_STAT_INC_ATOMIC 增加当前网络命名空间的诊断计数，不是业务引用。原方向 IP_CT_DIR_ORIGINAL 与回复方向 IP_CT_DIR_REPLY 是方向枚举值，下方公共入口按它们取得 zone id。

```c
/**
 * hash_conntrack_raw - 仓库补充阅读说明：一次初始化基础密钥，再把本次 zone 和网络命名空间混入局部副本。
 */
static u32 hash_conntrack_raw(const struct nf_conntrack_tuple *tuple,
			      unsigned int zoneid,
			      const struct net *net)
{
	siphash_key_t key;
	get_random_once(&nf_conntrack_hash_rnd, sizeof(nf_conntrack_hash_rnd));
	key = nf_conntrack_hash_rnd;
	key.key[0] ^= zoneid;
	key.key[1] ^= net_hash_mix(net);
	return siphash((void *)tuple,
			offsetofend(struct nf_conntrack_tuple, dst.__nfct_hash_offsetend),
			&key);
}

/**
 * nf_ct_key_equal - 仓库补充阅读说明：把 tuple、zone、确认状态和网络命名空间共同用于匹配。
 */
static inline bool
nf_ct_key_equal(struct nf_conntrack_tuple_hash *h,
		const struct nf_conntrack_tuple *tuple,
		const struct nf_conntrack_zone *zone,
		const struct net *net)
{
	struct nf_conn *ct = nf_ct_tuplehash_to_ctrack(h);
	return nf_ct_tuple_equal(tuple, &h->tuple) &&
	       nf_ct_zone_equal(ct, zone, NF_CT_DIRECTION(h)) &&
	       nf_ct_is_confirmed(ct) &&
	       net_eq(net, nf_ct_net(ct));
}

/**
 * ____nf_conntrack_find - 仓库补充阅读说明：C1 取得当前表与容量，遍历候选，并用链尾桶身份检查是否需要重扫。
 */
static struct nf_conntrack_tuple_hash *
____nf_conntrack_find(struct net *net, const struct nf_conntrack_zone *zone,
		      const struct nf_conntrack_tuple *tuple, u32 hash)
{
	struct nf_conntrack_tuple_hash *h;
	struct hlist_nulls_head *ct_hash;
	struct hlist_nulls_node *n;
	unsigned int bucket, hsize;
begin:
	nf_conntrack_get_ht(&ct_hash, &hsize);
	bucket = reciprocal_scale(hash, hsize);
	hlist_nulls_for_each_entry_rcu(h, n, &ct_hash[bucket], hnnode) {
		struct nf_conn *ct;
		ct = nf_ct_tuplehash_to_ctrack(h);
		if (nf_ct_is_expired(ct)) {
			nf_ct_gc_expired(ct);
			continue;
		}
		if (nf_ct_key_equal(h, tuple, zone, net))
			return h;
	}
	if (get_nulls_value(n) != bucket) {
		NF_CT_STAT_INC_ATOMIC(net, search_restart);
		goto begin;
	}
	return NULL;
}
```

tuple 的布局见[tuple 头文件](../../../../linux/include/net/netfilter/nf_conntrack_tuple.h)：哈希长度止于 __nfct_hash_offsetend，不把方向字段 dir 作为哈希数据。IPv4 TCP 的地址、端口与协议是方便理解的一种实例，通用结构还表达网络层协议及不同传输协议，不能把整份结构强制解释成五个整数。

nf_conn 的[原始结构](../../../../linux/include/net/netfilter/nf_conntrack.h)持有原方向、回复方向两个 tuplehash，共用 ct_general.use 与协议状态。哈希混入上下文并不能代替 nf_ct_key_equal 的完整比较。get_random_once 初始化基础密钥，不意味着每个包换种子，也没有这里每隔几分钟刷新全部桶的实现。

扫描使用 nf_conntrack_get_ht 取得表和容量，桶号来自 reciprocal_scale；不是旧例中 net->ct.hash 私有数组。遇到需要回收的过期候选时，nf_ct_gc_expired 尝试引用及删除后继续；本页不展开其全部回收实现。到链尾发现 nulls 标记的桶号不同，就重新读取表并重扫，不能把曾经走过一次链当成完整候选集合。

## 1.2\_候选之后还要取得并复核

```c
/**
 * __nf_conntrack_find_get - 仓库补充阅读说明：C2 非零引用取得后执行取得屏障，C3 再查身份；不匹配就撤销引用。
 */
static struct nf_conntrack_tuple_hash *
__nf_conntrack_find_get(struct net *net, const struct nf_conntrack_zone *zone,
			const struct nf_conntrack_tuple *tuple, u32 hash)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;
	h = ____nf_conntrack_find(net, zone, tuple, hash);
	if (h) {
		ct = nf_ct_tuplehash_to_ctrack(h);
		if (likely(refcount_inc_not_zero(&ct->ct_general.use))) {
			smp_acquire__after_ctrl_dep();
			if (likely(nf_ct_key_equal(h, tuple, zone, net)))
				return h;
			nf_ct_put(ct);
		}
		h = NULL;
	}
	return h;
}

/**
 * nf_conntrack_find_get - 仓库补充阅读说明：C0 建立读侧，按方向对应 zone 尝试，C4 退出读侧交付结果。
 */
struct nf_conntrack_tuple_hash *
nf_conntrack_find_get(struct net *net, const struct nf_conntrack_zone *zone,
		      const struct nf_conntrack_tuple *tuple)
{
	unsigned int rid, zone_id = nf_ct_zone_id(zone, IP_CT_DIR_ORIGINAL);
	struct nf_conntrack_tuple_hash *thash;
	rcu_read_lock();
	thash = __nf_conntrack_find_get(net, zone, tuple,
					hash_conntrack_raw(tuple, zone_id, net));
	if (thash)
		goto out_unlock;
	rid = nf_ct_zone_id(zone, IP_CT_DIR_REPLY);
	if (rid != zone_id)
		thash = __nf_conntrack_find_get(net, zone, tuple,
						hash_conntrack_raw(tuple, rid, net));
out_unlock:
	rcu_read_unlock();
	return thash;
}
```

候选扫描与引用增加不是一个原子事务。分配器使用 SLAB_TYPESAFE_BY_RCU，原位置可能已被另一连接复用：C2 的非零计数只能证明取得了当前对象的引用，C3 才验证是不是所请求的连接。源码在引用成功之后调用 smp_acquire__after_ctrl_dep，不能把它移动到比较之前或用普通整数增加替代。

失败有两种：计数已归零不能复活；成功取得后身份不符，需要 nf_ct_put 撤销。该内部路径本身返回 NULL，不在这里无限重试同一地址。公开入口还可能按回复方向的不同 zone id 再查一次，这是方向上下文选择，不能误说为任何失败都无条件重新扫描。

成功结果带引用，调用者以后释放。confirmed 表示已确认进入连接跟踪的状态要求，不证明包通过防火墙规则，也不证明 TCP 序号或状态合法。本页只覆盖索引与取得协议，不能据此宣称已经验证 NAT、协议解析、并发删除或安全攻击抵抗能力。
