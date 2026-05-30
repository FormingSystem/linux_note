
# 单链表（Singly Linked List）

---

## 章节内容说明

在本节中，我们将详细介绍**Linux内核5.10版本中的单链表**数据结构的定义、使用方式以及相关接口。链表作为一种基本的数据结构，广泛应用于内核的各个领域，包括进程调度、内存管理、文件系统等。了解如何在内核中实现和使用单链表，对于深入掌握内核的工作机制至关重要。

---

# 1. 是什么（历史渊源、发展进程、定位）

单链表是一种线性数据结构，它由多个节点（`node`）组成，每个节点包含数据部分和指向下一个节点的指针。在内核中，链表被广泛用于管理不同的资源和任务队列，例如进程调度、IO请求的调度、设备驱动的管理等。

Linux内核中提供了几种链表的实现方式，最常用的是通过内核提供的链表宏和结构来定义和操作链表。它的设计旨在提高性能，同时提供线程安全机制，适应内核的高并发环境。

---

# 2. 干什么（要解决的问题）

链表被用来在内核中有效地管理可变大小的元素集合。与数组不同，链表能够动态扩展或收缩，在添加或删除元素时不需要重新分配内存，因此在频繁变动的场景下特别有用。

在Linux内核中，单链表主要用于以下目的：

- **管理进程队列**：如就绪队列、等待队列。
  
- **网络数据包缓冲区管理**：如`sk_buff`队列。
  
- **内核模块管理**：如设备驱动和资源调度。
  

---

# 3. 怎么实现（底层原理、处理逻辑）

在Linux内核中，单链表的实现是通过以下结构定义的，源文件 [include/linux/types.h](../../kernel_source/include/linux/types.h.md) ：

```c
struct list_head {
    struct list_head *next, *prev;
};
```

`list_head` 是 Linux 内核链表的基本单元，它包含了两个指针：

- **next**：指向下一个节点的指针。
  
- **prev**：指向前一个节点的指针。

每个节点都通过这两个指针实现双向链接，虽然单链表只需要访问`next`指针，但为了支持双向遍历，内核链表使用了双向链接。

为了避免对内存的频繁分配和释放，Linux内核提供了用于管理链表的宏和接口函数。

从这里可以看出内核的链表，是双向链表，且不携带数据，一般使用单链表需要将它作为成员来使用。

```c
struct temp_data {
    struct list_head list;
    int    data;
};
```

