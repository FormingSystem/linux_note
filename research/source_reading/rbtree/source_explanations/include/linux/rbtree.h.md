---
id: research.source_reading.rbtree.lookup_implementation
title: "rbtree.h 查询接入与遍历接口实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_rbtree.h查询接入与遍历接口实现

先识别节点与根时，从[布局状态导读](../../../navigation/P07_节点布局与编码状态导读.md#7.1_先识别三个存储对象)进入本页的取父、业务地址还原和游离宏；查询与更新任务仍按下文各模块阅读。

[查找模块](../../../navigation/P02_查找路径与返回边界导读.md#2.2_按一次查找定位源码)已经建立共享根与局部游标的 L0～L3，后续插入、删除与[遍历模块](../../../navigation/P05_有序推进与整树销毁导读.md#5.1_拓扑与游标分别保存在哪)继续说明接入、标记和循环的职责。下面分别核对固定 [include/linux/rbtree.h](../../../../linux/include/linux/rbtree.h) 中的实际分支；版本和范围见[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界)。中文 Doxygen 为仓库补充，函数体保持上游语句；这些片段依赖内核头文件，不是独立可编译程序。

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

可修改性：把 first 换成普通 find 会跳过匹配区间前半段；把 cmp 的非零检查删除会越过同键组。[rb_next 的唯一实现](../../lib/rbtree.c.md#1.7_中序端点与父链推进)按树形决定是否沿父链上行；本节只保留调用契约。

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

## 1.8\_游离标记不等于成员搜索

[删除模块 D4](../../../navigation/P04_对象摘除与缺黑修复导读.md#4.4_怎样观察地址身份与退出条件)需要区分库已摘除和业务已经记录摘除。下面第一个宏读根入口，后两个宏只读写节点自身的打包字段，不沿 root 搜索。上游位置为本页的 include/linux/rbtree.h。

```c
/**
 * RB_EMPTY_ROOT - 仓库阅读说明：读取一次根指针，检查是否为空。
 * @root: 有效且仍存活的根对象，不是 rb_node。
 * 不遍历树、不检查缓存一致性，不替调用者提供互斥与回收保护。
 */
#define RB_EMPTY_ROOT(root)  (READ_ONCE((root)->rb_node) == NULL)
/**
 * RB_EMPTY_NODE - 仓库阅读说明：读取约定的游离标记。
 * @node: 必须是仍存活的节点地址，不能为 NULL。
 * 未初始化或删除后未清标记，不可据此推断真实成员关系。
 */
#define RB_EMPTY_NODE(node)  \
	((node)->__rb_parent_color == (unsigned long)(node))
/**
 * RB_CLEAR_NODE - 仓库阅读说明：将自身地址写入打包字段。
 * @node: 已确认不在树中且仍存活的节点。
 * 不释放对象、不清左右孩子、不更新父槽，也不提供锁。
 */
#define RB_CLEAR_NODE(node)  \
	((node)->__rb_parent_color = (unsigned long)(node))
```

中文 Doxygen 为仓库补充、非上游原文。**实现原理：** RB_EMPTY_ROOT 检查的是这次读取的根槽，READ_ONCE 不提供全树快照、锁或对象寿命；最左缓存也未被检查。根赋值如何只改变一个入口，可在 [P08 根值实验](../../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#2%29_观察根值复制与对象存活)观察。另两个宏利用节点自身地址表示已知游离：合法在树节点的父地址不会是自己；根的父地址为零。rb_link_node 会覆盖游离标记，而 rb_erase 不自动写此标记。这是调用者维护的一项约定，不是成员搜索，也不能识别任意树归属。

**可修改性：** 在节点仍被树引用时清标记会破坏父链，已经释放后再清则是无效访问。业务若使用此宏防重复删除，必须从初始化、成功入树、摘除后清理到复用始终维护同一协议；只有锁定寿命与成员变化之后，状态检查才有意义。已脱离树但仍有 RCU 读者使用旧节点时，更不能未审查读者路径就立即改写旧字段。

本簇对应[关系图](../../../navigation/P04_对象摘除与缺黑修复导读.md#4.1_谁拥有地址和颜色)中调用者的成员/寿命职责，以及[时序图](../../../navigation/P04_对象摘除与缺黑修复导读.md#4.2_从对象到缺黑父槽)最后返回后的 D4；这两个宏不属于 D1～D3 的内部自动动作。完整私有示例见[P11 取消模块](../../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.3.12_用完整模块观察取消请求)，返回[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。

## 1.9\_后序safe的两个局部游标

循环体准备释放当前对象时，下一次计算必须先于释放。宏使用 pos 和 n 两个调用者局部变量实现这个顺序，真实边界来自[后序销毁时序](../../../navigation/P05_有序推进与整树销毁导读.md#5.3_整树销毁为何不用逐个平衡)。以下宏体保留上游 include/linux/rbtree.h；中文 Doxygen 为仓库补充、非上游原文。

```c
/**
 * rb_entry_safe - 非空时由嵌入节点找外层对象，空时返回 NULL。
 * @ptr: 节点表达式，先存局部临时量，避免重复求值。
 * @type: 外层对象类型。
 * @member: rb_node 成员名。
 * 使用 GNU C typeof 和语句表达式，不检查寿命或地址有效性。
 */
#define rb_entry_safe(ptr, type, member) \
	({ typeof(ptr) ____ptr = (ptr); \
	   ____ptr ? rb_entry(____ptr, type, member) : NULL; \
	})

/**
 * rbtree_postorder_for_each_entry_safe - 允许循环体使当前对象失效。
 * @pos: 当前外层对象指针。
 * @n: 下一外层对象的临时指针。
 * @root: 有效根对象，内部根节点可空。
 * @field: 嵌入的 rb_node 成员名。
 * 只保护当前对象在循环体失效；不允许 rb_erase 等重排，也不加锁。
 */
#define rbtree_postorder_for_each_entry_safe(pos, n, root, field) \
	for (pos = rb_entry_safe(rb_first_postorder(root), typeof(*pos), field); \
	     pos && ({ n = rb_entry_safe(rb_next_postorder(&pos->field), \
			typeof(*pos), field); 1; }); \
	     pos = n)
```

**实现原理：** 初始化先取后序起点；每轮条件先确认 pos 非空，接着计算 n，再以 1 让循环体执行；迭代部分才把 n 交给 pos。最后一个对象的 n=NULL 仍会进入本轮循环体，因此根也能被处理。rb_entry_safe 保存的临时量只避免表达式求值两次，不提供任何引用计数或指针有效性检查。

**可修改性：** 把 n 的计算移进迭代部分会使其发生在循环体之后，当前内存可能已无效。改成“只有 n 非空才进循环体”则会跳过最后根对象。保存 n 不意味着可以旋转；旋转改变尚未访问对象的父关系，可能跳过节点。释放后应丢弃根，不能按普通查询继续使用半销毁的树。

本簇对应[关系图](../../../navigation/P05_有序推进与整树销毁导读.md#5.1_拓扑与游标分别保存在哪)中的 pos/n 交接，以及[销毁时序](../../../navigation/P05_有序推进与整树销毁导读.md#5.3_整树销毁为何不用逐个平衡)第 1、2、4 步；第 3、5 步属于调用者。后序函数的唯一实现见[lib/rbtree.c](../../lib/rbtree.c.md#1.8_后序推进只跨向未完成部分)，返回[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。

## 1.10\_替换时的最左缓存入口

普通 root 只有树根；cached 根还保存另一个直接到最小对象的地址。替换最左对象时两条入口都要改。上游位置仍为 include/linux/rbtree.h；中文 Doxygen 为仓库补充、非上游原文。

```c
/**
 * rb_replace_node_cached - 同键替换并保持最左缓存。
 * @victim: 此 cached 树中的旧成员。
 * @new: 同键且未入树的替代对象内节点。
 * @root: 同时拥有普通树根和最左缓存的根对象。
 * 需要覆盖整次操作的调用者保护，不是 RCU cached 替换接口。
 */
static inline void rb_replace_node_cached(struct rb_node *victim,
					  struct rb_node *new,
					  struct rb_root_cached *root)
{
	if (root->rb_leftmost == victim)
		root->rb_leftmost = new;
	rb_replace_node(victim, new, &root->rb_root);
}
```

**实现原理：** 只有 victim 正是缓存对象时才重写 rb_leftmost；随后复用[普通替换](../../lib/rbtree.c.md#1.9_同键替换的普通与RCU入口)。因此 cached 写入发生在 new 节点结构复制之前，独立读缓存的人不能在此时闯入。完成后树的排序位置、最左对象和缓存重新一致；非最左替换不动缓存。

**可修改性：** 省掉首个分支会让缓存留着旧对象地址，旧对象回收后会悬空。改为调用 RCU 替换也不能自动使这个普通缓存写入获得发布协议；固定头文件没有提供本函数的 RCU 合并变体。调用者须按真实读写协议设计保护，而不是拼两个名字相似的函数。

本节是[替换模块](../../../navigation/P06_同键替换与旧对象退出导读.md#6.3_附加入口与回收条件)中的附加入口分支，复用[角色图](../../../navigation/P06_同键替换与旧对象退出导读.md#6.1_地址身份与排序位置)及[时序](../../../navigation/P06_同键替换与旧对象退出导读.md#6.2_一轮替换怎样交接入口)：缓存改写在 R1 前，随后执行普通 R1～R3。返回[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。

## 1.11\_父地址与业务地址的两种还原

两者都返回地址，却读取不同的信息：rb_parent 从当前节点的打包字段还原另一个节点，rb_entry 根据成员偏移还原当前节点所属的外围对象。它们不分配对象，不取得引用，也不验证对象仍存活。上游位置为本页 rbtree.h；中文 Doxygen 是仓库补充。

```c
/**
 * rb_parent - 仓库阅读说明：清除父色字段低两位后还原父节点。
 * @r: 有效的在树节点地址；不能为 NULL，也不能把游离标记当作父链。
 * 黑根打包值为 1，掩码后的父地址仍为 NULL。
 */
#define rb_parent(r)   ((struct rb_node *)((r)->__rb_parent_color & ~3))
/**
 * rb_entry - 仓库阅读说明：根据正确的外围类型和嵌入成员还原对象。
 * @ptr: 该成员的有效地址，不能任意更换类型、成员或对象寿命前提。
 * @type: 外围结构体类型。
 * @member: 嵌入成员名。
 */
#define	rb_entry(ptr, type, member) container_of(ptr, type, member)
```

实现原理：取父读取父色字段，再在 unsigned long 宽度下屏蔽最低两位；取业务对象则调用通用 container_of，依靠编译时已知的成员偏移，具体[类型检查与 const 边界](container_of.h.md#1.1_一次还原中的求值与类型检查)在对应上游文件位置单独展开。宏不说明节点属于哪一棵树；游离时字段可能等于自身地址，误沿它回溯就可能得到自环。

可修改性：不能把取父简化为直接转换黑节点打包值，不能把两个还原操作互换。若业务一个对象嵌入两个树成员，rb_entry 必须选实际返回的那个成员；选错不会由红黑树修复检测出来。关系与成员周期见[布局导读 T0～T4](../../../navigation/P07_节点布局与编码状态导读.md#7.2_沿一个节点的成员周期读写字段)，返回[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。


## 1.12\_缓存取首只读取入口

上游位置仍为本页固定 include/linux/rbtree.h；下面中文 Doxygen 均为仓库阅读补充。先沿[缓存状态 C0～C6](../../../navigation/P08_最左缓存与结构更新导读.md#8.2_沿接入与摘除跟踪C0到C6)定位两个入口，再看 C4 的读取：

```c
/**
 * rb_first_cached - 仓库阅读说明：读取调用者维护的中序首入口。
 * @root: 缓存与拓扑一致且按业务协议受保护的根对象。
 * 不加锁、不取得引用，也不沿树复核该地址是否仍为成员。
 */
#define rb_first_cached(root) (root)->rb_leftmost
```

宏只读一个槽；常量时间仅描述取得地址，不包括后续摘除、锁等待或对象回收。改普通根而漏改缓存时，它会忠实返回错误旧值，不会自动修复。返回[缓存导读](../../../navigation/P08_最左缓存与结构更新导读.md#8.2_沿接入与摘除跟踪C0到C6)。

## 1.13\_缓存写入先于插入修复

```c
/**
 * rb_insert_color_cached - 仓库阅读说明：维护同一受保护操作内的缓存与拓扑。
 * @node: 刚通过正确空槽接入的有效节点。
 * @root: 本次操作的缓存根。
 * @leftmost: 调用者按实际搜索路径确定的新首节点标志。
 */
static inline void rb_insert_color_cached(struct rb_node *node,
					  struct rb_root_cached *root,
					  bool leftmost)
{
	if (leftmost)
		root->rb_leftmost = node;
	rb_insert_color(node, &root->rb_root);
}
```

C3 在条件成立时先写缓存，再进入普通 rb_insert_color；因此观察者不能越过调用者保护，在两次更新之间同时要求稳定树和缓存。false 不代表不插入，true 也不会重新验证排序。修改 leftmost 的来源会留下结构正确而缓存错误的树。 该函数不提供锁或读侧寿命保护。回到[同阶段模块导读](../../../navigation/P08_最左缓存与结构更新导读.md#8.2_沿接入与摘除跟踪C0到C6)核对调用者、槽地址和完成边界。

## 1.14\_缓存删除先取后继

```c
/**
 * rb_erase_cached - 仓库阅读说明：维护同一受保护操作内的缓存与拓扑。
 * @node: 此树中要摘除的成员。
 * @root: 与该成员对应的缓存根。
 * 返回新后继仅限删除旧首节点；其他删除也返回 NULL。
 */
static inline struct rb_node *
rb_erase_cached(struct rb_node *node, struct rb_root_cached *root)
{
	struct rb_node *leftmost = NULL;

	if (root->rb_leftmost == node)
		leftmost = root->rb_leftmost = rb_next(node);

	rb_erase(node, &root->rb_root);

	return leftmost;
}
```

C5 先比较对象身份，删中缓存时从仍在树中的 node 调用 rb_next，再保存后继，最后进行结构删除。不能换成删除后沿旧节点遍历。局部 leftmost 初始为 NULL，故非首节点删除返回 NULL；删完最后节点也返回 NULL，不能由返回值判断整树是否为空。函数不释放对象，不清游离标记，C6 的寿命条件仍归调用者。 该函数不提供锁或读侧寿命保护。回到[同阶段模块导读](../../../navigation/P08_最左缓存与结构更新导读.md#8.2_沿接入与摘除跟踪C0到C6)核对调用者、槽地址和完成边界。

## 1.15\_辅助插入如何产生最左标志

```c
/**
 * rb_add_cached - 仓库阅读说明：维护同一受保护操作内的缓存与拓扑。
 * @node: 未在树中且地址稳定的新节点。
 * @tree: 由调用者保护的缓存根。
 * @less: 严格比较谓词，规则需与该树既有顺序相容。
 * 返回 node 仅表示成为新首节点；NULL 不表示插入失败。
 */
static __always_inline struct rb_node *
rb_add_cached(struct rb_node *node, struct rb_root_cached *tree,
	      bool (*less)(struct rb_node *, const struct rb_node *))
{
	struct rb_node **link = &tree->rb_root.rb_node;
	struct rb_node *parent = NULL;
	bool leftmost = true;

	while (*link) {
		parent = *link;
		if (less(node, parent)) {
			link = &parent->rb_left;
		} else {
			link = &parent->rb_right;
			leftmost = false;
		}
	}

	rb_link_node(node, parent, link);
	rb_insert_color_cached(node, tree, leftmost);

	return leftmost ? node : NULL;
}
```

C1 的局部 leftmost 初始为 true，任何一次向右后置 false；后续再向左也不能越过曾位于自己左侧的祖先。相等时 less 为 false，继续向右，本接口不拒绝等价键。C2 接到空槽后调用 C3 包装；返回的标志只报告是否新成首节点。不能在调用后再重复执行普通挂接和修复，也不能把 NULL 当失败后释放已入树的对象。 该函数不提供锁或读侧寿命保护。回到[同阶段模块导读](../../../navigation/P08_最左缓存与结构更新导读.md#8.2_沿接入与摘除跟踪C0到C6)核对调用者、槽地址和完成边界。
