---
id: knowledge.linux.synchronization.rcu.tree_qs_eqs_context_tracking
title: "Tree RCU QS、EQS 与 Context Tracking"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [rcu, context_tracking, no_hz]
---

# 第8章\_Tree\_RCU\_QS\_EQS与\_Context\_Tracking

## 8.1\_事件与持续状态必须分开

- **QS：** 软件认可的一次安全边界事件。
- **EQS：** CPU 持续处于普通内核 RCU 不需要观察的区间，例如满足条件的 user 或 idle。
- **watching：** context tracking/dynticks 对“RCU 当前是否需要观察此 CPU”的软件状态表达。

硬件不会产生 QS 信号；Linux 根据调度、用户态、idle、IRQ/NMI 入口退出维护的软件状态进行证明。

## 8.2\_本地记录与共享报告不是同一步

本 CPU 观察到 QS 后，`rcu_qs()` 清本地 `rdp->cpu_no_qs.b.norm`。随后 `rcu_core()` 中的 `rcu_check_quiescent_state()` 才调用 `rcu_report_qs_rdp()`，把本地事实提交到共享 `rcu_node`。

```text
上下文事件
→ 本 CPU 形成 QS
→ cpu_no_qs=false
→ rcu_core 消费
→ 清叶节点 qsmask 位
```

## 8.3\_IRQ/NMI为什么使判断复杂

CPU 进入 idle 后仍可能被 IRQ/NMI 打断并执行内核代码，因此“执行过 idle 指令”不能直接等于持续 EQS。context tracking 必须维护嵌套和 watching 转换，使远端快照能够判断 CPU 自观察点以来是否真正经过安全区间。

## 8.4\_NO\_HZ\_FULL

NO_HZ_FULL CPU 可以长时间没有调度 tick。Tree RCU 因而不能把 GP 正确性建立在“每个 CPU 定期进一次时钟中断”上，而要结合 dynticks/context-tracking 快照、远端 force-QS 检查和必要的重调度催促。

源码入口：`kernel/rcu/tree.c` 的 watching/dynticks 快照、`rcu_sched_clock_irq()`、`rcu_qs()` 与 `rcu_check_quiescent_state()`，以及 context-tracking 公共路径。

上一篇：[Tree RCU 读者状态与被抢占任务](P07_Tree_RCU_读者状态与被抢占任务.md)。

下一篇：[Tree RCU rcu_node 树与分层汇聚](P09_Tree_RCU_rcu_node树与分层汇聚.md)。
