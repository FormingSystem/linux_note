---
id: knowledge.linux.synchronization.rcu.tree_hotplug
title: "Tree RCU CPU 热插拔与回调迁移"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [rcu, cpu_hotplug, callback_migration]
---

# 第16章\_Tree\_RCU\_CPU热插拔与回调迁移

CPU 下线会同时改变三个事实：它是否仍属于 GP 等待集合、它的 per-CPU 状态由谁收尾、它尚未执行的回调由谁接管。因此不能只从 `qsmask` 清除一位就结束。

## 16.1\_GP参与集合变化

online/offline 路径在节点锁和 GP 序列协议下更新 `qsmaskinit` 等参与状态。若 CPU 在 GP 中下线，离线路径或后续 GP 初始化必须确保旧等待位不会永久阻塞，也不能漏掉仍可能存在的旧读者证明。

## 16.2\_回调迁移

离线 CPU 的 `rcu_data::cblist` 不能留在永远不再运行的 CPU 上。回调需要迁移到仍在线 CPU或相应 nocb 管线，并保持分段 GP 归属，保证 `rcu_barrier()` 仍能覆盖它们。

## 16.3\_生命周期顺序

```text
禁止新的本 CPU RCU 工作
→ 收尾或转交 QS/GP 状态
→ flush nocb bypass
→ 迁移回调
→ 更新节点在线集合
→ CPU完成离线
```

实际顺序受 CPU hotplug 状态机约束，不能把上图当成可任意调用的 API 顺序。源码入口位于 `kernel/rcu/tree.c` 的 CPU preparing/starting/dying/dead 路径以及回调迁移辅助函数。

上一篇：[Tree RCU 同步等待与 rcu_barrier](P15_Tree_RCU_同步等待与rcu_barrier.md)。

下一篇：[RCU 实现家族与内核配置](P17_RCU_实现家族与内核配置.md)。
