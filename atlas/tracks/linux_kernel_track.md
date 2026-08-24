---
id: atlas.tracks.linux_kernel
title: "Linux内核机制学习路线"
kind: track
status: maintained
domains:
  - navigation
  - linux
  - kernel
---

# 第1章\_Linux内核机制学习路线

## 1.1\_路线目标

本路线面向希望建立 Linux 内核整体模型的读者。完成后应能说明内核如何启动、代码运行在哪些上下文、共享对象如何同步和回收，以及设备如何纳入统一模型。

开始前建议先在[知识库专题阅读与评审地图](../maps/knowledge_review_map.md)查看各专题和章节的人工确认程度；路线中的顺序表示认知依赖，不表示对应内容已经完成评审。

## 1.2\_第一阶段\_内核边界与源码定位

1. [宏内核和微内核](../../knowledge/foundations/operating_systems/concepts/宏内核和微内核.md)。
2. [Linux 内核概貌](../../knowledge/linux/architecture/kernel_composition/linux内核概貌.md)。
3. [Linux kernel 目录结构说明](../../knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md)。
4. 选读《奔跑吧 Linux 内核》编排中的[Linux 系统基础知识](../../publications/books/running_linux_kernel/P01_linux系统基础知识.md)。
5. 选读[内核引导和初始化](../../publications/books/running_linux_kernel/P03_内核引导和初始化.md)。

阶段验收：能从功能需求判断大致源码目录，区分用户空间、内核空间和模块边界。

## 1.3\_第二阶段\_数据组织与对象生命周期

1. [单链表](../../knowledge/linux/data_structures/单链表_linked_list/大纲.md)。
2. [哈希表理论基础](../../knowledge/linux/data_structures/哈希表_Hash_Table/P01_数据结构理论基础/P01_哈希表核心原理_空间与时间的终极博弈.md)。
3. [Linux hlist](../../knowledge/linux/data_structures/哈希表_Hash_Table/P02_Linux_内核_5.10_核心实现/P02_内核基石_hlist非对称链表.md)。
4. [红黑树基础](../../knowledge/linux/data_structures/红黑树_rb-tree/P01_树的基本概念.md)，随后按目录序号读至 Linux rbtree 和 Maple Tree。
5. [kref 要解决的问题](../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md)，随后按序完成生命周期专题。
6. [devres API](../../knowledge/linux/object_lifetime/devres/devres_API说明.md)。

阶段验收：能解释嵌入式节点、容器对象、所有权、引用计数和资源托管的边界。

## 1.4\_第三阶段\_并发与事件

1. 从[同步和异步机制总纲](../../knowledge/linux/synchronization_and_asynchrony/大纲.md)先区分“约束并发状态”和“让事件继续推进”两类问题。
2. 阅读[内存顺序](../../knowledge/linux/synchronization_and_asynchrony/synchronization/memory_ordering/大纲.md)，再进入[锁机制](../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/大纲.md)，先区分可睡与不可睡上下文，再沿[锁源码总阅读索引](../../research/source_reading/locking/navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.6_建议阅读顺序)核对 spinlock、mutex 与 rwsem；随后用 [Lockdep 专题](../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/大纲.md#1.1_专题定位)和[源码索引](../../research/source_reading/lockdep/navigation/P01_Linux_6.12_Lockdep源码导读.md#1.6_建议阅读顺序)把锁序、IRQ 上下文和持锁前置条件转成动态验证证据。
3. 对照学习[seqcount/seqlock](../../knowledge/linux/synchronization_and_asynchrony/synchronization/sequence_counters/大纲.md)与[RCU](../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/大纲.md)，理解读重试和延迟回收解决的是不同问题；版本化实现分别从[序列计数器源码总阅读索引](../../research/source_reading/sequence_counters/navigation/P01_Linux_6.12_序列计数器源码总阅读索引.md#1.5_建议阅读顺序)和 [RCU 源码总阅读索引](../../research/source_reading/rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.9_建议的源码阅读顺序)进入。
4. 阅读[等待队列与完成量](../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/大纲.md)，掌握条件等待、事件完成和唤醒规则，再从[等待与完成量源码总阅读索引](../../research/source_reading/waiting_notification/navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.5_建议阅读顺序)还原入队、wake 与 done 令牌。
5. 从[异步机制大纲](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/大纲.md)进入，按序阅读[中断机制](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/大纲.md)与[工作队列](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/workqueue/大纲.md)，并用[工作队列源码总阅读索引](../../research/source_reading/workqueue/navigation/P01_Linux_6.12_工作队列源码总阅读索引.md#1.6_建议阅读顺序)核对执行上下文、pool、worker 和 flush。
6. 阅读[定时与延迟执行](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/大纲.md)，区分忙等待、睡眠、timer、hrtimer 与 delayed work。

阶段验收：面对一段内核代码，能判断其执行上下文、能否睡眠、需要哪类同步以及退出时如何取消异步工作。

## 1.5\_第四阶段\_设备与I\_O

1. 按序阅读[VFS 子系统专题](../../knowledge/kernel_subsystems/vfs/大纲.md)，建立 path、mount、dentry、inode、file、页缓存和回收的完整模型。
2. 按序阅读[Linux 设备模型专题](../../knowledge/linux/device_model/大纲.md)。
3. 阅读[错误指针机制](../../knowledge/linux/error_handling/error_pointer/错误指针机制简介.md)。
4. 阅读[poll 与 epoll 的区别](../../knowledge/linux/io_model/blocking_io/poll与epoll的区别.md)。
5. 按序阅读[异步通知](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/大纲.md)。
6. 阅读[Linux 内核日志](../../knowledge/kernel_subsystems/tracing/logging/Linux_内核日志.md)，建立最基本的观测手段。

阶段验收：能描述路径和打开文件怎样进入 I/O，设备怎样注册、匹配和暴露节点，以及阻塞唤醒和异步通知怎样接回用户接口。

## 1.6\_阅读方法

- 每一阶段先画对象关系和调用方向，再进入 API 细节。
- 对版本敏感的实现记录内核版本；稳定文档只保留跨版本成立的模型。
- 使用[仓库内容索引](../indexes/content_index.md)查找扩展材料，使用实验或源码证据验证关键结论。
