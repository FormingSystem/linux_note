---
id: knowledge.linux.data_structures.红黑树_rb-tree.p12_linux_6.12_内核_rbtree_工程扩展_并发与验证
title: "Linux 6.12 内核 rbtree 工程扩展 并发与验证"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
---

# 第12章\_Linux\_6.12\_内核\_rbtree\_工程扩展\_并发与验证

## 12.1\_章节内容说明

### 12.1.1\_本章在\_Linux\_rbtree\_学习路线中的位置

第 8 章到第 11 章已经完成了 Linux rbtree 的主干：

```text
第 8 章：
	基础结构、父指针颜色压缩、rb_root、rb_root_cached。

第 9 章：
	嵌入式节点、业务对象、调用者接口、生命周期。

第 10 章：
	查找、插入落点、rb_link_node()、__rb_insert()。

第 11 章：
	结构删除、删除修复、遍历、替换。
```

本章收束工程扩展部分。

重点回答：

```text
cached rbtree 为什么只缓存最左节点？
augmented rbtree 如何在旋转和删除中维护增强信息？
rbtree 为什么不内置锁？
RCU 接口到底保证什么，不保证什么？
怎样写一个完整示例？
怎样验证红黑树结构没有坏？
内核哪些场景适合用 rbtree？
```

------

### 12.1.2\_本章参照的源码文件

本章主要参照：

```text
../../kernel_source/include/linux/rbtree.h
../../kernel_source/include/linux/rbtree_augmented.h
../../kernel_source/lib/rbtree.c
```

其中：

```text
rbtree.h：
	rb_root_cached、rb_first_cached()、rb_insert_color_cached()、
	rb_erase_cached()、rb_add_cached()、rb_find_rcu() 等。

rbtree_augmented.h：
	struct rb_augment_callbacks、
	RB_DECLARE_CALLBACKS()、
	RB_DECLARE_CALLBACKS_MAX()、
	rb_insert_augmented()、
	rb_erase_augmented()。

rbtree.c：
	lockless lookup 注释、WRITE_ONCE()、旋转、遍历和替换实现。
```

------

## 12.2\_cached\_rbtree\_struct\_rb\_root\_cached

### 12.2.1\_为什么要缓存最左节点

普通 `struct rb_root` 只保存：

```c
struct rb_node *rb_node;
```

如果要找最小节点，需要调用：

```c
rb_first(root);
```

`rb_first()` 的逻辑是：

```text
从根开始一直向左走。
```

复杂度是：

```text
O(log n)
```

对普通场景这已经足够。

但是某些内核场景会频繁获取最小 key 对象，例如：

```text
最早到期的定时器；
最小虚拟运行时间的调度实体；
最靠前的请求位置；
某个时间线上的下一个事件。
```

如果每次都从根向左走，虽然是 O(log n)，但仍然有重复成本。

`struct rb_root_cached` 增加：

```c
struct rb_node *rb_leftmost;
```

它直接缓存最左节点。

这样：

```c
rb_first_cached(root)
```

就是：

```c
(root)->rb_leftmost
```

复杂度变成：

```text
O(1)
```

------

### 12.2.2\_为什么只缓存最左节点\_不缓存最右节点

`struct rb_root_cached` 只缓存：

```text
rb_leftmost
```

不缓存：

```text
rb_rightmost
```

这是工程取舍。

缓存一个指针的成本是：

```text
每棵 cached rbtree 多一个指针字段；
插入时要判断新节点是不是最左；
删除最左节点时要更新缓存；
替换最左节点时要更新缓存。
```

如果同时缓存最右节点：

```text
结构体更大；
插入删除替换都要维护两套缓存；
所有 cached 用户都承担成本；
但只有少数用户真的需要 O(1) rb_last()。
```

所以 Linux 选择：

```text
内核统一提供 leftmost 缓存；
需要 rightmost 的用户可以自行维护。
```

这符合内核数据结构设计的一贯风格：

```text
只把广泛有价值的优化放进通用结构；
特殊需求留给具体使用者。
```

------

### 12.2.3\_rb\_insert\_color\_cached()\_的使用方式

cached 插入接口：

```c
static inline void rb_insert_color_cached(struct rb_node *node,
					  struct rb_root_cached *root,
					  bool leftmost)
{
	if (leftmost)
		root->rb_leftmost = node;
	rb_insert_color(node, &root->rb_root);
}
```

关键参数是：

```text
leftmost
```

它由调用者在搜索插入落点时判断。

搜索时初始：

```text
leftmost = true
```

只要向右走过一次：

```text
leftmost = false
```

因为一旦新节点落在某个节点右侧，它就不可能是整棵树最左节点。

`rb_add_cached()` 就是这样做的：

```text
从 root 开始搜索；
如果 less(node, parent)，向左；
否则向右并 leftmost = false；
挂接；
rb_insert_color_cached(node, tree, leftmost)。
```

