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

1. 从[同步机制总纲](../../knowledge/linux/synchronization/大纲.md)建立“竞争、顺序、互斥、等待和生命周期”问题地图。
2. 阅读[内存顺序](../../knowledge/linux/memory_ordering/大纲.md)，再进入[锁机制](../../knowledge/linux/synchronization/locks/大纲.md)，先区分可睡与不可睡上下文。
3. 对照学习[seqcount/seqlock](../../knowledge/linux/synchronization/sequence_counters/大纲.md)与[RCU](../../knowledge/linux/synchronization/rcu/大纲.md)，理解读重试和延迟回收解决的是不同问题。
4. 阅读[等待队列与完成量](../../knowledge/linux/waiting_notification/大纲.md)，掌握条件等待、事件完成和唤醒规则。
5. 按序阅读[中断机制](../../knowledge/kernel_subsystems/irq/中断机制简介)，再阅读[工作队列](../../knowledge/kernel_subsystems/workqueue/大纲.md)，理解执行上下文如何约束同步选择。
6. 阅读[时间管理](../../knowledge/linux/time_management/定时器简介)，区分忙等待、睡眠、timer、hrtimer 与 delayed work。

阶段验收：面对一段内核代码，能判断其执行上下文、能否睡眠、需要哪类同步以及退出时如何取消异步工作。

## 1.5\_第四阶段\_设备与I\_O

1. 按序阅读[Linux 设备模型专题](../../knowledge/linux/device_model/大纲.md)。
2. 阅读[错误指针机制](../../knowledge/linux/error_handling/error_pointer/错误指针机制简介.md)。
3. 阅读[poll 与 epoll 的区别](../../knowledge/linux/io_model/blocking_io/poll与epoll的区别.md)。
4. 按序阅读[异步通知](../../knowledge/linux/io_model/async_notification/大纲.md)。
5. 阅读[Linux 内核日志](../../knowledge/kernel_subsystems/tracing/logging/Linux_内核日志.md)，建立最基本的观测手段。

阶段验收：能描述设备注册、匹配、节点暴露、阻塞唤醒和异步通知之间的完整路径。

## 1.6\_阅读方法

- 每一阶段先画对象关系和调用方向，再进入 API 细节。
- 对版本敏感的实现记录内核版本；稳定文档只保留跨版本成立的模型。
- 使用[仓库内容索引](../indexes/content_index.md)查找扩展材料，使用实验或源码证据验证关键结论。
