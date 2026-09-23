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
