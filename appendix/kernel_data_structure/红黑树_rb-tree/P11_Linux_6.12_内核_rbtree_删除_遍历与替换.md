# 第11章 Linux 6.12 内核 rbtree 删除、遍历与替换

## 11.0 章节内容说明

### 11.0.1 本章在 Linux rbtree 学习路线中的位置

第 10 章已经讲完插入路径：

```text
调用者先按 BST 规则搜索落点；
rb_link_node() 把红色新节点挂到树上；
rb_insert_color() 调用 __rb_insert() 修复红红冲突；
旋转收尾由 __rb_rotate_set_parents() 统一处理。
```

本章进入删除路径。

删除比插入更难，原因是插入通常只会制造：

```text
红红冲突
```

而删除黑色节点可能制造：

```text
黑高缺失
```

黑高缺失不是一个真实节点颜色能直接表达的状态。Linux 源码用 `node == NULL`、`parent` 和循环不变量来表达这个“少一个黑色”的位置。

本章还会顺带讲遍历和替换接口，因为删除后的对象清理、遍历销毁、节点替换都和树结构维护密切相关。

------

### 11.0.2 本章参照的源码文件

本章主要参照：

\* [include/linux/rbtree.h](../../kernel_source/include/linux/rbtree.h)

\* [lib/rbtree.c](../../kernel_source/lib/rbtree.c)

\* [include/linux/rbtree_augmented.h](../../kernel_source/include/linux/rbtree_augmented.h)

其中：

```text
include/linux/rbtree_augmented.h
	实现 __rb_erase_augmented()，负责结构删除。

lib/rbtree.c
	实现 rb_erase()、____rb_erase_color()、遍历、替换。

include/linux/rbtree.h
	声明遍历、替换、postorder 遍历宏和 cached 包装接口。
```

删除阅读顺序建议：

```text
先看 rb_erase()
	↓
再看 __rb_erase_augmented()
	↓
再看 ____rb_erase_color()
```

不要一上来就读 `____rb_erase_color()`。

如果没有先理解结构删除返回的 `rebalance` 是什么，删除修复的 `parent`、`node == NULL`、`sibling` 都会显得很抽象。

------

## 11.1 rbtree 删除前半段：`rb_erase()` 与结构删除

`__rb_change_child()` 源码展示：

[include/linux/rbtree_augmented.h](../../kernel_source/include/linux/rbtree_augmented.h)

```c
/*
 * __rb_change_child - 替换父节点指向的孩子节点
 * @old:    原来的孩子节点。
 *          也就是即将被替换掉的节点。
 *
 * @new:    新的孩子节点。
 *          用它来替换 @old。
 *          可以为 NULL，表示把原来的孩子位置清空。
 *
 * @parent: @old 的父节点。
 *          如果 parent 非 NULL，说明 old 不是根节点；
 *          如果 parent 为 NULL，说明 old 是整棵红黑树的根节点。
 *
 * @root:   红黑树根。
 *          当 old 是根节点时，需要更新 root->rb_node。
 *
 * 功能：
 *   把 parent 或 root 中原来指向 old 的指针，改成指向 new。
 *
 * 注意：
 *   这个函数只修改“父节点指向孩子”的链接。
 *   它不负责修改 new 的 parent 指针。
 *
 *   也就是说：
 *
 *      parent -> old
 *
 *   会被改成：
 *
 *      parent -> new
 *
 *   但是：
 *
 *      new->__rb_parent_color
 *
 *   需要调用者自己设置。
 */
static inline void
__rb_change_child(struct rb_node *old,
                  struct rb_node *new,
                  struct rb_node *parent,
                  struct rb_root *root)
{
	/*
	 * 如果 parent 非 NULL，说明 old 不是根节点。
	 *
	 * 此时 old 一定位于 parent 的左孩子或者右孩子位置。
	 */
	if (parent) {
		if (parent->rb_left == old)
			WRITE_ONCE(parent->rb_left, new);
		else
			WRITE_ONCE(parent->rb_right, new);
	} else
		/*
		 * parent 为 NULL，说明 old 是根节点。
		 */
		WRITE_ONCE(root->rb_node, new);
}
```



### `__rb_erase_augmented()` 源码展示：

[include/linux/rbtree_augmented.h](../../kernel_source/include/linux/rbtree_augmented.h)

