---
id: knowledge.linux.synchronization.rcu.tree_expedited_gp
title: "Tree RCU Expedited GP"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [rcu, expedited_grace_period]
---

# 第11章\_Tree\_RCU\_Expedited\_GP

普通 GP 倾向于等待 CPU 自然经过 QS；expedited GP 为降低等待延迟，更积极地拍摄参与集合、检查 CPU/任务并使用 IPI 或调度协作取得证明。

## 11.1\_独立状态与共同语义

`synchronize_rcu_expedited()` 仍保证调用前既存读者已经结束，不会降低安全标准；它改变的是取得证明的策略和成本。Linux 6.12.20 的主要实现位于 `kernel/rcu/tree_exp.h`，使用 expedited 序列、节点等待状态和专用推进路径。

## 11.2\_为什么不能循环滥用

更积极的 IPI、远端状态检查和调度干预会增加系统扰动。源码注释明确提醒不要在循环中无节制调用；高频更新更适合异步回调、批量等待或共享普通 GP。

## 11.3\_选择边界

| 需求 | 普通 GP | Expedited GP |
| --- | --- | --- |
| 吞吐与低扰动优先 | 更适合 | 通常不选 |
| 控制路径必须尽快确认旧读者离场 | 可能偏慢 | 可以评估 |
| 高频循环调用 | 应先重构/批量化 | 尤其不应滥用 |

源码：`kernel/rcu/tree_exp.h::synchronize_rcu_expedited()`、expedited funnel/selection/wait 路径。

上一篇：[Tree RCU force-QS、迟延与 Stall](P10_Tree_RCU_force_QS迟延与Stall.md)。

下一篇：[Tree RCU rcu_segcblist 回调状态机](P12_Tree_RCU_rcu_segcblist回调状态机.md)。
