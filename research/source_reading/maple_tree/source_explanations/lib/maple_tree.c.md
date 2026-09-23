---
id: research.maple_tree.implementation.core
title: "lib/maple_tree.c 内部节点资源去向"
kind: source
status: evolving
domains:
  - linux
  - memory
---

# 第1章\_lib/maple\_tree.c\_内部节点资源去向

## 1.1\_固定实现边界

上游 lib/maple_tree.c，固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob 8d73ccf66f3aa0588d5ee00a6e7dad3258110d83。[原始实现](../../../linux/lib/maple_tree.c)用于核对；[树模式导读](../../navigation/P03_树对象与模式选择.md#3.2_从未发布到受保护使用)说明 R0～R3 背景，[总索引](../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)连接其他模块。本文件目前只展开一个退休节点分派函数，不宣称完整 Maple 回收已审完。

## 1.2\_退休节点根据模式选择去向

```c
/**
 * @brief 仓库补充阅读说明：处理调用方交来的已退休编码节点；RCU 模式与操作池复用选择不同。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
static inline void mas_free(struct ma_state *mas, struct maple_enode *used)
{
	/* 仓库补充：编码节点先解码，模式决定延迟释放还是操作池复用。 */
	struct maple_node *tmp = mte_to_node(used);

	if (mt_in_rcu(mas->tree))
		ma_free_rcu(tmp);
	else
		mas_push_node(mas, tmp);
}
```

先将 used 解码为内部 maple_node，随后读取 mas->tree 的模式。RCU 模式交给 ma_free_rcu；非 RCU 模式调用 mas_push_node，资源归入当前 ma_state 的分配管理。这里处理的是内部节点，不是叶中存的 VMA entry，也不是一个可接受任意活跃节点的销毁接口。

前置条件由调用路径建立：节点已按算法退出使用、调用者拥有合适的修改保护，并且当前模式符合读者生命周期。这个函数本身没有摘除根或父槽，没有等待宽限期，也不验证 VMA 引用。其分支足以纠正“USE_RCU 允许立即复用”的误读，却不足以证明读侧检测、延迟回调与所有批量销毁路径；这些算法尚需后续单独展开。

## 1.3\_节点缓存按实际结构大小申请对齐

```c
/**
 * @brief 仓库补充阅读说明：对象大小与申请的对齐均由 sizeof(maple_node) 给出。
 * @note 以下定义或语句保持官方固定版本；不自动构成完整算法保证。
 */
void __init maple_tree_init(void)
{
	maple_node_cache = kmem_cache_create("maple_node",
			sizeof(struct maple_node), sizeof(struct maple_node),
			SLAB_PANIC, NULL);
}
```

maple_node_cache 保存后续节点分配使用的缓存。kmem_cache_create 的对象大小和对齐参数相同，SLAB_PANIC 是初始化分配失败策略；本函数不创建一棵业务树。源码头部说明节点容器按 256 字节布局和对齐，但具体构建仍应结合类型与 ABI 核对。

本次从固定头提取相关定义，显式提供 __rcu 与双指针形状的 rcu_head 适配，以 Clang 的 ARM32 和 x86_64 freestanding 前端检查 sizeof：maple_node/range 均为 256，arange 分别为 256/248。此项只验证明确适配后的字段布局，没有完整 Kbuild、调试配置组合或分配器运行验证。回到[节点布局导读](../../navigation/P04_节点布局与范围分区.md#4.2_按问题读取布局)。

## 1.4\_编码节点保存类型而不是父槽

```c
/**
 * @brief 仓库补充阅读说明：编码节点类型、根入口标记和 NULL 位属于当前入口表示。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
#define MAPLE_ROOT_NODE			0x02
/* maple_type stored bit 3-6 */
#define MAPLE_ENODE_TYPE_SHIFT		0x03
/* Bit 2 means a NULL somewhere below */
#define MAPLE_ENODE_NULL		0x04
```

```c
/**
 * @brief 仓库补充阅读说明：构造编码节点时加入类型和 NULL 标记。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static inline struct maple_enode *mt_mk_node(const struct maple_node *node,
					     enum maple_type type)
{
	return (void *)((unsigned long)node |
			(type << MAPLE_ENODE_TYPE_SHIFT) | MAPLE_ENODE_NULL);
}
```

```c
/**
 * @brief 仓库补充阅读说明：位 3 到 6 解出当前节点类型。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static __always_inline enum maple_type mte_node_type(
		const struct maple_enode *entry)
{
	return ((unsigned long)entry >> MAPLE_NODE_TYPE_SHIFT) &
		MAPLE_NODE_TYPE_MASK;
}
```

```c
/**
 * @brief 仓库补充阅读说明：仅对合法编码节点清低八位恢复裸节点地址。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static __always_inline struct maple_node *mte_to_node(
		const struct maple_enode *entry)
{
	return (struct maple_node *)((unsigned long)entry & ~MAPLE_NODE_MASK);
}
```

```c
/**
 * @brief 仓库补充阅读说明：在共享根入口增加 bit 1 标记。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static inline void *mte_mk_root(const struct maple_enode *node)
{
	return (void *)((unsigned long)node | MAPLE_ROOT_NODE);
}
```

```c
/**
 * @brief 仓库补充阅读说明：只移除根入口标记，不移除其他编码信息。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static inline void *mte_safe_root(const struct maple_enode *node)
{
	return (void *)((unsigned long)node & ~MAPLE_ROOT_NODE);
}
```

这些 helper 解释下行节点表示，不读取父槽号。mt_mk_node 默认置 MAPLE_ENODE_NULL；本节不由构造位推出完整空洞维护结论。解码后的地址还需要正确生命周期保护。

## 1.5\_父关系编码与根例外

```c
/**
 * @brief 仓库补充阅读说明：node.parent 的根关系使用 bit 0。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
#define MA_ROOT_PARENT 1
```

```c
/**
 * @brief 仓库补充阅读说明：父槽掩码实际覆盖位 3 到 7，与头部旧注释的四位说法不同。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
#define MAPLE_PARENT_ROOT		0x01

#define MAPLE_PARENT_SLOT_SHIFT		0x03
#define MAPLE_PARENT_SLOT_MASK		0xF8

#define MAPLE_PARENT_16B_SLOT_SHIFT	0x02
#define MAPLE_PARENT_16B_SLOT_MASK	0xFC

#define MAPLE_PARENT_RANGE64		0x06
#define MAPLE_PARENT_RANGE32		0x04
#define MAPLE_PARENT_NOT_RANGE16	0x02
```

```c
/**
 * @brief 仓库补充阅读说明：先检查 node.parent 是否是根关系。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static __always_inline bool ma_is_root(struct maple_node *node)
{
	return ((unsigned long)node->parent & MA_ROOT_PARENT);
}
```

```c
/**
 * @brief 仓库补充阅读说明：父关系位形决定槽号使用的移位。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static inline unsigned long mte_parent_shift(unsigned long parent)
{
	/* Note bit 1 == 0 means 16B */
	if (likely(parent & MAPLE_PARENT_NOT_RANGE16))
		return MAPLE_PARENT_SLOT_SHIFT;

	return MAPLE_PARENT_16B_SLOT_SHIFT;
}
```

```c
/**
 * @brief 仓库补充阅读说明：写入当前实现允许的非叶 range/arange 父关系。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static inline
void mas_set_parent(struct ma_state *mas, struct maple_enode *enode,
		    const struct maple_enode *parent, unsigned char slot)
{
	unsigned long val = (unsigned long)parent;
	unsigned long shift;
	unsigned long type;
	enum maple_type p_type = mte_node_type(parent);

	MAS_BUG_ON(mas, p_type == maple_dense);
	MAS_BUG_ON(mas, p_type == maple_leaf_64);

	switch (p_type) {
	case maple_range_64:
	case maple_arange_64:
		shift = MAPLE_PARENT_SLOT_SHIFT;
		type = MAPLE_PARENT_RANGE64;
		break;
	default:
	case maple_dense:
	case maple_leaf_64:
		shift = type = 0;
		break;
	}

	val &= ~MAPLE_NODE_MASK; /* Clear all node metadata in parent */
	val |= (slot << shift) | type;
	mte_to_node(enode)->parent = ma_parent_ptr(val);
}
```

```c
/**
 * @brief 仓库补充阅读说明：根返回零作为该分支结果，普通父关系再提取槽位。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static __always_inline
unsigned int mte_parent_slot(const struct maple_enode *enode)
{
	unsigned long val = (unsigned long)mte_to_node(enode)->parent;

	if (unlikely(val & MA_ROOT_PARENT))
		return 0;

	/*
	 * Okay to use MAPLE_PARENT_16B_SLOT_MASK as the last bit will be lost
	 * by shift if the parent shift is MAPLE_PARENT_SLOT_SHIFT
	 */
	return (val & MAPLE_PARENT_16B_SLOT_MASK) >> mte_parent_shift(val);
}
```

```c
/**
 * @brief 仓库补充阅读说明：普通父节点地址按节点对齐掩码恢复，根关系必须另行处理。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static __always_inline
struct maple_node *mte_parent(const struct maple_enode *enode)
{
	return (void *)((unsigned long)
			(mte_to_node(enode)->parent) & ~MAPLE_NODE_MASK);
}
```

mas_set_parent 清掉 parent 编码节点的元数据后，按父类型重新写入槽号与父格式。其 slot 必须由合法调用者约束，函数不为任意超大槽号提供错误返回。根 parent 关联 maple_tree 对象，树对象不继承 256 字节节点对齐，不能先清低八位再声称找回根所属树。SLOT_MASK 0xF8 与当前写入解释优先于旧头注释；16-bit 分支的存在不证明当前节点类型会进入该写入分支。

## 1.6\_保留entry与操作错误分别判断

```c
/**
 * @brief 仓库补充阅读说明：小于 4096 且 low bits 为 10 才属于这个小值保留检查。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static __always_inline bool mt_is_reserved(const void *entry)
{
	return ((unsigned long)entry < MAPLE_RESERVED_RANGE) &&
		xa_is_internal(entry);
}
```

```c
/**
 * @brief 仓库补充阅读说明：同时写错误载荷和独立 ma_error 状态。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static __always_inline void mas_set_err(struct ma_state *mas, long err)
{
	mas->node = MA_ERROR(err);
	mas->status = ma_error;
}
```

xa_is_internal 的具体低位判断见[xarray 辅助](../include/linux/xarray.h.md#1.2_整数值使用低位标记并限制有效位宽)；MA_ERROR 与 mas_is_err 见[独立状态](../include/linux/maple_tree.h.md#1.9_错误载荷与独立状态)。不要把 mt_is_reserved 返回假当成对象地址有效，也不要由叶 entry 位形推断 ma_state.status。回到[编码导读](../../navigation/P05_字段编码与状态分工.md#5.2_同一数值先按存储位置解读)。

## 1.7\_从操作状态选择树入口

```c
/**
 * @brief 仓库补充阅读说明：只在 start 状态建立根范围，并区分节点树、空树和根直存。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
static inline struct maple_enode *mas_start(struct ma_state *mas)
{
	if (likely(mas_is_start(mas))) {
		struct maple_enode *root;

		mas->min = 0;
		mas->max = ULONG_MAX;

retry:
		mas->depth = 0;
		root = mas_root(mas);
		/* Tree with nodes */
		if (likely(xa_is_node(root))) {
			mas->depth = 1;
			mas->status = ma_active;
			mas->node = mte_safe_root(root);
			mas->offset = 0;
			if (mte_dead_node(mas->node))
				goto retry;

			return NULL;
		}

		mas->node = NULL;
		/* empty tree */
		if (unlikely(!root)) {
			mas->status = ma_none;
			mas->offset = MAPLE_NODE_SLOTS;
			return NULL;
		}

		/* Single entry tree */
		mas->status = ma_root;
		mas->offset = MAPLE_NODE_SLOTS;

		/* Single entry tree. */
		if (mas->index > 0)
			return NULL;

		return root;
	}

	return NULL;
}
```

```c
/**
 * @brief 仓库补充阅读说明：先处理根分支，再进入维护范围状态的树行走。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
static inline void *mas_state_walk(struct ma_state *mas)
{
	void *entry;

	entry = mas_start(mas);
	if (mas_is_none(mas))
		return NULL;

	if (mas_is_ptr(mas))
		return entry;

	return mtree_range_walk(mas);
}
```

节点树设置 active 与节点入口，发现死节点时重取根；空根设置 none；直接 entry 的根先设 root，但输入 index 大于零时仍可返回 NULL。因此 root 不等于本次必然命中。mtree_range_walk 的完整内部算法未在此展开，不将短分派函数当作完整搜索证明。

## 1.8\_从根定位与walk重走

```c
/**
 * @brief 仓库补充阅读说明：当前固定表达式会将入口状态设置为 start，再处理重试与根边界。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
void *mas_walk(struct ma_state *mas)
{
	void *entry;

	if (!mas_is_active(mas) || !mas_is_start(mas))
		mas->status = ma_start;
retry:
	entry = mas_state_walk(mas);
	if (mas_is_start(mas)) {
		goto retry;
	} else if (mas_is_none(mas)) {
		mas->index = 0;
		mas->last = ULONG_MAX;
	} else if (mas_is_ptr(mas)) {
		if (!mas->index) {
			mas->last = 0;
			return entry;
		}

		mas->index = 1;
		mas->last = ULONG_MAX;
		mas->status = ma_none;
		return NULL;
	}

	return entry;
}
```

条件是 !mas_is_active || !mas_is_start，同一枚举不可能同时等于 active 与 start，所以此版本进入函数时会设为 start。之后可能因并发读到失效节点而重试，none/root 分支调整结果范围；这是固定代码的行为，不用注释或函数名替它猜测缓存复用。这里没有改动外部源码，也没有证明整个重试协议的并发正确性。

## 1.9\_暂停继续与有界find

```c
/**
 * @brief 仓库补充阅读说明：仅设置 pause 并清 node，不解锁且保留范围。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
void mas_pause(struct ma_state *mas)
{
	mas->status = ma_pause;
	mas->node = NULL;
}
```

```c
/**
 * @brief 仓库补充阅读说明：根据先前状态决定继续起点、边界早退或重新行走。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
static __always_inline bool mas_find_setup(struct ma_state *mas, unsigned long max, void **entry)
{
	switch (mas->status) {
	case ma_active:
		if (mas->last < max)
			return false;
		return true;
	case ma_start:
		break;
	case ma_pause:
		if (unlikely(mas->last >= max))
			return true;

		mas->index = ++mas->last;
		mas->status = ma_start;
		break;
	case ma_none:
		if (unlikely(mas->last >= max))
			return true;

		mas->index = mas->last;
		mas->status = ma_start;
		break;
	case ma_underflow:
		/* mas is pointing at entry before unable to go lower */
		if (unlikely(mas->index >= max)) {
			mas->status = ma_overflow;
			return true;
		}

		mas->status = ma_active;
		*entry = mas_walk(mas);
		if (*entry)
			return true;
		break;
	case ma_overflow:
		if (unlikely(mas->last >= max))
			return true;

		mas->status = ma_active;
		*entry = mas_walk(mas);
		if (*entry)
			return true;
		break;
	case ma_root:
		break;
	case ma_error:
		return true;
	}

	if (mas_is_start(mas)) {
		/* First run or continue */
		if (mas->index > max)
			return true;

		*entry = mas_walk(mas);
		if (*entry)
			return true;

	}

	if (unlikely(mas_is_ptr(mas)))
		goto ptr_out_of_range;

	if (unlikely(mas_is_none(mas)))
		return true;

	if (mas->index == max)
		return true;

	return false;

ptr_out_of_range:
	mas->status = ma_none;
	mas->index = 1;
	mas->last = ULONG_MAX;
	return true;
}
```

```c
/**
 * @brief 仓库补充阅读说明：先运行 setup，必要时走下一槽；该分支结束将状态置回 active。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
void *mas_find(struct ma_state *mas, unsigned long max)
{
	void *entry = NULL;

	if (mas_find_setup(mas, max, &entry))
		return entry;

	/* Retries on dead nodes handled by mas_next_slot */
	entry = mas_next_slot(mas, max, false);
	/* Ignore overflow */
	mas->status = ma_active;
	return entry;
}
```

S2 pause 由调用者在放锁前执行，S4 重获保护后 find 的 pause 分支先检查 last<max，再推进 index=++last；S5 reset 则保持当前 index。active 且 last 已到 max 时 setup 直接结束，返回 NULL 不改变 active。进入下一槽分支后，本函数也明确覆盖状态为 active，不把任意 NULL 推断为 overflow。ma_error 分支只早退，不帮调用者恢复或释放资源。

本批私有模块和模型化树行走的宿主夹具只核对这些控制路径；mas_next_slot/mtree_range_walk 的完整动态算法、RCU 重试与回收仍未运行。模块导读见[一次遍历周期](../../navigation/P06_操作游标与暂停继续.md#6.2_沿一次遍历追踪状态)。

## 1.10\_普通点查与读侧边界

```c
/**
 * @brief 仓库补充阅读说明：点查临时状态只服务当前索引，返回前退出内部 RCU 读侧。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
void *mtree_load(struct maple_tree *mt, unsigned long index)
{
	MA_STATE(mas, mt, index, index);
	void *entry;

	trace_ma_read(__func__, &mas);
	rcu_read_lock();
retry:
	entry = mas_start(&mas);
	if (unlikely(mas_is_none(&mas)))
		goto unlock;

	if (unlikely(mas_is_ptr(&mas))) {
		if (index)
			entry = NULL;

		goto unlock;
	}

	entry = mtree_lookup_walk(&mas);
	if (!entry && unlikely(mas_is_start(&mas)))
		goto retry;
unlock:
	rcu_read_unlock();
	if (xa_is_zero(entry))
		return NULL;

	return entry;
}
```

从 S4 看本函数：mas_start 处理空根、根直接 entry 和节点根，普通快速 walk 不承诺维护完整游标，失效入口会重试。最后把 XA_ZERO_ENTRY 归为 NULL。业务对象的引用与返回后寿命不由这里建立，见[普通接口导读](../../navigation/P07_普通接口与范围契约.md#7.2_沿一次调用划分责任)。

## 1.11\_普通写入与整段擦除

```c
/**
 * @brief 仓库补充阅读说明：S0 参数检查后取得内部锁，交给存储协议，释放锁并返回结果。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
int mtree_store_range(struct maple_tree *mt, unsigned long index,
		unsigned long last, void *entry, gfp_t gfp)
{
	MA_STATE(mas, mt, index, last);
	int ret = 0;

	trace_ma_write(__func__, &mas, 0, entry);
	if (WARN_ON_ONCE(xa_is_advanced(entry)))
		return -EINVAL;

	if (index > last)
		return -EINVAL;

	mtree_lock(mt);
	ret = mas_store_gfp(&mas, entry, gfp);
	mtree_unlock(mt);

	return ret;
}
```

```c
/**
 * @brief 仓库补充阅读说明：点写入只是起止相同的范围请求。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
int mtree_store(struct maple_tree *mt, unsigned long index, void *entry,
		 gfp_t gfp)
{
	return mtree_store_range(mt, index, index, entry, gfp);
}
```

```c
/**
 * @brief 仓库补充阅读说明：只有请求范围未被占用时插入；保留分配重试及资源销毁路径。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
int mtree_insert_range(struct maple_tree *mt, unsigned long first,
		unsigned long last, void *entry, gfp_t gfp)
{
	MA_STATE(ms, mt, first, last);
	int ret = 0;

	if (WARN_ON_ONCE(xa_is_advanced(entry)))
		return -EINVAL;

	if (first > last)
		return -EINVAL;

	mtree_lock(mt);
retry:
	mas_insert(&ms, entry);
	if (mas_nomem(&ms, gfp))
		goto retry;

	mtree_unlock(mt);
	if (mas_is_err(&ms))
		ret = xa_err(ms.node);

	mas_destroy(&ms);
	return ret;
}
```

```c
/**
 * @brief 仓库补充阅读说明：点插入沿用范围插入的条件。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
int mtree_insert(struct maple_tree *mt, unsigned long index, void *entry,
		 gfp_t gfp)
{
	return mtree_insert_range(mt, index, index, entry, gfp);
}
```

```c
/**
 * @brief 仓库补充阅读说明：定位 index 后擦除整个命中范围，返回旧 entry 而不释放业务对象。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
void *mtree_erase(struct maple_tree *mt, unsigned long index)
{
	void *entry = NULL;

	MA_STATE(mas, mt, index, index);
	trace_ma_op(__func__, &mas);

	mtree_lock(mt);
	entry = mas_erase(&mas);
	mtree_unlock(mt);

	return entry;
}
```

store 的范围覆盖与 insert 的拒绝覆盖是两种业务契约。固定 mas_insert 的冲突路径调用 mas_set_err(mas, -EEXIST)，因此错误名是 EEXIST；上游 insert 文档中的 EEXISTS 拼写不能照抄为有效常量。普通参数检查用 xa_is_advanced，不等于 mt_is_reserved。

mtree_lock 宏直接取 ma_lock，本封装没有外部锁自动分派；已持有该内部锁时也不能再套一层普通写接口。S2 内部 mas_store_gfp/mas_nomem 的资源与重试协议尚未在此完整展开，不能把这个外围锁对理解成整个分配过程绝不放锁。erase 的完整内部算法也仍在后续范围内，此处契约由固定函数文档及调用路径确定。

## 1.12\_向后查找与回绕终止

```c
/**
 * @brief 仓库补充阅读说明：搜索起点或其后可见 entry；成功写回整个命中范围的 last+1。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
void *mt_find(struct maple_tree *mt, unsigned long *index, unsigned long max)
{
	MA_STATE(mas, mt, *index, *index);
	void *entry;
#ifdef CONFIG_DEBUG_MAPLE_TREE
	unsigned long copy = *index;
#endif

	trace_ma_read(__func__, &mas);

	if ((*index) > max)
		return NULL;

	rcu_read_lock();
retry:
	entry = mas_state_walk(&mas);
	if (mas_is_start(&mas))
		goto retry;

	if (unlikely(xa_is_zero(entry)))
		entry = NULL;

	if (entry)
		goto unlock;

	while (mas_is_active(&mas) && (mas.last < max)) {
		entry = mas_next_entry(&mas, max);
		if (likely(entry && !xa_is_zero(entry)))
			break;
	}

	if (unlikely(xa_is_zero(entry)))
		entry = NULL;
unlock:
	rcu_read_unlock();
	if (likely(entry)) {
		*index = mas.last + 1;
#ifdef CONFIG_DEBUG_MAPLE_TREE
		if (MT_WARN_ON(mt, (*index) && ((*index) <= copy)))
			pr_err("index not increased! %lx <= %lx\n",
			       *index, copy);
#endif
	}

	return entry;
}
```

```c
/**
 * @brief 仓库补充阅读说明：后续迭代遇到回绕零直接结束，第一次从零查询应使用 mt_find。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
void *mt_find_after(struct maple_tree *mt, unsigned long *index,
		    unsigned long max)
{
	if (!(*index))
		return NULL;

	return mt_find(mt, index, max);
}
```

index>max 直接返回且不改游标；其他 NULL 返回也不写回。max 限制搜索，而命中范围可以延伸过 max。若 last=ULONG_MAX，成功后 index 回绕零，DEBUG 分支也显式允许这个零值。zero-entry 不作为普通可见对象返回。函数返回时内部 RCU 读侧已经退出，不替载荷取得引用。见[普通接口模块](../../navigation/P07_普通接口与范围契约.md#7.3_结果与证明边界)。
