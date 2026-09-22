---
id: research.source_reading.rbtree.parent_implementation
title: "rbtree_augmented.h 的父色与入口槽实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_rbtree\_augmented.h的父色与入口槽实现

一个节点记录“谁是我的父”，父节点又记录“谁是我的孩子”，这两种存储地址不会互相自动更新。插入 I5 的公共收尾以及后续删除都依赖本篇两种小操作。先看[模块的地址关系](../../../navigation/P03_红叶接入与冲突修复导读.md#3.1_空槽与修复游标各归谁所有)，再核对赋值方向。

| 关联任务 | 入口 |
| --- | --- |
| 固定版本 | [总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界) |
| 上游位置 | [include/linux/rbtree_augmented.h](../../../../linux/include/linux/rbtree_augmented.h) |
| 插入的调用位置 | [公共收尾](../../lib/rbtree.c.md#1.2_父槽与颜色收尾) |
| 另一个使用场景 | [P11 结构删除](../../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除_遍历与替换.md#11.2_rbtree_删除前半段_rb_erase%28%29_与结构删除) |

中文 Doxygen 与中文行内注释为仓库补充、非上游原文。函数体保持固定语句；这里是内部辅助层，不是供调用者跳过插入/删除接口直接修坏树的公共契约。

## 1.1\_父色打包写入

一般取父会去掉低两位，颜色取最低一位。该版本红为 0、黑为 1；Linux 的节点对齐为打包提供低位空间。这里一次写入的是新父地址加给定颜色，旧父/色都会被覆盖。

```c
/**
 * rb_set_parent_color - 仓库阅读说明：同时替换节点的父地址与颜色。
 * @rb: 要改写的有效节点。
 * @p: 新父节点，整树根时为 NULL。
 * @color: 该版本的红 0 或黑 1。
 *
 * 不修改 p 的孩子槽，不检查地址对齐或参数是否合法。
 */
static inline void rb_set_parent_color(struct rb_node *rb,
				       struct rb_node *p, int color)
{
	rb->__rb_parent_color = (unsigned long)p + color;
}
```

实现原理：I1 挂接用红色零位初始化；I3 在父子关系不变时重写颜色；I5 则同时改变下沉节点的父与颜色。它只修改 rb 自身的打包字段，没有替调用者更新任何外部入口。不要把打包整型加法解释为 C 指针的“加一个节点”。

可修改性：传错颜色、未对齐地址或改变打包格式都会影响所有取父与测色路径。若要保留旧颜色，应使用相应的另一接口，不能直接调用本函数却期待它自动保留；相反，只想变色却给错父地址也会破坏遍历。

## 1.2\_替换父节点或根的入口槽

调用者已经知道 old 占哪个外部位置，现在只把那个位置改指向 new。new 可以是有效节点，也可以是 NULL，表示把这个槽清空。这正是插入旋转和删除回接都需要的公共动作。

```c
/**
 * __rb_change_child - 仓库阅读说明：替换父节点或根保存的一条入口边。
 * @old: 原来占据父孩子槽或根槽的节点。
 * @new: 新的槽值，允许 NULL。
 * @parent: old 的真实父节点；NULL 表示 old 为整树根。
 * @root: 整棵树的根对象，仅根替换路径使用。
 *
 * old 必须是 parent 的左孩子或右孩子之一。
 * 只改 parent->left/right 或 root->rb_node，不改 old/new 的
 * parent、color、left、right；新节点的父色由调用者另行设置。
 */
static inline void
__rb_change_child(struct rb_node *old, struct rb_node *new,
		  struct rb_node *parent, struct rb_root *root)
{
	/* 仓库补充：parent 非空时，old 必须是它的一个真实孩子。 */
	if (parent) {
		if (parent->rb_left == old)
			WRITE_ONCE(parent->rb_left, new);
		else
			WRITE_ONCE(parent->rb_right, new);
	} else
		/* 仓库补充：old 原为整树根，因此改根槽而非父孩子槽。 */
		WRITE_ONCE(root->rb_node, new);
}
```

实现原理：父非空时先比较左孩子；左边不是 old，就按调用前提写右边。代码没有再次检查右孩子是否真的是 old。所以这个 else 是对已成立前提的使用，不是“无论 old 来自哪里都能正确找到槽”的搜索。父为空才进入根替换。WRITE_ONCE 作用于这一条共享入口边，不把整次旋转变成原子动作。

可修改性：把 unrelated 节点当 old 传入会覆盖错误的右孩子；只调用此函数而不维护 new 的父色，向下与向上关系可能矛盾。若参数 parent=NULL 但 old 并非根，则会直接覆盖根槽。插入 I5 由[收尾函数](../../lib/rbtree.c.md#1.2_父槽与颜色收尾)先完成身份交接再调用它；完整时序见[模块 I5](../../../navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)，对应最后一条外部槽写入箭头。

两函数都不取得锁、不分配/释放对象，也不执行动态树验证。调用者必须先持有该树要求的写侧保护。返回[插入导读](../../../navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)或[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。