在这里有一个疑惑，我们的链表只是指向了链表，却没有访问数据的权限，那么在使用链表时，我们要怎么感知到数据呢？当链表作为对象成员存在时，采用 [container_of()宏](../GNU_extension_of_C_language/C_language_extension.md#3.2 `container_of`（结构体反向查找）) 可以反推对象的地址，然后再访问数据部分。

---

# 4. 怎么用（方法与步骤）

关于List_head的接口都位于 [include/linux/list.h](../../kernel_source/include/linux/list.h.md)。

## 4.1 链表初始化 INIT_LIST_HEAD()

链表通常在使用之前进行初始化，使用 `INIT_LIST_HEAD` 宏来初始化链表头：

```c
struct list_head my_list;
INIT_LIST_HEAD(&my_list);
```

这个宏将 `my_list` 的 `next` 和 `prev` 指针都设置为指向自己，从而实现一个空链表。

```c
/**
 * INIT_LIST_HEAD - Initialize a list_head structure
 * @list: list_head structure to be initialized.
 *
 * Initializes the list_head to point to itself.  If it is a list header,
 * the result is an empty list.
 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
	WRITE_ONCE(list->next, list);
	list->prev = list;
}
```

## 4.2 **插入节点**

定义原型：
```c
/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
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
```

这里只有prev->next采用了WRITE_ONCE()有什么说法吗？

> 在Linux内核中，`WRITE_ONCE()` 宏用于确保对变量的写操作不会被优化掉或缓存到寄存器中。这个宏的目的是防止编译器或CPU对写操作进行重新排序或优化，确保对该变量的写操作是直接、稳定地写入内存。
>
> 在你提供的 `__list_add` 函数中，使用了 `WRITE_ONCE(prev->next, new);`，而其他节点指针的赋值没有使用 `WRITE_ONCE`。下面是这个写操作使用 `WRITE_ONCE` 的原因和其背后的目的：
>
> ### 1. **写操作的可见性与内存屏障**
>
> 内核中链表的操作通常涉及多个CPU或多个线程。为了避免在并发执行时出现数据不一致性问题，需要保证对链表指针的写操作在不同的CPU核之间是可见的。具体到链表的操作，`prev->next = new` 这一写操作在多核系统上是一个关键点，因为：
>
> - **前一个节点的 `next` 指针** 与当前节点的 `prev` 指针通常是相关联的，若没有保证写操作的顺序性，可能导致其他CPU核访问链表时，读取到不一致的指针（即“脏读”），从而引发不可预期的行为。
> - 使用 `WRITE_ONCE()` 来保证 `prev->next = new` 写操作的内存访问是 **"稳定"** 的，不会被编译器优化掉，也不会被 CPU 缓存。
>
> ### 2. **防止内存重排序**
>
> 在并发场景中，现代编译器和处理器可能会将一些内存操作进行重排序，目的是优化性能。然而，这种优化可能破坏链表操作的顺序。例如，如果 `next->prev = new` 和 `new->next = next` 先执行，而 `prev->next = new` 后执行，可能导致链表指针出现错乱，导致不一致的链表结构。
>
> - 使用 `WRITE_ONCE()` 可以防止这种重排序，使得 `prev->next = new` 操作 **严格按顺序** 执行，确保链表的操作按照预期的顺序来更新每个节点的指针。
>
> ### 3. **其他赋值操作为何不需要 `WRITE_ONCE`**
>
> `next->prev = new;`、`new->next = next;` 和 `new->prev = prev;` 是链表节点指针的直接赋值操作。与 `prev->next = new;` 不同，这些操作不涉及到可能需要对共享内存进行同步的操作。它们主要是在本地的寄存器或内存中进行，且不依赖于其他CPU对内存的可见性保证。
>
> - `WRITE_ONCE` 主要用于与可能涉及并发的内存写入进行配合，以确保对外部内存的写入操作是**原子化**的且不被优化或重排序。
>
> ### 4. **总结**
>
> - **`WRITE_ONCE(prev->next, new);`** 的使用是为了确保在多核系统中，`prev->next` 的写操作能够正确地被所有CPU访问，并防止因内存重排序或编译器优化导致的潜在问题。
> - 其他指针赋值操作不使用 `WRITE_ONCE`，因为它们并不会直接影响到其他CPU的访问，也不涉及并发控制。
>
> 总之，`WRITE_ONCE` 用于防止写操作的优化或重排序，确保操作顺序和内存一致性。在内核链表的操作中，这是保证数据结构一致性的一个重要手段，尤其是在多核、并发环境下。

`WRITE_ONCE` 主要是为了解决**内存序**的问题，确保对 `prev->next` 的写操作不会被编译器重排序或优化，保证操作的顺序性和内存可见性。这是一个针对**内存访问顺序**的优化，确保内存中的写操作按预期执行并对其他处理器或线程可见。

<span style="color:red;">但是，`WRITE_ONCE` 并不解决**并发访问**中的**竞态条件**问题，即多个线程或CPU同时访问和修改链表结构时可能导致的数据不一致或破坏。在高并发环境下，多个线程可能同时访问链表的同一部分（例如同时修改 `prev->next` 指针），这种情况下，就需要使用**锁机制**来确保**独占访问**。</span>

### 插入新节点到链表的头部：

```c
struct list_head new_node;
INIT_LIST_HEAD(&new_node);
list_add(&new_node, &my_list); // 添加到链表头部
```
接口实现定义：

```c

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}
```



### 插入新节点到链表的尾部：

```c
list_add_tail(&new_node, &my_list); // 添加到链表尾部
```

`list_add` 和 `list_add_tail` 宏分别用于在链表的头部和尾部插入节点。它们根据节点的 `prev` 和 `next` 指针将新节点链接到链表中。

接口实现定义：
```c
/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}
```

## 4.3 删除节点

删除链表中的某个节点：

```c
list_del(&new_node); // 从链表中删除
```

`list_del` 宏会删除指定节点，并调整前后节点的指针。



### list_del()定义

```c
/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void list_del(struct list_head *entry)
{
	__list_del_entry(entry);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}
```



### 关于为什么list_head的删除操作后，删除节点的成员值不为NULL

> 这是一份为您整理好的笔记，结构上涵盖了**核心机制、设计原因、地址空间分布对比**以及**内核防御性编程哲学**。
>
> 您可以直接将其誊抄到您的笔记本中，建议保留表格和内存分布图，这对后续复习非常有帮助。
>
> ------
>
> # 笔记：Linux Kernel 链表删除与指针毒化 (List Poisoning)
>
> ## 1. 核心现象
>
> 在 Linux 内核中调用 `list_del(entry)` 删除链表节点时，`entry->prev` 和 `entry->next` 指针**不会被置为 NULL**，而是指向两个特定的、非法的**低地址常量**。
>
> - **LIST_POISON1** (通常为 `0x100` + 偏移): 赋值给 `next` 指针。
> - **LIST_POISON2** (通常为 `0x200` + 偏移): 赋值给 `prev` 指针。
>
> > **术语**：这种机制被称为 **List Poisoning (链表毒化)**。
>
> ## 2. 为什么要“毒化”而不是用 NULL？
>
> 核心目的是为了**调试 (Debugging)** 和 **故障诊断 (Fault Diagnosis)**。
>
> 1. **特征识别 (Signature)**：
>    - **NULL (0x0)** 太常见，可能是未初始化、分配失败或逻辑错误，难以定位来源。
>    - **POISON (0x100/0x200)** 是独特的“墓志铭”。一旦内核 Crash 在这个地址，可以**立即断定**是 **Use-After-Free (释放后使用)** 错误（即访问了已删除的节点）。
> 2. **方向定位**：
>    - Crash 在 `0x100` $\rightarrow$ 代码尝试访问 `next`。
>    - Crash 在 `0x200` $\rightarrow$ 代码尝试访问 `prev`。
> 3. **防止重复删除 (Double Delete)**：
>    - 内核调试选项 (`CONFIG_DEBUG_LIST`) 可以在执行删除前检查指针是否等于 POISON 值。如果是，则直接报错“节点已被删除”，防止内存破坏。
>
> ## 3. 关键辨析：NULL vs POISON vs ERR_PTR
>
> **误区澄清**：`list_del` 的毒化地址和 `ERR_PTR` 的错误码地址**并不冲突**，它们分别位于 64 位地址空间的**最底端**和**最顶端**。
>
> | **指针类型** | **典型地址值 (64-bit Hex)**               | **所在区域**                    | **物理含义 (语义)**                                          |
> | ------------ | ----------------------------------------- | ------------------------------- | ------------------------------------------------------------ |
> | **ERR_PTR**  | `0xFFFFFFFF_FFFFFFFx`   (如 `-12` 的补码) | **极高地址**   (内核空间最末页) | **逻辑错误**：函数调用失败，指针携带了错误码 (如 `-ENOMEM`, `-EFAULT`)。 |
> | **POISON**   | `0x00000000_00000100`   (小正整数)        | **极低地址**   (用户空间保留区) | **生命周期结束**：对象已被删除/释放，不可再访问。            |
> | **NULL**     | `0x00000000_00000000`                     | **绝对零地址**                  | **空/无**：未初始化、分配失败或链表结尾。                    |
>
> ## 4. 内存地址空间分布图解 (Memory Map)
>
> (建议画在笔记边缘辅助记忆)
>
> Plaintext
>
> ```
> [ 0xFFFFFFFF FFFFFFFF ]  <-- 顶部 (Top)
> +---------------------+
> |   ERR_PTR 区间      |  (存放 -1, -12 等错误码的补码)
> | (保留页，禁止映射)  |
> +---------------------+
> |                     |
> |    正常内核空间     |
> |                     |
> +---------------------+
> |                     |
> | ... (巨大空洞) ...  |
> |                     |
> +---------------------+
> |   LIST_POISON1/2    |  <-- 0x100, 0x200 (在此处触发缺页异常)
> +---------------------+
> |      NULL (0x0)     |  <-- 底部 (Bottom)
> +---------------------+
> ```
>
> ## 5. 总结：内核的防御性哲学
>
> Linux 内核并不假设代码是完美的，而是通过**“显式崩溃” (Fail Fast)** 来应对错误。
>
> - 指向 **NULL** = “我不知道发生了什么”。
> - 指向 **POISON** = “我清楚地告诉你，这个节点已经死（被删）了”。
>
> 这种设计将隐蔽的内存破坏 bug，转化为具有**极高辨识度**的 Kernel Panic，极大地降低了调试难度。

## 4.4 遍历链表

遍历链表的所有节点：

```c
struct list_head *pos;
list_for_each(pos, &my_list) {
    // 对 pos 节点执行操作
}
```

`list_for_each` 宏用于遍历链表，`pos` 是遍历时的当前节点。

### list_for_each()定义

```c
/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)
```



## 4.5 **检查链表是否为空**

```c
if (list_empty(&my_list)) {
    // 链表为空
}
```

`list_empty` 宏用于检查链表是否为空。如果链表头的 `next` 和 `prev` 指针指向自己，则说明链表为空。

### list_empty()定义

```c
/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head)
{
	return READ_ONCE(head->next) == head;
}
```

## 4.6 关于list_head数据结构中并发原语与原子性深度解析（AI补充）

这是为您调整后的**最终独立笔记版本**。

这次我将 **“读/写撕裂 (Tearing)”** 的核心概念提前到了**核心作用**章节中直接阐述，明确了它在 32 位系统下的背景。这样逻辑更顺畅：先讲清楚通用的痛点（优化和撕裂），再讲具体的案例（链表），最后通过 64 位系统的特例进行对比深化。

### 1. READ_ONCE 与 WRITE_ONCE 的本质

这两个宏本质上是将变量强制转换为 `volatile` 类型进行访问。

- **作用对象**：**编译器 (Compiler)**。
- **非作用对象**：**CPU** (它们**不**提供 CPU 层面的内存屏障，**不**保证全局可见性，也不提供锁的功能)。

### 2. 核心作用：为什么要用它们？

它们主要解决并发编程中的两个核心问题：**编译器过度优化** 和 **读写撕裂**。

#### A. 防止编译器过度优化 (Compiler Optimization)

编译器通常假设单线程执行，通过 `READ_ONCE/WRITE_ONCE` 我们可以禁止以下行为：

- **防止缓存 (Reloading)**：
  - *现象*：编译器认为变量没变，直接复用寄存器里的旧值。
  - *对策*：强制每次都从内存重新读取。
- **防止省略 (Fusing)**：
  - *现象*：编译器认为变量没被使用，直接跳过读取或写入。
  - *对策*：强制生成读写指令。

#### B. 防止读写撕裂 (Load/Store Tearing) —— **32 位系统的痛点**

这是并发中最隐蔽的 Bug 之一，尤其是在 **32 位架构** 上处理 **64 位数据** 时。

- **现象描述**：

  在 32 位系统上，寄存器和总线宽度通常只有 32 位。要读取/写入一个 64 位的数据（如 `u64` 或 `long long`），CPU 必须分**两次**操作（两条指令）来完成：一次读高 32 位，一次读低 32 位。

- **并发风险**：

  如果在读完“高 32 位”之后，还没来得及读“低 32 位”时，另一个 CPU 修改了整个 64 位数值：

  - 结果：你读到的值 = **新值的高位 + 旧值的低位**。
  - 结论：这是一个完全错误的、拼凑出来的“撕裂”值。

- **`READ_ONCE` 的作用**：

  它告诉编译器：“这是一个整体，请尽量使用该架构下最原子的方式来访问”。(虽然在纯 32 位硬件上无法改变物理限制，但在支持原子指令的架构上，它能防止编译器错误地拆分指令)。

------

### 3. 经典案例：INIT_LIST_HEAD 的不对称设计

```c
static inline void INIT_LIST_HEAD(struct list_head *list)
{
    WRITE_ONCE(list->next, list); // 关键点：保护 next
    list->prev = list;            // 关键点：不保护 prev
}
```

#### Q1: 为什么要用 WRITE_ONCE？

- **保护无锁读者**：内核中存在大量只通过 `list_empty(head)`（只读 `next`）来检查链表状态的无锁代码。
- **防止撕裂危害**：如果编译器把 `next` 指针的写入拆分成两半（在 32 位系统上），并发的 `list_empty` 可能会读到一个指向无效地址的“撕裂指针”，导致系统崩溃。

#### Q2: 为什么 next 加了，prev 没加？

- **读者只关心 next**：绝大多数无锁检查只看 `head->next`。
- **prev 无人无锁关心**：`prev` 字段通常只在链表修改（如 `list_add/del`）时用到，而修改操作通常是**加锁**的。
- **结论**：既然没有无锁读者去读 `prev`，那么 `prev` 即使发生写撕裂也是无害的。

#### Q3: 为什么这里没有同步原语（锁/屏障）？

- **生命周期原则**：初始化 (Initialization) 通常发生在对象发布 (Publication) 之前。此时对象是私有的，无需同步。让其他 CPU “看到”这个初始化的责任，在于后续的**发布操作**。

------

### 4. 深度辨析：64 位系统的原子性迷思

#### 核心疑问

*“既然 32 位系统有撕裂问题，那在 64 位系统上，总线够宽，读取 64 位指针本身就是原子的，还需要 `READ_ONCE` 吗？”*

#### 硬件层面的事实

- **对齐即原子**：在 64 位架构（如 x86_64, ARM64）上，只要 64 位数据在内存中是**自然对齐** (8 字节对齐) 的，CPU 的数据总线能一次性搬运完毕。
- **物理上不撕裂**：无论指令集长短，硬件总线是单次事务，物理上不会出现“一半新一半旧”的情况。

#### 为什么 64 位系统依然必须用 `READ_ONCE`？

即使硬件上没有“撕裂”风险，**“编译器优化”** 依然是致命的。

- **场景 A：循环中的状态检查 (防缓存)**

  C

  ```
  while (flag == 0); // 等待 flag 变 1
  ```

  - **无 `READ_ONCE`**：编译器分析循环体内没人改 `flag`，优化成只读一次寄存器 $\rightarrow$ **死循环**。
  - **有 `READ_ONCE`**：强制编译器每次循环都生成 `LDR` 指令去内存读取。

- **场景 B：指令重排 (防乱序)**

  - 编译器可能会为了性能打乱读取顺序。`READ_ONCE` 能限制这种重排。

------

### 5. 总结：32 位 vs 64 位 的防御侧重点对比

此表并非表示 64 位无撕裂问题是冗余信息，而是强调在不同架构下，开发者使用 `READ_ONCE` 时的心理模型差异。

| **架构环境**  | **物理现象 (Hardware)**                               | **READ_ONCE 的主要任务**                                     |
| ------------- | ----------------------------------------------------- | ------------------------------------------------------------ |
| **32 位系统** | **存在物理撕裂**。 读写 64 位数据需 2 个周期。        | **1. 防撕裂 (关键)**：防止读取到拼凑的错误地址。 **2. 防优化**：防止缓存和重排。 |
| **64 位系统** | **天然原子** (若对齐)。 读写 64 位数据仅需 1 个周期。 | **1. 防优化 (核心)**：必须防止编译器缓存变量值或省略读取。 **2. 逻辑正确性**：即便无撕裂风险，也要保证每次都从内存拿最新值。 |



这是为您整理好的**独立笔记**，专门针对“内核中的一次性初始化”这一主题。

------

## 4.7 Linux Kernel 笔记：内核中的“一次性初始化” (One-Time Initialization)

### 1. 核心疑问

**Q:** 针对全局资源（如内存池、缓存）的懒加载场景，如果存在并发竞争，内核是否提供类似用户态 `pthread_once` 或 C++ `std::call_once` 这样的通用保障机制？

**A:** **没有。**

Linux Kernel **不提供** 通用的、阻塞式的“傻瓜式”初始化接口来自动处理所有并发竞争。

**原因**：

1. **上下文复杂**：初始化操作差异巨大（有的分配内存可能休眠，有的只是原子置位）。通用接口很难同时适配中断上下文和进程上下文。
2. **错误处理**：内核初始化（如 `mempool_create`）极易失败（OOM），通用接口难以优雅地传递错误码并控制重试逻辑。

因此，内核要求开发者根据具体场景，**显式地** 控制同步。

------

### 2. 解决方案一：模块生命周期 (Best Practice)

**核心哲学**：**“不要并发初始化”。**

这是内核最推荐、最安全、性能最高的方式。

- **原理**：利用 Linux 模块加载机制的串行特性。`module_init` 函数在加载时是**单线程串行执行**的，天然无竞争。
- **适用场景**：绝大多数全局资源（内存池 `mempool`、工作队列 `workqueue`、Slab 缓存 `kmem_cache`）。

```c
static struct kmem_cache *my_cache;

static int __init my_module_init(void)
{
    // 此时系统保证是串行的，绝对安全，不需要锁
    my_cache = kmem_cache_create("my_cache", ...);
    if (!my_cache)
        return -ENOMEM;
    return 0;
}
module_init(my_module_init);
```

------

### 3. 解决方案二：显式锁 + 双重检查 (Standard Pattern)

**核心哲学**：**“懒加载必须显式同步”。**

如果必须使用懒汉式初始化（Lazy Initialization），这是内核中的标准范式。

- **原理**：利用 `Mutex` 保证同一时刻只有一个线程在做初始化，结合 `READ_ONCE` 防止编译器优化。
- **代码范式**：

```c
static struct kmem_cache *my_cache;
static DEFINE_MUTEX(my_cache_init_lock); // 显式的锁

struct kmem_cache *get_my_cache(void)
{
    // 1. 快速检查 (Fast path, 无锁读)
    // 必须用 READ_ONCE 防止编译器缓存 NULL 值
    struct kmem_cache *c = READ_ONCE(my_cache); 
    if (c)
        return c;

    // 2. 加锁 (Slow path)
    mutex_lock(&my_cache_init_lock);
    
    // 3. 双重检查 (Double check)
    // 必须再查一次，防止等待锁的时候别人已经初始化完了
    c = my_cache; 
    if (!c) {
        c = kmem_cache_create("my_cache", ...);
        // 4. 发布 (Publication)
        // 使用 smp_store_release 确保初始化动作在赋值之前完成
        // 或者简单的 WRITE_ONCE (取决于有没有依赖关系)
        smp_store_release(&my_cache, c);
    }
    mutex_unlock(&my_cache_init_lock);
    
    return c;
}
```

------

### 4. 解决方案三：无锁原子操作 (Atomic CAS)

**核心哲学**：**“谁快谁赢，输的丢弃”。**

仅适用于非常轻量级、无副作用的初始化（如计算某个整数值）。

- **原理**：利用 `cmpxchg` (Compare and Exchange)。
- **缺点**：存在**重复计算**的浪费（两个 CPU 同时计算，只有一个能赋值成功，另一个计算结果被丢弃）。

```c
static int global_val = 0;

int get_val(void) {
    int val = READ_ONCE(global_val);
    if (val)
        return val;

    int new_val = calculate_complex_value(); // 可能耗时
    
    // 尝试把 global_val 从 0 改成 new_val
    // 如果返回不为 0，说明被别人抢先改了
    if (cmpxchg(&global_val, 0, new_val) != 0) {
        // 竞争失败，丢弃我的 new_val，重新读全局的
        return READ_ONCE(global_val);
    }
    return new_val;
}
```

------

### 5. 避坑指南：DO_ONCE / ONCE 宏

内核中确实存在 `DO_ONCE` 或 `get_random_once` 等宏，但它们有局限性：

- **适用范围**：主要用于**非关键路径**（如 `printk_once` 打印一次日志、`WARN_ON_ONCE` 警告一次）。
- **风险**：通常**不包含锁**。如果有两个 CPU 同时进入，它们可能都会执行一次操作。
- **结论**：**严禁**用于内存池或复杂数据结构的初始化（会导致内存泄漏或指针损坏）。

------

### 总结

在 Linux Kernel 中处理初始化的优先级顺序：

1. **Module Init (首选)**：将初始化移至模块加载阶段，利用生命周期自然隔离并发。
2. **Mutex + Double Check (次选)**：用于必须懒加载的重型资源。
3. **CAS (特殊场景)**：用于轻量级数值计算。
4. **禁止寻找通用魔法**：不要试图寻找类似 `pthread_once` 的通用函数，内核需要你显式掌控一切。

---

# 5. 通用接口/工具方法表与逐步详解

|操作类型|宏/函数|说明|
|---|---|---|
|链表初始化|`INIT_LIST_HEAD(&head)`|初始化链表头，将 `next` 和 `prev` 都指向自己，标记为空链表。|
|插入节点到头部|`list_add(&new_node, &head)`|将节点添加到链表头部。|
|插入节点到尾部|`list_add_tail(&new_node, &head)`|将节点添加到链表尾部。|
|删除节点|`list_del(&node)`|从链表中删除指定节点。|
|遍历链表|`list_for_each(pos, &head)`|遍历链表中的每个节点。|
|判断链表是否为空|`list_empty(&head)`|判断链表是否为空。|

---

# 6. 对比/避坑/限制/注意点

- **避免悬空指针问题**：在删除节点时，一定要确保删除后不再使用该节点。否则会出现悬空指针，导致不可预测的行为。
  
- **空链表检查**：链表头初始化后，使用 `list_empty` 宏时，链表的 `next` 和 `prev` 指针一定是指向头部节点本身，保证对空链表的判断不会出错。
  
- **线程安全性**：Linux内核链表本身并不保证线程安全，若在多线程环境中使用链表，需要加锁或采用其他同步机制来确保数据的完整性。
  
- **性能问题**：虽然链表提供了灵活的节点插入和删除操作，但访问速度较慢（O(n)），因此在对性能要求较高的场景中，可能需要考虑其他数据结构，如哈希表或红黑树。

---

# 7. 完整示例与讲解

以下是一个简单的单链表示例，展示了如何初始化链表、插入节点、删除节点并遍历链表：

```c
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/init.h>

struct my_node {
    int    data;
    struct list_head list;
};

static LIST_HEAD(my_list);  // 定义并初始化一个链表头

static int __init list_demo_init(void)
{
    struct my_node *node1, *node2;

    // 分配并初始化节点
    node1 = kmalloc(sizeof(*node1), GFP_KERNEL);
    node2 = kmalloc(sizeof(*node2), GFP_KERNEL);

    node1->data = 10;
    node2->data = 20;

    // 将节点插入到链表头部
    list_add(&node1->list, &my_list);
    list_add(&node2->list, &my_list);

    // 遍历链表并打印节点数据
    struct my_node *pos;
    list_for_each_entry(pos, &my_list, list) {
        printk(KERN_INFO "Node data: %d\n", pos->data);
    }

    // 删除第一个节点
    list_del(&node1->list);
    kfree(node1); // 释放内存

    return 0;
}

static void __exit list_demo_exit(void)
{
    printk(KERN_INFO "Exiting list demo\n");
}

module_init(list_demo_init);
module_exit(list_demo_exit);

MODULE_LICENSE("GPL");
```

这个示例展示了如何使用内核提供的链表接口：

- 初始化链表头；
  
- 创建和初始化节点；
  
- 使用 `list_add` 将节点插入到链表；
  
- 使用 `list_for_each_entry` 遍历链表；
  
- 使用 `list_del` 删除节点并释放内存。
  

<span style="color:red;">该示例未验证，必然无可能运行</span>：这里我本来想要写一个字符驱动来做验证的，不过我想了下，没做，因为字符驱动那边会用得到咱就自然会做这个示例，但是如果用不到，到也没有必要在这里硬塞一个示例做误导。

---

# 8. 总结

本节详细介绍了 Linux 内核中的单链表数据结构，包括其定义、使用方式以及常见的操作接口。通过理解内核链表的实现和使用，开发人员可以更好地管理内核中的资源、任务队列和其他动态数据结构。在实际开发中，链表作为一个灵活且高效的基础数据结构，能够为复杂的内核机制提供强大的支持。

------

# 9. 调试与验证

调试链表相关的问题可能涉及到内存泄漏、悬空指针、重复节点等问题。为了确保链表操作的正确性，内核提供了一些辅助方法来帮助开发人员调试链表相关的问题。

## 9.1 调试链表的状态

可以使用 `list_for_each_entry` 遍历链表，输出每个节点的数据，以验证链表操作是否正确：

```c
struct my_node *pos;
list_for_each_entry(pos, &my_list, list) {
    printk(KERN_INFO "Node data: %d\n", pos->data);
}
```

这段代码能够帮助开发人员在链表操作后检查链表内容，确保节点按预期插入或删除。

仔细看这里的示例，它的好处是可以直接不用管包含 `struct list_head` 的结构体是什么类型，直接做到类型兼容，访问 `struct my_node` 对象的数据成员。它本质是采用 [container_of()](../GNU_extension_of_C_language/C_language_extension.md#3.2 `container_of`（结构体反向查找）) 宏来获取。以下是它的组合结果：

```c
/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_first_entry(head, typeof(*pos), member);	\
	     !list_entry_is_head(pos, head, member);			\
	     pos = list_next_entry(pos, member))

struct my_node *pos;
/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_first_entry(head, typeof(*pos), member);	\
	     !list_entry_is_head(pos, head, member);			\
	     pos = list_next_entry(pos, member))(pos, &my_list, list) {
    printk(KERN_INFO "Node data: %d\n", pos->data);
}
```



## 9.2 验证内存泄漏

在内核中，链表节点常常动态分配内存，因此开发人员需要小心管理内存。可以使用 `kfree()` 来释放内存，防止内存泄漏：

```c
kfree(node1); // 释放已经删除的节点内存
```

确保每次删除链表节点时，都对应释放其占用的内存空间。

## 9.3 调试链表的插入和删除

插入和删除节点是链表操作中的常见问题。如果链表头和尾部的指针没有正确调整，可能会导致链表断裂或数据丢失。可以通过在插入、删除操作后打印链表的状态来调试问题：

```c
pr_info("Before deleting node1\n");
list_for_each_entry(pos, &my_list, list) {
    printk(KERN_INFO "Node data: %d\n", pos->data);
}

list_del(&node1->list);  // 删除节点

pr_info("After deleting node1\n");
list_for_each_entry(pos, &my_list, list) {
    printk(KERN_INFO "Node data: %d\n", pos->data);
}
```

这样可以帮助确认删除操作是否影响了链表的结构。

## 9.4 检查链表内存越界

链表操作时，错误的指针操作（例如指针未正确初始化或链表节点被误删除）可能会导致内存越界或数据破坏。内核提供了 `list_empty` 来检查链表是否为空：

```c
if (list_empty(&my_list)) {
    pr_info("The list is empty\n");
}
```

使用这个函数可以避免在空链表上执行删除或访问操作，减少错误发生的机会。

------

# 10. 小结（与相关章节勾连）

在本节中，我们全面探讨了 Linux 内核中单链表的定义、操作、接口以及调试方法。链表作为一种高效的动态数据结构，已经成为内核中管理和调度资源的重要工具。

- **数据结构视角**：通过 `struct list_head` 及其相关操作，我们了解到如何在内核中实现和使用单链表。
- **开发者视角**：内核提供了丰富的接口宏来操作链表，在实际开发中，熟练掌握这些操作能够帮助我们高效管理资源。
- **用户视角**：对于开发者而言，掌握单链表的调试方法与验证流程至关重要，尤其是在多线程环境中对链表进行操作时，必须确保线程安全性。

------

**与其他章节的联系：**

- 本节与**进程调度**相关的章节密切相关，尤其是在处理就绪队列时，链表的使用非常广泛。
- 另外，内存管理章节中的`slab`分配器也与链表有很多交集，它们共同帮助内核高效地管理内存资源。

通过进一步学习和理解链表的工作原理，您将能够更好地理解和优化内核中各种复杂数据结构的应用，尤其是在内存、文件系统以及网络管理等模块中。

------

# 11. 附加学习资源

如果您希望更深入地了解单链表和其他数据结构的实现，以下资源可能会有所帮助：

- **Linux内核源码**：直接查看内核中 `list.h` 和其他数据结构的实现。
- **内核文档**：`Documentation/` 目录下关于内存管理、进程调度等方面的文档。
- **《Linux Device Drivers》**：本书详细介绍了内核数据结构和设备驱动开发中的链表应用。

在本节的继续部分，我们将进一步探讨**单链表的高级应用**，包括其在内核中的特定用途、优化技巧以及内存管理方面的详细内容。通过这些高级应用，您将能够理解单链表如何高效地与其他内核数据结构配合使用，以实现更复杂的功能。

------

# 12. 单链表的高级应用

单链表作为一个基本的线性数据结构，在内核中的应用远超基础的节点插入和删除。在内核中，单链表不仅仅用于存储数据，它还在内存管理、进程调度、网络传输等多个领域发挥着重要作用。以下是几个典型的应用场景：

## 12.1 等待队列与进程调度

在Linux内核的进程调度中，等待队列是单链表的一种重要应用。等待队列用于管理那些需要等待某个事件（如I/O操作完成、锁释放等）的进程。在进程调度模块中，`wait_queue_head_t` 和 `wait_queue_t` 就是基于链表实现的。

- **链表在等待队列中的作用**：`wait_queue_head_t` 是一个链表头，它管理着一个等待队列的所有进程，而每个等待队列元素（`wait_queue_t`）则是一个链表节点，保存了进程信息和与该进程相关的条件。

```c
struct wait_queue_head my_wq;
INIT_WAITQUEUE_HEAD(&my_wq);
```

## 12.2 网络数据包的缓冲管理

在网络协议栈中，网络数据包（`sk_buff`）也是通过单链表进行管理的。每个数据包都有一个`sk_buff`结构，其中包含了指向下一个数据包的指针，从而形成了一个链表。内核通过链表来实现数据包的缓存和调度。

```c
struct sk_buff *skb;
INIT_LIST_HEAD(&skb->list);
list_add(&skb->list, &my_skb_queue);
```

在网络处理过程中，内核可以高效地在链表中插入、删除和遍历数据包，实现数据包的顺序管理。

## 12.3 内存池和Slab分配器中的链表

内核中的Slab分配器（`slab` allocator）使用链表来管理内存池。Slab分配器将内存池分成多个“Slab”块，每个Slab块可能会有一个链表，用来存储已分配和未分配的内存块。

- **链表的作用**：通过链表，Slab分配器可以轻松地管理不同类型的内存对象，并优化内存分配和回收的效率。

```c
struct kmem_cache *cache;
INIT_LIST_HEAD(&cache->free_list);
```

每个 `kmem_cache` 结构会维护一个空闲内存块的链表，内核通过链表来实现内存块的复用。

## 12.4 驱动管理中的链表

内核中的设备驱动通常需要管理一个设备列表。使用单链表是非常合适的，因为设备驱动的列表会随着设备的插拔频繁变化。内核中的 `device` 结构通常使用单链表来管理设备对象。

```c
struct device *dev;
INIT_LIST_HEAD(&dev->dev_list);
list_add(&dev->dev_list, &device_list_head);
```

这种方式可以高效地插入、删除和遍历设备列表，从而实现对设备的动态管理。

------

# 13. 链表的优化与注意事项

尽管单链表在许多场景下都能提供高效的操作，但在某些高并发或对性能要求极高的场景下，我们需要对其进行优化，确保它在内核中的高效性和稳定性。

## 13.1 减少锁的竞争

在内核的并发环境中，链表的操作可能会涉及到锁竞争问题。为了减少锁竞争，Linux内核提供了对链表操作的锁机制。例如，在操作链表时，可以通过使用自旋锁来保护链表的修改。

```c
spin_lock(&my_list_lock);
list_add(&new_node, &my_list);
spin_unlock(&my_list_lock);
```

通过自旋锁或其他同步机制，可以有效地保护链表在多线程环境下的并发操作，避免数据的不一致性。

## 13.2 内存对齐与缓存优化

链表节点的数据存储方式对内存访问速度有很大影响。为了解决内存对齐问题和提高缓存命中率，内核中经常采用特定的内存对齐方式，以提高链表操作的性能。

例如，在节点结构中使用 `__aligned()` 属性，确保每个节点按缓存行对齐，从而减少内存访问的延迟。

```c
struct my_node {
    int data;
    struct list_head list;
} __aligned(PAGE_SIZE); // 使每个节点的内存对齐
```

## 13.3 链表长度的动态调整

当链表的长度较长时，遍历链表的效率会下降，因此在使用链表时，我们需要对链表的长度和节点数进行合理的控制。在一些高频率操作中，可以考虑使用其他数据结构（如哈希表、红黑树等）来替代单链表，以提高性能。

------

# 14. 总结

在本节中，我们深入探讨了Linux内核中单链表的定义、使用方法、优化技巧和典型应用场景。通过链表，内核能够高效地管理进程、内存、设备以及网络数据包等资源。此外，我们还讲解了调试和优化链表操作的常见方法，以确保链表在高并发、高负载的内核环境中能够稳定运行。

**小结**：

- **数据结构视角**：链表是一种基础且灵活的数据结构，在内核中扮演着重要角色。
- **开发者视角**：了解如何使用内核提供的链表宏和接口，能够帮助开发者在内核中高效地管理数据和资源。
- **用户视角**：对于开发者来说，掌握链表的调试、验证和优化方法，对于提高内核性能和稳定性至关重要。

通过深入学习和理解单链表的实现和应用，您将能够更好地理解Linux内核的资源管理机制，并能在实际开发中高效地运用这一基础数据结构。

------

(已完结)