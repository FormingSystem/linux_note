---
id: knowledge.linux.synchronization.concurrency_and_competition.p03_子模块详解.p19_rcu_读无锁_写延迟回收
title: "RCU(读无锁 写延迟回收)"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
topics:
  - synchronization
  - rcu
---

# 第19章\_RCU(读无锁\_写延迟回收)

## 19.1\_本章在并发专题中的位置

第 18 章介绍了 seqcount/seqlock 的“原地更新、读者重试”模型。本章转向另一条读多写少路线：RCU 允许旧读者继续使用旧版本，写者发布新版本，并在宽限期结束后回收旧对象。

由于 RCU 同时涉及同步、内存顺序、数据结构和对象生命周期，完整内容已拆分到独立的[RCU 专题](../../rcu/大纲.md)。本文保留为当前并发专题的第 19 章，负责衔接前后章和给出最小阅读路径。

## 19.2\_从\_seqcount/seqlock\_转向\_RCU

| 问题 | seqcount/seqlock | RCU |
| --- | --- | --- |
| 更新方式 | 原地修改数据 | 准备新版本并发布指针 |
| 读者行为 | 检测到并发更新后重试 | 允许旧读者读完旧版本 |
| 对象回收 | 不是机制主体 | 必须等待宽限期 |
| 典型对象 | 数值、时间戳、小型快照 | 链表、哈希表、可替换对象 |
| 主要代价 | 读者可能反复重试 | 写侧更复杂且回收延后 |

如果尚不能解释“为什么包含可释放指针的数据结构不适合简单使用 seqcount”，应先回顾[第 18 章](P18_seqcount_seqlock(读重试快照机制).md)。

## 19.3\_RCU\_最小模型

RCU 的正确性可以先拆成四个动作：

1. 读者进入 RCU 读侧临界区。
2. 读者使用 `rcu_dereference()` 取得已发布的指针。
3. 更新者准备新对象，并使用 `rcu_assign_pointer()` 发布。
4. 旧对象经过宽限期后，由 `call_rcu()`、`kfree_rcu()` 或同步等待路径回收。

```c
/* 读侧：在临界区内取得并使用指针 */
rcu_read_lock();
p = rcu_dereference(global_ptr);
use_object(p);
rcu_read_unlock();

/* 写侧：发布新指针，延迟回收旧对象 */
old = rcu_replace_pointer(global_ptr, new, lockdep_is_held(&update_lock));
kfree_rcu(old, rcu);
```

这个模型只回答指针发布和旧对象回收，不自动提供写者之间的互斥，也不保证对象内部多个字段的业务不变量。

## 19.4\_专题阅读路径

按下列顺序进入拆分后的正文：

1. [RCU 核心概念与工作机制](../../rcu/P01_RCU_核心概念与工作机制.md)。
2. [RCU 的硬件基础与内存模型](../../rcu/P02_RCU_的硬件基础与内存模型.md)：先分清缓存一致性、内存顺序与 RCU 的职责。
3. [RCU 种类与内核配置](../../rcu/P03_RCU_种类与内核配置.md)：区分 Tree RCU、Tiny RCU、SRCU 和 Tasks RCU。
4. [Tree RCU 读侧与静止状态](../../rcu/P04_Tree_RCU_读侧与静止状态.md)：追踪读侧记账、QS/EQS 与抢占读者。
5. [Tree RCU 宽限期与回调机制](../../rcu/P05_Tree_RCU_宽限期与回调机制.md)：理解 `rcu_state`、`rcu_node`、宽限期和分段回调链表。
6. [SRCU 私有域与双 index 运行机制](../../rcu/P06_SRCU_私有域与双_index_运行机制.md)：理解可睡眠读者为何需要另一套实现。
7. [RCU 专题大纲](../../rcu/大纲.md)：继续进入 API、应用模板、误用核对与源码阅读。

前六篇负责回答“机器与内核究竟如何使 RCU 成立”，API 从第七篇才开始。若要在驱动中落地，再沿专题大纲继续阅读应用与审查章节。

## 19.5\_与后续章节的衔接

RCU 解决的是读者、更新者与对象回收的并发关系，它不负责让任务因条件不成立而睡眠，也不负责在条件变化时唤醒任务。

因此，完成 RCU 专题后，下一步进入[第 20 章等待队列](P20_等待队列(waitqueue).md)，从“读多写少的对象可见性”转向“条件等待与事件唤醒”。

## 19.6\_本章验收

在继续阅读等待队列前，至少应能回答：

1. RCU 与 seqcount/seqlock 对待并发读者的方式有何不同？
2. `rcu_assign_pointer()` 和 `rcu_dereference()` 各自承担什么职责？
3. 删除对象后为什么不能立即 `kfree()`？
4. RCU 为什么不能代替写侧互斥锁？
5. 什么时候需要 SRCU，什么时候需要 kref/refcount？

