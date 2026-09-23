---
id: research.maple_tree.implementation.root_header
title: "include/linux/maple_tree.h 树根与模式"
kind: source
status: evolving
domains:
  - linux
  - memory
---

# 第1章\_include/linux/maple\_tree.h\_树根与模式

## 1.1\_固定位置与调用背景

上游 include/linux/maple_tree.h，固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob c2c11004085e5a98702a2aa8b1671d7a0f5bfe25；[原始头文件](../../../../linux/include/linux/maple_tree.h)保留原文。下列 Doxygen 与标为仓库补充的中文注释由本仓库添加，宏和函数语句保持固定版本。回到[树模式导读](../../../navigation/P03_树对象与模式选择.md#3.2_从未发布到受保护使用)或[总索引](../../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)。

## 1.2\_共享树字段与模式位

```c
/**
 * @brief 仓库补充阅读说明：共享根与模式属于树；union 的锁字段按锁模式解释。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
struct maple_tree {
	union {
		spinlock_t	ma_lock;
		lockdep_map_p	ma_external_lock;
	};
	unsigned int	ma_flags;
	void __rcu      *ma_root;
};
```

```c
/**
 * @brief 仓库补充阅读说明：模式与高度位共享 ma_flags，但应按各自掩码读取。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
#define MT_FLAGS_ALLOC_RANGE	0x01
#define MT_FLAGS_USE_RCU	0x02
#define MT_FLAGS_HEIGHT_OFFSET	0x02
#define MT_FLAGS_HEIGHT_MASK	0x7C
#define MT_FLAGS_LOCK_MASK	0x300
#define MT_FLAGS_LOCK_IRQ	0x100
#define MT_FLAGS_LOCK_BH	0x200
#define MT_FLAGS_LOCK_EXTERN	0x300
#define MT_FLAGS_ALLOC_WRAPPED	0x0800

#define MAPLE_HEIGHT_MAX	31
```

ma_root 是表示入口，空根、index 0 的直接 entry 和编码节点不是同一种对象。头文件对根直存的限制还包括 entry 的低两位。ma_flags 中 ALLOC_RANGE 是创建时选择的布局能力；高度和 RCU 模式有动态使用，不应将整个字段当作不变量。LOCK_EXTERN 是 LOCK_MASK 的完整取值，不能只以任一锁位非零判断外部锁。

## 1.3\_初始化先选择锁模式再建立空根

```c
/**
 * @brief 仓库补充阅读说明：先掩码再比较，只有完整外部模式才返回真。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
static inline bool mt_external_lock(const struct maple_tree *mt)
{
	/* 仓库补充：只比较锁模式位，不能把任意锁位非零当作外部锁。 */
	return (mt->ma_flags & MT_FLAGS_LOCK_MASK) == MT_FLAGS_LOCK_EXTERN;
}
```

```c
/**
 * @brief 仓库补充阅读说明：初始化尚未发布的树，不清理既有节点或 entry。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
static inline void mt_init_flags(struct maple_tree *mt, unsigned int flags)
{
	/* 仓库补充：先决定 union 的锁分支，再建立空根；没有分配节点。 */
	mt->ma_flags = flags;
	if (!mt_external_lock(mt))
		spin_lock_init(&mt->ma_lock);
	rcu_assign_pointer(mt->ma_root, NULL);
}
```

```c
/**
 * @brief 仓库补充阅读说明：默认初始化选择 flags 为零。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
static inline void mt_init(struct maple_tree *mt)
{
	/* 仓库补充：默认模式交回带标志的初始化入口。 */
	mt_init_flags(mt, 0);
}
```

R0 阶段由创建者初始化树：写 ma_flags 决定后续锁解释，内部模式初始化 ma_lock；外部模式跳过它。最后发布空根值。rcu_assign_pointer 在这里写 NULL 并不等于整个树的初始化可与任意读者并发，也没有为树或业务对象分配存储。初始化必须在受保护使用之前完成。

## 1.4\_外部锁登记不是取得锁

```c
/**
 * @brief 仓库补充阅读说明：登记锁依赖描述与检查调用约定，不执行实际加锁。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
#ifdef CONFIG_LOCKDEP
typedef struct lockdep_map *lockdep_map_p;
#define mt_lock_is_held(mt)                                             \
	(!(mt)->ma_external_lock || lock_is_held((mt)->ma_external_lock))

#define mt_write_lock_is_held(mt)					\
	(!(mt)->ma_external_lock ||					\
	 lock_is_held_type((mt)->ma_external_lock, 0))

#define mt_set_external_lock(mt, lock)					\
	(mt)->ma_external_lock = &(lock)->dep_map

#define mt_on_stack(mt)			(mt).ma_external_lock = NULL
#else
typedef struct { /* nothing */ } lockdep_map_p;
#define mt_lock_is_held(mt)		1
#define mt_write_lock_is_held(mt)	1
#define mt_set_external_lock(mt, lock)	do { } while (0)
#define mt_on_stack(mt)			do { } while (0)
#endif

```

CONFIG_LOCKDEP 打开时，ma_external_lock 指向外部锁 dep_map，检查宏查询当前持有情况；空描述时相应表达式可以放行，所以也不能把这项检查当成强制安全机制。关闭时类型为空结构、检查返回常量、登记不执行动作。实际同步责任在两个分支中都存在。kernel/fork.c 的 mm_init 在 mt_init_flags 之后登记 mmap_lock；这两步本身不是为后续整个 VMA 操作持锁。

## 1.5\_RCU模式读写不代替生命周期协议

```c
/**
 * @brief 仓库补充阅读说明：返回当前模式；显式禁用 Maple RCU 的构建分支恒为假。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
static inline bool mt_in_rcu(struct maple_tree *mt)
{
	/* 仓库补充：构建禁用分支优先于运行时模式位。 */
#ifdef CONFIG_MAPLE_RCU_DISABLED
	return false;
#endif
	return mt->ma_flags & MT_FLAGS_USE_RCU;
}
```

```c
/**
 * @brief 仓库补充阅读说明：按锁模式清除 USE_RCU 位，不等待读侧结束。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
static inline void mt_clear_in_rcu(struct maple_tree *mt)
{
	/* 仓库补充：这里只清位和维护锁约定，没有等待读侧完成。 */
	if (!mt_in_rcu(mt))
		return;

	if (mt_external_lock(mt)) {
		WARN_ON(!mt_lock_is_held(mt));
		mt->ma_flags &= ~MT_FLAGS_USE_RCU;
	} else {
		mtree_lock(mt);
		mt->ma_flags &= ~MT_FLAGS_USE_RCU;
		mtree_unlock(mt);
	}
}
```

```c
/**
 * @brief 仓库补充阅读说明：按锁模式设置 USE_RCU 位，不建立业务对象引用。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
static inline void mt_set_in_rcu(struct maple_tree *mt)
{
	/* 仓库补充：这里只设置模式，不建立 VMA 的长期持有权。 */
	if (mt_in_rcu(mt))
		return;

	if (mt_external_lock(mt)) {
		WARN_ON(!mt_lock_is_held(mt));
		mt->ma_flags |= MT_FLAGS_USE_RCU;
	} else {
		mtree_lock(mt);
		mt->ma_flags |= MT_FLAGS_USE_RCU;
		mtree_unlock(mt);
	}
}
```

检查当前状态后可提前返回；需要修改时，外部模式检查持锁约定，内部模式取得并释放树锁。WARN_ON 不是取得锁，也不是失败后自动阻止后续清位的控制流。三个函数都没有 synchronize_rcu，设置与清除的许可来自更大的生命周期协议。

R2 受保护使用期间，更新算法读取模式选择内部节点的处理方式；R3 退休路径见[mas_free](../../../source_explanations/lib/maple_tree.c.md#1.2_退休节点根据模式选择去向)。固定 mm/mmap.c 的 exit_mmap 在最后用户退出及相应外部同步下清位，是特定调用背景，不是可以忽略旧读者的通用捷径。CONFIG_MAPLE_RCU_DISABLED 分支也使 mt_in_rcu 的结果不能只由裸标志位推断。

## 1.6\_构建条件决定数组容量

```c
/**
 * @brief 仓库补充阅读说明：先按构建条件选择数组容量，不能由类型名字推断平台位宽。
 * @note 以下定义或语句保持官方固定版本；不自动构成完整算法保证。
 */
#if defined(CONFIG_64BIT) || defined(BUILD_VDSO32_64)
/* 64bit sizes */
#define MAPLE_NODE_SLOTS	31	/* 256 bytes including ->parent */
#define MAPLE_RANGE64_SLOTS	16	/* 256 bytes */
#define MAPLE_ARANGE64_SLOTS	10	/* 240 bytes */
#define MAPLE_ALLOC_SLOTS	(MAPLE_NODE_SLOTS - 1)
#else
/* 32bit sizes */
#define MAPLE_NODE_SLOTS	63	/* 256 bytes including ->parent */
#define MAPLE_RANGE64_SLOTS	32	/* 256 bytes */
#define MAPLE_ARANGE64_SLOTS	21	/* 240 bytes */
#define MAPLE_ALLOC_SLOTS	(MAPLE_NODE_SLOTS - 2)
#endif /* defined(CONFIG_64BIT) || defined(BUILD_VDSO32_64) */

```

CONFIG_64BIT 或 BUILD_VDSO32_64 选择 31/16/10，否则选择 63/32/21。这里仅定义数组容量；有效槽的终点与当前节点类型另行决定。arange 宏旁 240 bytes 是原注释，不能忽略 metadata 与 ABI 填充后将它当作当前完整结构的 sizeof。

## 1.7\_范围布局与元数据

```c
/**
 * @brief 仓库补充阅读说明：枚举选择节点解释；leaf 槽与非叶槽承载不同对象。
 * @note 以下定义或语句保持官方固定版本；不自动构成完整算法保证。
 */
enum maple_type {
	maple_dense,
	maple_leaf_64,
	maple_range_64,
	maple_arange_64,
};
```

```c
/**
 * @brief 仓库补充阅读说明：两个字节分别参与有效终点与空洞位置的维护。
 * @note 以下定义或语句保持官方固定版本；不自动构成完整算法保证。
 */
struct maple_metadata {
	unsigned char end;
	unsigned char gap;
};
```

```c
/**
 * @brief 仓库补充阅读说明：pivot 为包含式上界，slot 末端与 metadata 在 union 中复用存储。
 * @note 以下定义或语句保持官方固定版本；不自动构成完整算法保证。
 */
struct maple_range_64 {
	struct maple_pnode *parent;
	unsigned long pivot[MAPLE_RANGE64_SLOTS - 1];
	union {
		void __rcu *slot[MAPLE_RANGE64_SLOTS];
		struct {
			void __rcu *pad[MAPLE_RANGE64_SLOTS - 1];
			struct maple_metadata meta;
		};
	};
};
```

```c
/**
 * @brief 仓库补充阅读说明：在范围索引以外为孩子范围增加 gap 数组。
 * @note 以下定义或语句保持官方固定版本；不自动构成完整算法保证。
 */
struct maple_arange_64 {
	struct maple_pnode *parent;
	unsigned long pivot[MAPLE_ARANGE64_SLOTS - 1];
	void __rcu *slot[MAPLE_ARANGE64_SLOTS];
	unsigned long gap[MAPLE_ARANGE64_SLOTS];
	struct maple_metadata meta;
};
```

pivot 字段类型是 unsigned long，并非固定 uint64_t。range 中 slot 与 pad/meta 是同一 union 的两种视角，不应把 metadata 当成所有槽以外又多出的存储。arange 独立保存 gap 和 meta，更多信息降低相应槽数。实际槽内容由节点类型决定：叶是 entry，非叶是孩子节点的编码入口。内核节点的完整状态与有效槽检测还需读具体算法，不能对容量数组做无条件遍历。

## 1.8\_容器复用与节点资源

```c
/**
 * @brief 仓库补充阅读说明：操作预分配节点的资源管理布局。
 * @note 以下定义或语句保持官方固定版本；不自动构成完整算法保证。
 */
struct maple_alloc {
	unsigned long total;
	unsigned char node_count;
	unsigned int request_count;
	struct maple_alloc *slot[MAPLE_ALLOC_SLOTS];
};
```

```c
/**
 * @brief 仓库补充阅读说明：同一容器根据节点类型与生命周期解释为不同成员。
 * @note 以下定义或语句保持官方固定版本；不自动构成完整算法保证。
 */
struct maple_node {
	union {
		struct {
			struct maple_pnode *parent;
			void __rcu *slot[MAPLE_NODE_SLOTS];
		};
		struct {
			void *pad;
			struct rcu_head rcu;
			struct maple_enode *piv_parent;
			unsigned char parent_slot;
			enum maple_type type;
			unsigned char slot_len;
			unsigned int ma_flags;
		};
		struct maple_range_64 mr64;
		struct maple_arange_64 ma64;
		struct maple_alloc alloc;
	};
};
```

parent/slot、mr64、ma64、alloc 和 rcu 相关成员重叠，不能同时当作有效独立状态。节点类型在树中保持其约定，退出与 RCU 保护完成前不能任意换类型；此处不展开完整节点退休算法。外层 ma_flags 与这里退休视图的 ma_flags 也属于不同存储，不能凭相同字段名混为一份全局状态。

布局的教学入口见[P38](../../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P38_Maple节点中的范围与空洞.md#38.3_同一块节点存储有几种解释)，固定容量与容器对应关系由[节点导读](../../../navigation/P04_节点布局与范围分区.md#4.2_按问题读取布局)组织。这里的定义仍以本文件 1.1 的固定头文件为证据。

## 1.9\_错误载荷与独立状态

```c
/**
 * @brief 仓库补充阅读说明：节点地址、类型掩码和小值保留范围分别使用。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
#define MAPLE_NODE_MASK		255UL
#define MAPLE_NODE_TYPE_MASK	0x0F
#define MAPLE_NODE_TYPE_SHIFT	0x03

#define MAPLE_RESERVED_RANGE	4096
```

```c
/**
 * @brief 仓库补充阅读说明：先转换为 unsigned long，再移位编码错误号。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
#define MA_ERROR(err) \
		((struct maple_enode *)(((unsigned long)err << 2) | 2UL))
```

```c
/**
 * @brief 仓库补充阅读说明：当前固定实现只比较 status 是否等于 ma_error。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static inline bool mas_is_err(struct ma_state *mas)
{
	return mas->status == ma_error;
}
```

错误写入者是[mas_set_err](../../../source_explanations/lib/maple_tree.c.md#1.6_保留entry与操作错误分别判断)，它同时写 node 与 status；查询者不能只取一个字段就假设状态一致。头文件早期文字中的右移说法不替代当前宏的左移语句；原始文件保留原注释，本说明明确区分注释与执行代码。start/none/pause 等状态由 maple_status 表达，不能作为同一种 node 指针编码列表背诵。

## 1.10\_操作状态与初始化

```c
/**
 * @brief 仓库补充阅读说明：状态枚举独立于 node 载荷，不能由返回 NULL 推断唯一状态。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
enum maple_status {
	ma_active,
	ma_start,
	ma_root,
	ma_none,
	ma_pause,
	ma_overflow,
	ma_underflow,
	ma_error,
};
```

```c
/**
 * @brief 仓库补充阅读说明：写入路径分类是另一状态轴，不是游标 status 的别名。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
enum store_type {
	wr_invalid,
	wr_new_root,
	wr_store_root,
	wr_exact_fit,
	wr_spanning_store,
	wr_split_store,
	wr_rebalance,
	wr_append,
	wr_node_store,
	wr_slot_store,
};
```

```c
/**
 * @brief 仓库补充阅读说明：请求范围、节点位置、状态与资源分别存放。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
struct ma_state {
	struct maple_tree *tree;	/* The tree we're operating in */
	unsigned long index;		/* The index we're operating on - range start */
	unsigned long last;		/* The last index we're operating on - range end */
	struct maple_enode *node;	/* The node containing this entry */
	unsigned long min;		/* The minimum index of this node - implied pivot min */
	unsigned long max;		/* The maximum index of this node - implied pivot max */
	struct maple_alloc *alloc;	/* Allocated nodes for this operation */
	enum maple_status status;	/* The status of the state (active, start, none, etc) */
	unsigned char depth;		/* depth of tree descent during write */
	unsigned char offset;
	unsigned char mas_flags;
	unsigned char end;		/* The end of the node */
	enum store_type store_type;	/* The type of store needed for this operation */
};
```

```c
/**
 * @brief 仓库补充阅读说明：为新变量初始化状态，未指定聚合成员按 C 规则归零。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
#define MA_STATE(name, mt, first, end)					\
	struct ma_state name = {					\
		.tree = mt,						\
		.index = first,						\
		.last = end,						\
		.node = NULL,						\
		.status = ma_start,					\
		.min = 0,						\
		.max = ULONG_MAX,					\
		.alloc = NULL,						\
		.mas_flags = 0,						\
		.store_type = wr_invalid,				\
	}
```

```c
/**
 * @brief 仓库补充阅读说明：清零整份操作存储再设初值，不负责释放其此前可能拥有的资源。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
static inline void mas_init(struct ma_state *mas, struct maple_tree *tree,
			    unsigned long addr)
{
	memset(mas, 0, sizeof(struct ma_state));
	mas->tree = tree;
	mas->index = mas->last = addr;
	mas->max = ULONG_MAX;
	mas->status = ma_start;
	mas->node = NULL;
}
```

MA_STATE 建立 S0 输入范围，普通与高级 API 对后续字段的维护并不相同。alloc 可能表示资源请求或分配管理，不是一个 status 枚举；store_type 的选择属于写入路径，本节不展开完整写入分类。

## 1.11\_重置与重新指定范围

```c
/**
 * @brief 仓库补充阅读说明：读取独立 status。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
static inline bool mas_is_active(struct ma_state *mas)
{
	return mas->status == ma_active;
}
```

```c
/**
 * @brief 仓库补充阅读说明：只写 status 与 node，保留 index/last 和资源字段。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
static __always_inline void mas_reset(struct ma_state *mas)
{
	mas->status = ma_start;
	mas->node = NULL;
}
```

```c
/**
 * @brief 仓库补充阅读说明：当前已定位范围的局部修改，警告不等于拒绝。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
static inline void __mas_set_range(struct ma_state *mas, unsigned long start,
		unsigned long last)
{
	/* Ensure the range starts within the current slot */
	MAS_WARN_ON(mas, mas_is_active(mas) &&
		   (mas->index > start || mas->last < start));
	mas->index = start;
	mas->last = last;
}
```

```c
/**
 * @brief 仓库补充阅读说明：先 reset 再设置输入区间，下一次需要重走。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
static inline
void mas_set_range(struct ma_state *mas, unsigned long start, unsigned long last)
{
	mas_reset(mas);
	__mas_set_range(mas, start, last);
}
```

```c
/**
 * @brief 仓库补充阅读说明：把点查询转换为起止相同的范围。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
static inline void mas_set(struct ma_state *mas, unsigned long index)
{

	mas_set_range(mas, index, index);
}
```

S5 的 reset 保留当前位置，可能再次找到同一对象；S6 的 set 则同时改变请求索引。__mas_set_range 的诊断只检查当前 active 时新 start 是否落在原 slot 区间，不替调用者证明全部输入和资源前置条件。回到[游标导读](../../../navigation/P06_操作游标与暂停继续.md#6.2_沿一次遍历追踪状态)。
