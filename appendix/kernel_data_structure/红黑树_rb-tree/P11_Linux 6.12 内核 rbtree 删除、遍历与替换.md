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

\* [include/linux/rbtree.h](../../kernel_source/include/linux/rbtree.h.md)

\* [lib/rbtree.c](../../kernel_source/lib/rbtree.c.md)

\* [include/linux/rbtree_augmented.h](../../kernel_source/include/linux/rbtree_augmented.h.md)

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

```text
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