这里要注意：

```text
rb_insert_color_cached() 本身不会重新判断 node 是否最左；
它相信调用者传入的 leftmost。
```

如果这个参数传错，树结构仍然可能合法，但 `rb_first_cached()` 会返回错误节点。

------

### 12.2.4\_rb\_erase\_cached()\_如何更新最左缓存

删除 cached 节点时，最重要的问题是：

```text
被删节点是不是 rb_leftmost？
```

如果不是：

```text
rb_leftmost 不变。
```

如果是：

```text
新的最左节点就是被删节点的中序后继。
```

源码逻辑：

```text
if (root->rb_leftmost == node)
	root->rb_leftmost = rb_next(node);

rb_erase(node, &root->rb_root);
```

为什么先 `rb_next(node)` 再 `rb_erase()`？

因为删除和修复可能旋转。

在节点还在树中时，`rb_next(node)` 可以根据当前结构找到中序后继。

删除完成后，node 已经不再适合作为遍历起点。

所以 cached 删除顺序是：

```text
先算新的 leftmost；
再执行结构删除和颜色修复。
```

------

### 12.2.5\_cached\_rbtree\_的使用边界

cached rbtree 适合：

```text
频繁取最小 key；
插入删除也比较频繁；
希望避免每次 rb_first() 从根向左走；
最左节点有明确业务意义。
```

不一定适合：

```text
很少取最小节点；
树规模很小；
主要按 key 查找而不是取最小；
需要同时缓存最小和最大。
```

使用 cached tree 时要统一使用 cached 接口：

```text
rb_first_cached()
rb_insert_color_cached()
rb_add_cached()
rb_erase_cached()
rb_replace_node_cached()
```

如果混用普通接口：

```text
rb_insert_color()
rb_erase()
rb_replace_node()
```

就可能忘记维护 `rb_leftmost`。

------

### 12.2.6\_本节小结

cached rbtree 的核心结论：

```text
第一，rb_root_cached 在普通 rb_root 外增加 rb_leftmost。

第二，rb_first_cached() 是 O(1) 获取最左节点。

第三，插入时 leftmost 参数必须由搜索路径正确计算。

第四，删除最左节点时，新 leftmost 是 rb_next(node)。

第五，cached 接口只维护最左节点，不维护最右节点。
```

------

## 12.3\_augmented\_rbtree\_增强红黑树

### 12.3.1\_什么是\_augmented\_rbtree

普通 rbtree 只维护：

```text
BST 排序关系；
红黑性质；
父子指针；
颜色。
```

augmented rbtree 还要求每个业务节点保存某种“子树聚合信息”。

典型例子：

```text
区间树中，每个节点保存子树最大 end；
这样查询某个点或区间是否重叠时，可以跳过不可能命中的子树。
```

增强信息可能是：

```text
子树最大值；
子树最小值；
子树区间上界；
子树统计量；
调度或内存管理中的聚合元数据。
```

普通 rbtree 不知道这些业务字段。

所以 Linux 用回调让使用者参与维护。

------

### 12.3.2\_struct\_rb\_augment\_callbacks\_的三个回调

增强树回调结构：

```c
struct rb_augment_callbacks {
	void (*propagate)(struct rb_node *node, struct rb_node *stop);
	void (*copy)(struct rb_node *old, struct rb_node *new);
	void (*rotate)(struct rb_node *old, struct rb_node *new);
};
```

三个回调分别处理三类变化。

`propagate`：

```text
从某个节点向上重新计算增强信息；
直到 stop 或根。
```

插入、删除后，沿路径上的祖先子树内容变了，需要传播更新。

`copy`：

```text
删除有两个孩子的节点时，successor 接替 node 的位置；
successor 需要复制 node 的增强信息。
```

`rotate`：

```text
旋转改变两个节点的子树范围；
old 和 new 的增强信息需要更新。
```

这三个回调正好对应 rbtree 结构变化的三个位置：

```text
路径变化；
节点替换；
旋转变化。
```

------

### 12.3.3\_增强信息为什么需要随旋转更新

旋转保持中序顺序，但会改变子树归属。

例如左旋：

```text
    old                 new
      \                /
      new     -->    old
      /                \
     T                T
```

中序顺序不变：

```text
old 左侧
old
T
new
new 右侧
```

但子树范围变了：

```text
old 旋转后不再覆盖 new 的右子树；
new 旋转后覆盖 old 整个局部子树。
```

如果增强信息是“子树最大 end”，那么：

```text
new 的增强信息通常先继承 old；
old 的增强信息需要根据新左右孩子重新计算。
```

`RB_DECLARE_CALLBACKS()` 生成的 rotate 回调就是这个思路：

```text
new->augmented = old->augmented;
重新计算 old。
```

这和旋转后的结构关系一致：

```text
new 接替 old 原来的局部子树根位置；
old 变成 new 的一个孩子。
```