```c
/*
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
 *             如果 stop 为 NULL，则一直向根方向更新。
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
		} else {
			/*
			 * 删除叶子 node。
			 *
			 * 如果 node 是黑色，删除后这条路径少一个黑节点，
			 * 需要从 parent 开始删除修复。
			 *
			 * 如果 node 是红色，删除后不影响黑高。
			 */
			rebalance = __rb_is_black(pc) ? parent : NULL;
		}

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
		 * 不能直接用左孩子或右孩子替换 node，
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
		 *          node 左子树全部 < successor
		 *          successor < node 右子树剩余节点
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
		 *   node 左子树所有节点 < node
		 *   successor 来自 node 右子树，因此 successor > node
		 *
		 * 所以：
		 *
		 *   node 左子树所有节点 < successor
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



### `____rb_erase_color()` 源码展示：

[lib/rbtree.c](../../kernel_source/lib/rbtree.c)

```c
/*
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
		 *    如果缺黑一路上推到了根，那么把根染黑即可结束，
		 *    不需要继续在这里修复。
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
				 * 这一步后，sl 成为 parent 的新兄弟方向节点。
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
				 * 现在 sibling 的远侄子方向已经是红色条件。
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
				 *   把“近侄子红”转换成“远侄子红”，
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



### `rb_erase()` 源码展示：

[lib/rbtree.c](../../kernel_source/lib/rbtree.c)

```c
/*
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



### 11.1.1 `rb_erase()` 的对外语义

普通删除接口是：

```c
void rb_erase(struct rb_node *node, struct rb_root *root);
```

注意它接收的是：

```text
要删除的 rb_node；
所在树的 root。
```

它不是：

```text
按 key 删除。
```

所以调用者通常要先查找：

```c
item = demo_search(root, key);
if (!item)
	return -ENOENT;

rb_erase(&item->rb, root);
```

`rb_erase()` 的语义是：

```text
从 root 所代表的 rbtree 中摘除 node；
如果删除破坏红黑性质，就做颜色修复；
不释放业务对象。
```

它不负责：

```text
根据 key 查找；
判断 node 是否真的属于这棵树；
释放外层业务对象；
维护引用计数；
维护调用者的锁；
清理 node 的游离状态。
```

删除后是否调用 `RB_CLEAR_NODE()`，由调用者决定。

------

### 11.1.2 删除为什么拆成“结构删除”和“颜色修复”

Linux 删除路径是两段式：

```c
rebalance = __rb_erase_augmented(node, root, &dummy_callbacks);
if (rebalance)
	____rb_erase_color(rebalance, root, dummy_rotate);
```

第一段：

```text
__rb_erase_augmented()
```

负责 BST 层面的结构删除：

```text
没有左孩子；
没有右孩子；
左右孩子都存在，需要找中序后继。
```

第二段：

```text
____rb_erase_color()
```

负责红黑性质修复：

```text
如果删除黑色节点导致某条路径少一个黑色；
就从 rebalance 指示的位置开始修复。
```

为什么要拆开？

因为结构删除和颜色修复关注的问题不同。

结构删除关注：

```text
BST 有序性；
父子指针；
中序后继；
被替换节点的位置；
augmented 信息复制和传播。
```

颜色修复关注：

```text
黑高缺失；
兄弟节点颜色；
侄子节点颜色；
旋转和染色。
```

这两层分开以后，源码中的 `rebalance` 就成了连接点：

```text
结构删除返回一个 parent；
如果 parent 非 NULL，说明需要从这个 parent 下方的缺黑位置开始修复。
```

------

### 11.1.3 `__rb_erase_augmented()` 的基础作用

`__rb_erase_augmented()` 位于 `include/linux/rbtree_augmented.h`。

虽然名字里有 augmented，但普通 `rb_erase()` 也复用它。

普通 rbtree 传入的是 dummy callbacks：

```text
propagate：空函数；
copy：空函数；
rotate：空函数。
```

增强树则传入真实回调。

因此 `__rb_erase_augmented()` 是通用结构删除骨架。

它返回：

```c
struct rb_node *rebalance;
```

返回值语义：

```text
rebalance == NULL：
	结构删除已经局部解决颜色问题；
	不需要进入 ____rb_erase_color()。

rebalance != NULL：
	删除导致某个位置缺少一个黑色；
	rebalance 是缺黑位置的父节点；
	需要进入删除修复。
```

这里要特别注意：

```text
rebalance 不是“被删节点”；
rebalance 也不是“替换节点”；
rebalance 是删除修复入口所需的 parent。
```

------

### 11.1.4 Case 1：被删节点没有左孩子

源码开头：

```c
struct rb_node *child = node->rb_right;
struct rb_node *tmp = node->rb_left;
```

如果：

```text
tmp == NULL
```

说明：

```text
node 没有左孩子。
```

此时 `child` 是右孩子，可能为 NULL。

结构上只需要让 `child` 接替 `node` 的位置：

```text
parent = node 的父节点；
__rb_change_child(node, child, parent, root);
```

如果 `child` 存在，源码会：

```text
child->__rb_parent_color = node->__rb_parent_color;
rebalance = NULL;
```

这表示：

```text
child 接替 node 的父节点和颜色；
不需要额外删除修复。
```

为什么？

因为在红黑树中，如果被删节点只有一个非空孩子，那么这个孩子必然是红色，而被删节点必然是黑色。

让红色 child 接替黑色 node 的位置并继承黑色，相当于：

```text
用 child 补上 node 原来的黑色贡献；
黑高不缺失。
```

如果 `child == NULL`：

```text
删除的是一个没有孩子的节点。
```

此时：

```text
如果 node 是红色：
	删掉红色叶子不会影响黑高，不需要修复。

如果 node 是黑色：
	这条路径少了一个黑色，需要从 parent 开始修复。
```

源码对应：

```text
rebalance = node 是黑色 ? parent : NULL;
```

------

### 11.1.5 Case 2：被删节点没有右孩子

如果第一种情况不成立，但：

```text
child == NULL
```

说明：

```text
node 有左孩子；
node 没有右孩子。
```

此时左孩子 `tmp` 接替 node 的位置。

源码做：

```text
tmp->__rb_parent_color = node->__rb_parent_color;
__rb_change_child(node, tmp, parent, root);
rebalance = NULL;
```

这里也不需要进入删除修复。

原因和 Case 1 中有一个非空孩子的场景一样：

```text
这个唯一孩子必须是红色；
被删节点必须是黑色；
孩子继承被删节点颜色后，局部黑高保持不变。
```

所以 Case 1 和 Case 2 本质上都是：

```text
被删节点最多只有一个非空孩子。
```

如果有非空孩子，它会继承被删节点颜色，避免缺黑。

如果没有非空孩子，则要看被删节点是不是黑色。

------

### 11.1.6 Case 3：被删节点左右孩子都存在

如果：

```text
node->rb_left != NULL
node->rb_right != NULL
```

就不能直接用某个孩子接替 node。

BST 删除规则要求：

```text
找 node 的中序后继 successor；
用 successor 接替 node 的位置；
再从 successor 原来的位置删掉 successor。
```

中序后继是：

```text
node 右子树中的最左节点。
```

Linux 源码把两孩子删除分成两个子情况。

第一种：

```text
node 的右孩子本身就是 successor。
```

也就是：

```text
node->rb_right->rb_left == NULL
```

结构：

```text
    (n)             (s)
    / \             / \
  (x) (s)   -->   (x) (c)
        \
        (c)
```

第二种：

```text
successor 是 node 右子树中更深的最左节点。
```

结构：

```text
    (n)             (s)
    / \             / \
  (x) (y)   -->   (x) (y)
      /               /
    (p)             (p)
    /               /
  (s)             (c)
    \
    (c)
```

这两个子情况都要保证：

```text
successor 接替 node 的位置；
node 左子树挂到 successor 左边；
node 右子树挂到 successor 右边；
successor 原位置由 child2 接替；
successor 继承 node 的父节点和颜色；
如果 successor 原位置删掉黑色贡献，则返回 rebalance。
```

------

### 11.1.7 为什么两孩子删除要寻找中序后继

BST 有序性要求：

```text
左子树所有 key < node key < 右子树所有 key。
```

删除有两个孩子的节点时，不能随便拿一个孩子上来。

如果直接让左孩子上来：

```text
左孩子的右子树如何接回？
原右子树如何接回？
局部有序性容易复杂化。
```

使用中序后继的好处是：

```text
successor 是右子树中最小的节点；
successor 大于 node 左子树所有节点；
successor 小于或等于 node 右子树中其他节点；
所以 successor 可以接替 node 的排序位置。
```

也可以使用中序前驱。

Linux rbtree 选择中序后继。

------

### 11.1.8 后继节点如何接管被删节点的位置

无论 successor 是右孩子还是右子树深处的最左节点，最终都要做：

```text
successor->rb_left = node->rb_left;
node->rb_left 的 parent 改成 successor；
successor 继承 node 原来的 parent 和 color；
node 原父节点的孩子指针改成 successor。
```

源码中关键动作包括：

```text
WRITE_ONCE(successor->rb_left, tmp);
rb_set_parent(tmp, successor);

pc = node->__rb_parent_color;
tmp = __rb_parent(pc);
__rb_change_child(node, successor, tmp, root);
successor->__rb_parent_color = pc;
```

其中：

```text
pc 保存 node 原来的父指针和颜色；
successor->__rb_parent_color = pc 表示 successor 继承 node 的位置颜色。
```

这一步非常重要。

因为从 node 的父节点以上看：

```text
这棵子树的根从 node 换成 successor；
但这棵子树对外的黑高贡献应该保持一致。
```

所以 successor 必须继承 node 的颜色。

------

### 11.1.9 后继原位置如何处理 `child2`

successor 原位置被挪走后，需要让它原来的右孩子 `child2` 接上。

为什么只有右孩子？

因为 successor 是右子树的最左节点。

所以：

```text
successor 没有左孩子；
successor 可能有右孩子 child2。
```

如果 successor 是 node 的右孩子：

```text
parent = successor;
child2 = successor->rb_right;
```

如果 successor 在更深处：

```text
parent 是 successor 原来的父节点；
child2 = successor->rb_right;
parent->rb_left = child2;
```

接下来判断是否需要颜色修复：

```text
如果 child2 存在：
	child2 接替 successor 原位置；
	child2 染黑；
	rebalance = NULL。

如果 child2 不存在：
	如果 successor 原来是黑色：
		删掉 successor 原位置会造成缺黑；
		rebalance = parent。
	否则：
		删掉红色 successor 不影响黑高；
		rebalance = NULL。
```

源码对应：

```text
if (child2) {
	rb_set_parent_color(child2, parent, RB_BLACK);
	rebalance = NULL;
} else {
	rebalance = rb_is_black(successor) ? parent : NULL;
}
```

注意这里判断的是：

```text
successor 原位置的颜色。
```

之后 successor 会继承 node 的颜色。

------

### 11.1.10 删除路径中 augmented 信息如何维护

`__rb_erase_augmented()` 同时服务普通树和增强树。

增强树需要维护子树增强信息，所以删除过程中有三个回调点：

```text
augment->copy(node, successor)
augment->propagate(parent, successor)
augment->propagate(tmp, NULL)
```

含义分别是：

```text
copy：
	successor 接替 node 的位置时，复制 node 的增强信息。

propagate(parent, successor)：
	successor 从原位置移走后，原路径上的增强信息需要向上更新，
	直到 successor 位置为止。

propagate(tmp, NULL)：
	结构删除最终完成后，从受影响节点继续向根传播更新。
```

普通 rbtree 传入 dummy callbacks，所以这些动作会被优化为空。

这就是 Linux rbtree 结构删除写在 `rbtree_augmented.h` 中的原因：

```text
普通树和增强树共用删除骨架；
增强树在必要位置插入回调；
普通树靠 dummy callback 消除额外成本。
```

------

### 11.1.11 本节小结

本节固定结构删除的几个结论：

```text
第一，rb_erase() 先调用 __rb_erase_augmented() 做结构删除。

第二，__rb_erase_augmented() 返回 rebalance，表示是否需要颜色修复。

第三，没有左孩子或没有右孩子时，最多一个孩子接替 node。

第四，如果唯一孩子存在，它继承 node 颜色，通常不需要颜色修复。

第五，左右孩子都存在时，用中序后继 successor 接替 node。

第六，successor 继承 node 原来的父节点和颜色。

第七，successor 原位置被删掉后，是否缺黑取决于 successor 原来的颜色和 child2。

第八，augmented rbtree 在结构删除中通过 copy 和 propagate 维护增强信息。
```

------

## 11.2 `____rb_erase_color()`：删除修复核心

### 11.2.1 删除修复循环的不变量

`____rb_erase_color()` 位于 `lib/rbtree.c`。

函数入口：

```c
____rb_erase_color(struct rb_node *parent, struct rb_root *root,
		   void (*augment_rotate)(struct rb_node *old,
					  struct rb_node *new))
```

它没有传入缺黑节点。

内部初始化：

```c
struct rb_node *node = NULL, *sibling, *tmp1, *tmp2;
```

也就是说，第一次循环中：

```text
node == NULL
parent == rebalance
```

这正是在表达：

```text
parent 的某个孩子位置缺少一个黑色；
这个缺黑位置可能是 NULL。
```

源码注释给出循环不变量：

```text
node is black, or NULL on first iteration;
node is not the root;
all leaf paths going through parent and node have black count 1 lower.
```

翻译成学习语言：

```text
当前 node 位置可以看成一个黑色位置；
它不是整棵树根；
经过 parent -> node 方向的路径，比 parent 另一侧路径少一个黑色；
修复目标就是把这个缺少的黑色补掉、转移掉或在更高层解决。
```

------

### 11.2.2 为什么删除修复处理的是“少一个黑色”的位置

教材经常用“双黑节点”描述删除修复。

Linux 源码没有真的创建 double-black 节点。

它用：

```text
node
parent
sibling
```

来表达缺黑位置。

第一次进入时 `node == NULL`，但仍然可以修复，是因为：

```text
parent 告诉我们缺黑位置的父节点是谁；
sibling 可以通过 parent 的另一个孩子找到；
缺黑方向可以通过 node 和 sibling 的关系判断。
```

源码一开始：

```c
sibling = parent->rb_right;
if (node != sibling) {
	/* node == parent->rb_left */
	...
} else {
	/* node == parent->rb_right */
	...
}
```

如果 `node != parent->rb_right`，说明缺黑位置在左边。

否则缺黑位置在右边。

因为第一次 `node == NULL`，这段判断也能工作：

```text
如果 parent->rb_right 不是 NULL，则 node != sibling，缺黑在左；
如果 parent->rb_right 也是 NULL，则进入镜像侧，这种情况由红黑树结构约束保证不会走到非法访问路径。
```

理解删除修复时，最好不要死盯“node 是哪个真实节点”。

更准确的说法是：

```text
node 表示当前缺黑方向上的节点位置；
它可能是真实黑节点，也可能是 NULL 叶子位置。
```

------

### 11.2.3 左侧删除修复总览

先看左侧分支：

```text
node == parent->rb_left
sibling = parent->rb_right
```

结构可以画成：

```text
      P
     / \
    N   S
       / \
      Sl  Sr
