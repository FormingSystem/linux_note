---
id: knowledge.linux.synchronization.rcu.tree_force_qs_stall
title: "Tree RCU force-QS、迟延与 Stall"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [rcu, force_qs, stall]
---

# 第10章\_Tree\_RCU\_force\_QS迟延与\_Stall

## 10.1\_force\_QS不是强迫读者退出

GP kthread等待过久时，只扫描仍在节点 `qsmask` 中的 CPU。它可以读取远端 watching/dynticks 快照，确认远端已经经过 EQS；也可以设置 urgent 状态、请求重调度或发送 IPI，促使远端更快进入本地 RCU 路径。

> **边界：** 协调者可以发现已经存在的证据或催促远端产生证据，但不能在读者仍可能使用旧对象时伪造 QS。

## 10.2\_正常路径与慢路径

| 路径 | 谁形成事实 | 谁提交节点状态 | 跨 CPU 成本 |
| --- | --- | --- | --- |
| 正常 QS | 本 CPU | 本 CPU `rcu_core()` | 节点报告 |
| 远端确认 EQS | 远端此前维护 watching 状态 | 扫描者据快照推进 | 读取远端缓存行 |
| urgent/resched | 远端被催促后形成 QS | 仍由正常报告路径提交 | 标志写入、调度或 IPI |

## 10.3\_Stall检测说明什么

stall detector 表示 GP 长时间无法取得所需证明，常见原因包括过长读侧临界区、CPU 长时间关中断、调度或时钟异常、RCU kthread得不到运行机会。它报告的是“证明链停滞”，不能仅凭一条告警断言某个对象或某个 API 必然错误。

## 10.4\_源码入口

普通 GP 的强制扫描在 `kernel/rcu/tree.c::rcu_gp_fqs_loop()` 及其 force-QS 辅助路径；stall 检测与输出分布于 `kernel/rcu/tree_stall.h`；调度催促与 `rcu_sched_clock_irq()` 配合。

上一篇：[Tree RCU rcu_node 树与分层汇聚](P09_Tree_RCU_rcu_node树与分层汇聚.md)。

下一篇：[Tree RCU Expedited GP](P11_Tree_RCU_Expedited_GP.md)。
