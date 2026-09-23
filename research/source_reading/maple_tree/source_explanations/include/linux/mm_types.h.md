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