```

其中：

```text
N 是缺黑方向；
S 是兄弟；
Sl 是近侄；
Sr 是远侄。
```

左侧删除修复有四个 case：

```text
Case 1：
	兄弟 S 是红色。

Case 2：
	兄弟 S 是黑色，两个侄子都是黑色。

Case 3：
	兄弟 S 是黑色，远侄 Sr 是黑色，近侄 Sl 是红色。

Case 4：
	兄弟 S 是黑色，远侄 Sr 是红色。
```

这四个 case 的目标不是并列的。

它们的关系是：

```text
Case 1 把红兄弟转换成黑兄弟；
Case 3 把近侄红转换成远侄红；
Case 4 进行最终旋转并结束；
Case 2 可能把缺黑向上推进。
```

------

### 11.2.4 Case 1：兄弟为红，先转换成黑兄弟

如果：

```text
rb_is_red(sibling)
```

说明兄弟 S 是红色。

结构：

```text
      P(B)
     /   \
    N     s(R)
         /   \
       Sl(B) Sr(B)
```

由于红节点不能有红孩子，所以 S 的两个孩子必然是黑色。

Case 1 的动作是：

```text
围绕 parent 左旋；
sibling 成为局部子树根；
parent 变成 sibling 的左孩子；
parent 染红；
sibling 继承 parent 原来的颜色；
缺黑位置仍然在 parent 的左侧；
新的 sibling 变成原来的 Sl。
```

源码：

```text
tmp1 = sibling->rb_left;
parent->rb_right = tmp1;
sibling->rb_left = parent;
tmp1 的 parent 改成 parent；
__rb_rotate_set_parents(parent, sibling, root, RB_RED);
augment_rotate(parent, sibling);
sibling = tmp1;
```

Case 1 不会直接结束。

它只是把局面转换成：

```text
兄弟为黑的情况。
```

这样后面就可以进入 Case 2、Case 3 或 Case 4。

从 2-3-4 树视角看，红兄弟表示父节点和兄弟处在一种倾斜编码中，先旋转是为了换一个视角，让真正可借位或可合并的黑兄弟暴露出来。

------

### 11.2.5 Case 2：兄弟为黑且双侄黑，染色并向上推进

Case 1 处理后，或者一开始兄弟就是黑色。

源码先看远侄：

```text
tmp1 = sibling->rb_right;
if (!tmp1 || rb_is_black(tmp1)) {
	tmp2 = sibling->rb_left;
	if (!tmp2 || rb_is_black(tmp2)) {
		Case 2
	}
}
```

左侧删除中：

```text
tmp1 = Sr，远侄；
tmp2 = Sl，近侄。
```

Case 2 条件：

```text
S 是黑色；
Sl 是黑色或 NULL；
Sr 是黑色或 NULL。
```

结构：

```text
      (p)
     /   \
    N     S(B)
         /   \
       Sl(B) Sr(B)