------

### 12.3.4\_RB\_DECLARE\_CALLBACKS()\_与\_RB\_DECLARE\_CALLBACKS\_MAX()

`rbtree_augmented.h` 提供宏帮助生成回调。

通用宏：

```text
RB_DECLARE_CALLBACKS()
```

需要调用者提供：

```text
业务结构体类型；
rb_node 成员名；
增强字段名；
重新计算函数。
```

它生成：

```text
xxx_propagate()
xxx_copy()
xxx_rotate()
struct rb_augment_callbacks xxx
```

另一个常用宏：

```text
RB_DECLARE_CALLBACKS_MAX()
```

用于这种典型模式：

```text
节点的增强字段 = 当前节点值、左子树增强值、右子树增强值三者最大值。
```

这正适合区间树的 `subtree_last` / `max_end` 一类字段。

宏的价值是：

```text
减少手写回调错误；
统一旋转、复制、传播的处理模板；
让常见“子树最大值”增强模式更容易使用。
```

------

### 12.3.5\_rb\_insert\_augmented()\_的插入流程

增强树插入不能只调用：

```c
rb_insert_color()
```

而是调用：

```c
rb_insert_augmented(node, root, augment);
```

但在调用之前，使用者还必须：

```text
沿插入搜索路径更新增强信息。
```

原因是：

```text
新节点加入后，它的所有祖先子树内容都变了；
即使后面没有旋转，这些祖先的增强字段也可能需要更新。
```

插入流程应该是：

```text
搜索插入落点；
沿路径根据新节点更新祖先增强字段；
rb_link_node() 挂接；
rb_insert_augmented() 做红黑修复；
如果修复中发生旋转，rotate 回调更新旋转点增强字段。
```

`rb_add_augmented_cached()` 的源码中也体现了这一点：

```text
rb_link_node()
augment->propagate(parent, NULL)
rb_insert_augmented_cached()
```

注释里标了 `suboptimal`，因为它是在挂接后从 parent 向上统一传播，不一定是最优路径更新方式，但语义是完整的。

------

### 12.3.6\_rb\_erase\_augmented()\_的删除流程

增强树删除调用：

```c
rb_erase_augmented(node, root, augment);
```

内部仍然是两段：

```text
__rb_erase_augmented()
	结构删除，同时调用 copy / propagate；

__rb_erase_color()
	如果需要颜色修复，旋转时调用 augment->rotate。
```

删除时增强信息最容易出错的位置有两个。

第一，后继节点接替被删节点。

这时需要：

```text
augment->copy(node, successor)
```

让 successor 继承 node 原位置的增强信息。

第二，successor 从原位置移走。

这会改变 successor 原路径上的子树内容，所以需要：

```text
augment->propagate(parent, successor)
```

最后结构删除完成后，还会：

```text
augment->propagate(tmp, NULL)
```

继续向上修正。

如果删除修复发生旋转，则：

```text
augment_rotate(parent, sibling)
```

会更新旋转相关节点。

------

### 12.3.7\_增强树为什么容易让代码体积膨胀

`rbtree_augmented.h` 注释提到：

```text
被编译单元最好只有一个 rb_erase_augmented() 调用点，
因为内联会导致代码体积增加。
```

原因是：

```text
增强树为了性能，大量使用 __always_inline；
回调和删除骨架会被内联展开；
每个不同调用点都可能实例化一份较大的代码。
```

这是性能和代码体积的取舍。

内核倾向于：

```text
热点数据结构路径尽量减少间接调用；
允许局部代码体积增加；
但提醒使用者控制调用点。
```

------

### 12.3.8\_本节小结

augmented rbtree 的核心结论：

```text
第一，增强树在普通排序关系之外维护子树聚合信息。

第二，propagate、copy、rotate 分别处理路径传播、节点替换和旋转更新。

第三，插入增强树时，调用者要先维护插入路径上的增强信息。

第四，删除增强树时，结构删除和颜色修复都可能触发增强信息更新。

第五，增强树为了性能大量内联，代码体积更容易膨胀。
```

------

## 12.4\_rbtree\_与并发控制

### 12.4.1\_为什么\_rbtree\_核心不内置锁

Linux rbtree 不保存锁。

原因是不同使用场景的并发模型不同：

```text
有的树只在单线程初始化阶段使用；
有的树由 spinlock 保护；
有的树由 mutex 保护；
有的读侧走 RCU；
有的对象还有引用计数；
有的树嵌在更大的对象锁之下。
```

如果 rbtree 核心内置锁，会带来问题：

```text
锁类型无法统一；
锁粒度无法统一；
中断上下文和进程上下文需求不同；
可能和调用者已有锁重复；
无法处理对象生命周期。
```

所以 Linux rbtree 只提供结构操作。

并发保护由调用者决定。

------

### 12.4.2\_使用者需要保护哪些操作

至少需要保护：

