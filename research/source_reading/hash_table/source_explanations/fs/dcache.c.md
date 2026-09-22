---
id: research.source_reading.hash_table.dcache_implementation
title: "dcache.c的名称候选与序列交接"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_dcache.c的名称候选与序列交接

上游位置 fs/dcache.c，证据为[固定原文](../../../linux/fs/dcache.c)，版本从[总索引](../../navigation/P01_Linux_6.12_哈希计算源码阅读索引.md)进入；调用关系返回[名称导读](../../navigation/P05_子系统索引身份与寿命导读.md#5.2_名称候选怎样交给路径遍历者)。以下删去上游长注释，保留函数体；中文 Doxygen 是仓库补充说明。

## 1.1\_候选匹配与序列交接

本节沿用已学的 RCU 读侧保护；DCACHE_OP_COMPARE 是父 dentry 上的能力标志，表示其文件系统提供自定义名字比较。它决定进入哪个比较分支，不是一次比较的成功结果。

```c
/**
 * d_hash - 仓库补充阅读说明：从混合的 hashlen 取桶入口，不完成名字比较。
 */
static inline struct hlist_bl_head *d_hash(unsigned long hashlen)
{
	return runtime_const_ptr(dentry_hashtable) +
		runtime_const_shift_right_32(hashlen, d_hash_shift);
}

/**
 * __d_lookup_rcu - 仓库补充阅读说明：在调用者的 RCU 路径保护内扫描，返回候选并写出序列样本。
 */
struct dentry *__d_lookup_rcu(const struct dentry *parent,
				const struct qstr *name,
				unsigned *seqp)
{
	u64 hashlen = name->hash_len;
	const unsigned char *str = name->name;
	struct hlist_bl_head *b = d_hash(hashlen);
	struct hlist_bl_node *node;
	struct dentry *dentry;
	if (unlikely(parent->d_flags & DCACHE_OP_COMPARE))
		return __d_lookup_rcu_op_compare(parent, name, seqp);
	hlist_bl_for_each_entry_rcu(dentry, node, b, d_hash) {
		unsigned seq;
		seq = raw_seqcount_begin(&dentry->d_seq);
		if (dentry->d_parent != parent)
			continue;
		if (d_unhashed(dentry))
			continue;
		if (dentry->d_name.hash_len != hashlen)
			continue;
		if (dentry_cmp(dentry, str, hashlen_len(hashlen)) != 0)
			continue;
		*seqp = seq;
		return dentry;
	}
	return NULL;
}
```

d_hash 使用已经初始化的全局桶地址与位移，runtime_const 系列并非随机种子接口。普通分支中 d_parent 检查父目录身份，d_unhashed 排除已撤下项，hash_len 同时筛选哈希与长度，dentry_cmp 最后比较名字；前面筛选不能省略最后一步。seqp 是输出到调用者栈变量的样本，不是给对象加引用。

raw_seqcount_begin 不等待写者稳定，但仍有相应读屏障；调用者必须再验证序列。源码明确该入口只供核心 VFS 的 RCU 路径遍历使用，不能提取成任意驱动的“无锁名称服务”。

```c
/**
 * __d_lookup_rcu_op_compare - 仓库补充阅读说明：为自定义比较取得一致的名字与长度对，再调用文件系统比较器。
 */
static noinline struct dentry *__d_lookup_rcu_op_compare(
	const struct dentry *parent,
	const struct qstr *name,
	unsigned *seqp)
{
	u64 hashlen = name->hash_len;
	struct hlist_bl_head *b = d_hash(hashlen);
	struct hlist_bl_node *node;
	struct dentry *dentry;
	hlist_bl_for_each_entry_rcu(dentry, node, b, d_hash) {
		int tlen;
		const char *tname;
		unsigned seq;
seqretry:
		seq = raw_seqcount_begin(&dentry->d_seq);
		if (dentry->d_parent != parent)
			continue;
		if (d_unhashed(dentry))
			continue;
		if (dentry->d_name.hash != hashlen_hash(hashlen))
			continue;
		tlen = dentry->d_name.len;
		tname = dentry->d_name.name;
		if (read_seqcount_retry(&dentry->d_seq, seq)) {
			cpu_relax();
			goto seqretry;
		}
		if (parent->d_op->d_compare(dentry, tlen, tname, name) != 0)
			continue;
		*seqp = seq;
		return dentry;
	}
	return NULL;
}
```

此分支先检查 d_seq，再把 tname/tlen 交给 d_compare；若样本变化就原候选重试。函数仍把 seqp 交给调用者，局部检查没有消除后续保护责任。大小写或编码语义不能用普通字节相等强行代替文件系统比较器。

桶头采用 hlist_bl，定义见[include/linux/list_bl.h](../../../linux/include/linux/list_bl.h)。其 LIST_BL_LOCKMASK 在 SMP 或 DEBUG_SPINLOCK 配置为 1，否则为 0；读入口屏蔽标志位，修改路径调用位自旋锁接口。它减少独立锁存储并不代表抢占、竞争或排序成本消失；本页不复制已经讲清的普通 hlist 宏体。

## 1.2\_并发改名下的未命中边界

```c
/**
 * d_lookup - 仓库补充阅读说明：在 __d_lookup 未命中时检查 rename_lock 序列，必要时重试。
 */
struct dentry *d_lookup(const struct dentry *parent, const struct qstr *name)
{
	struct dentry *dentry;
	unsigned seq;
	do {
		seq = read_seqbegin(&rename_lock);
		dentry = __d_lookup(parent, name);
		if (dentry)
			break;
	} while (read_seqretry(&rename_lock, seq));
	return dentry;
}
```

改名可能改变名字、父目录与桶归属。一次快速扫描漏掉候选时，不能立即推出磁盘中不存在；d_lookup 用 rename_lock 的序列检查排除相应改名干扰。__d_lookup 的成功结果已带引用，调用者用 dput 释放；__d_lookup_rcu 交接的是受路径保护的候选和序列，两种结果不可混用。

本页没有展开 __d_lookup 的全部锁实现、路径权限、负目录项重新验证或文件系统 I/O。名称缓存 miss 与实际磁盘访问之间仍有多个策略步骤；相关 VFS 正文保持独立职责。
