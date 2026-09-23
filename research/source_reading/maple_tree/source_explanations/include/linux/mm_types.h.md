---
id: research.maple_tree.implementation.mm_tree_flags
title: "include/linux/mm_types.h VMA 树模式"
kind: source
status: evolving
domains:
  - linux
  - memory
---

# 第1章\_include/linux/mm\_types.h\_VMA树模式

## 1.1\_固定定义的位置

上游 include/linux/mm_types.h，固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob 6894de506b364fa7f3396146f53216d0d40b80d2。核对[原始头文件](../../../../linux/include/linux/mm_types.h)，回到[树模式导读](../../../navigation/P03_树对象与模式选择.md#3.2_从未发布到受保护使用)及[总索引](../../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)。

## 1.2\_VMA树的三项模式

```c
/**
 * @brief 仓库补充阅读说明：VMA 索引同时选择空洞摘要、外部锁和 RCU 模式。
 * @note 保留固定版本语句；调用前的保护与对象有效性由调用者保证。
 */
#define MM_MT_FLAGS	(MT_FLAGS_ALLOC_RANGE | MT_FLAGS_LOCK_EXTERN | \
			 MT_FLAGS_USE_RCU)
```

宏只产生组合值，不会取得 mmap_lock、分配树节点或保护返回的 VMA。kernel/fork.c 的 mm_init 将该值交给 mt_init_flags，再登记外部锁的 lockdep 描述。ALLOC_RANGE 换取空洞搜索信息，同时付出节点容量和维护成本；LOCK_EXTERN 让地址空间调用者承担锁责任；USE_RCU 决定内核节点处理路径，不等同于 CONFIG_PER_VMA_LOCK 已启用，也不提供 VMA 的长期持有权。各位的唯一解释见[树字段与模式](maple_tree.h.md#1.2_共享树字段与模式位)。

## 1.3\_VMA游标与两种初始化

```c
/**
 * @brief 仓库补充阅读说明：局部游标内嵌状态，tree 指向共享地址空间索引。
 * @note 原样保留固定版本代码；外围锁和对象寿命由调用者建立。
 */
struct vma_iterator {
	struct ma_state mas;
};
```

```c
/**
 * @brief 仓库补充阅读说明：聚合宏显式设置 index，未指定的 last 初始为零。
 * @note 原样保留固定版本代码；外围锁和对象寿命由调用者建立。
 */
#define VMA_ITERATOR(name, __mm, __addr)				\
	struct vma_iterator name = {					\
		.mas = {						\
			.tree = &(__mm)->mm_mt,				\
			.index = __addr,				\
			.node = NULL,					\
			.status = ma_start,				\
		},							\
	}
```

```c
/**
 * @brief 仓库补充阅读说明：函数式初始化调用 mas_init，index 与 last 都设为 addr。
 * @note 原样保留固定版本代码；外围锁和对象寿命由调用者建立。
 */
static inline void vma_iter_init(struct vma_iterator *vmi,
		struct mm_struct *mm, unsigned long addr)
{
	mas_init(&vmi->mas, &mm->mm_mt, addr);
}
```

S1 两种初始化建立 start 入口，但不保证每个字段相同；尚未查询时 last 不表示已经命中的 VMA 末端。mm_struct 的 mm_mt 成员归地址空间所有，多个共享 mm 的线程并不各自复制这棵树。模块入口见[VMA 适配](../../../navigation/P09_VMA游标与边界适配.md#9.2_从地址空间到局部游标)。
