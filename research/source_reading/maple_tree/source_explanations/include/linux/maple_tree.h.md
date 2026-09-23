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
