---
id: knowledge.kernel_subsystems.fundamentals.running_linux_kernel.p02_linux内核中常用的数据结构和算法
title: "Linux内核中常用的数据结构和算法"
kind: publication
status: evolving
domains:
  - linux
  - publication
---

# 第2章\_Linux内核中常用的数据结构和算法

#### (1)\_内核数据结构总览

- 1.1 [**链表（Linked List）**](../../../knowledge/linux/data_structures/单链表_linked_list/大纲.md)
  - 1.1.1 单链表

    - 1.1.2 双向链表

    - 1.1.3 循环链表

    - 1.1.4 链表操作（插入、删除、遍历等）

- 1.2 [**哈希表（Hash Table）**](../../../knowledge/linux/data_structures/哈希表_Hash_Table/P01_数据结构理论基础/P01_哈希表核心原理_空间与时间的终极博弈.md)
  - 1.2.1 基本原理

    - 1.2.2 内核实现：`hlist_head`, `hlist_node`

    - 1.2.3 哈希冲突与解决

- 1.3 [**红黑树（Red-Black Tree）**](../../../knowledge/linux/data_structures/红黑树_rb-tree/大纲.md)

    - 1.3.1 基本原理

    - 1.3.2 内核实现：`struct rb_node`

    - 1.3.3 应用场景：内存管理、进程调度

- 1.4 **AVL树（AVL Tree，理论对照）**

    - 1.4.1 基本原理

    - 1.4.2 与红黑树的平衡策略和旋转次数对比

    - 1.4.3 Linux 内核没有提供通用的 AVL 树 API，也没有标准的 `struct avl_tree`

    - 1.4.4 内核模块可以按业务需要自行实现 AVL，但通常应优先使用内核 `rbtree`