```

动作：

```text
S 染红。
```

这样做的含义是：

```text
兄弟侧少一个黑色；
与缺黑侧 N 对齐；
parent 子树内部黑高恢复一致。
```

但是 parent 这一层可能出现两种情况。

如果 parent 是红色：

```text
把 parent 染黑；
缺黑被 parent 的红色补掉；
修复结束。
```

如果 parent 是黑色：

```text
parent 这棵子树整体对外少了一个黑色；
缺黑向上推进到 parent；
继续循环。
```

源码对应：

```text
rb_set_parent_color(sibling, parent, RB_RED);
if (rb_is_red(parent))
	rb_set_black(parent);
else {
	node = parent;
	parent = rb_parent(node);
	if (parent)
		continue;
}
break;
```

这就是删除修复比插入更难的地方：

```text
插入 Case 1 是红色上推；
删除 Case 2 是缺黑上推。
```

------

### 11.2.6 Case 3：兄弟为黑且近侄红，转换成远侄红

Case 3 条件：

```text
S 是黑色；
远侄 Sr 是黑色；
近侄 Sl 是红色。
```

结构：

```text
      (p)
     /   \
    N     S(B)
         /   \
       sl(R) sr(B)
```

这个结构不能直接用 parent 左旋结束，因为远侄不是红色。

所以先围绕 sibling 右旋：

```text
Sl 上来；
S 下去；
把近侄红转换成远侄红形态。
```

源码：

```text
tmp1 = tmp2->rb_right;
sibling->rb_left = tmp1;
tmp2->rb_right = sibling;
parent->rb_right = tmp2;
if (tmp1)
	rb_set_parent_color(tmp1, sibling, RB_BLACK);
