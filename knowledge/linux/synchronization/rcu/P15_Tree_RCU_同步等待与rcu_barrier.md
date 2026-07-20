---
id: knowledge.linux.synchronization.rcu.tree_sync_barrier
title: "Tree RCU 同步等待与 rcu_barrier"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [rcu, synchronize_rcu, rcu_barrier]
---

# 第15章\_Tree\_RCU\_同步等待与\_rcu\_barrier

## 15.1\_两种等待对象不同

| 接口 | 等待对象 | 返回时保证 |
| --- | --- | --- |
| `synchronize_rcu()` | 调用前既存读者 | 一个满足语义的 GP 已过去 |
| `rcu_barrier()` | 调用前已登记的 RCU 回调 | 这些回调已经执行完成 |

多等一个 GP 不能替代 `rcu_barrier()`：回调可能已经成熟却还排队等待执行。

## 15.2\_同步等待怎样交付结果

同步路径把等待请求接入 GP 基础设施并睡眠；GP cleanup 推进完成序列后，通过完成量等等待机制唤醒调用者。多个调用者可以共享已经进行或即将开始的 GP，而不需要各自创建一轮物理 GP。

## 15.3\_barrier怎样覆盖分散回调

`rcu_barrier()` 串行化并发 barrier 请求，保守检查各 CPU 回调状态，必要时在对应 `cblist` 末尾 entrain 一个 barrier 回调；最后一个 barrier 回调完成时唤醒调用者。NOCB bypass 必须先 flush，CPU hotplug 状态也必须纳入扫描。

源码：`kernel/rcu/tree.c::rcu_barrier()`、`rcu_barrier_entrain()` 及 barrier callback；同步路径在 `tree.c` 和 `sync.c` 的公共封装中。

上一篇：[Tree RCU NOCB 回调卸载](P14_Tree_RCU_NOCB回调卸载.md)。

下一篇：[Tree RCU CPU 热插拔与回调迁移](P16_Tree_RCU_CPU热插拔与回调迁移.md)。
