---
id: knowledge.linux.synchronization.rcu.tree_callback_execution
title: "Tree RCU 回调执行、批处理与限流"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [rcu, callback, softirq]
---

# 第13章\_Tree\_RCU\_回调执行\_批处理与限流

## 13.1\_从可执行到真正执行

非 offload CPU 上，RCU 工作由 `invoke_rcu_core()` 触发，`rcu_core()` 检查本 CPU 状态并在存在 DONE 回调时调用 `rcu_do_batch()`。因此应区分：

```text
GP完成 ≠ 回调执行完成
GP完成 → 回调可执行 → RCU core得到运行机会 → 批量调用func
```

## 13.2\_为什么必须批处理和限流

回调逐个唤醒线程会制造巨大调度成本；无限制执行积压回调又会长期占用 CPU。`rcu_do_batch()` 因而抽取一批 DONE 回调执行，并受数量、时间和重新调度需求约束。Linux 6.12.20 `tree.c` 还定义批处理退出时间限制。

## 13.3\_kfree\_rcu()的边界

`kfree_rcu()` 把释放动作编码为 GP 后的延迟回收，但仍受回调管线和批处理约束；调用时不是立即释放。`kvfree_rcu()` 还存在批量化和内存压力相关路径，不能简单等同于普通 `call_rcu(head, kfree)`。

源码：`kernel/rcu/tree.c::invoke_rcu_core()`、`rcu_core()`、`rcu_do_batch()`，以及 `kernel/rcu/tree.c` 中的 kvfree RCU 路径。

上一篇：[Tree RCU rcu_segcblist 回调状态机](P12_Tree_RCU_rcu_segcblist回调状态机.md)。

下一篇：[Tree RCU NOCB 回调卸载](P14_Tree_RCU_NOCB回调卸载.md)。