augment_rotate(sibling, tmp2);
tmp1 = sibling;
sibling = tmp2;
```

这里旋转后：

```text
sibling 变成原来的 Sl；
tmp1 变成原来的 S；
```

然后继续落入 Case 4。

Case 3 也不是最终修复。

它的目标是：

```text
把近侄红转换成远侄红，交给 Case 4 一步结束。
```

------

### 11.2.7 Case 4：兄弟为黑且远侄红，旋转并结束修复

Case 4 条件：

```text
S 是黑色；
远侄 Sr 是红色。
```

结构：

```text
      (p)
     /   \
    N     S(B)
         /   \
      (sl)  sr(R)
```

动作：

```text
围绕 parent 左旋；
S 接替 parent 的位置，并继承 parent 的颜色；
parent 染黑；
Sr 染黑；
缺黑被消除；
修复结束。
```

源码关键动作：

```c
tmp2 = sibling->rb_left;
parent->rb_right = tmp2;
sibling->rb_left = parent;
rb_set_parent_color(tmp1, sibling, RB_BLACK);
if (tmp2)
	rb_set_parent(tmp2, parent);
__rb_rotate_set_parents(parent, sibling, root, RB_BLACK);
augment_rotate(parent, sibling);
break;
```

这里：

```text
tmp1 是远侄 Sr；
tmp2 是近侄 Sl。
```

`rb_set_parent_color(tmp1, sibling, RB_BLACK)` 把远侄染黑。

`__rb_rotate_set_parents(parent, sibling, root, RB_BLACK)` 让：

```text
sibling 继承 parent 原来的颜色；
parent 成为 sibling 的孩子；
parent 被设置为黑色。
```

为什么可以结束？

因为旋转和染色以后：

```text
缺黑方向补上了黑色；
兄弟侧也保持黑高；
局部子树对外黑高恢复到删除前的状态。
```

------

### 11.2.8 右侧删除与左侧删除的镜像关系

右侧分支是左侧分支的镜像。

条件：

```text
node == parent->rb_right
sibling = parent->rb_left
```

结构：

```text
      P
     / \
    S   N
   / \
 Sl  Sr
