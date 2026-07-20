---
id: knowledge.linux.synchronization.rcu.tree_init_topology_execution
title: "Tree RCU 初始化、拓扑与执行上下文"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [rcu, initialization, execution_context]
---

# 第5章\_Tree\_RCU\_初始化\_拓扑与执行上下文

第四章展示了运行中的状态，本章回答这些状态怎样建立，以及哪些执行者负责推进它们。

## 5.1\_从\_rcu\_init()开始

Linux 6.12.20 的 `kernel/rcu/tree.c::rcu_init()` 初始化 Tree RCU，全局 `rcu_state` 由 `rcu_init_one()` 准备节点层次，`rcutree_prepare_cpu()` 为 CPU 准备 `rcu_data`。每个 `rdp` 通过 `mynode` 指向叶 `rcu_node`，通过 `grpmask` 表示自己在叶节点 `qsmask` 中的位。

```text
rcu_state.node[]
    └─ 根／中间／叶 rcu_node
                       ↑ rdp->mynode
CPU N 的 rcu_data ────┘
                       rdp->grpmask = 本 CPU 在叶节点中的位
```

## 5.2\_五类执行者

| 执行者 | 主要职责 | 是否代表业务写者 |
| --- | --- | --- |
| 当前读者任务 | 维护读侧本地状态 | 否 |
| 本 CPU 调度/context-tracking 路径 | 形成 QS/EQS 或登记被抢占任务 | 否 |
| 本 CPU `rcu_core()` | 识别 GP、提交 QS、推进回调 | 否 |
| GP kthread | 建立等待集合、force-QS、结束 GP | 代表多个 GP 请求 |
| 回调执行上下文/nocb kthread | 执行已经过 GP 的回调 | 代表回调登记者 |

## 5.3\_RCU\_core怎样被调度

`rcu_sched_clock_irq()` 是观测和催促入口之一；`invoke_rcu_core()` 请求本 CPU 运行 RCU core；`rcu_core()` 消费 deferred QS、检查新 GP、提交报告并调用 `rcu_do_batch()`。它们不是一个函数的别名，也不保证只由周期 tick 驱动。

## 5.4\_源码入口

- `kernel/rcu/tree.c::rcu_init()`、`rcu_init_one()`、`rcutree_prepare_cpu()`。
- `kernel/rcu/tree.h::rcu_data`、`rcu_node`、`rcu_state`。
- `kernel/rcu/tree.c::invoke_rcu_core()`、`rcu_core()`、`rcu_sched_clock_irq()`。

上一篇：[Linux Tree RCU 状态与通知机制](P04_Linux_Tree_RCU_状态与通知机制.md)。

下一篇：[Tree RCU GP 请求与全局生命周期](P06_Tree_RCU_GP请求与全局生命周期.md)。
