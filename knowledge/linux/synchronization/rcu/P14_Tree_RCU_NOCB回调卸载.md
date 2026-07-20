---
id: knowledge.linux.synchronization.rcu.tree_nocb
title: "Tree RCU NOCB 回调卸载"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [rcu, nocb, no_hz_full]
---

# 第14章\_Tree\_RCU\_NOCB回调卸载

NOCB 不会把读侧 QS 责任从 CPU 上删除；它主要把回调等待与执行工作从指定 CPU 卸载给 kthread，减少隔离 CPU 上的回调扰动。

## 14.1\_两个线程角色

- `rcu_nocb_gp_kthread()`：代表一组 offload CPU 等待回调所需 GP并唤醒回调线程。
- `rcu_nocb_cb_kthread()`：在回调成熟后调用 `rcu_do_batch()`。

## 14.2\_bypass为什么存在

高频 `call_rcu()` 若每次都争用 nocb 主锁，会把卸载路径本身变成热点。bypass 链允许生产者先低成本积累，再由 flush 路径并入 `cblist`。但 `rcu_barrier()`、offload 切换和回调长度判断前必须正确 flush，否则可能漏等回调。

```text
业务CPU call_rcu
→ nocb bypass／cblist
→ nocb GP kthread等待成熟
→ 唤醒 nocb CB kthread
→ rcu_do_batch
```

## 14.3\_配置与动态切换

`rcu_nocbs=`、`nohz_full` 或 CPU 隔离配置可以形成 offload mask；Linux 6.12.20 还提供 `rcu_nocb_cpu_offload()` 与 `rcu_nocb_cpu_deoffload()`。动态切换必须与回调、barrier 和线程 park 协调。

源码集中在 `kernel/rcu/tree_nocb.h`，关键入口为 `call_rcu_nocb()`、bypass flush、两个 nocb kthread及动态 offload/deoffload 路径。

上一篇：[Tree RCU 回调执行、批处理与限流](P13_Tree_RCU_回调执行_批处理与限流.md)。

下一篇：[Tree RCU 同步等待与 rcu_barrier](P15_Tree_RCU_同步等待与rcu_barrier.md)。
