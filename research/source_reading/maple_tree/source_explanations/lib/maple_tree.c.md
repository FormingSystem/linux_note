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