```

此时：

```text
近侄 = Sr
远侄 = Sl
```

四个 case 镜像为：

```text
Case 1：
	兄弟 S 为红，围绕 parent 右旋。

Case 2：
	兄弟 S 黑，两个侄子黑，S 染红，缺黑可能上推。

Case 3：
	兄弟 S 黑，远侄 Sl 黑，近侄 Sr 红，
	围绕 sibling 左旋。

Case 4：
	兄弟 S 黑，远侄 Sl 红，
	围绕 parent 右旋并结束。
```

阅读右侧源码时，直接把左侧的方向互换：

```text
left  <-> right
rb_left <-> rb_right
left rotate <-> right rotate
Sl <-> Sr
```

不要再背一套新的逻辑。

------

### 11.2.9 删除修复与 2-3-4 树借位 / 合并的对应关系

删除修复可以从 2-3-4 树角度理解。

```text
缺黑位置：
	对应 2-3-4 树中某个下行分支缺少 key，需要修复。

兄弟为黑且双侄黑：
	兄弟逻辑节点也不可借；
	只能合并，缺失向父层传播。

兄弟为黑且远侄红：
	兄弟逻辑节点可借；
	通过旋转和染色完成借位，修复结束。

兄弟为红：
	先旋转改变兄弟形态；
	把问题转换成黑兄弟场景。

近侄红远侄黑：
	先在兄弟内部调整；
	把可借 key 调整到远侄方向，再进入最终借位。
```

这能解释为什么删除 Case 2 会继续向上，而 Case 4 会结束。

```text
Case 2：
	合并后父层可能少 key，所以向上。

Case 4：
	借位成功，局部修复完成。
```

------

### 11.2.10 删除修复为什么比插入修复更难读

删除修复难读有几个原因。

第一，缺黑不是一个真实节点。

```text
插入时 node 是真实红节点；
删除时 node 可能是 NULL，表示缺黑位置。
```

第二，入口传的是 parent。

```text
____rb_erase_color(parent, ...)
```

而不是传“被删节点”。

第三，删除分成结构删除和颜色修复。

如果不理解 `rebalance`，就不知道 `parent` 从哪里来。

第四，case 之间是转换关系。

```text
Case 1 转成黑兄弟；
Case 3 转成远侄红；
Case 4 最终结束；
Case 2 可能向上。
```

第五，左右镜像全部展开写。

这让源码长度翻倍，也让变量 `tmp1`、`tmp2` 的含义随方向变化。

所以读删除修复时，建议固定一个方向先读。

比如先读：

```text
node == parent->rb_left
```

把左侧四个 case 完全理解后，再把方向镜像到右侧。

------

### 11.2.11 本节小结

删除修复的核心是处理黑高缺失。

Linux 源码用：

```text
node
parent
sibling
tmp1
tmp2
```

表达教材中的：

```text
x
parent
brother
near nephew
far nephew
```

左侧删除四个 case 可以这样记：

```text
兄弟红：
	先旋转，转成黑兄弟。

兄弟黑，双侄黑：
	兄弟染红，缺黑可能向上。

兄弟黑，近侄红，远侄黑：
	先围绕兄弟旋转，转成远侄红。

兄弟黑，远侄红：
	围绕 parent 旋转并染色，修复结束。