```text
查找和插入之间的竞争；
两个插入之间的竞争；
插入和删除之间的竞争；
两个删除之间的竞争；
遍历和删除之间的竞争；
删除和对象释放之间的竞争；
替换和读者访问之间的竞争。
```

一个简单模型是：

```c
spin_lock(&tree->lock);
/* search / insert / erase / replace */
spin_unlock(&tree->lock);
```

如果查找结果要在解锁后使用，还需要：

```text
引用计数；
RCU；
对象生命周期保证；
或者复制数据而不是返回裸指针。
```

否则容易出现：

```text
查找到 item；
释放锁；
另一个 CPU 删除并释放 item；
当前 CPU 继续使用 item；
use-after-free。
```

------

### 12.4.3\_WRITE\_ONCE()\_在\_rbtree\_实现中的意义

`lib/rbtree.c` 开头有一段 lockless lookup 注释。

它强调：

```text
所有对 rb_left 和 rb_right 的树结构写入必须使用 WRITE_ONCE()。
```

目的不是提供完整无锁正确性。

它保证的是：

```text
读者不会因为临时结构看到循环而卡死；
遍历会最终结束；
如果读者返回某个元素，这个元素是正确的。
```

它不保证：

```text
读者一定能看到所有节点；
读者一定不会漏掉并发旋转影响的子树；
查找返回 NULL 就代表节点不存在；
对象生命周期自动安全。
```

所以 `WRITE_ONCE()` 是结构指针并发可遍历性的底线，不是完整同步方案。

------

### 12.4.4\_RCU\_查找与普通修改路径的区别

RCU 相关接口包括：

```text
rb_link_node_rcu()
rb_find_rcu()
rb_replace_node_rcu()
```

它们分别解决不同问题。

`rb_link_node_rcu()`：

```text
用 rcu_assign_pointer() 发布新节点链接；
保证读者看到链接时，新节点基本字段已经初始化。
```

`rb_find_rcu()`：

```text
读侧用 rcu_dereference_raw() 读取左右孩子；
允许 RCU 读路径下降查找。
```

`rb_replace_node_rcu()`：

```text
先准备 replacement；
最后用 RCU 方式更新父节点孩子指针；
让读者看到 new 时，new 内部关系已经设置好。
```

但是写侧修改仍然需要同步。

RCU 不是多个写者同时旋转、插入、删除的许可证。

删除后释放对象也必须等待 RCU 宽限期：

```text
rb_erase()
call_rcu()
宽限期后释放对象
```

------

### 12.4.5\_lockless\_lookup\_能保证什么\_不能保证什么

可以把 lockless lookup 的保证写成两列。

能保证：

```text
不会因为临时环导致无限循环；
遍历到的节点是有效结构节点；
如果找到了匹配元素，它是正确的。
```

不能保证：

```text
一定找到并发存在的节点；
一定看到完整树结构；
不需要锁或 RCU 生命周期；
删除对象后可以立即释放；
多个写者可以无锁并发修改。
```

所以工程上不能把 rbtree 当成自动无锁容器。

更准确的理解是：

```text
rbtree 的指针写入方式尽量不给无锁读者制造灾难；
但正确并发语义仍然由调用者设计。
```

------

### 12.4.6\_本节小结

并发部分的结论：

```text
第一，rbtree 不内置锁，因为锁模型属于使用场景。

第二，调用者必须保护 search/insert/erase/replace 的并发关系。

第三，返回业务对象指针时必须处理生命周期。

第四，WRITE_ONCE() 保证遍历不会陷入临时环，但不保证查找完整性。

第五，RCU 接口只处理读侧访问和发布顺序，不替代写侧同步。
```

------

## 12.5\_Linux\_内核\_rbtree\_示例代码

### 12.5.1\_示例目标与约束

下面构造一个最小示例：

```text
按 int key 管理 demo_rb_item；
不允许重复 key；
使用 spinlock 保护树；
插入、查找、删除都使用同一套比较规则；
删除后返回对象，由调用者释放。
```

这不是完整内核模块，只是展示 rbtree 使用骨架。

------

### 12.5.2\_定义业务结构体与树对象

```c
struct demo_rb_item {
	int key;
	int value;
	struct rb_node rb;
};

struct demo_rb_tree {
	struct rb_root root;
	spinlock_t lock;
	unsigned int count;
};
```

初始化：

```c
static void demo_tree_init(struct demo_rb_tree *tree)
{
	tree->root = RB_ROOT;
	spin_lock_init(&tree->lock);
	tree->count = 0;
}
```

这里 `struct rb_root` 只保存根节点。

锁和计数都是业务层自己加的。

------

### 12.5.3\_实现统一比较函数

```c
static int demo_cmp_key(int key, const struct demo_rb_item *item)
{
	if (key < item->key)
		return -1;
	if (key > item->key)
		return 1;
	return 0;
}

static int demo_cmp_item(const struct demo_rb_item *a,
			 const struct demo_rb_item *b)
{
	return demo_cmp_key(a->key, b);
}
```

