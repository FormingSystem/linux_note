---
id: knowledge.linux.synchronization.rcu.tasks_tiny_boundaries
title: "Tasks RCU 与 Tiny RCU 实现边界"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [tasks_rcu, tiny_rcu]
---

# 第19章\_Tasks\_RCU与\_Tiny\_RCU实现边界

## 19.1\_名字相同不代表等待同一类读者

普通 Tree RCU 根据普通 RCU 读侧、CPU QS/EQS 和被抢占任务定义等待集合；Tasks RCU 家族面向任务执行轨迹，其 quiescent 条件和扫描对象不同。使用前必须先问“这个域把谁定义成读者”，不能只因为 API 名字含 RCU 就互换。

## 19.2\_Tasks家族

- Tasks RCU 面向任务上下文切换、用户态等任务轨迹。
- Tasks Trace RCU 支持显式 trace 读侧并跟踪相应任务状态。
- Tasks Rude RCU 使用更强、更粗粒度的方式等待相关内核执行轨迹。

Linux 6.12.20 的公共实现集中在 `kernel/rcu/tasks.h`，不同 flavor 拥有各自 GP kthread、任务扫描与回调状态。

## 19.3\_Tiny\_RCU为什么没有树

Tiny RCU 面向非 SMP 的小型配置。只有一个 CPU 时，不需要用 `rcu_node` 层次树汇聚多个 CPU 的 QS；调度/idle 边界与单 CPU 回调队列即可完成更简单的证明。源码位于 `kernel/rcu/tiny.c`。

## 19.4\_选择结论

Tree、Tiny、SRCU 和 Tasks RCU 的差异不是“性能档位”，而是部署条件、读者定义、状态位置和等待证明不同。

上一篇：[SRCU 私有域与双 index 状态机](P18_SRCU_私有域与双_index_状态机.md)。

下一篇：[RCU 通用 API 与调用契约](P20_RCU_通用API与调用契约.md)。
