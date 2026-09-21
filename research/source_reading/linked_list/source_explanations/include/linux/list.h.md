---
id: research.source_reading.linked_list.list_implementation
title: "list.h 的连接与游标实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_list.h的连接与游标实现

[模块导读](../../../navigation/P02_拓扑修改与发布边界导读.md)已区分调用者的同步与宿主寿命。下面只展开 list.h 内部决定拓扑的语句，固定版本见[总索引](../../../navigation/P01_Linux_6.12_链表源码阅读索引.md)。上游文件为 [include/linux/list.h](../../../../linux/include/linux/list.h)；中文 Doxygen 阅读说明为仓库补充，函数体保持上游语句，不可把这些摘录当独立头文件。

## 1.1\_初始化与四条接链赋值

结构定义位于 types.h，next 与 prev 都是 struct list_head 指针。头与成员节点使用相同结构，业务对象并不在此定义。INIT_LIST_HEAD 适用于尚未发布或已按协议脱链的节点；它不遍历旧成员，也不释放旧对象。

```c
/**
 * 仓库阅读说明：将已有节点的两个方向都设为自身；list 是待初始化地址。
 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
	WRITE_ONCE(list->next, list);
	WRITE_ONCE(list->prev, list);
}

/**
 * 仓库阅读说明：在两个已知相邻节点之间接入 new；调用者先保证同步和成员资格。
 */
static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	if (!__list_add_valid(new, prev, next))
		return;

	next->prev = new;
	new->next = next;
	new->prev = prev;
	WRITE_ONCE(prev->next, new);
}

/**
 * 仓库阅读说明：在 head 后接入 new；连续调用呈头插顺序。
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

/**
 * 仓库阅读说明：在 head 前接入 new；head->prev 是当前尾部。
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}
```

__list_add_valid 的配置与检查入口在[检查导读](../../../navigation/P02_拓扑修改与发布边界导读.md#2.2_检查发生在修改之前)唯一归纳。它读的是实际邻接状态，不为 new 建立全局成员登记；返回 false 时，本函数不再执行后面的连接赋值。

成功路径先让 next.prev 指向 new，再填写 new 的两条边，最后写 prev.next。独占执行时，最终建立 prev↔new↔next。最后一条 WRITE_ONCE 只标记该次访问，不能把前面所有普通写自动变成一个通用 release 发布。READ/WRITE_ONCE 的访问与屏障边界见导读的固定 rwonce.h 与 ARM 位置，完整并发协议由调用者负责。

可修改性：换头插与尾插只改变邻居选择；若更改赋值次序并试图获得无锁遍历，必须重新证明读者可见路径、内存序和删除回收，不能从这四行推导 RCU 语义。重复把同一成员入表，也不是靠这些函数自动清理旧关系。

## 1.2\_摘链与删除后状态

删除分为“邻居相互直连”和“被摘成员留下什么状态”。以下代码没有业务分配器入口，因此 list_del 并不意味着 kfree。

```c
/**
 * 仓库阅读说明：把两个邻居直接相连；prev、next 必须来自有效拓扑。
 */
static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	WRITE_ONCE(prev->next, next);
}

/**
 * 仓库阅读说明：先校验 entry 的邻接关系，再执行摘链。
 */
static inline void __list_del_entry(struct list_head *entry)
{
	if (!__list_del_entry_valid(entry))
		return;

	__list_del(entry->prev, entry->next);
}

/**
 * 仓库阅读说明：摘链之后毒化 entry 的两个成员，帮助暴露后续误用。
 */
static inline void list_del(struct list_head *entry)
{
	__list_del_entry(entry);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

/**
 * 仓库阅读说明：摘链后重建 entry 自环；不释放宿主对象。
 */
static inline void list_del_init(struct list_head *entry)
{
	__list_del_entry(entry);
	INIT_LIST_HEAD(entry);
}

/**
 * 仓库阅读说明：仅观察 head->next 是否等于 head，不验证整张表。
 */
static inline int list_empty(const struct list_head *head)
{
	return READ_ONCE(head->next) == head;
}
```

LIST_POISON1/2 定义在 [poison.h](../../../../linux/include/linux/poison.h)，固定值为 0x100 和 0x122 加配置相关偏移；读到它们不构成“宿主已经释放”的证据。被删除的是成员关系，宿主可能仍然存活并有其他引用。

需要特别跟踪检查失败：__list_del_entry 可以提前返回，但调用它的 list_del 随后仍执行毒化，list_del_init 随后仍初始化。检查失败不构成整个外层操作的事务回滚；调用者应将损坏视为协议被破坏，不能根据“报告函数返回 false”继续假定原链不变。

可修改性：若要复用脱链对象，选择符合协议的初始化状态；若要释放，先证明所有使用者退出。list_empty 只读 next，返回真不证明 prev 一致或无泄漏，也不替下一次操作保留空状态。

## 1.3\_游标和批次转移

遍历宏里的 pos 与 n 都是宿主指针类型；list_first_entry 从首成员找宿主，list_next_entry 从当前成员的 next 找宿主，list_entry_is_head 通过成员地址判断是否已回到哨兵。它们基于 list_entry/container_of 的布局换算，没有运行时类型表。

```c
/**
 * 仓库阅读说明：先保存下一宿主游标 n，允许循环体删除当前 pos。
 * head 是循环哨兵，member 是宿主内实际连接成员名；
 * 不为 n 增加引用，不允许据此忽略并发删除。
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_first_entry(head, typeof(*pos), member),	\
		n = list_next_entry(pos, member);			\
	     !list_entry_is_head(pos, head, member); 			\
	     pos = n, n = list_next_entry(n, member))
```

即使表空，宏也按照内核的容器游标惯用方式构造候选地址并通过成员地址检查退出；循环体不应执行，不能在循环结束后把 pos 当作最后一个有效宿主继续解引用。教材普通 C 模型采用原始连接游标，避免把这套内核宏习惯误写成任意 ISO C 宿主程序都可复制的保证。

整批移动只改批次两端和目标邻居，内部连接不变：

```c
/**
 * 仓库阅读说明：把非空 list 的首尾接到 prev 与 next 之间；内部成员顺序保持。
 */
static inline void __list_splice(const struct list_head *list,
				 struct list_head *prev,
				 struct list_head *next)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

/**
 * 仓库阅读说明：将源 list 并到目标 head 尾部，并重置源头；调用者保证两头不同且关系受保护。
 */
static inline void list_splice_tail_init(struct list_head *list,
					 struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head->prev, head);
		INIT_LIST_HEAD(list);
	}
}
```

源头恢复为空，使教材的栈上 staging 可以结束寿命，而节点不再依赖它。这里没有对象复制，也没有原子事务、内存分配或成员引用计数。若目标本来非空，源批次接在既有尾部之后；源为空时不改变目标。

可修改性：去掉 INIT_LIST_HEAD 会改变源头后续可用性；调换 head->prev/head 与 head/head->next 会改变批次插入位置。若多表分别有锁，需要先确定统一的锁顺序与所有权转移，不能把同一段指针赋值放在各自不同锁下就认为两边都安全。

返回[模块导读](../../../navigation/P02_拓扑修改与发布边界导读.md)、[总索引](../../../navigation/P01_Linux_6.12_链表源码阅读索引.md)或[教材](../../../../../../knowledge/linux/data_structures/单链表_linked_list/大纲.md)。