这样查找和插入都能使用同一套 key 规则。

这比到处手写 `<`、`>` 更不容易写偏。

------

### 12.5.4\_实现查找

```c
static struct demo_rb_item *
demo_search_locked(struct demo_rb_tree *tree, int key)
{
	struct rb_node *node = tree->root.rb_node;

	while (node) {
		struct demo_rb_item *item;
		int cmp;

		item = rb_entry(node, struct demo_rb_item, rb);
		cmp = demo_cmp_key(key, item);

		if (cmp < 0)
			node = node->rb_left;
		else if (cmp > 0)
			node = node->rb_right;
		else
			return item;
	}

	return NULL;
}
```

函数名里带 `_locked`，表示调用者必须已经持有 `tree->lock`。

这是内核代码中常见的命名习惯：

```text
把锁语义写进函数名，避免误用。
```

------

### 12.5.5\_实现插入

```c
static int demo_insert(struct demo_rb_tree *tree,
		       struct demo_rb_item *item)
{
	struct rb_node **link = &tree->root.rb_node;
	struct rb_node *parent = NULL;

	spin_lock(&tree->lock);

	while (*link) {
		struct demo_rb_item *this;
		int cmp;

		parent = *link;
		this = rb_entry(parent, struct demo_rb_item, rb);
		cmp = demo_cmp_item(item, this);

		if (cmp < 0)
			link = &parent->rb_left;
		else if (cmp > 0)
			link = &parent->rb_right;
		else {
			spin_unlock(&tree->lock);
			return -EEXIST;
		}
	}

	rb_link_node(&item->rb, parent, link);
	rb_insert_color(&item->rb, &tree->root);
	tree->count++;

	spin_unlock(&tree->lock);
	return 0;
}
```

这里有几个关键点：

```text
发现重复 key 时，不调用 rb_link_node()；
只有成功找到空 link 后才挂接；
rb_link_node() 后立刻 rb_insert_color()；
count 在修复完成后增加；
整个修改路径在锁内完成。
```

------

### 12.5.6\_实现删除

```c
static int demo_remove(struct demo_rb_tree *tree, int key,
		       struct demo_rb_item **removed)
{
	struct demo_rb_item *item;

	if (!removed)
		return -EINVAL;

	*removed = NULL;

	spin_lock(&tree->lock);

	item = demo_search_locked(tree, key);
	if (!item) {
		spin_unlock(&tree->lock);
		return -ENOENT;
	}

	rb_erase(&item->rb, &tree->root);
	RB_CLEAR_NODE(&item->rb);
	tree->count--;
	*removed = item;

	spin_unlock(&tree->lock);
	return 0;
}
```

这个函数只从树中摘除节点，不释放对象。

调用者可以根据生命周期选择：

```text
kfree(item);
demo_item_put(item);
call_rcu(&item->rcu, demo_item_free_rcu);
```

这正是 Linux rbtree 的对象生命周期边界。

------

### 12.5.7\_实现中序遍历

```c
static void demo_dump_locked(struct demo_rb_tree *tree)
{
	struct rb_node *node;

	for (node = rb_first(&tree->root); node; node = rb_next(node)) {
		struct demo_rb_item *item;

		item = rb_entry(node, struct demo_rb_item, rb);
		pr_info("key=%d value=%d\n", item->key, item->value);
	}
}
```

中序遍历输出顺序就是 key 从小到大。

如果遍历期间可能有并发修改，也要持锁或使用合适的生命周期模型。

------

### 12.5.8\_实现整棵树清理

一种简单清理方式是反复取最小节点：

```c
static void demo_clear(struct demo_rb_tree *tree)
{
	struct rb_node *node;

	spin_lock(&tree->lock);
	while ((node = rb_first(&tree->root))) {
		struct demo_rb_item *item;

		item = rb_entry(node, struct demo_rb_item, rb);
		rb_erase(node, &tree->root);
		RB_CLEAR_NODE(node);
		tree->count--;
		spin_unlock(&tree->lock);

		kfree(item);

		spin_lock(&tree->lock);
	}
	spin_unlock(&tree->lock);
}
```

也可以使用后序遍历做销毁，但要注意第 11 章讲过的限制：

```text
postorder safe 不适合循环体中随意 rb_erase() 导致重平衡后继续依赖原遍历关系。
```

最保守的写法是：

```text
每次 rb_first()；
每次 rb_erase()；
直到树空。
```

逻辑简单，不容易被旋转影响。

------

### 12.5.9\_示例代码的边界

这个示例没有覆盖：

```text
RCU 读侧；
引用计数；
重复 key；
cached rbtree；
augmented rbtree；
错误注入；
模块参数；
调试断言。
```

但它覆盖了普通 rbtree 使用的核心闭环：