> **归属说明：** AVL 树在这里用于理解自平衡搜索树及其与红黑树的取舍，不应被理解为 Linux 内核已经提供的通用容器。Linux 内核正式提供的红黑树接口位于 `include/linux/rbtree.h`，核心实现位于 `lib/rbtree.c`，增强接口位于 `include/linux/rbtree_augmented.h`。官方文档说明，红黑树与 AVL 树相比，查找仍为 `O(log n)`，插入和删除的平衡旋转分别最多为 2 次和 3 次，因而具有有界的最坏情况更新性能。参见 [Linux 内核 rbtree 官方文档](https://docs.kernel.org/core-api/rbtree.html)。

- 1.5 **跳表（Skip List）**

    - 1.5.1 跳表的概念

    - 1.5.2 跳表在内核中的应用

- 1.6 **队列（Queue）**

    - 1.6.1 FIFO队列（先进先出）

    - 1.6.2 优先级队列

    - 1.6.3 循环队列（Circular Queue）


#### (2)\_内存管理相关数据结构

- 2.1 **内存池（Memory Pool）**

    - 2.1.1 伙伴系统（Buddy System）

    - 2.1.2 Slab分配器（Slab Allocator）

- 2.2 **页表（Page Tables）**

    - 2.2.1 分页与分段（Paging & Segmentation）

    - 2.2.2 页目录与页表

    - 2.2.3 直接映射与虚拟地址空间

- 2.3 **虚拟内存（Virtual Memory）**

    - 2.3.1 页表映射

    - 2.3.2 延迟分配与交换空间（Swap Space）

- 2.4 **内存区域管理（Memory Zones）**

    - 2.4.1 HighMem, LowMem，DMA区域


#### (3)\_进程与调度相关数据结构

- 3.1 **任务控制块（Task Control Block, TCB）**

    - 3.1.1 `task_struct`的结构与字段

- 3.2 **调度队列（Scheduler Queue）**

    - 3.2.1 任务调度器：调度策略（CFS，实时调度，等等）

    - 3.2.2 时间片与优先级调度

- 3.3 **等待队列（Wait Queue）**

    - 3.3.1 `wait_queue_head_t`，`wait_queue_t`

    - 3.3.2 等待队列的使用场景

- 3.4 **信号量与互斥锁（Semaphores & Mutexes）**

    - 3.4.1 信号量的实现

    - 3.4.2 互斥锁的实现

    - 3.4.3 自旋锁与睡眠锁


#### (4)\_文件系统与磁盘相关数据结构

- 4.1 **文件描述符（File Descriptor）**

    - 4.1.1 `file_operations`，`inode`结构

- 4.2 **文件系统缓存（Page Cache）**

    - 4.2.1 缓存页面管理

    - 4.2.2 `dentry`与`vfs`的实现

- 4.3 **目录项（Dentry）**

    - 4.3.1 Dentry缓存（`dentry_cache`）

- 4.4 **索引节点（Inode）**

    - 4.4.1 `inode`结构分析

    - 4.4.2 文件系统操作：读写、索引管理


#### (5)\_中断与同步相关数据结构

- 5.1 **中断描述符（Interrupt Descriptor）**

    - 5.1.1 `irq_desc`结构

    - 5.1.2 中断处理函数与屏蔽

- 5.2 **自旋锁与读写锁（Spinlock & Rwlock）**

    - 5.2.1 自旋锁与内存屏障

    - 5.2.2 读写锁的实现与应用场景

- 5.3 **工作队列（Workqueue）**

    - 5.3.1 内核工作队列的实现与应用

    - 5.3.2 异步工作队列：`queue_work()`，`flush_work()`


#### (6)\_网络相关数据结构

- 6.1 **套接字（Socket）**

    - 6.1.1 `sock`结构与协议栈

    - 6.1.2 网络协议处理

- 6.2 **网络数据包缓冲区（Skb Buffers）**

    - 6.2.1 `sk_buff`结构解析

    - 6.2.2 网络数据包的接收与发送

- 6.3 **网络队列（Network Queue）**

    - 6.3.1 网络接口队列：`net_device`，`tx_queue`

    - 6.3.2 中断与非中断模式的切换


#### (7)\_算法与操作

- 7.1 **排序算法（Sorting Algorithms）**

    - 7.1.1 快速排序（Quick Sort）

    - 7.1.2 合并排序（Merge Sort）

    - 7.1.3 内核中的排序优化

- 7.2 **查找算法（Search Algorithms）**

    - 7.2.1 二分查找（Binary Search）

    - 7.2.2 哈希查找（Hash Search）

- 7.3 **并发算法（Concurrency Algorithms）**

    - 7.3.1 读写锁与无锁数据结构

    - 7.3.2 屏障与内存屏障（Memory Barrier）


#### (8)\_驱动开发与硬件相关数据结构

- 8.1 **设备模型（Device Model）**

    - 8.1.1 `device`, `driver`, `bus`的关系

    - 8.1.2 设备初始化与注册：`device_register()`

- 8.2 **中断请求（IRQ）**

    - 8.2.1 `request_irq()`与中断处理

    - 8.2.2 中断分配与处理流程

- 8.3 **GPIO管理**

    - 8.3.1 `gpio_desc`, `gpio_chip`结构

    - 8.3.2 GPIO操作与设备树配置


---

#### (9)\_学习建议

1. **理解内核数据结构的底层实现：**每个数据结构都对应着某个内核机制或算法，深入理解这些结构是学习内核的基础。

2. **结合实际代码：** 每学完一个数据结构或算法，最好能查找相关的内核源代码，通过阅读和调试理解其实际应用。

3. **阅读内核文档与源代码：** 阅读 `Documentation/` 文件夹中的文档，有助于更好地理解各个数据结构的使用场景。


这个大纲按模块组织，希望能帮助你在内核学习过程中有一个清晰的学习路径。如果有某些部分需要更详细的内容或解释，可以随时告诉我，我可以继续展开讲解。
