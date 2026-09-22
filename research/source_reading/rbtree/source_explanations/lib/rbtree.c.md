---
id: research.source_reading.rbtree.insert_implementation
title: "rbtree.c 插入修复与旋转收尾实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_rbtree.c插入修复与旋转收尾实现

[插入模块导读](../../navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)已经解释 I0～I5：找到空槽，接入红叶，按叔节点颜色推进或旋转。本篇兑现真正改边与染色的语句，保留原教材中文注释和形状图，但函数的分支、变量与访问形式以固定上游为准。

| 关联任务 | 入口 |
| --- | --- |
| 版本身份 | [固定索引](../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界) |
| 上游位置 | [lib/rbtree.c](../../../linux/lib/rbtree.c) |
| 读者场景 | [P26 插入修复](../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P26_Linux红叶接入与插入修复.md#26.3_rbtree_插入后半段_rb_insert_color%28%29_与插入修复) |
| 状态地址与时序 | [模块 3.1](../../navigation/P03_红叶接入与冲突修复导读.md#3.1_空槽与修复游标各归谁所有)、[模块 3.2](../../navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进) |

中文 Doxygen 和中文行内注释为仓库补充、非上游原文；下面函数体去注释后保持固定提交的语句。所有操作依赖合法树和调用者的写侧串行化，不内置参数检查或锁。

## 1.1\_rb\_red\_parent的红色前提

父地址与颜色打包在 __rb_parent_color 中。一般取父需屏蔽低位；这里利用 I1 后以及每次上推时 node 都为红色、颜色位为零的事实，直接把打包值解释为地址。

```c
/**
 * rb_red_parent - 仓库阅读说明：仅对已知红节点提取父地址。
 * @red: 颜色低位为零的有效节点。
 *
 * 不掩码，不允许把黑节点传入后仍当作有效父指针。
 */
static inline struct rb_node *rb_red_parent(struct rb_node *red)
{
	return (struct rb_node *)red->__rb_parent_color;
}
```

实现原理：此函数只读一个节点字段，不确认传入条件。第一次来自新挂红叶；叔红上推后，代码把新的 node 染红再继续循环，保持这个前提。只有父节点已经判红，才可同样调用它取 gparent。

可修改性：若改变节点打包格式或保留位使用规则，必须同步一般取父和这条快捷路径。不能把此处替换成对任意节点都合法的接口承诺；节点还必须满足内核结构体的对齐约定。

## 1.2\_父槽与颜色收尾

旋转已把局部孩子边排好后，I5 还要让外部父节点或树根指向新的局部根。old 原来的父/色必须在覆盖前保存或继承；否则无法恢复外部入口与原黑高角色。

```c
/**
 * __rb_rotate_set_parents - 仓库阅读说明：交接局部根的外部身份。
 * @old: 旋转后下沉的旧局部根。
 * @new: 旋转后上移的新局部根。
 * @root: 整棵树的根槽。
 * @color: old 下沉后的颜色，插入修复这里传红。
 *
 * 调用前局部左右孩子由调用者改好；本函数不是完整左右旋。
 */
static inline void
__rb_rotate_set_parents(struct rb_node *old, struct rb_node *new,
			struct rb_root *root, int color)
{
	struct rb_node *parent = rb_parent(old);
	new->__rb_parent_color = old->__rb_parent_color;
	rb_set_parent_color(old, new, color);
	__rb_change_child(old, new, parent, root);
}
```

实现原理：先读 old 的旧父地址；new 复制 old 原来的整份父色值；old 的父地址和颜色重新打包；最后由[父槽替换](../include/linux/rbtree_augmented.h.md#1.2_替换父节点或根的入口槽)把外部边接到 new。插入修复中 old=gparent 原本黑，new 因此继承黑色；old 则改红。内侧 Case 2 并不调用这个收尾。

可修改性：这里没有修改 new 的左右孩子，也没有修复任意孤立节点。若先覆盖 old 的父色再读取旧值，就会破坏继承；若只改父指针不回接外部槽，整棵树仍可能从旧入口出发。不能把回调插在此函数中再假设它覆盖所有旋转，因为 Case 2 的回调由内部核心单独调用。

## 1.3\_插入修复的两侧分支

读下面代码时，让同一个 node 游标承担“当前可能与父红红冲突的红节点”。它不一定还是最初插入的对象：叔红分支会把它上移到祖父。中文注释里的 uncle 表示叔节点这一角色，上游实际复用 tmp 保存该地址，没有新增独立 uncle 变量。

```c
/**
 * __rb_insert - 仓库阅读说明：从已挂接红叶修复局部红红冲突。
 * @node: 已按排序规则接入的红节点。
 * @root: 原合法红黑树的入口。
 * @augment_rotate: 同步旋转所需的局部增强字段回调。
 *
 * I2 根/父黑退出；I3 叔红上推；I4 内侧预处理；I5 外侧收尾。
 * 无分配、锁、失败返回或损坏树检查。
 */
static __always_inline void
__rb_insert(struct rb_node *node, struct rb_root *root,
	    void (*augment_rotate)(struct rb_node *old, struct rb_node *new))
{
	/*
	 * 新插入节点默认是红色。
	 * rb_red_parent(node) 的语义是：
	 *     node 是红色节点，直接从 __rb_parent_color 中取 parent。
	 */
	struct rb_node *parent = rb_red_parent(node), *gparent, *tmp;

	while (true) {
		/*
		 * 循环不变式：
		 *     node 一定是红色节点。
		 *
		 * 所以每一轮只需要判断：
		 *     1. node 是否已经到根；
		 *     2. parent 是否为黑；
		 *     3. parent 若为红，如何修复红红冲突。
		 */
		if (unlikely(!parent)) {
			/*
			 * 情况 0：
			 *     node 已经成为根节点。
			 *
			 * 根节点必须是黑色。
			 */
			rb_set_parent_color(node, NULL, RB_BLACK);
			break;
		}

		/*
			 * 情况 1：
			 *     parent 是黑色。
			 *
			 * 新插入 node 是红色，不改变黑高；
			 * parent 又是黑色，没有红红冲突；
			 * 所以修复结束。
			 */
		if(rb_is_black(parent))
			break;

		/*
		 * 走到这里说明：
		 *     node   是红色；
		 *     parent 是红色；
		 *
		 * 出现红红冲突。
		 *
		 * parent 不可能是根，因为根必须黑。
		 * 因此一定存在 gparent。
		 */
		gparent = rb_red_parent(parent);

		/*
		 * 下面先处理 parent 是 gparent 左孩子的情况。
		 *
		 *          G
		 *         / \
		 *        P   U
		 *       /
		 *      N
		 */
		tmp = gparent->rb_right;
		if (parent != tmp) {	/* parent == gparent->rb_left */
			if (tmp && rb_is_red(tmp)) {
				/*
				 * Case 1：叔叔节点是红色。
				 *
				 *          G(B)                 G(R)
				 *         /   \                /   \
				 *      P(R)   U(R)    ->    P(B)   U(B)
				 *      /
				 *    N(R)
				 *
				 * 处理：
				 *     parent 染黑；
				 *     uncle  染黑；
				 *     gparent 染红；
				 *
				 * 结果：
				 *     当前局部黑高不变；
				 *     但 gparent 变红后，可能和更上层父节点继续红红冲突。
				 *
				 * 所以：
				 *     node 上移到 gparent；
				 *     继续 while。
				 */
				rb_set_parent_color(tmp, gparent, RB_BLACK);
				rb_set_parent_color(parent, gparent, RB_BLACK);
				node = gparent;
				parent = rb_parent(node);
				rb_set_parent_color(node, parent, RB_RED);
				continue;
			}

			/*
			 * 走到这里：
			 *     uncle 是黑色或 NULL。
			 *
			 * 需要通过旋转解决。
			 */
			tmp = parent->rb_right;
			if (node == tmp) {
				/*
				 * Case 2：内侧插入，左右型。
				 *
				 *          G(B)                 G(B)
				 *         /   \                /   \
				 *      P(R)   U(B)    ->    N(R)   U(B)
				 *        \                  /
				 *        N(R)             P(R)
				 *
				 * 处理：
				 *     先对 parent 左旋；
				 *     把“左右型”转换成“左左型”。
				 *
				 * 注意：
				 *     Case 2 自己不完成最终修复；
				 *     它只是把结构转换成 Case 3。
				 */
				tmp = node->rb_left;
				WRITE_ONCE(parent->rb_right, tmp);
				WRITE_ONCE(node->rb_left, parent);
				if (tmp)
					rb_set_parent_color(tmp, parent,
							    RB_BLACK);
				rb_set_parent_color(parent, node, RB_RED);
				/*
				 * 增强型 rbtree 在旋转后同步增强字段。
				 * 普通 rbtree 这里传 dummy_rotate，最终会被编译器优化掉。
				 */
				augment_rotate(parent, node);
				parent = node;
				tmp = node->rb_right;
			}

			/*
			 * Case 3：外侧插入，左左型。
			 *
			 *          G(B)                 P(B)
			 *         /   \                /   \
			 *      P(R)   U(B)    ->    N(R)   G(R)
			 *      /                            \
			 *    N(R)                           U(B)
			 *
			 * 处理：
			 *     parent 染黑；
			 *     gparent 染红；
			 *     对 gparent 右旋；
			 *
			 * 修复结束。
			 */
			WRITE_ONCE(gparent->rb_left, tmp); /* == parent->rb_right */
			WRITE_ONCE(parent->rb_right, gparent);
			if (tmp)
				rb_set_parent_color(tmp, gparent, RB_BLACK);
			__rb_rotate_set_parents(gparent, parent, root, RB_RED);
			augment_rotate(gparent, parent);
			break;
		} else {
			/*
		 * 镜像分支：
		 *     parent 是 gparent 的右孩子。
		 *
		 *          G
		 *         / \
		 *        U   P
		 *             \
		 *              N
		 */
			tmp = gparent->rb_left;
			if (tmp && rb_is_red(tmp)) {
				/*
				 * Case 1 镜像：
				 *     叔叔红，只变色，上推。
				 */
				rb_set_parent_color(tmp, gparent, RB_BLACK);
				rb_set_parent_color(parent, gparent, RB_BLACK);
				node = gparent;
				parent = rb_parent(node);
				rb_set_parent_color(node, parent, RB_RED);
				continue;
			}

			tmp = parent->rb_left;
			if (node == tmp) {
				/*
				 * Case 2 镜像：右左型。
				 *
				 *          G(B)                 G(B)
				 *         /   \                /   \
				 *      U(B)   P(R)    ->    U(B)   N(R)
				 *             /                        \
				 *           N(R)                       P(R)
				 *
				 * 先对 parent 右旋；
				 * 转成右右型。
				 */
				tmp = node->rb_right;
				WRITE_ONCE(parent->rb_left, tmp);
				WRITE_ONCE(node->rb_right, parent);
				if (tmp)
					rb_set_parent_color(tmp, parent,
							    RB_BLACK);
				rb_set_parent_color(parent, node, RB_RED);
				augment_rotate(parent, node);
				parent = node;
				tmp = node->rb_left;
			}

			/*
			 * Case 3 镜像：右右型。
			 *
			 *          G(B)                 P(B)
			 *         /   \                /   \
			 *      U(B)   P(R)    ->    G(R)   N(R)
			 *               \          /
			 *               N(R)     U(B)
			 *
			 * 对 gparent 左旋；
			 * 修复结束。
			 */
			WRITE_ONCE(gparent->rb_right, tmp); /* == parent->rb_left */
			WRITE_ONCE(parent->rb_left, gparent);
			if (tmp)
				rb_set_parent_color(tmp, gparent, RB_BLACK);
			__rb_rotate_set_parents(gparent, parent, root, RB_RED);
			augment_rotate(gparent, parent);
			break;
		}
	}
}
```

实现原理：I2 先解决根和黑父两种退出。若父红，原合法树保证祖父存在且黑；拿到祖父右孩子后，通过 parent 是否等于这个地址选择左右镜像。I3 给父和叔各加一层黑，再把祖父黑改红，保持局部所有路径的黑数，可能把红红冲突推到更高处。

叔黑时进入 I4/I5。内侧 Case 2 先撤 parent 到 node 的旧边，再反接 node 到 parent，修复移交的中间子树与下沉节点的父色；但祖父的孩子槽和上移 node 的父色尚未完全闭合。随后局部变量 parent 改为 node，tmp 选择新的中间子树，立即进入 Case 3。Case 3 改祖父与新局部根的孩子边，再调用公共收尾完成外部槽和颜色交接。外侧输入直接从 I2 跳到 I5。

**Case 2 回调发生在中间态。** 此时不能把 augment_rotate 当成“全树已经一致”的通知，更不能从回调沿父链随意上行。标准增强方案利用旋转两端的孩子及聚合值维护局部结果；完整回调约束由 P12 的增强单元继续解释。普通路径使用空回调。

中间子树被重新写成黑色也有前提：初始 node 是新红叶，其孩子为空；如果 node 来自 Case 1 上推，它的两个孩子刚被染黑。原红父另一侧的孩子在插入前也必须为黑。因此这里对存在的 tmp 写 RB_BLACK 是使用不变量，不是把任意颜色的子树强行涂黑来掩盖错误。

可修改性：不能把 WRITE_ONCE 改成普通赋值后仍称为同一上游实现；它也不能替代代码已经采用的不成环写序。不能省略 parent=node 或 tmp 的重新选择，否则 Case 3 会按旧局部角色继续改边。空叔是黑叶，叔红分支才允许解引用叔；通过颜色前提成立的 rb_red_parent 不应移到父色判断之前。

这些是功能路径，不是检查器。代码没有为调用者判断“节点已在树中”“重复键允许吗”或“对象是否存活”；关闭或开启调试配置都不能替代这些接口前提。状态地址和通信方向沿[模块关系图](../../navigation/P03_红叶接入与冲突修复导读.md#3.1_空槽与修复游标各归谁所有)，循环各分支对应[模块 I2～I5](../../navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)。

## 1.4\_普通与增广入口

普通插入只需改结构与颜色，增强树还要维护每棵子树的统计信息。因此核心接受 rotate 回调，两个入口选择不同回调后进入同一核心。

```c
/**
 * dummy callbacks - 仓库阅读说明：普通树没有额外聚合字段。
 * propagate/copy 为普通删除包装共用；本节插入只用 rotate。
 */
/*
 * 非增强型 rbtree 操作函数。
 *
 * 这里使用空的增强回调函数，并让编译器在生成
 * rb_insert_color() 和 rb_erase() 函数定义时，
 * 将这些空回调优化掉。
 */
static inline void dummy_propagate(struct rb_node *node, struct rb_node *stop) {}
static inline void dummy_copy(struct rb_node *old, struct rb_node *new) {}
static inline void dummy_rotate(struct rb_node *old, struct rb_node *new) {}

/**
 * rb_insert_color - 仓库阅读说明：普通红黑树插入的修复入口。
 * @node: 刚挂入的红节点。
 * @root: 调用者保护的树。
 */
void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
	__rb_insert(node, root, dummy_rotate);
}

/**
 * __rb_insert_augmented - 仓库阅读说明：接入调用者的旋转回调。
 * @node: 刚挂入的红节点。
 * @root: 树入口。
 * @augment_rotate: 局部聚合信息的旋转回调。
 */
void __rb_insert_augmented(struct rb_node *node, struct rb_root *root,
	void (*augment_rotate)(struct rb_node *old, struct rb_node *new))
{
	__rb_insert(node, root, augment_rotate);
}
```

裁剪说明：本节保留三种空回调定义和两种插入包装，省略导出声明、普通删除使用的 dummy_callbacks 结构实例及其他非插入函数。实现原理是回调参数选择；上游将核心标记为 always_inline，普通路径以消除空回调开销为意图，但具体目标机器码仍要以相应构建结果确认。

可修改性：替换回调必须遵守 Case 2 中间态，不允许它发布“插入完成”、释放节点或再次改这棵树。增强插入前的路径聚合更新也不会因有 rotate 回调就自动完成。对应普通入口的完整观察程序见[教材插入实验](../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P26_Linux红叶接入与插入修复.md#26.3.15_在内核模块中观察五组插入)。

返回[插入导读](../../navigation/P03_红叶接入与冲突修复导读.md#3.3_回调不等于整个操作完成)或[总索引](../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。