```text
定义对象；
嵌入 rb_node；
统一比较；
查找；
插入；
删除；
遍历；
生命周期交给调用者。
```

------

## 12.6\_Linux\_rbtree\_调试与验证

### 12.6.1\_如何验证\_BST\_有序性

最直接的方法是中序遍历。

遍历时记录上一个 key：

```text
prev_key <= current_key
```

如果不允许重复：

```text
prev_key < current_key
```

一旦出现逆序，说明：

```text
插入比较规则错误；
替换节点 key 错误；
手写 search / insert 不一致；
或者某处错误修改了 rb_left / rb_right。
```

红黑修复不会主动检查业务 key。

所以 BST 有序性验证必须由业务层或调试工具完成。

------

### 12.6.2\_如何验证父指针正确性

递归或栈遍历整棵树，对每个节点检查：

```text
如果 node->rb_left 存在：
	rb_parent(node->rb_left) == node

如果 node->rb_right 存在：
	rb_parent(node->rb_right) == node
```

根节点检查：

```text
rb_parent(root->rb_node) == NULL
```

父指针错误常见来源：

```text
手写旋转错误；
错误使用 rb_replace_node()；
破坏 __rb_parent_color；
把节点重复插入不同树；
删除后继续把旧节点当树中节点使用。
```

------

### 12.6.3\_如何验证红节点没有红孩子

遍历每个节点：

```text
如果 node 是红色：
	left 必须是 NULL 或黑色；
	right 必须是 NULL 或黑色。
```

Linux 中 NULL 叶子按黑色理解。

所以检查逻辑是：

```text
NULL 不算红；
非 NULL 才需要 rb_is_red()。
```

如果出现红红冲突，重点排查：

```text
插入后是否忘记 rb_insert_color()；
删除修复是否被跳过；
是否手动改过颜色；
是否误用 rb_replace_node() 替换了不等价节点。
```

------

### 12.6.4\_如何验证所有路径黑高一致

黑高验证可以递归实现。

对每个节点：

```text
左子树黑高；
右子树黑高；
二者必须相等；
当前节点是黑色则返回子树黑高 + 1；
当前节点是红色则返回子树黑高。
```

NULL 叶子按黑色叶子处理时，要统一计数规则。

可以约定：

```text
NULL 返回 1；
黑色实体节点在子树黑高基础上 +1；
红色实体节点不增加。
```

也可以约定：

```text
NULL 返回 0；
只统计实体黑节点。
```

关键是整棵检查使用同一套规则。

黑高不一致通常说明：

```text
删除黑色节点后没有正确修复；
Case 2 向上推进处理错；
Case 4 染色错；
父指针或旋转导致子树接错。
```

------

### 12.6.5\_如何验证\_cached\_rbtree\_的\_rb\_leftmost

cached 验证很简单：

```text
rb_first(&root->rb_root) == root->rb_leftmost
```

如果不相等，说明 cached 信息失效。

常见原因：

```text
插入时 leftmost 参数算错；
对 cached tree 调用了普通 rb_insert_color()；
删除时调用了普通 rb_erase()；
替换最左节点时调用了普通 rb_replace_node()；
手动移动节点但没有维护 rb_leftmost。
```

------

### 12.6.6\_如何验证\_augmented\_rbtree\_的增强信息

增强树验证要按业务字段重算。

例如增强字段是子树最大 end：

```text
expected = node->end;
if (left)
	expected = max(expected, left->subtree_max);
if (right)
	expected = max(expected, right->subtree_max);
node->subtree_max 必须等于 expected。
```

可以整树递归重新计算一遍，并与节点保存值比较。

如果错误，重点排查：

```text
插入搜索路径上是否更新了增强信息；
rotate 回调是否正确；
copy 回调是否正确；
删除 successor 原路径是否 propagate；
是否混用了普通 rb_insert_color() / rb_erase()。
```

------

### 12.6.7\_如何构造插入修复测试序列

可以构造三类插入序列。

父红叔红：

```text
插入形成 4-node 分裂。
例如先让祖父有两个红孩子，再向其中一个红孩子下插入。
```

内侧结构：

```text
LR：插入 30, 10, 20
RL：插入 10, 30, 20
```

外侧结构：

```text
LL：插入 30, 20, 10
RR：插入 10, 20, 30
```

这些序列能触发：

```text
Case 1 染色；
Case 2 预旋转；
Case 3 最终旋转。
```

------

### 12.6.8\_如何构造删除修复测试序列

删除修复测试更适合从目标形态反推。

要覆盖：

```text
兄弟红；
兄弟黑双侄黑；
兄弟黑近侄红；
兄弟黑远侄红。
```

测试思路：

```text
先构造一棵合法红黑树；
选择删除一个黑色叶子或黑色单子树位置；
观察 rebalance parent、sibling、near nephew、far nephew。
```

不要只看最终中序结果。