```

------

## 11.3 rbtree 遍历接口

### 11.3.1 遍历为什么本质上是 BST 中序关系

rbtree 是红黑树，也是 BST。

所以按 key 从小到大遍历，本质上是中序遍历：

```text
左子树
当前节点
右子树
```

Linux rbtree 没有递归遍历接口，而是提供：

```text
rb_first()
rb_last()
rb_next()
rb_prev()
```

它们允许调用者从某个节点开始按排序顺序前进或后退。

------

### 11.3.2 `rb_first()` 与 `rb_last()`

`rb_first()` 返回最小节点。

逻辑很简单：

```text
从 root->rb_node 开始；
一直向 rb_left 走；
直到没有左孩子；
这个节点就是最小节点。
```

`rb_last()` 返回最大节点：

```text
从 root->rb_node 开始；
一直向 rb_right 走；
直到没有右孩子；
这个节点就是最大节点。
```

空树时二者都返回 `NULL`。

这两个函数不关心颜色。

原因是：

```text
最小 / 最大只由 BST 有序性决定；
与红黑颜色无关。
```

------

### 11.3.3 `rb_next()`：中序后继

`rb_next(node)` 返回中序后继。

分两种情况。

第一，当前节点有右孩子：

```text
后继在右子树中；
具体是右子树里的最左节点。
```

逻辑：

```text
node = node->rb_right;
while (node->rb_left)
	node = node->rb_left;
return node;
```

第二，当前节点没有右孩子：

```text
后继在祖先方向。
```

向上找第一个满足：

```text
当前节点是其父节点的左孩子
```

的父节点。

这个父节点就是后继。

源码逻辑：

```text
while ((parent = rb_parent(node)) && node == parent->rb_right)
	node = parent;

return parent;
```

如果一路向上都没有找到，说明当前节点已经是最大节点，返回 `NULL`。

------

### 11.3.4 `rb_prev()`：中序前驱

`rb_prev(node)` 是 `rb_next()` 的镜像。

如果当前节点有左孩子：

```text
前驱是左子树里的最右节点。
```

如果当前节点没有左孩子：

```text
向上找第一个满足：
当前节点是其父节点右孩子
的父节点。
```

颜色同样不参与。

因为前驱 / 后继只取决于 BST 结构。

------

### 11.3.5 `RB_EMPTY_NODE()` 对遍历的保护

`rb_next()` 和 `rb_prev()` 开头都有：

```text
if (RB_EMPTY_NODE(node))
	return NULL;
```

`RB_EMPTY_NODE()` 判断的是：

```text
node->__rb_parent_color == (unsigned long)node
```

这是 `RB_CLEAR_NODE()` 设置出来的游离状态。

它用于表达：

```text
这个节点已知不在任何 rbtree 中。
```

如果一个节点已经从树中摘除并清理，再调用 `rb_next()` 没有意义。

这个保护可以让这种场景返回 NULL。

但要注意：

```text
RB_EMPTY_NODE() 不是并发安全判断；
也不能替代“节点是否真的属于某棵树”的完整校验。
```

------

### 11.3.6 后序遍历接口与整棵树销毁场景

Linux rbtree 还提供后序遍历：

```text
rb_first_postorder()
rb_next_postorder()
rbtree_postorder_for_each_entry_safe()
```

后序遍历顺序是：

```text
先访问孩子；
再访问父节点。
```

这很适合销毁整棵树。

原因是：

```text
释放父节点之前，先释放它的左右子树；
不会因为父节点先释放而丢失孩子指针。
```

`rb_first_postorder()` 会找到：

```text
从根开始，优先向左；没有左则向右；
直到最深的叶子。
```

`rb_next_postorder()` 根据当前节点和父节点关系决定下一个后序节点。

------

### 11.3.7 遍历过程中删除节点的限制

`rbtree_postorder_for_each_entry_safe()` 名字里有 safe，但它的 safe 有边界。

它允许：

```text
循环体释放当前 pos 指向的对象内存；
因为下一步 n 已经提前保存。
```

但它不能处理：

```text
循环过程中调用 rb_erase() 导致树重新平衡。
```

原因是 `rb_erase()` 可能旋转。

旋转会改变尚未访问节点的结构关系，导致遍历漏节点。

所以文档语义是：

```text
适合整棵树销毁；
不适合边遍历边做会重排树结构的删除。
```

如果需要遍历并删除，常见做法是：

```text
先用 rb_first() / rb_next() 保存 next；
再删除当前节点；
或者根据业务设计专门的删除循环。
```

------

### 11.3.8 本节小结

遍历接口的核心结论：

```text
第一，rb_first() / rb_last() 分别找最左 / 最右节点。

第二，rb_next() / rb_prev() 基于 BST 中序关系，不依赖颜色。

第三，RB_EMPTY_NODE() 可以识别已清理游离节点，但不是并发保护。

第四，后序遍历适合销毁整棵树。

第五，postorder safe 不等于可以任意 rb_erase() 并继续遍历。
```

------

## 11.4 `rb_replace_node()` 与 `rb_replace_node_rcu()`

### 11.4.1 替换节点与删除再插入的区别

`rb_replace_node()` 的语义是：

```text
用 new 替换 victim 在树中的位置；
不重新比较 key；
不重新平衡；
不改变排序位置。
```

它不是：

```text
删除旧节点，再按新 key 插入新节点。
```

因此它有一个硬性条件：

```text
new 必须和 victim 处在同一个排序位置。
```

也就是说：

```text
new 的 key 必须等价于 victim 的 key；
或者至少对树中所有其他节点的比较结果保持一致。
```

如果 new 的 key 变了，使用 `rb_replace_node()` 会破坏 BST 有序性。

这种情况应该：

```text
先 rb_erase(victim)；
再按新 key 搜索落点；
再 rb_link_node() + rb_insert_color()。
```

------

### 11.4.2 `*new = *victim` 的工程意义

源码中最关键的一句是：

```c
*new = *victim;
```

这会复制：

```text
victim->__rb_parent_color
victim->rb_left
victim->rb_right
```

含义是：

```text
new 直接继承 victim 的父指针、颜色、左右孩子。
```

然后修正左右孩子的父指针：

```text
如果 victim->rb_left 存在：
	它的 parent 改成 new。

