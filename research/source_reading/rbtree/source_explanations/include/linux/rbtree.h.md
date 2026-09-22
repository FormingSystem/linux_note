---
id: research.source_reading.rbtree.lookup_implementation
title: "rbtree.h 查找与节点接入实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_rbtree.h查找与节点接入实现

[模块导读](../../../navigation/P02_查找路径与返回边界导读.md#2.2_按一次查找定位源码)已经建立共享根与局部游标的 L0～L3。下面核对固定 [include/linux/rbtree.h](../../../../linux/include/linux/rbtree.h) 中的实际分支；版本和范围见[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界)。中文 Doxygen 为仓库补充，函数体保持上游语句；这些片段依赖内核头文件，不是独立可编译程序。

## 1.1\_rb\_find的任意匹配

调用者提供查询值的地址和 cmp；cmp 按这次查询解释 key，并从 rb_node 找回业务字段。零只是“符合当前查询条件”，不证明树中只有一个对象。该函数不改树，唯一逐轮更新的是局部 node。

```c
/**
 * rb_find - 仓库阅读说明：沿排序路径返回一个匹配节点。
 * @key: 比较函数理解的查询对象。
 * @tree: 已由调用者提供适当保护的树。
 * @cmp: 负数向左、正数向右、零表示匹配。
 *
 * 返回匹配节点或 NULL；不会取得引用、加锁或验证树结构。
 */
static __always_inline struct rb_node *
rb_find(const void *key, const struct rb_root *tree,
	int (*cmp)(const void *key, const struct rb_node *))
{
	struct rb_node *node = tree->rb_node;

	while (node) {
		int c = cmp(key, node);

		if (c < 0)
			node = node->rb_left;
		else if (c > 0)
			node = node->rb_right;
		else
			return node;
	}

	return NULL;
}
```

L0 复制根，L1 调用 cmp，L2 根据符号只取一个孩子。相等立即退出，所以三个同键对象中先遇到谁就返回谁。稳定树与相容比较下，走到 NULL 才能表示这次查询没有匹配；并发弱查询另见 1.4。

修改边界：比较不能通过 int 相减来偷懒处理任意 int 键，否则极值可能有溢出风险。应由业务 cmp 使用关系比较产生负/零/正结果。不能把返回节点当成已取得引用，也不能在比较期间随意改变排序键。

## 1.2\_rb\_find\_first的候选保存

在 L2 的相等分支只保存候选，然后继续向左；这正是旋转可能把相等成员放到左侧时需要的增量。match 属于本次调用，不是树上的缓存字段。

```c
/**
 * rb_find_first - 仓库阅读说明：找到查询等价区间的最左节点。
 * @key: 查询值。
 * @tree: 排序关系与查询比较相容的树。
 * @cmp: 将匹配对象组织为连续区间的比较函数。
 *
 * 相等时先保存候选再向左，空树或完全无匹配返回 NULL。
 */
static __always_inline struct rb_node *
rb_find_first(const void *key, const struct rb_root *tree,
	      int (*cmp)(const void *key, const struct rb_node *))
{
	struct rb_node *node = tree->rb_node;
	struct rb_node *match = NULL;

	while (node) {
		int c = cmp(key, node);

		if (c <= 0) {
			if (!c)
				match = node;
			node = node->rb_left;
		} else if (c > 0) {
			node = node->rb_right;
		}
	}

	return match;
}
```

如果当前比较小于零，应向左排除右侧；如果等于零，当前节点可以作为答案，但左侧可能还有更早候选，所以两种情况共用向左分支。如果最终向左走空，之前保存的 match 不丢失。若从未相等，match 保持初始 NULL。

可以改变的是业务等价区间，例如按复合键建树后仅按第一字段查询。不能改成与中序次序不相容的比较，也不能遇到相等就立即返回后仍声称得到最左匹配。实际状态流程和 first/next 协作见[模块 2.3](../../../navigation/P02_查找路径与返回边界导读.md#2.3_等价区间与后继协作)。

## 1.3\_rb\_next\_match与匹配遍历宏

这里输入的是已经匹配的当前节点。函数先求中序后继，再检查这个候选；后继不匹配时就结束连续区间，而不是继续扫描整棵树寻找下一个偶然相等者。

```c
/**
 * rb_next_match - 仓库阅读说明：沿中序后继前进一个匹配成员。
 * @key: 与当前组相同的查询值。
 * @node: 有效的当前匹配节点，不能传 NULL 代替起点。
 * @cmp: 与 first 相容的比较规则。
 *
 * 依赖 rb_next 的稳定树契约；父指针遍历不继承向下弱查询保证。
 */
static __always_inline struct rb_node *
rb_next_match(const void *key, struct rb_node *node,
	      int (*cmp)(const void *key, const struct rb_node *))
{
	node = rb_next(node);
	if (node && cmp(key, node))
		node = NULL;
	return node;
}

/**
 * rb_for_each - 仓库阅读说明：从最左匹配开始遍历同一个查询组。
 * @node: 调用者提供的游标变量。
 * @key: 查询值。
 * @tree: 树入口。
 * @cmp: 本组的比较函数。
 *
 * 宏不加锁、不增加引用，也不允许随意旋转或删除正在遍历的树。
 */
#define rb_for_each(node, key, tree, cmp) \
	for ((node) = rb_find_first((key), (tree), (cmp)); \
	     (node); (node) = rb_next_match((key), (node), (cmp)))
```

宏先调用 first；若它返回 NULL，循环体根本不执行。非空才会在循环尾调用 next_match。不要从“遍历宏”推断它是 safe 变体：如果循环体释放当前对象，下一步可能再用该地址调用 rb_next；若发生结构重排，也可能破坏遍历路径。这里只检查区间边界，不承担删除协议。

可修改性：把 first 换成普通 find 会跳过匹配区间前半段；把 cmp 的非零检查删除会越过同键组。rb_next 的实现位于固定 lib/rbtree.c，它是否沿父链上行取决于树形；本节保留调用契约，不重复展开后继函数体。

## 1.4\_rb\_find\_rcu的孩子读取与缺失边界

这仍是 L0～L3 的向下查找。读取孩子时使用 rcu_dereference_raw，并不自动建立读侧临界区；raw 也不替调用者执行其寿命方案的正确性检查。关系图与一次旋转交错见[模块 2.1](../../../navigation/P02_查找路径与返回边界导读.md#2.1_谁保存状态谁决定答案)和[模块 2.4](../../../navigation/P02_查找路径与返回边界导读.md#2.4_旋转期间沿什么路径继续)。

```c
/**
 * rb_find_rcu - 仓库阅读说明：用 RCU 方式取得下一孩子地址。
 * @key: 查询对象。
 * @tree: 调用者保证入口访问与对象寿命的树。
 * @cmp: 读取有效且排序语义稳定的业务字段。
 *
 * 并发旋转可能造成假阴性；不加锁、不重扫、不取得引用。
 */
static __always_inline struct rb_node *
rb_find_rcu(const void *key, const struct rb_root *tree,
	    int (*cmp)(const void *key, const struct rb_node *))
{
	struct rb_node *node = tree->rb_node;

	while (node) {
		int c = cmp(key, node);

		if (c < 0)
			node = rcu_dereference_raw(node->rb_left);
		else if (c > 0)
			node = rcu_dereference_raw(node->rb_right);
		else
			return node;
	}

	return NULL;
}
```

注意第一句与普通 find 相同：`tree->rb_node` 没有在本函数内改成 rcu_dereference_raw。源码没有授予调用者任意发布或替换根的权利，调用点仍须证明入口访问符合自己的并发协议。之后取得孩子地址，也不意味着外围对象能在退出保护后继续使用。

固定 lib/rbtree.c 的开头注释同时要求孩子写入使用 WRITE_ONCE 和程序顺序不制造临时环，并声明旋转非原子、可能漏掉子树、父指针循环不在该检查范围。不要把这段论证压缩为“WRITE_ONCE 保证一切遍历安全”。返回候选只在比较与寿命前提下有意义；返回 NULL 不能排除并发存在的目标。

可修改性：不要把这里的 raw 读取机械替换进 first/next 就宣称得到并发等价遍历，也不要只增加一次读屏障就承诺查找完整性。若业务需要严格缺失，须由外层同步或经过证明的复核协议提供。通用 raw 取得本体已在[RCU 公共实现](../../../../rcu/source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.3.4_rcu_dereference_raw的无检查取得)唯一展开，本节只解释它在孩子读取处的职责。

返回[模块导读](../../../navigation/P02_查找路径与返回边界导读.md#2.5_返回以后还缺什么)或[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。

## 1.5\_红叶挂接与发布

查找单元只是读一个槽；插入 I0 的循环保存槽本身的地址，I1 才有权把新节点写进去。`rb_link_node` 只完成接入，必须由后续颜色修复闭合红黑性质。相应状态图与时序见[插入模块](../../../navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)，本节落在 I1 箭头。

```c
/**
 * rb_link_node - 仓库阅读说明：将私有红叶接入已找到的空槽。
 * @node: 尚未属于任何树的有效新节点。
 * @parent: 新父节点或 NULL。
 * @rb_link: 当前为空的根槽或父孩子槽。
 *
 * 不取得锁、不分配对象，也不检测节点是否已经在树中。
 */
static inline void rb_link_node(struct rb_node *node, struct rb_node *parent,
				struct rb_node **rb_link)
{
	node->__rb_parent_color = (unsigned long)parent;
	node->rb_left = node->rb_right = NULL;

	*rb_link = node;
}

/**
 * rb_link_node_rcu - 仓库阅读说明：以 RCU 发布语义完成最后的槽写入。
 * @node: 已初始化业务字段的新节点。
 * @parent: 新父节点或 NULL。
 * @rb_link: 调用者已找到并保护的空槽。
 *
 * 不取得锁、不分配对象，也不检测节点是否已经在树中。
 */
static inline void rb_link_node_rcu(struct rb_node *node, struct rb_node *parent,
				    struct rb_node **rb_link)
{
	node->__rb_parent_color = (unsigned long)parent;
	node->rb_left = node->rb_right = NULL;

	rcu_assign_pointer(*rb_link, node);
}
```

实现原理：先在新节点内写父地址和空孩子，再写共享槽。红色为零，因此父地址转成 unsigned long 后低颜色位保持红。RCU 版本只改变最后的发布方式，不能让后面的旋转原子化，也不获取写锁。普通版本适用于调用者已经建立相应保护的路径；RCU 版本需要对象字段在发布前完成初始化并有完整的读侧/回收协议。

可修改性：`*rb_link` 必须为预期空槽，不能拿一个已入树节点再初始化，否则会清空它的孩子并破坏原树。不能先发布地址再初始化业务字段；颜色修复也必须晚于节点接入。没有错误返回不意味着任意参数都被接受。

## 1.6\_不查重的rb\_add

如果业务允许等价节点，或已经在外层保证这次不重复，辅助接口可以把 I0、I1 与修复入口串起来。less 返回 false 时都会向右，包括相等；旋转后相等成员并不保证仍只在右侧。

```c
/**
 * rb_add - 仓库阅读说明：用 less 找落点并完成普通插入。
 * @node: 私有待插节点。
 * @tree: 调用者保护的树。
 * @less: 定义中序次序的布尔比较，不负责查重。
 *
 * 不取得锁、不分配对象，也不检测节点是否已经在树中。
 */
static __always_inline void
rb_add(struct rb_node *node, struct rb_root *tree,
       bool (*less)(struct rb_node *, const struct rb_node *))
{
	struct rb_node **link = &tree->rb_node;
	struct rb_node *parent = NULL;

	while (*link) {
		parent = *link;
		if (less(node, parent))
			link = &parent->rb_left;
		else
			link = &parent->rb_right;
	}

	rb_link_node(node, parent, link);
	rb_insert_color(node, tree);
}
```

实现原理：link 总是某个可写指针字段的地址，parent 保存该字段所属的父节点；退出 while 才调用挂接和修复。这里没有 cmp=0 分支，不会因全序比较存在就自动拒绝完全相同的键。

可修改性：less 必须建立一致的严格排序关系，不能在搜索途中改键或返回不稳定结果。若业务要求发现已存在对象，应该选择下一节的接口或外层查重，而不是在 rb_add 返回后再猜是否插入。

## 1.7\_查重后插入与RCU发布变体

find_add 的返回语义与 find 不同：非 NULL 表示已有等价对象，待插节点没有接入；NULL 表示查找未命中并已完成插入。调用者若事先分配了待插对象，重复分支还需自行回收或保留它。

```c
/**
 * rb_find_add - 仓库阅读说明：命中已有等价节点或接入新节点。
 * @node: 用来查重或插入的私有节点。
 * @tree: 调用者保护的树。
 * @cmp: 负/零/正的节点比较函数。
 *
 * 不取得锁、不分配对象，也不检测节点是否已经在树中。
 */
static __always_inline struct rb_node *
rb_find_add(struct rb_node *node, struct rb_root *tree,
	    int (*cmp)(struct rb_node *, const struct rb_node *))
{
	struct rb_node **link = &tree->rb_node;
	struct rb_node *parent = NULL;
	int c;

	while (*link) {
		parent = *link;
		c = cmp(node, parent);

		if (c < 0)
			link = &parent->rb_left;
		else if (c > 0)
			link = &parent->rb_right;
		else
			return parent;
	}

	rb_link_node(node, parent, link);
	rb_insert_color(node, tree);
	return NULL;
}

/**
 * rb_find_add_rcu - 仓库阅读说明：在未命中分支采用 RCU 发布。
 * @node: 私有待插节点。
 * @tree: 调用者保护并负责寿命协议的树。
 * @cmp: 与树序相容的节点比较函数。
 *
 * 不取得锁、不分配对象，也不检测节点是否已经在树中。
 */
static __always_inline struct rb_node *
rb_find_add_rcu(struct rb_node *node, struct rb_root *tree,
		int (*cmp)(struct rb_node *, const struct rb_node *))
{
	struct rb_node **link = &tree->rb_node;
	struct rb_node *parent = NULL;
	int c;

	while (*link) {
		parent = *link;
		c = cmp(node, parent);

		if (c < 0)
			link = &parent->rb_left;
		else if (c > 0)
			link = &parent->rb_right;
		else
			return parent;
	}

	rb_link_node_rcu(node, parent, link);
	rb_insert_color(node, tree);
	return NULL;
}
```

实现原理：两者搜索时都使用普通指针读取和同一 cmp 分支；区别是未命中后使用普通挂接还是 RCU 发布挂接。二者都在同一次调用内调用颜色修复，但都没有写侧互斥。这也是为什么名字带 RCU 不意味着可以让两个写者同时操作。

可修改性：把 NULL 当作插入失败会颠倒所有权处理；非 NULL 时释放已有对象而非私有待插对象也会破坏索引。RCU 变体没有把搜索改成读侧接口，仍须按写者路径串行化。根/父槽、修复游标和回调时机见[插入导读](../../../navigation/P03_红叶接入与冲突修复导读.md#3.1_空槽与修复游标各归谁所有)，公共修复入口见[lib/rbtree.c](../../lib/rbtree.c.md#1.4_普通与增广入口)。

返回[插入模块](../../../navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)或[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。