删除测试应该同时验证：

```text
BST 有序性；
父指针；
红红冲突；
黑高一致；
root 为黑；
遍历前驱后继；
cached / augmented 信息。
```

------

### 12.6.9\_本节小结

调试验证要分层：

```text
BST 层：
	中序顺序。

结构层：
	父指针、root、左右孩子。

红黑层：
	根黑、红节点无红孩子、黑高一致。

工程扩展层：
	cached leftmost、augmented 字段。

生命周期层：
	删除后不再通过树访问、对象释放安全。
```

只验证中序遍历不够。

一棵树可能中序顺序正确，但红黑性质已经坏了，后续复杂插入删除迟早出问题。

------

## 12.7\_Linux\_rbtree\_常见误区

### 12.7.1\_误以为内核\_rbtree\_会自动比较\_key

不会。

`struct rb_node` 不保存 key。

比较逻辑必须由调用者提供。

------

### 12.7.2\_误以为\_rb\_link\_node()\_已完成红黑修复

没有。

`rb_link_node()` 只做 BST 挂接。

挂接后必须调用：

```c
rb_insert_color()
```

或增强树版本：

```c
rb_insert_augmented()
```

------

### 12.7.3\_误以为\_rb\_erase()\_会释放业务对象

不会。

`rb_erase()` 只摘除 `rb_node`。

业务对象释放由调用者决定。

------

### 12.7.4\_误以为\_rb\_replace\_node()\_可以替换任意\_key

不能。

`rb_replace_node()` 不重新比较。

replacement 必须保持相同排序位置。

------

### 12.7.5\_误以为遍历时可以任意删除节点

不能。

`rb_erase()` 可能旋转，破坏遍历过程中预期的结构关系。

删除遍历要专门设计。

------

### 12.7.6\_误以为\_rbtree\_自带并发保护

没有。

锁、RCU、引用计数都属于调用者责任。

------

### 12.7.7\_误以为\_RCU\_接口让所有修改路径都无锁安全

不会。

RCU 接口主要处理读侧访问和发布顺序。

多个写者之间仍然需要同步。

------

### 12.7.8\_误以为\_cached\_/\_augmented\_会自动维护业务字段

不会。

cached 需要正确维护 `leftmost`。

augmented 需要正确提供并调用回调。

------

## 12.8\_Linux\_rbtree\_在内核中的典型使用场景

### 12.8.1\_高精度定时器为什么适合缓存最小节点

高精度定时器关心：

```text
下一个最早到期的定时器是谁？
```

这就是最小 key 查询。

如果 key 是到期时间，最早到期就是最左节点。

因此 cached rbtree 非常适合这种场景：

```text
插入删除保持有序；
rb_first_cached() O(1) 获取最早到期事件。
```

------

### 12.8.2\_调度实体按虚拟运行时间排序的思路

调度器需要在可运行实体中选择合适对象。

如果按虚拟运行时间排序：

```text
vruntime 小的实体更靠左；
最左节点代表当前最应该运行的实体之一。
```

rbtree 能提供：

```text
动态插入；
动态删除；
按 vruntime 排序；
快速找到最小 vruntime。
```

这种场景同样能从 leftmost 缓存受益。

------

### 12.8.3\_I/O\_调度与按位置排序

I/O 请求可能按扇区、偏移或设备位置排序。

有序结构可以支持：

```text
找到相邻请求；
合并相邻区间；
按位置选择下一个请求；
减少随机跳转成本。
```

哈希表适合等值查找，但不适合前驱后继和范围邻近关系。

rbtree 的中序关系正适合这类需求。

------

### 12.8.4\_epoll\_等对象集合为什么可能需要有序管理

某些对象集合不仅需要保存对象，还需要：

```text
按 fd、地址、时间或其他 key 管理；
快速查找；
有序遍历；
插入删除稳定。
```

rbtree 可以作为底层有序集合。

但是否使用 rbtree，要看具体内核版本和具体子系统实现。

不要把“某场景历史上用过 rbtree”理解成“永远必须用 rbtree”。

------

### 12.8.5\_VMA\_历史上使用\_rbtree\_与后来转向\_Maple\_Tree\_的原因

VMA 是虚拟内存区域。

它天然是范围结构：

```text
[start, end)
```

历史上可以用 rbtree 按起始地址组织 VMA。

这样能支持：

```text
按地址查找所在 VMA；
查找前驱后继；
插入删除区间。
```

但 VMA 管理不是单纯的点 key 有序集合。

它更偏向：

```text
范围查找；
范围更新；
减少锁竞争；
更适合缓存和批量遍历的数据结构。
```

Maple Tree 正是面向这类范围映射场景的更现代结构。

更准确地说，新内核的 VMA 管理已经从传统：

```text
mm_struct
	-> mmap        // VMA 链表
	-> mm_rb       // VMA 红黑树
```

转向：

