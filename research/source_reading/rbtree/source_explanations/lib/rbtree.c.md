---
id: research.source_reading.rbtree.insert_implementation
title: "rbtree.c 更新与遍历实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_rbtree.c更新与遍历实现

[插入模块导读](../../navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)已经解释 I0～I5：找到空槽，接入红叶，按叔节点颜色推进或旋转。[删除模块](../../navigation/P04_对象摘除与缺黑修复导读.md#4.2_从对象到缺黑父槽)接着追踪 D0～D4 的摘除、缺口和修复。本篇分别兑现两轮操作真正改边与染色的语句，保留原教材中文注释和形状图，但函数的分支、变量与访问形式以固定上游为准。

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

## 1.5\_缺黑修复的四种转换

[删除模块 D3](../../navigation/P04_对象摘除与缺黑修复导读.md#4.3_转换与上推怎样结束)固定了 N/P/S 三个角色：缺黑方向、父节点、兄弟。这里的 node 初始为 NULL，上推后为真实黑节点；兄弟指针由 parent 的另一侧重新读取。先看左侧，再按方向对照右侧。上游位置为本页的 lib/rbtree.c。

以下完整函数保留原教材中文注释和全部 ASCII 图。Case 3 的“先旋转并改色”的图是抽象算法；固定 Linux 实现将相关父色赋值合并到 Case 4，不能把图中的颜色直接套在中间回调现场。中文 Doxygen 与注释为仓库补充、非上游原文。

```c
/**
 * ____rb_erase_color - 修复 parent 下方少一个黑色的路径。
 * rb_erase() 使用的内联版本。
 *
 * 这里特意写成 inline，是为了让普通 rb_erase() 场景下传入的
 * dummy_rotate 回调可以被编译器内联优化掉，避免普通红黑树删除
 * 还额外保留一个无意义的增强回调调用开销。
 *
 * 参数说明：
 *
 * @parent:
 *   删除节点后出现“缺黑”位置的父节点。
 *
 *   注意：
 *     node 本身在本函数里初始化为 NULL。
 *     这表示实际被删除的位置可能已经是空位置。
 *
 *   本函数的核心语义是：
 *
 *     parent 的某个孩子方向少了一个黑节点；
 *     需要从 parent 开始，通过兄弟节点调整颜色和旋转恢复红黑性质。
 *
 * @root:
 *   红黑树根。
 *
 *   删除修复过程中可能发生旋转。
 *   如果旋转影响根节点，需要通过 root 更新整棵树的根。
 *
 * @augment_rotate:
 *   增强型红黑树的旋转回调。
 *
 *   每次发生红黑树旋转后，增强字段也要同步更新。
 *   普通红黑树里，这个回调通常是 dummy_rotate，
 *   编译器可以把它优化掉。
 */
static __always_inline void
____rb_erase_color(struct rb_node *parent, struct rb_root *root,
	void (*augment_rotate)(struct rb_node *old, struct rb_node *new))
{
	/*
	 * node:
	 *   当前缺黑位置。
	 *
	 *   初始为 NULL，因为删除黑叶子或者删除黑节点且无红孩子补位时，
	 *   缺黑位置可能就是一个空孩子槽位。
	 *
	 * sibling:
	 *   node 的兄弟节点。
	 *
	 * tmp1/tmp2:
	 *   临时指针，用于保存 sibling 的左右孩子，或者旋转过程中的中间节点。
	 */
	struct rb_node *node = NULL, *sibling, *tmp1, *tmp2;

	while (true) {
		/*
		 * 循环不变式：
		 *
		 * 1. node 是黑色节点，或者第一次循环时 node 是 NULL。
		 *
		 *    删除修复中的“缺黑”可以理解为：
		 *
		 *      node 这个方向少了一个黑色贡献。
		 *
		 *    node 可以是真实节点，也可以是 NULL。
		 *    NULL 叶子按红黑树规则视为黑色。
		 *
		 * 2. node 不是根节点。
		 *
		 *    因此 parent 一定不是 NULL。
		 *
		 *    如果缺黑上推到已有的黑根，则所有路径统一少一个黑色，
		 *    直接结束，不再执行一次根染黑。
		 *
		 * 3. 所有经过 parent -> node 方向的叶子路径，
		 *    黑节点数量都比其他叶子路径少 1。
		 *
		 *    换句话说：
		 *
		 *      parent 的 node 这一侧缺一个黑节点；
		 *      sibling 那一侧黑高正常。
		 *
		 * 本函数的目标就是补回这个“少的 1 个黑色贡献”。
		 */

		/*
		 * 先假设 node 是 parent 的左孩子。
		 *
		 * 那么 sibling 就是 parent 的右孩子。
		 */
		sibling = parent->rb_right;

		if (node != sibling) {	/* node == parent->rb_left */
			/*
			 * 进入这里表示：
			 *
			 *        P
			 *       / \
			 *      N   S
			 *
			 * N 是缺黑方向；
			 * S 是兄弟节点。
			 */

			if (rb_is_red(sibling)) {
				/*
				 * Case 1：兄弟节点 S 是红色。
				 *
				 * 处理方式：
				 *
				 *   对 parent 左旋。
				 *
				 * 原结构：
				 *
				 *       P(B)              S(B)
				 *      /   \            /    \
				 *     N     S(R)  ->   p(R)  Sr(B)
				 *          /   \      /   \
				 *        Sl(B) Sr(B) N    Sl(B)
				 *
				 * 说明：
				 *
				 *   1. S 是红色时，P 必须是黑色；
				 *      否则会违反“红节点不能有红孩子”。
				 *
				 *   2. S 的两个孩子 Sl/Sr 必须是黑色；
				 *      否则同样违反红黑性质。
				 *
				 *   3. 这个 case 本身不直接完成缺黑修复。
				 *      它只是把“红兄弟”转换成“黑兄弟”场景。
				 *
				 *   4. 旋转后：
				 *        S 继承 P 原来的颜色，变黑；
				 *        P 被染红；
				 *        新的 sibling 变成原来的 Sl。
				 *
				 *      之后继续落入 Case 2 / 3 / 4。
				 */
				tmp1 = sibling->rb_left;

				/*
				 * 左旋第一步：
				 *
				 *   P 的右孩子改成 S 的左孩子 Sl。
				 */
				WRITE_ONCE(parent->rb_right, tmp1);

				/*
				 * 左旋第二步：
				 *
				 *   S 的左孩子改成 P。
				 */
				WRITE_ONCE(sibling->rb_left, parent);

				/*
				 * Sl 的父节点改成 P，并保持黑色。
				 *
				 * 在 Case 1 的红黑性质约束下，Sl 是黑色。
				 */
				rb_set_parent_color(tmp1, parent, RB_BLACK);

				/*
				 * 完成 parent 和 sibling 的父子关系、颜色更新。
				 *
				 * __rb_rotate_set_parents(old, new, root, color)
				 *
				 * 这里 old = parent，new = sibling。
				 *
				 * 效果：
				 *   - sibling 接到 parent 原来的父节点下面；
				 *   - sibling 继承 parent 原来的颜色；
				 *   - parent 的父节点改成 sibling；
				 *   - parent 被设置为 RB_RED。
				 */
				__rb_rotate_set_parents(parent, sibling, root,
							RB_RED);

				/*
				 * 增强型红黑树旋转回调。
				 *
				 * 普通红黑树中通常是 dummy_rotate。
				 */
				augment_rotate(parent, sibling);

				/*
				 * 旋转后，新的兄弟节点变成原来的 Sl。
				 *
				 * 也就是：
				 *
				 *       S
				 *      /
				 *     P
				 *    / \
				 *   N   Sl
				 *
				 * 此时 N 的兄弟是 Sl。
				 */
				sibling = tmp1;
			}

			/*
			 * 到这里，sibling 一定是黑色。
			 *
			 * 接下来检查 sibling 的远侄子 Sr。
			 *
			 * 因为当前 node 是 parent 的左孩子，
			 * 所以：
			 *
			 *   sibling = parent->right
			 *   tmp1    = sibling->right = 远侄子 Sr
			 */
			tmp1 = sibling->rb_right;

			if (!tmp1 || rb_is_black(tmp1)) {
				/*
				 * 远侄子 Sr 是黑色或者 NULL。
				 *
				 * 此时不能直接通过 Case 4 借远侄子的红色来完成修复。
				 * 继续检查近侄子 Sl。
				 */
				tmp2 = sibling->rb_left;

				if (!tmp2 || rb_is_black(tmp2)) {
					/*
					 * Case 2：兄弟 S 是黑色，且两个侄子都是黑色。
					 *
					 * 原结构：
					 *
					 *       (p)             (p)
					 *       / \             / \
					 *      N   S(B)  ->    N   s(R)
					 *         / \             / \
					 *       Sl(B) Sr(B)     Sl(B) Sr(B)
					 *
					 * 处理方式：
					 *
					 *   把 sibling 从黑色染成红色。
					 *
					 * 这等价于：
					 *
					 *   sibling 这一侧减少一个黑色贡献，
					 *   从而和 node 缺黑侧对齐。
					 *
					 * 然后看 parent 的颜色：
					 *
					 *   1. 如果 parent 是红色：
					 *
					 *        把 parent 染黑。
					 *
					 *        parent(R) -> parent(B) 正好补回一个黑色贡献，
					 *        修复结束。
					 *
					 *   2. 如果 parent 是黑色：
					 *
					 *        parent 这一层也开始缺黑。
					 *        缺黑向上递归，把 node 更新为 parent，
					 *        parent 更新为 parent 的父节点，继续循环。
					 *
					 * 特别说明：
					 *
					 *   如果从 Case 1 进入这里，
					 *   parent 会是红色。
					 *   因此 Case 2 可以直接把 parent 染黑并结束。
					 */
					rb_set_parent_color(sibling, parent,
							    RB_RED);

					if (rb_is_red(parent))
						/*
						 * parent 是红色：
						 *
						 * parent 染黑后，补回缺失的黑色贡献。
						 * 删除修复结束。
						 */
						rb_set_black(parent);
					else {
						/*
						 * parent 是黑色：
						 *
						 * sibling 染红只能让 sibling 侧也少一个黑色，
						 * 当前 parent 子树内部平衡了，
						 * 但 parent 整体相对上层少一个黑色。
						 *
						 * 所以把缺黑向上推到 parent。
						 */
						node = parent;
						parent = rb_parent(node);

						/*
						 * 如果 parent 还有父节点，就继续向上修复。
						 *
						 * 如果 parent 为 NULL，说明缺黑已经推到根以上。
						 * 根路径统一减少一个黑色贡献，不再违反红黑树性质。
						 */
						if (parent)
							continue;
					}

					break;
				}

				/*
				 * Case 3：兄弟 S 是黑色，远侄子 Sr 是黑色，
				 *         近侄子 Sl 是红色。
				 *
				 * 当前 node 是 parent 的左孩子，所以结构是：
				 *
				 *       (p)              (p)
				 *       / \              / \
				 *      N   S(B)   ->    N   sl(B)
				 *         / \                \
				 *       sl(R) sr(B)           S(R)
				 *                              \
				 *                              sr(B)
				 *
				 * 处理方式：
				 *
				 *   对 sibling 右旋。
				 *
				 * 目的：
				 *
				 *   把“近侄子红”转换成“远侄子红”。
				 *
				 *   下图的转换后颜色表示抽象的先变色算法，并非下方代码回调时的字段值。
				 *   此实现将父色收尾推迟到 Case 4；此时 sl 仍红、旧 S 仍黑。
				 *   Case 3 本身不是最终修复，
				 *   它只是为 Case 4 做结构转换。
				 *
				 * 注意：
				 *
				 *   p 的颜色可能是红，也可能是黑。
				 *
				 *   如果 p 是红色，右旋后 p 和 sl 可能暂时连续红，
				 *   这会短暂违反性质 4：
				 *
				 *     红节点不能有红孩子。
				 *
				 *   这个临时问题会在随后的 Case 4 中修复：
				 *
				 *     __rb_rotate_set_parents() 会让 sl 继承 p 的颜色，
				 *     并把 p 设置为黑色。
				 *
				 * 转换后的局部结构会进入 Case 4：
				 *
				 *       (p)               (sl)
				 *       / \               /  \
				 *      N   sl     ->     P    S
				 *           \           /      \
				 *            S         N        sr
				 *             \
				 *              sr
				 */
				tmp1 = tmp2->rb_right;

				/*
				 * sibling 的左孩子改成 sl 的右孩子。
				 */
				WRITE_ONCE(sibling->rb_left, tmp1);

				/*
				 * sl 的右孩子改成 sibling。
				 */
				WRITE_ONCE(tmp2->rb_right, sibling);

				/*
				 * parent 的右孩子改成 sl。
				 *
				 * 这一步后，sl 成为缺黑位置的新兄弟，挂在 parent 的右侧。
				 */
				WRITE_ONCE(parent->rb_right, tmp2);

				if (tmp1)
					/*
					 * 如果 sl 原来的右孩子存在，
					 * 它现在变成 sibling 的左孩子，
					 * 父节点要改成 sibling，并保持黑色。
					 */
					rb_set_parent_color(tmp1, sibling,
							    RB_BLACK);

				/*
				 * 增强型红黑树旋转回调：
				 *
				 * sibling 被 tmp2，也就是 sl，旋转替代。
				 */
				augment_rotate(sibling, tmp2);

				/*
				 * 为 Case 4 准备变量。
				 *
				 * tmp1 保存旧 sibling。
				 * sibling 更新为 tmp2，也就是原来的 sl。
				 *
				 * 现在远侄方向指向旧 sibling，但它的实际颜色仍为黑；
				 * 连续进入 Case 4 完成父色交接，不能当作已独立结束的单旋。
				 */
				tmp1 = sibling;
				sibling = tmp2;
			}

			/*
			 * Case 4：兄弟 S 是黑色，远侄子 Sr 是红色。
			 *
			 * 当前 node 是 parent 的左孩子，所以结构是：
			 *
			 *        (p)                  (s)
			 *        / \                  / \
			 *       N   S(B)      ->     P(B) Sr(B)
			 *          / \              / \
			 *        (sl) sr(R)        N  (sl)
			 *
			 * 处理方式：
			 *
			 *   对 parent 左旋，并做颜色调整。
			 *
			 * 调整规则：
			 *
			 *   1. sibling 继承 parent 原来的颜色；
			 *   2. parent 被染成黑色；
			 *   3. 远侄子 Sr 被染成黑色；
			 *   4. 近侄子 Sl 颜色保持不变。
			 *
			 * 修复结果：
			 *
			 *   缺黑被彻底补回；
			 *   红黑性质恢复；
			 *   删除修复结束。
			 */
			tmp2 = sibling->rb_left;

			/*
			 * parent 的右孩子改成 sibling 的左孩子 Sl。
			 */
			WRITE_ONCE(parent->rb_right, tmp2);

			/*
			 * sibling 的左孩子改成 parent。
			 */
			WRITE_ONCE(sibling->rb_left, parent);

			/*
			 * tmp1 在这里表示远侄子 Sr。
			 *
			 * Case 4 的关键之一：
			 *
			 *   Sr(R) -> Sr(B)
			 */
			rb_set_parent_color(tmp1, sibling, RB_BLACK);

			if (tmp2)
				/*
				 * 如果 Sl 存在，它现在变成 parent 的右孩子，
				 * 所以父节点改成 parent。
				 *
				 * Sl 的颜色保持不变。
				 */
				rb_set_parent(tmp2, parent);

			/*
			 * 完成 parent / sibling 之间的左旋父子关系和颜色调整。
			 *
			 * old = parent
			 * new = sibling
			 *
			 * 效果：
			 *   sibling 继承 parent 原来的颜色；
			 *   parent 的父节点改成 sibling；
			 *   parent 被设置为 RB_BLACK。
			 */
			__rb_rotate_set_parents(parent, sibling, root,
						RB_BLACK);

			/*
			 * 增强型红黑树旋转回调。
			 */
			augment_rotate(parent, sibling);

			/*
			 * Case 4 完成后，删除修复结束。
			 */
			break;
		} else {
			/*
			 * 进入这里表示 node 是 parent 的右孩子。
			 *
			 * 这是上面逻辑的完全镜像版本：
			 *
			 *        P
			 *       / \
			 *      S   N
			 *
			 * N 是缺黑方向；
			 * S 是兄弟节点。
			 *
			 * 上半部分处理的是：
			 *
			 *        N 在左，S 在右
			 *
			 * 这里处理的是：
			 *
			 *        S 在左，N 在右
			 */

			sibling = parent->rb_left;

			if (rb_is_red(sibling)) {
				/*
				 * Case 1：兄弟节点 S 是红色。
				 *
				 * 镜像操作：
				 *
				 *   对 parent 右旋。
				 *
				 * 原结构：
				 *
				 *        P(B)              S(B)
				 *       /   \            /    \
				 *     S(R)  N     ->   Sl(B)  p(R)
				 *     / \                     /   \
				 *  Sl(B) Sr(B)              Sr(B)  N
				 *
				 * 目的：
				 *
				 *   把红兄弟转换成黑兄弟，
				 *   然后继续进入 Case 2 / 3 / 4。
				 */
				tmp1 = sibling->rb_right;

				/*
				 * parent 的左孩子改成 sibling 的右孩子 Sr。
				 */
				WRITE_ONCE(parent->rb_left, tmp1);

				/*
				 * sibling 的右孩子改成 parent。
				 */
				WRITE_ONCE(sibling->rb_right, parent);

				/*
				 * Sr 的父节点改成 parent，并保持黑色。
				 */
				rb_set_parent_color(tmp1, parent, RB_BLACK);

				/*
				 * 右旋后：
				 *
				 *   sibling 继承 parent 原来的颜色；
				 *   parent 被染红。
				 */
				__rb_rotate_set_parents(parent, sibling, root,
							RB_RED);

				augment_rotate(parent, sibling);

				/*
				 * 旋转后，新的 sibling 变成原来的 Sr。
				 */
				sibling = tmp1;
			}

			/*
			 * 镜像方向下：
			 *
			 *   node 是 parent 的右孩子；
			 *   sibling 是 parent 的左孩子；
			 *   远侄子是 sibling->left。
			 */
			tmp1 = sibling->rb_left;

			if (!tmp1 || rb_is_black(tmp1)) {
				/*
				 * 远侄子为黑色或 NULL，检查近侄子。
				 *
				 * 镜像方向下：
				 *
				 *   近侄子是 sibling->right。
				 */
				tmp2 = sibling->rb_right;

				if (!tmp2 || rb_is_black(tmp2)) {
					/*
					 * Case 2：兄弟 S 是黑色，两个侄子都是黑色。
					 *
					 * 镜像结构：
					 *
					 *        (p)              (p)
					 *        / \              / \
					 *      S(B) N     ->    s(R) N
					 *      / \              / \
					 *   Sl(B) Sr(B)      Sl(B) Sr(B)
					 *
					 * 处理方式：
					 *
					 *   sibling 染红。
					 *
					 * 如果 parent 是红色：
					 *   parent 染黑，修复结束。
					 *
					 * 如果 parent 是黑色：
					 *   缺黑上推到 parent，继续循环。
					 */
					rb_set_parent_color(sibling, parent,
							    RB_RED);

					if (rb_is_red(parent))
						rb_set_black(parent);
					else {
						node = parent;
						parent = rb_parent(node);
						if (parent)
							continue;
					}

					break;
				}

				/*
				 * Case 3：兄弟 S 是黑色，远侄子 Sl 是黑色，
				 *         近侄子 Sr 是红色。
				 *
				 * 镜像操作：
				 *
				 *   对 sibling 左旋。
				 *
				 * 原结构：
				 *
				 *        (p)                (p)
				 *        / \                / \
				 *      S(B) N      ->     sr(B) N
				 *      / \                /
				 *   sl(B) sr(R)          S(R)
				 *                        /
				 *                      sl(B)
				 *
				 * 目的：
				 *
				 *   图中右侧颜色是抽象目标形态；实际回调时 sr 仍红、旧 S 仍黑，
				 *   父色尚未收尾。把“近侄子红”转换成可由远侧收尾的形状，
				 *   然后进入 Case 4。
				 */
				tmp1 = tmp2->rb_left;

				/*
				 * sibling 的右孩子改成 sr 的左孩子。
				 */
				WRITE_ONCE(sibling->rb_right, tmp1);

				/*
				 * sr 的左孩子改成 sibling。
				 */
				WRITE_ONCE(tmp2->rb_left, sibling);

				/*
				 * parent 的左孩子改成 sr。
				 */
				WRITE_ONCE(parent->rb_left, tmp2);

				if (tmp1)
					rb_set_parent_color(tmp1, sibling,
							    RB_BLACK);

				augment_rotate(sibling, tmp2);

				/*
				 * 为 Case 4 准备：
				 *
				 * tmp1 保存旧 sibling；
				 * sibling 更新为原来的近侄子 sr。
				 */
				tmp1 = sibling;
				sibling = tmp2;
			}

			/*
			 * Case 4：兄弟 S 是黑色，远侄子 Sl 是红色。
			 *
			 * 镜像结构：
			 *
			 *          (p)                  (s)
			 *          / \                  / \
			 *        S(B) N        ->     Sl(B) P(B)
			 *        / \                       / \
			 *     sl(R) (sr)                 (sr) N
			 *
			 * 镜像操作：
			 *
			 *   对 parent 右旋，并做颜色调整。
			 *
			 * 调整规则：
			 *
			 *   1. sibling 继承 parent 原来的颜色；
			 *   2. parent 被染黑；
			 *   3. 远侄子 Sl 被染黑；
			 *   4. 近侄子 Sr 颜色保持不变。
			 *
			 * 修复结束。
			 */
			tmp2 = sibling->rb_right;

			/*
			 * parent 的左孩子改成 sibling 的右孩子 Sr。
			 */
			WRITE_ONCE(parent->rb_left, tmp2);

			/*
			 * sibling 的右孩子改成 parent。
			 */
			WRITE_ONCE(sibling->rb_right, parent);

			/*
			 * tmp1 是远侄子 Sl。
			 *
			 * Case 4 中把远侄子染黑。
			 */
			rb_set_parent_color(tmp1, sibling, RB_BLACK);

			if (tmp2)
				/*
				 * Sr 现在变成 parent 的左孩子，
				 * 父节点改成 parent。
				 *
				 * Sr 颜色保持不变。
				 */
				rb_set_parent(tmp2, parent);

			/*
			 * 完成 parent / sibling 的右旋父子关系和颜色调整。
			 *
			 * sibling 继承 parent 原来的颜色；
			 * parent 被染黑。
			 */
			__rb_rotate_set_parents(parent, sibling, root,
						RB_BLACK);

			augment_rotate(parent, sibling);

			/*
			 * Case 4 完成后，删除修复结束。
			 */
			break;
		}
	}
}
```

**实现原理：** Case 1 改变兄弟的角色而不消除缺黑；Case 2 让两侧一起少一个黑色，红父可变黑抵偿，黑父则把缺口向上交；Case 3 只准备可用的外侧结构；Case 4 完成父色交接并停止。若 Case 2 到达已有黑根，所有根到叶路径统一减一，直接退出，无须额外染根。

进入 Case 1 时，红 S 的两个孩子不仅是黑色，而且非空：缺黑 N 一侧在删除前多一个黑色，兄弟路径必须有对应贡献；红 S 自己不贡献黑色，只能由其下方黑节点提供。这是 tmp1 无 NULL 检查仍可写父色的原因，不能只援引“红节点孩子为黑”——NULL 也黑，但黑高在这里排除了 NULL。

Case 3 回调时旧 S 仍黑、近侄仍红，近侄的 parent 仍可能指向旧 S；沿 parent 回溯可能经过未收尾关系。rotate 只维护孩子结构决定的局部增强量，紧接的 Case 4 才写最终父色。Case 4 的 tmp1 在直接进入时是红远侄，从 Case 3 进入时却是旧黑 S；相同写黑动作对两条路径都有意义，不能把入口注释理解成每个执行点都必须远侄实际为红。

**可修改性：** 不能把 Case 3 后插入“全树已稳定”的通知、任意父链遍历或提前 return。不能删去 Case 2 的向上更新，也不能在两侧黑高尚未对齐时只将根染黑。红兄弟/侄子前提来自合法树；该函数不是损坏树恢复器。左右镜像应保持同一不变式，修改一侧需检查另一侧及增强回调。

本簇复用[角色/状态图](../../navigation/P04_对象摘除与缺黑修复导读.md#4.1_谁拥有地址和颜色)及[时序图](../../navigation/P04_对象摘除与缺黑修复导读.md#4.2_从对象到缺黑父槽)：D3 写边/父色、调用 rotate、重新读取上层兄弟三类箭头均落在本函数。完整应用见[教材缺黑修复](../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.3_rb_erase_color%28%29_删除修复核心)。

## 1.6\_删除入口与黑色位辅助

普通 rb_erase 把结构删除的返回值交给内联修复；增强接口则通过可导出的 __rb_erase_color 进入相同核心。两个下划线和四个下划线属于不同的函数名，不能混作同一个入口。已有[dummy 回调](#1.4_普通与增广入口)不重复展开。

```c
/**
 * rb_erase - 从普通红黑树中删除一个节点
 * @node: 要删除的红黑树节点。
 *        该节点必须已经挂在 @root 所表示的红黑树中。
 *
 * @root: 红黑树根节点。
 *        如果删除过程中根节点发生变化，会通过 @root 更新。
 *
 * 说明：
 *   这是普通红黑树删除接口。
 *
 *   它内部复用增强型红黑树删除函数 __rb_erase_augmented()，
 *   但传入的是 dummy_callbacks。
 *
 *   dummy_callbacks 不维护任何额外增强信息，
 *   编译器通常会把这些空回调优化掉。
 */
void rb_erase(struct rb_node *node, struct rb_root *root)
{
	/*
	 * rebalance 用来保存是否需要进行删除后的颜色修复。
	 *
	 * NULL:
	 *   表示 __rb_erase_augmented() 在删除结构调整阶段
	 *   已经完成了局部颜色修复，不需要再平衡。
	 *
	 * 非 NULL:
	 *   表示删除了一个黑节点，且没有红孩子可以直接染黑补位，
	 *   红黑树黑高被破坏，需要从 rebalance 这个父节点开始修复。
	 */
	struct rb_node *rebalance;

	/*
	 * 执行红黑树删除的“结构调整阶段”。
	 *
	 * 这个函数负责：
	 *
	 *   1. 把 node 从树中摘掉；
	 *   2. 如果 node 有两个孩子，则找到中序后继 successor 替换 node；
	 *   3. 修改父子指针；
	 *   4. 能局部修复颜色的，直接局部修复；
	 *   5. 如果仍然存在缺黑问题，则返回修复起点 rebalance。
	 *
	 * &dummy_callbacks 表示普通红黑树不需要维护增强字段。
	 */
	rebalance = __rb_erase_augmented(node, root, &dummy_callbacks);

	/*
	 * 如果 rebalance 非 NULL，说明删除后产生了“缺黑”。
	 *
	 * 典型场景：
	 *
	 *   删除的是黑节点；
	 *   并且没有红色孩子可以顶上来染黑补位。
	 *
	 * 此时需要调用 ____rb_erase_color() 做红黑树删除修复。
	 *
	 * dummy_rotate 是普通红黑树使用的空旋转回调。
	 * 对普通 rb_erase() 来说，旋转后不需要维护额外增强信息。
	 */
	if (rebalance)
		____rb_erase_color(rebalance, root, dummy_rotate);
}
EXPORT_SYMBOL(rb_erase);
```

以下两个短助手同样来自 lib/rbtree.c。中文 Doxygen 为仓库补充、非上游原文。

```c
/**
 * rb_set_black - 将已知红节点的零颜色位变为黑色位。
 * @rb: 有效红节点；该加法不是对任意颜色反复调用的通用置位。
 */
static inline void rb_set_black(struct rb_node *rb)
{
	rb->__rb_parent_color += RB_BLACK;
}

/**
 * __rb_erase_color - 增强删除使用的可导出修复入口。
 * @parent: 缺黑方向的父节点，非 NULL。
 * @root: 正在修复的树。
 * @augment_rotate: 仅维护局部增强值的同步回调。
 */
void __rb_erase_color(struct rb_node *parent, struct rb_root *root,
	void (*augment_rotate)(struct rb_node *old, struct rb_node *new))
{
	____rb_erase_color(parent, root, augment_rotate);
}
```

**实现原理：** rb_erase 只在 rebalance 非空时进入 D3；NULL 包含红叶、红孩子补位、删除唯一根等情况，不表示业务对象已经被释放。rb_set_black 在 Case 2 的红父分支使用，加一保留合法父地址并置最低位；若对黑节点重复加一，就可能改坏低位编码。导出包装不改变修复算法，只为另一调用路径提供符号。

**可修改性：** 普通删除不能换成任意调用 copy/rotate 来“通知成员减少”；零旋转路径也会成功删除。调用者必须在 D0 确认对象属于此 root，D4 自行更新业务标记和回收协议。参数不合法时没有自动回滚。状态地址与端到端图仍为[删除模块 D0～D4](../../navigation/P04_对象摘除与缺黑修复导读.md#4.2_从对象到缺黑父槽)，本节对应入口调用、条件分支和最终返回箭头。返回[总索引](../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。

## 1.7\_中序端点与父链推进

取消当前对象之前，先找到下一对象的地址。这个地址靠排序关系确定，但找它既可能向孩子走，也可能沿父链回溯。[遍历模块 W1/W2](../../navigation/P05_有序推进与整树销毁导读.md#5.2_同一中序关系如何跨过删除)先解释两条路径，再对照本页上游 lib/rbtree.c 的四个函数。中文 Doxygen 与行内注释为仓库补充、非上游原文，功能语句保持固定版本。

```c
/**
 * rb_first - 仓库阅读说明：返回稳定树的最左节点，空树返回 NULL。
 * @root: 有效根对象，调用者保证树与节点存活。
 */
struct rb_node *rb_first(const struct rb_root *root)
{
	struct rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}

/**
 * rb_last - 仓库阅读说明：返回稳定树的最右节点，空树返回 NULL。
 * @root: 有效根对象，调用者保证树与节点存活。
 */
struct rb_node *rb_last(const struct rb_root *root)
{
	struct rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_right)
		n = n->rb_right;
	return n;
}

/**
 * rb_next - 仓库阅读说明：返回下一中序对象，而非严格更大的键。
 * @node: 仍存活的非 NULL 节点；游离标记只允许提前退出。
 */
struct rb_node *rb_next(const struct rb_node *node)
{
	struct rb_node *parent;

	if (RB_EMPTY_NODE(node))
		return NULL;

	/* 有右子树，先向右一步，再沿左孩子到底。 */
	if (node->rb_right) {
		node = node->rb_right;
		while (node->rb_left)
			node = node->rb_left;
		return (struct rb_node *)node;
	}

	/* 无右子树，向上越过右孩子边，停在首次由左侧到达的父节点。 */
	while ((parent = rb_parent(node)) && node == parent->rb_right)
		node = parent;

	return parent;
}

/**
 * rb_prev - 仓库阅读说明：返回上一中序对象。
 * @node: 仍存活的非 NULL 节点；需要稳定的孩子和父链。
 */
struct rb_node *rb_prev(const struct rb_node *node)
{
	struct rb_node *parent;

	if (RB_EMPTY_NODE(node))
		return NULL;

	/* 有左子树，先向左一步，再沿右孩子到底。 */
	if (node->rb_left) {
		node = node->rb_left;
		while (node->rb_right)
			node = node->rb_right;
		return (struct rb_node *)node;
	}

	/* 无左子树，向上越过左孩子边，停在首次由右侧到达的父节点。 */
	while ((parent = rb_parent(node)) && node == parent->rb_left)
		node = parent;

	return parent;
}
```

**实现原理：** first/last 从根下行，next/prev 先处理各自内侧子树，否则上行越过已完成的一侧。父指针本身保存了“递归本来需要返回到哪里”，所以函数不建立递归栈，也不保存版本号。相等键也有不同节点地址，中序后继可能与当前对象 key 相等。RB_EMPTY_NODE 只识别约定标记；next/prev 没有在访问该宏前检查 node=NULL，不能将空指针当作合法参数。

单次端点或后继操作最坏走一条高 h 的路径；稳定树完整走一轮时，每条相关边只经历常数次下行/上行，因此总计 O(n)、额外游标 O(1)。这不表示每次 next 都是常数时间，也不证明一边改树一边遍历仍具有相同总成本。

**可修改性：** 不能先清当前父色或释放当前对象，再调用 next；必须在 W2 保存下一仍存活对象，W3 才处理当前。父链遍历不在仅向下 RCU 查询的有限路径论证内；这些函数没有取得读锁、引用或快照。调用者让其他写者改树，或者释放保存的下一对象，都会使原协议失效。

[角色图](../../navigation/P05_有序推进与整树销毁导读.md#5.1_拓扑与游标分别保存在哪)的字段读取，以及[中序取消时序](../../navigation/P05_有序推进与整树销毁导读.md#5.2_同一中序关系如何跨过删除)第 1、2 步对应本组函数，第 5 步为调用者游标赋值；删除本身在 W3 调用既有[删除入口](#1.6_删除入口与黑色位辅助)。教材见[P27 有序遍历](../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P27_Linux有序遍历与整树销毁.md#27.2_rbtree_遍历接口)。

## 1.8\_后序推进只跨向未完成部分

中序关注 key 次序，后序关注“孩子完成之后才处理父”。如果准备销毁全部对象，保留排序合法性已不再是每一步的目标，但仍须保证下次计算不会读取已经释放的对象。下面三函数来自同一 lib/rbtree.c；先看[后序销毁时序](../../navigation/P05_有序推进与整树销毁导读.md#5.3_整树销毁为何不用逐个平衡)。

```c
/**
 * rb_left_deepest_node - 仓库阅读说明：按左优先到叶，得到局部第一个后序节点。
 * @node: 非 NULL 的子树根；不是寻找全树深度最大的叶子。
 */
static struct rb_node *rb_left_deepest_node(const struct rb_node *node)
{
	for (;;) {
		if (node->rb_left)
			node = node->rb_left;
		else if (node->rb_right)
			node = node->rb_right;
		else
			return (struct rb_node *)node;
	}
}

/**
 * rb_next_postorder - 仓库阅读说明：返回当前节点之后的后序节点。
 * @node: 当前仍存活节点，允许 NULL 并返回 NULL。
 */
struct rb_node *rb_next_postorder(const struct rb_node *node)
{
	const struct rb_node *parent;
	if (!node)
		return NULL;
	parent = rb_parent(node);

	/* 当前对象的孩子已经完成，接下来只判断父与兄弟关系。 */
	if (parent && node == parent->rb_left && parent->rb_right) {
		/* 当前在左侧且父有右子树，进入右子树的第一个后序位置。 */
		return rb_left_deepest_node(parent->rb_right);
	} else
		/* 右侧已完成或没有右侧，父节点就是下一对象。 */
		return (struct rb_node *)parent;
}

/**
 * rb_first_postorder - 仓库阅读说明：从有效根对象取得第一个后序节点。
 * @root: 有效根对象；内部根指针可空。
 */
struct rb_node *rb_first_postorder(const struct rb_root *root)
{
	if (!root->rb_node)
		return NULL;

	return rb_left_deepest_node(root->rb_node);
}
```

**实现原理：** left_deepest 沿有左取左、无左取右的路径到叶，不比较整树叶子深度。next_postorder 从当前的父出发：当前为左孩子且还有右子树，就下降到右子树第一个后序节点；否则父就是下一个。它不重新读取当前已经完成的左右子树。safe 宏必须在释放当前以前调用它，把结果保存在另一个局部游标中。

**可修改性：** 这是固定拓扑下的完成次序，旋转会改变“哪个父/右子树尚未处理”的答案。不能因为保存了一个下一地址就允许 rb_erase 重排整树。仅计算第一个后序位置时 root 必须有效；next_postorder 允许 NULL，和 next/prev 的输入条件不同。也不能只因对象设置了 RB_CLEAR_NODE 就把它当作本函数可遍历的合法树成员，后序路径没有检查那个标记。

[状态图](../../navigation/P05_有序推进与整树销毁导读.md#5.1_拓扑与游标分别保存在哪)中 pos/n 属于当前调用栈；[销毁时序](../../navigation/P05_有序推进与整树销毁导读.md#5.3_整树销毁为何不用逐个平衡)第 1、2 步是本组读取，释放和最终置空根由调用者负责。宏的确切求值顺序见[后序 safe](../include/linux/rbtree.h.md#1.9_后序safe的两个局部游标)，返回[总索引](../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。

## 1.9\_同键替换的普通与RCU入口

旧对象可能还有使用者，新对象需要进入同一个排序位置。函数负责替换 rb_node 的结构关系，业务 key/载荷、其他索引、增强值和回收由调用者负责。先沿[替换模块 R0～R4](../../navigation/P06_同键替换与旧对象退出导读.md#6.2_一轮替换怎样交接入口)区分地址和排序，再读本页上游 lib/rbtree.c 的两个函数。中文 Doxygen 与注释为仓库补充、非上游原文。

```c
/**
 * rb_replace_node - 在调用者保护下把同键新节点接到旧位置。
 * @victim: 此 root 中的有效成员，替换后不再由根可达。
 * @new: 已准备业务载荷、尚未入树的独立对象内节点。
 * @root: victim 所属的有效根。
 * 不搜索、不比较键、不旋转、不复制业务对象、不清旧标记。
 */
void rb_replace_node(struct rb_node *victim, struct rb_node *new,
		     struct rb_root *root)
{
	struct rb_node *parent = rb_parent(victim);

	/* 只复制嵌入节点的父色与孩子，不复制外层业务载荷。 */
	*new = *victim;

	/* 先让孩子的反向父指针改到替代对象。 */
	if (victim->rb_left)
		rb_set_parent(victim->rb_left, new);
	if (victim->rb_right)
		rb_set_parent(victim->rb_right, new);
	__rb_change_child(victim, new, parent, root);
}

/**
 * rb_replace_node_rcu - 先准备新结构，再以 RCU 方式发布外部入口。
 * @victim: 旧节点；可能还有旧读者持有，不在这里释放。
 * @new: 同键、私有准备好的新对象内节点。
 * @root: 写者已按协议串行化的根；读者采用匹配的读取和寿命协议。
 * 不自动建立读侧、不等待宽限期，也不承诺父链的并发快照。
 */
void rb_replace_node_rcu(struct rb_node *victim, struct rb_node *new,
			 struct rb_root *root)
{
	struct rb_node *parent = rb_parent(victim);

	/* 只复制嵌入节点的父色与孩子，不复制外层业务载荷。 */
	*new = *victim;

	/* 先让孩子的反向父指针改到替代对象。 */
	if (victim->rb_left)
		rb_set_parent(victim->rb_left, new);
	if (victim->rb_right)
		rb_set_parent(victim->rb_right, new);

	/* 最后以 RCU 方式发布父/根入口；新节点的前向孩子已准备好。 */
	__rb_change_child_rcu(victim, new, parent, root);
}
```

**实现原理：** R1 的结构体赋值只覆盖 struct rb_node 的成员，保留 new 外层载荷。R2 改写孩子的 parent 为 new；R3 才修改父/根指向 new 的入口。普通路径调用[父槽替换](../include/linux/rbtree_augmented.h.md#1.2_替换父节点或根的入口槽)，RCU 路径调用[发布助手](../include/linux/rbtree_augmented.h.md#1.5_RCU外部入口发布)。旧 victim 自身的三个字段不清零，它仍可能指着原孩子；这不是继续从旧对象作成员遍历的许可。

RCU 最后发布使采用匹配读取协议的向下读者能从新入口读到已准备字段，不能据此证明孩子与 parent 所有方向同时切换。孩子反向父边在发布前已经改变，旧读者可能继续持有 victim，故父链遍历尤其不能借用这一发布结论。接口也不取得外部引用、不迁移其他索引中的指针。

**可修改性：** 不可把外部发布提前到新节点复制之前，也不可把整个业务结构赋值给 new 来冒充当前语句；那会覆盖调用者刚准备的新载荷。改变排序键时应删除后重新寻找落点。替换增强树还要维护新对象的子树摘要；若新载荷参与统计，需重新计算受影响路径。把 cached 包装遗漏或把旧对象立刻释放，都超出本函数保证。

[角色图](../../navigation/P06_同键替换与旧对象退出导读.md#6.1_地址身份与排序位置)中的结构字段和外部槽在 R1～R3 交接，[时序](../../navigation/P06_同键替换与旧对象退出导读.md#6.2_一轮替换怎样交接入口)中旧读者退出和 R4 回收由调用者执行。本函数不产生“旧对象已无人使用”的通知。教材场景见[P28 同键替换](../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P28_Linux同键替换与旧对象退出.md#28.2_rb_replace_node%28%29_与_rb_replace_node_rcu%28%29)，返回[总索引](../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。