如果 victim->rb_right 存在：
	它的 parent 改成 new。
```

最后：

```text
__rb_change_child(victim, new, parent, root)
```

把 victim 在父节点或 root 中的位置替换成 new。

整个过程没有旋转，也没有染色修复。

原因是：

```text
树的结构形状没有改变；
new 完全接管 victim 的结构位置和颜色；
红黑性质保持不变。
```

------

### 11.4.3 为什么 replacement 必须保持相同排序位置

红黑树旋转和替换都默认 BST 中序顺序不被破坏。

`rb_replace_node()` 不调用比较函数。

它不会检查：

```text
new 是否大于左子树所有节点；
new 是否小于右子树所有节点；
new 是否符合父节点方向。
```

所以调用者必须保证：

```text
new 放在 victim 的位置仍然满足业务排序。
```

适合场景：

```text
替换对象壳子；
迁移对象内存；
同 key 对象更新；
需要保留树位置但换业务结构体实例。
```

不适合场景：

```text
修改 key；
从按地址排序改成按长度排序；
替换成另一个排序位置不同的对象。
```

------

### 11.4.4 `rb_replace_node_rcu()` 与 RCU 读侧安全

RCU 版本和普通版本结构相似，也会：

```text
*new = *victim;
修正子节点 parent；
替换父节点孩子指针或 root。
```

区别在最后一步：

```text
__rb_change_child_rcu(victim, new, parent, root)
```

它使用：

```text
rcu_assign_pointer()
```

而且源码注释强调：

```text
最后才更新父节点指向 new 的指针。
```

原因是：

```text
RCU 读者一旦通过父节点看到 new；
就应该能看到 new 内部已经复制好的左右孩子和父子关系。
```

所以 RCU 替换的顺序是：

```text
先准备 new；
先修正 new 周围的子节点关系；
最后发布父节点到 new 的指针。
```

这仍然不等于：

```text
可以不管理 victim 生命周期。
```

RCU 读者可能仍然持有 victim 指针，所以旧对象释放必须等待宽限期或遵循业务引用规则。

------

### 11.4.5 `rb_replace_node_cached()` 如何维护最左缓存

cached rbtree 额外保存：

```text
root->rb_leftmost
```

所以替换时如果：

```text
victim 正好是 rb_leftmost
```

就要把缓存改成：

```text
new
```

逻辑是：

```text
if (root->rb_leftmost == victim)
	root->rb_leftmost = new;
rb_replace_node(victim, new, &root->rb_root);
```

这再次说明：

```text
cached 信息不属于普通 rb_root；
使用 cached 接口时，必须走 cached 包装函数维护它。
```

如果直接对 cached tree 调用普通 `rb_replace_node()`，最左缓存可能失效。

------

### 11.4.6 本节小结

替换接口的核心结论：

```text
第一，rb_replace_node() 是原地结构替换，不是删除再插入。

第二，new 会复制 victim 的父指针、颜色、左右孩子。

第三，replacement 必须保持相同排序位置，不能改变 key 语义。

第四，RCU 版本最后发布父节点孩子指针，保证读者看到 new 时 new 已初始化。

第五，cached tree 替换最左节点时必须维护 rb_leftmost。
```

------

## 11.5 本章小结

本章补齐了 Linux rbtree 删除、遍历和替换三条源码路径。

删除路径要记成两段：

```text
rb_erase()
	↓
__rb_erase_augmented()
	结构删除，返回 rebalance
	↓
____rb_erase_color()
	如果 rebalance 非 NULL，修复黑高缺失
```

删除修复四个 case 要记成转换关系：

```text
兄弟红：
	先旋转，转成黑兄弟。

兄弟黑，双侄黑：
	兄弟染红，缺黑可能向上。

兄弟黑，近侄红：
	先旋转兄弟，转成远侄红。

兄弟黑，远侄红：
	旋转 parent 并染色，修复结束。
```

遍历路径要记成：

```text
rb_first / rb_last：
	找最左 / 最右。

rb_next / rb_prev：
	找中序后继 / 前驱。

postorder：
	适合整棵树销毁，但不能随意和 rb_erase() 重排混用。
```

替换路径要记成：

```text
rb_replace_node() 不比较 key；
new 必须保持 victim 的排序位置；
它只是复制结构关系并替换父节点指针。
```

下一章继续讲 cached rbtree、augmented rbtree、并发控制、示例代码、调试验证和内核使用场景。