```text
mm_struct
	-> mm_mt       // maple_tree
```

也就是：

```c
struct maple_tree mm_mt;
```

VMA 查找、遍历、插入、删除更多走 `maple_tree` / `vma_iterator` 这一套。

Maple Tree 官方文档把它描述为一种 B-Tree 数据类型，优化用于保存非重叠范围，支持范围迭代、cache-efficient 的 previous / next 访问和 RCU-safe 模式，并明确说它最重要的用途是跟踪 VMA。([Linux Kernel Documentation](https://docs.kernel.org/core-api/maple_tree.html))

Maple Tree 引入补丁系列也明确提到，它替换了 VMA 管理里的 augmented rbtree、VMA cache 和 VMA linked list；补丁组织中还包含从 `mm_struct` 移除 rbtree、引入 VMA iterator 等修改。([LKML](https://lkml.iu.edu/2202.1/09876.html))

但这句话不能扩大成：

```text
新内核已经不用 rbtree；
任务管理也改成 Maple Tree；
所有有序集合都应该换成 Maple Tree。
```

更稳的边界是：

| 子系统 | 新内核主要结构或方向 |
| --- | --- |
| 虚拟内存 VMA 管理 | Maple Tree |
| 页缓存 / 一些整数 ID 索引 | XArray / radix tree 演进 |
| 普通内核有序集合 | rbtree 仍然大量存在 |
| 公平调度任务选择 | 从 CFS 语义走向 EEVDF，不是 Maple Tree |

任务调度容易和这里混淆。

老 CFS 经典讲法里，可运行实体按 `vruntime` 组织在 `tasks_timeline` 红黑树上。

新内核公平调度的核心语义转向 EEVDF，关注的是 lag 和 virtual deadline，选择 eligible 且虚拟截止时间更早的任务。([Linux Kernel Documentation](https://docs.kernel.org/scheduler/sched-eevdf.html))

这属于调度算法语义变化，不是“调度任务也改用 Maple Tree”。

所以这里的学习重点是：

```text
rbtree 适合有序对象集合；
Maple Tree 更适合某些范围映射和 VMA 场景；
数据结构选择要看访问模式，而不是只看复杂度公式。
```

一句话总结：

```text
Maple Tree 主要替代的是内存管理里的 VMA rbtree / linked list 模型；
它不是泛泛替代整个内核中的 rbtree，也不是任务调度 EEVDF 的同义词。
```

------

### 12.8.6\_interval\_tree\_与\_augmented\_rbtree\_的关系

区间树是 augmented rbtree 的典型应用。

每个节点保存：

```text
区间起点；
区间终点；
子树最大终点。
```

按起点排序形成 BST。

查询某个区间是否重叠时，根据子树最大终点剪枝：

```text
如果左子树最大终点小于查询起点；
左子树不可能有重叠区间；
可以跳过。
```

这就是增强信息的价值：

```text
不改变 rbtree 的排序结构；
额外保存子树摘要；
让查询可以剪枝。
```

------

### 12.8.7\_从使用场景反推结构选择

可以用下面的方式选择结构：

| 需求 | 更可能适合的结构 |
| --- | --- |
| 小规模简单遍历 | 链表 |
| 等值查找 | 哈希表 |
| 整数索引到对象 | XArray / radix 类结构 |
| 有序 key、前驱后继、最小最大 | rbtree |
| 频繁取最小 key | cached rbtree |
| 需要子树聚合信息 | augmented rbtree |
| 范围映射和区间管理 | Maple Tree / interval tree 等 |

不要把 rbtree 当万能结构。

它解决的是：

```text
动态有序集合。
```

------

## 12.9\_本章小结

本章把 Linux rbtree 的工程扩展收束起来。

cached rbtree 的核心是：

```text
用 rb_leftmost 把 rb_first() 优化成 O(1)；
代价是插入、删除、替换时必须维护缓存。
```

augmented rbtree 的核心是：

```text
在普通红黑树结构之外维护子树增强信息；
通过 propagate、copy、rotate 三个回调覆盖路径传播、节点替换和旋转更新。
```

并发控制的核心是：

```text
rbtree 不内置锁；
WRITE_ONCE() 让无锁遍历不至于陷入结构环；
RCU 接口处理发布和读侧访问；
完整并发语义仍然属于调用者。
```

调试验证的核心是：

```text
不能只验证中序顺序；
还要验证父指针、红黑性质、黑高、cached leftmost、augmented 字段和对象生命周期。
```

到这里，Linux 6.12 rbtree 的工程实现主线已经完整：

```text
基础结构
	↓
嵌入式节点与使用者接口
	↓
查找、插入与旋转修复
	↓
删除、遍历与替换
	↓
cached / augmented / 并发 / 验证 / 场景
```

下一章可以从红黑树扩展到 B 树 / B+ 树，重点不再是内存中的二叉平衡，而是多路平衡、页级索引和范围查询。
