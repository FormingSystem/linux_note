---
id: knowledge.linux.synchronization.rcu.tree_node_aggregation
title: "Tree RCU rcu_node 树与分层汇聚"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [rcu, rcu_node, scalability]
---

# 第9章\_Tree\_RCU\_rcu\_node树与分层汇聚

## 9.1\_等待集合怎样建立

GP 初始化时，`rcu_gp_init()` 为各层节点设置当前 GP 序列，并令 `qsmask` 从 `qsmaskinit` 取得保守等待集合。叶节点的位代表 CPU；更高层节点的位代表子节点。

## 9.2\_报告怎样向根传播

```text
CPU 本地 QS
→ rcu_report_qs_rdp() 锁叶节点
→ 清除 rdp->grpmask 对应位
→ 叶 qsmask 非零：停止
→ 叶 qsmask 为零且无阻塞旧任务：向父节点清位
→ 重复直到根
```

`rcu_report_qs_rnp()` 只有在当前节点不再等待 CPU/子节点，并且 PREEMPT_RCU 没有阻塞当前 GP 的旧任务时才继续向上。根节点满足组合条件后调用根报告路径唤醒 GP kthread。

## 9.3\_树解决什么又付出什么

树避免所有 CPU 直接争抢一个全局完成缓存行；争用被限制在叶节点和逐层完成事件上。但它不是零成本：提交 QS 需要节点锁、缓存一致性通信和跨层内存顺序。优势来自这些操作按 CPU、按 GP 发生，而不是每次读取都发生。

## 9.4\_源码入口

- `kernel/rcu/tree.h::rcu_node`、`rcu_data::mynode/grpmask`。
- `kernel/rcu/tree.c::rcu_gp_init()`。
- `kernel/rcu/tree.c::rcu_report_qs_rdp()`、`rcu_report_qs_rnp()`。

上一篇：[Tree RCU QS、EQS 与 Context Tracking](P08_Tree_RCU_QS_EQS与Context_Tracking.md)。

下一篇：[Tree RCU force-QS、迟延与 Stall](P10_Tree_RCU_force_QS迟延与Stall.md)。
