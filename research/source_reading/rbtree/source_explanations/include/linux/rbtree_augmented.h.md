---
id: research.source_reading.rbtree.parent_implementation
title: "rbtree_augmented.h 父槽与结构删除实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_rbtree\_augmented.h父槽与结构删除实现

一个节点记录“谁是我的父”，父节点又记录“谁是我的孩子”，这两种存储地址不会互相自动更新。先看[插入模块的地址关系](../../../navigation/P03_红叶接入与冲突修复导读.md#3.1_空槽与修复游标各归谁所有)，核对两种小操作的赋值方向；再沿[删除周期](../../../navigation/P04_对象摘除与缺黑修复导读.md#4.2_从对象到缺黑父槽)追踪后继移位和缺黑父槽。

| 关联任务 | 入口 |
| --- | --- |
| 固定版本 | [总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界) |
| 上游位置 | [include/linux/rbtree_augmented.h](../../../../linux/include/linux/rbtree_augmented.h) |
| 插入的调用位置 | [公共收尾](../../lib/rbtree.c.md#1.2_父槽与颜色收尾) |
| 另一个使用场景 | [P11 结构删除](../../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.2_rbtree_删除前半段_rb_erase%28%29_与结构删除) |

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

## 1.3\_结构摘除与缺黑父槽

取消的是业务对象 node，移动的是同一棵树中的 successor；copy 只复制增强值，不复制业务对象。先在[删除模块 D1/D2](../../../navigation/P04_对象摘除与缺黑修复导读.md#4.2_从对象到缺黑父槽)定位两种地址，再读以下固定函数。上游位置仍为本页表格的 include/linux/rbtree_augmented.h。

以下中文 Doxygen 与行内说明为仓库补充、非上游原文。保留原教材逐步图解，函数语句与固定对象一致。注释中的“原位置”必须区分两种后继：深后继留下 parent.left；直接右孩子移到 node 位置后，留下的是 successor.right，所以 parent 设为 successor。删除唯一黑根得到空树时，parent=NULL，返回 NULL，不进入颜色修复。

```c
/**
 * __rb_erase_augmented - 删除增强型红黑树中的一个节点
 * @node:    要删除的红黑树节点。
 *           该节点必须已经链接在 @root 表示的红黑树中。
 *
 * @root:    红黑树根。
 *           如果删除的是根节点，或者替换后根节点发生变化，
 *           会通过 @root 更新 root->rb_node。
 *
 * @augment: 增强型红黑树回调。
 *           普通红黑树不需要维护额外字段，会传入 dummy callbacks。
 *
 *           copy(old, new):
 *             当 new 替代 old 的树中位置时，
 *             把 old 的增强信息复制给 new。
 *
 *           propagate(node, stop):
 *             从 node 开始向上更新增强信息；
 *             stop 不包含在更新范围内；NULL 表示没有更低的停止界。
 *             回调也可在增强值不变时提前停止，不保证每次都走到根。
 *
 *           rotate(old, new):
 *             删除后的颜色修复阶段如果发生旋转，用于同步增强信息。
 *
 * 返回：
 *   NULL:
 *     删除结构调整阶段已经完成局部颜色处理，
 *     不需要再进入 __rb_erase_color()。
 *
 *   非 NULL:
 *     返回删除修复的起始父节点。
 *     表示某条路径少了一个黑色贡献，需要后续做红黑树删除修复。
 */
static __always_inline struct rb_node *
__rb_erase_augmented(struct rb_node *node,
		     struct rb_root *root,
		     const struct rb_augment_callbacks *augment)
{
	/*
	 * child = node 的右孩子。
	 * tmp   = node 的左孩子。
	 *
	 * 注意：
	 *   这里 tmp 只是临时变量。
	 *   后面它会被复用为其他含义，阅读时必须看最近一次赋值。
	 *
	 * 删除分两层看：
	 *
	 *   第一层：BST 结构删除
	 *     - 0 个孩子 / 1 个孩子：直接用孩子替换 node；
	 *     - 2 个孩子：找中序后继 successor 替换 node。
	 *
	 *   第二层：红黑树颜色处理
	 *     - 如果实际被摘掉的位置损失了黑色贡献，
	 *       并且没有红孩子可以染黑补位，
	 *       就返回 rebalance 起点给 __rb_erase_color()。
	 */
	struct rb_node *child = node->rb_right;
	struct rb_node *tmp = node->rb_left;
	struct rb_node *parent, *rebalance;
	unsigned long pc;

	if (!tmp) {
		/*
		 * Case 1-A：node 没有左孩子。
		 *
		 * 这里 tmp 仍然表示 node->rb_left。
		 *
		 * 结构可能是：
		 *
		 *        node
		 *          \
		 *          child
		 *
		 * 也可能是：
		 *
		 *        node
		 *
		 * BST 删除上，直接用 child 替换 node。
		 *
		 * 红黑树颜色上：
		 *
		 *   1. 如果 child 存在：
		 *
		 *          node(B)
		 *             \
		 *            child(R)
		 *
		 *      这是合法红黑树反推出来的。
		 *      因为 node 只有一个真实孩子，另一侧是 NIL(B)，
		 *      所以唯一真实孩子只能是红色，node 只能是黑色。
		 *
		 *      删除 node 后，让 child 继承 node 的 parent + color。
		 *      等价于 child(R) -> child(B)，黑高局部恢复。
		 *
		 *   2. 如果 child 不存在：
		 *
		 *      删除的是叶子 node。
		 *      删除红叶子不破坏黑高；
		 *      删除黑叶子会造成缺黑，需要从 parent 开始修复。
		 */
		pc = node->__rb_parent_color;
		parent = __rb_parent(pc);

		/*
		 * 用 child 替换 node 在父节点中的位置。
		 *
		 * 只修改 parent/root 指向孩子的边：
		 *
		 *        parent -> node
		 *
		 * 改成：
		 *
		 *        parent -> child
		 *
		 * child 的 parent/color 由后续代码负责设置。
		 */
		__rb_change_child(node, child, parent, root);

		if (child) {
			/*
			 * child 顶替 node。
			 *
			 * child 继承 node 的 parent + color。
			 * 在这个分支中，node 必然是黑色，
			 * 所以 child 从红色变成黑色，局部黑高已经补回。
			 */
			child->__rb_parent_color = pc;
			rebalance = NULL;
		} else
			/*
			 * 删除叶子 node。
			 *
			 * 如果 node 是黑色，删除后这条路径少一个黑节点，
			 * 需要从 parent 开始删除修复。
			 *
			 * 如果 node 是红色，删除后不影响黑高。
			 */
			rebalance = __rb_is_black(pc) ? parent : NULL;

		/*
		 * node 被 child/NULL 替换后，
		 * parent 的子树结构发生变化。
		 *
		 * 增强字段从 parent 开始向上更新。
		 */
		tmp = parent;
	} else if (!child) {
		/*
		 * Case 1-B：node 有左孩子，但没有右孩子。
		 *
		 * 结构：
		 *
		 *        node
		 *        /
		 *      tmp
		 *
		 * BST 删除上，直接用 node->left 替换 node。
		 *
		 * 红黑树颜色上，合法结构只能是：
		 *
		 *        node(B)
		 *        /
		 *      tmp(R)
		 *
		 * 删除 node 后，让 tmp 继承 node 的 parent + color。
		 * 等价于 tmp(R) -> tmp(B)，局部黑高恢复。
		 */
		tmp->__rb_parent_color = pc = node->__rb_parent_color;
		parent = __rb_parent(pc);

		/*
		 * 用 node->left 替换 node。
		 */
		__rb_change_child(node, tmp, parent, root);

		/*
		 * tmp 已经继承 node 的黑色，
		 * 不需要进入删除颜色修复。
		 */
		rebalance = NULL;

		/*
		 * parent 的子树结构发生变化，
		 * 增强字段从 parent 开始更新。
		 */
		tmp = parent;
	} else {
		/*
		 * Case 2 / Case 3：node 同时有左孩子和右孩子。
		 *
		 * 这是 BST 删除里的“双孩子删除”。
		 *
		 * 不能仅改入口而丢下另一个孩子；直接提某个孩子还须另行回接，
		 * 否则可能破坏 BST 的中序顺序。
		 *
		 * 正确做法：
		 *
		 *   1. 找 node 的中序后继 successor；
		 *      successor 是 node 右子树中的最小节点。
		 *
		 *   2. 用 successor 接管 node 的树中位置；
		 *      这样能保证：
		 *
		 *          node 左子树全部 <= successor（唯一键时严格小于）
		 *          successor <= node 右子树剩余节点
		 *
		 *      中序遍历顺序不乱。
		 *
		 *   3. successor 从原位置被拿走后，
		 *      它原来的位置相当于发生一次“至多一个孩子”的删除。
		 *
		 *   4. 后续再根据 successor 原位置损失的颜色贡献，
		 *      判断是否需要进入红黑树删除修复。
		 *
		 * 注意：
		 *   这里的 child = node->rb_right。
		 *   successor 初始设为 child，也就是先假设右孩子就是后继。
		 */
		struct rb_node *successor = child, *child2;

		/*
		 * 这里 tmp 被重新赋值。
		 *
		 * 之前 tmp 表示 node->rb_left；
		 * 现在 tmp 表示：
		 *
		 *     child->rb_left
		 *     也就是 node->rb_right->rb_left
		 *
		 * 它用来判断：
		 *
		 *   Case 2:
		 *     node 的右孩子没有左孩子，
		 *     所以后继就是 node->rb_right 本身。
		 *
		 *   Case 3:
		 *     node 的右孩子还有左子树，
		 *     所以后继在右子树更深处，需要沿左链查找。
		 */
		tmp = child->rb_left;
		if (!tmp) {
			/*
			 * Case 2：node 的中序后继就是 node 的右孩子。
			 *
			 * 条件：
			 *
			 *     successor = node->rb_right
			 *     successor->rb_left == NULL
			 *
			 * 删除前：
			 *
			 *        node
			 *        /  \
			 *       x    successor
			 *              \
			 *              child2
			 *
			 * 删除后：
			 *
			 *        successor
			 *        /       \
			 *       x        child2
			 *
			 * 这个 case 的关键点：
			 *
			 *   1. successor 本来就是 node->rb_right。
			 *
			 *   2. 因此不需要设置：
			 *
			 *          successor->rb_right = child;
			 *
			 *      因为 child 本身就是 successor。
			 *      如果这么写，会变成 successor->rb_right = successor，
			 *      形成自环。
			 *
			 *   3. 这里只需要记录：
			 *
			 *          parent = successor;
			 *          child2 = successor->rb_right;
			 *
			 *      parent 表示 successor 原位置的父修复点。
			 *      在 Case 2 中，successor 原位置和新位置贴在一起，
			 *      所以 parent 就是 successor 自己。
			 *
			 *   4. 后面的公共代码会继续完成：
			 *
			 *          successor->rb_left = node->rb_left;
			 *          用 successor 替换 node；
			 *          successor 继承 node 的 parent + color。
			 */
			parent = successor;
			child2 = successor->rb_right;

			/*
			 * successor 将接管 node 的树中位置。
			 *
			 * 对增强型红黑树来说，successor 在逻辑上替代 node，
			 * 所以需要复制 node 的增强信息。
			 */
			augment->copy(node, successor);
		} else {
			/*
			 * Case 3：node 的中序后继在右子树更深处。
			 *
			 * 条件：
			 *
			 *     node->rb_right->rb_left != NULL
			 *
			 * 也就是说，node 的右孩子不是右子树最小节点。
			 * 必须继续沿左链查找最左节点。
			 *
			 * 删除前：
			 *
			 *        node
			 *        /  \
			 *       x    child
			 *            /
			 *          ...
			 *          /
			 *       parent
			 *        /
			 *   successor
			 *        \
			 *        child2
			 *
			 * 删除后：
			 *
			 *        successor
			 *        /       \
			 *       x        child
			 *                /
			 *              ...
			 *              /
			 *           parent
			 *            /
			 *         child2
			 *
			 * 这个 case 和 Case 2 的区别：
			 *
			 *   1. successor 不是 node->rb_right。
			 *
			 *   2. 必须先把 successor 从原位置摘掉：
			 *
			 *          parent->rb_left = child2;
			 *
			 *   3. successor 替换 node 后，必须接管 node 的右子树：
			 *
			 *          successor->rb_right = child;
			 *
			 *      否则 node 的右子树会丢失。
			 */
			do {
				/*
				 * 沿 node 右子树一路向左，
				 * 找到右子树中的最小节点 successor。
				 *
				 * 循环结束后：
				 *
				 *   successor = node 右子树中的最左节点；
				 *   parent    = successor 原位置上的父节点；
				 *   successor->rb_left == NULL。
				 */
				parent = successor;
				successor = tmp;
				tmp = tmp->rb_left;
			} while (tmp);

			/*
			 * successor 是最左节点，所以没有左孩子。
			 * 但它可能有右孩子 child2。
			 *
			 * successor 从原位置被拿走后，
			 * child2 会顶替 successor 原来的位置。
			 */
			child2 = successor->rb_right;

			/*
			 * 把 successor 从原位置摘掉。
			 *
			 * 原来：
			 *
			 *        parent
			 *        /
			 *   successor
			 *        \
			 *       child2
			 *
			 * 改成：
			 *
			 *        parent
			 *        /
			 *      child2
			 */
			WRITE_ONCE(parent->rb_left, child2);

			/*
			 * successor 将替代 node。
			 *
			 * 因为 Case 3 中 successor != child，
			 * 所以必须让 successor 接管 node 的右子树 child。
			 */
			WRITE_ONCE(successor->rb_right, child);
			rb_set_parent(child, successor);

			/*
			 * successor 替代 node 的逻辑位置，
			 * 复制 node 的增强信息。
			 */
			augment->copy(node, successor);

			/*
			 * successor 原位置被 child2 替代后，
			 * parent 到 successor 之间这段路径的增强信息需要更新。
			 */
			augment->propagate(parent, successor);
		}

		/*
		 * Case 2 / Case 3 共同部分：
		 *
		 * successor 已经确定要接管 node 的位置。
		 *
		 * node 的左子树整体可以挂到 successor 左边。
		 *
		 * 依据是 BST 顺序：
		 *
		 *   node 左子树所有节点 <= node
		 *   successor 来自 node 右子树，因此 successor >= node（唯一键时严格大于）
		 *
		 * 所以：
		 *
		 *   node 左子树所有节点 <= successor
		 */
		tmp = node->rb_left;
		WRITE_ONCE(successor->rb_left, tmp);
		rb_set_parent(tmp, successor);

		/*
		 * successor 接到 node 原来的父节点下面。
		 *
		 * pc 保存 node 原来的 parent + color。
		 * __rb_change_child() 只负责把父节点指向 node 的边，
		 * 改成指向 successor。
		 */
		pc = node->__rb_parent_color;
		tmp = __rb_parent(pc);
		__rb_change_child(node, successor, tmp, root);

		if (child2) {
			/*
			 * successor 原位置由 child2 顶替。
			 *
			 * 因为 successor 是右子树最左节点，
			 * 所以 successor 没有左孩子。
			 *
			 * 如果 child2 存在，那么从合法红黑树性质可以反推：
			 *
			 *     successor 原来是黑色；
			 *     child2 原来是红色。
			 *
			 * successor 从原位置被拿走后，
			 * 把 child2 染黑即可补回这一侧的黑色贡献。
			 *
			 * 因此不需要进入 __rb_erase_color()。
			 */
			rb_set_parent_color(child2, parent, RB_BLACK);
			rebalance = NULL;
		} else {
			/*
			 * successor 原位置没有 child2 可以顶替。
			 *
			 * 此时要看 successor 被拿走前的原始颜色：
			 *
			 *   如果 successor 原来是红色：
			 *     红节点不贡献黑高，拿走后不需要修复。
			 *
			 *   如果 successor 原来是黑色：
			 *     这条路径少了一个黑色贡献，
			 *     需要从 parent 开始做删除修复。
			 *
			 * 注意：
			 *   这里必须在 successor 继承 node 的颜色之前判断。
			 *
			 *   因为下面才会执行：
			 *
			 *       successor->__rb_parent_color = pc;
			 *
			 *   执行之后，successor 的颜色就变成 node 原来的颜色了，
			 *   不再是 successor 原位置上的原始颜色。
			 */
			rebalance = rb_is_black(successor) ? parent : NULL;
		}

		/*
		 * successor 正式继承 node 原来的 parent + color。
		 *
		 * 这表示：
		 *
		 *   successor 在 node 原来的位置上，
		 *   对外表现为 node 原来的颜色。
		 *
		 * 这样 node 原位置的黑高关系尽量保持不变。
		 *
		 * 真正可能需要修复的是 successor 原来的位置，
		 * 上面已经通过 child2 / rebalance 判断处理。
		 */
		successor->__rb_parent_color = pc;

		/*
		 * successor 是替换后的局部根。
		 * 最终增强字段从 successor 开始继续向上更新。
		 */
		tmp = successor;
	}

	/*
	 * 删除或替换完成后，向上更新增强字段。
	 *
	 * tmp 在不同分支中被设置为不同的更新起点：
	 *
	 *   Case 1:
	 *     tmp = parent
	 *
	 *   Case 2 / Case 3:
	 *     tmp = successor
	 */
	augment->propagate(tmp, NULL);

	/*
	 * 返回删除修复起点。
	 *
	 * NULL:
	 *   删除过程中已经完成局部颜色处理，
	 *   不需要再进入 __rb_erase_color()。
	 *
	 * 非 NULL:
	 *   successor/node 原位置损失了一个黑色贡献，
	 *   需要从 rebalance 开始做红黑树删除修复。
	 */
	return rebalance;
}
```

**实现原理：** D1 先确定可摘位置，再由 D2 读该位置原颜色。两个孩子时，successor 接替 node 的外部身份，而缺黑发生在 successor 离开的方向；必须先测 successor 原颜色，再覆盖为 node 的父色。child2 非空时，它只能是红色，设置黑色便抵偿原位置的黑贡献。返回值只在调用栈中交给删除修复函数，没有写进 rb_node 的某个“缺黑字段”。

copy 只是给 successor 一份旧子树增强值作为起点。深后继分支先改 parent.left，再令 successor.right 指向原右子树，随后 propagate(parent, successor) 更新受影响的旧路径；此时 successor 的父色尚未完成交接，stop 是排他的停止对象，不能越过它。最后 propagate(tmp,NULL) 从移位后的 successor 或单孩子删除的父节点更新上层；具体回调可因值不变早停。拓扑方向可用不等于所有父指针都已闭合，不能把此回调当作任意全树遍历点。

**可修改性：** 不能把 successor 颜色检查移到父色覆盖之后，也不能在直接后继分支执行 successor.right=child，那会让节点指向自身。不能把 copy 替换成业务结构整体赋值；增强字段之外的 key、请求身份与引用关系应保持原对象含义。WRITE_ONCE 的写序与此前路径约束仍须保留。功能代码不检验成员、不取锁、不释放对象，错误前提不会转成可恢复的错误码。

[状态关系图](../../../navigation/P04_对象摘除与缺黑修复导读.md#4.1_谁拥有地址和颜色)中的“写孩子槽/父色”由本函数执行；[完整时序](../../../navigation/P04_对象摘除与缺黑修复导读.md#4.2_从对象到缺黑父槽)中 D1 的摘除、copy/propagate 和 D2 的返回正对应这些语句。回到[教材结构删除](../../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.2_rbtree_删除前半段_rb_erase%28%29_与结构删除)或[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。

## 1.4\_保持颜色的父地址替换

孩子的父节点改变，但颜色不应因此变化。rb_color 取得最低颜色位，rb_set_parent 在保留这一位的前提下换入新的父地址；不要和 1.1 中同时指定新颜色的函数混淆。上游位置仍为 include/linux/rbtree_augmented.h，中文 Doxygen 为仓库补充、非上游原文。

```c
/**
 * rb_set_parent - 保留颜色，替换打包字段中的父地址。
 * @rb: 存活的有效节点。
 * @p: 新父地址；根节点可为 NULL。
 * 不反向更新 p 的孩子槽。
 */
static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
	rb->__rb_parent_color = rb_color(rb) + (unsigned long)p;
}
```

**实现原理：** Linux 节点对齐留下低位空间，父地址与最低颜色位相加不会改掉地址部分。本函数在[替换 R2](../../../navigation/P06_同键替换与旧对象退出导读.md#6.2_一轮替换怎样交接入口)配合前向结构复制使用；只改它而不改前向边会使两个方向矛盾。**可修改性：** 不可把保色赋值随意换成强制黑色，也不可绕过成员与寿命前提。宿主 Windows 的 unsigned long 不能装下 64 位指针，教材行为检查明确用 uintptr_t 适配；本页保留 Linux 原文。

## 1.5\_RCU外部入口发布

此助手与 1.2 选择同一条外部槽，但通过 rcu_assign_pointer 发布。需要先理解[RCU 公共接口索引](../../../../rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.6_建议的源码阅读顺序)的匹配读写和寿命前提；发布宏本体只在[RCU 唯一实现](../../../../rcu/source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.3.1_rcu_assign_pointer发布实现)展开。本函数仍来自上游 include/linux/rbtree_augmented.h。

```c
/**
 * __rb_change_child_rcu - 最后发布已准备节点到父或根入口。
 * @old: 原槽中的有效节点。
 * @new: 已完成内部初始化的替代节点。
 * @parent: old 的实际父；NULL 表示 old 为根。
 * @root: 根对象；调用者已串行化写者。
 * 不检查 old 是否属于 parent，不等待读者，也不修 new 自身父色。
 */
static inline void
__rb_change_child_rcu(struct rb_node *old, struct rb_node *new,
		      struct rb_node *parent, struct rb_root *root)
{
	if (parent) {
		if (parent->rb_left == old)
			rcu_assign_pointer(parent->rb_left, new);
		else
			rcu_assign_pointer(parent->rb_right, new);
	} else
		rcu_assign_pointer(root->rb_node, new);
}
```

中文 Doxygen 为仓库补充、非上游原文。**实现原理：** parent 非空时，old 若不等于左孩子就按前提写右槽；parent 为空则发布 root.rb_node。RCU 版本的差别在该槽的发布方式，不是自动给整棵树加读写锁。读者必须采用匹配的入口取得和寿命协议；旧节点仍需在 R4 满足回收条件。

**可修改性：** 不能把“父槽已发布”解释为旧读者不再使用 old，也不能擅自重写旧孩子边后立即释放。操作错误树或错误 parent 会发布到错误槽，函数没有失败回滚。复用[地址关系图](../../../navigation/P06_同键替换与旧对象退出导读.md#6.1_地址身份与排序位置)与[完整时序](../../../navigation/P06_同键替换与旧对象退出导读.md#6.2_一轮替换怎样交接入口)：1.4 对应 R2 孩子父地址写入，本节对应 R3 外部入口发布；R4 等待/回收不在本页函数内。返回[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。
