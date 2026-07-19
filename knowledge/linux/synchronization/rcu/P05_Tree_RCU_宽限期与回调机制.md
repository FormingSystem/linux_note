---
id: knowledge.linux.synchronization.rcu.tree_rcu_grace_period_and_callbacks
title: "Tree RCU 宽限期与回调机制"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
topics:
  - rcu
  - grace_period
  - callback
---

# 第5章\_Tree\_RCU\_宽限期与回调机制

## 5.1\_宽限期是一个分布式判定过程

一个 Tree RCU GP 需要证明：在 GP 开始前已存在的相关读侧临界区已经全部结束。这不是对某个对象的引用计数，而是对 CPU 和被抢占任务的执行轨迹做保守判定。

## 5.2\_rcu\_data、rcu\_node\_和\_rcu\_state

| 层次 | 职责 |
| --- | --- |
| `rcu_data` | 每 CPU 保存 GP 快照、QS 状态、dynticks 跟踪和回调列表 |
| `rcu_node` | 一个节点聚合一组 CPU/子节点的 QS，并跟踪被抢占读者 |
| `rcu_state` | 全局 GP 序列、GP 线程、层次树和全局调度状态 |

`rcu_node->qsmask` 中的位表示当前 GP 仍在等待哪些 CPU 或子节点。叶节点的位清零后，`rcu_report_qs_rnp()` 向父节点逐层上报；根节点的等待位全部清零且没有仍在阻塞本 GP 的旧任务读者时，GP 才能完成。

## 5.3\_宽限期线程主线

```mermaid
flowchart TD
    A["回调或同步者提出 GP 需求"] --> B["rcu_gp_kthread() 醒来"]
    B --> C["rcu_gp_init() 推进 gp_seq"]
    C --> D["为 rcu_node 树建立 qsmask 快照"]
    D --> E["rcu_gp_fqs_loop() 等待报告"]
    E --> F["CPU/EQS/被抢占任务状态推进"]
    F --> G["rcu_report_qs_rnp() 向根聚合"]
    G --> H{"所有旧读者都已覆盖？"}
    H -- 否 --> E
    H -- 是 --> I["GP 完成，回调分段前移"]
```

`rcu_gp_init()` 使用 `rcu_seq_start()` 推进 `gp_seq`，再处理 CPU online/offline 缓冲状态并初始化各层节点。`rcu_gp_fqs_loop()` 负责正常等待和必要时的 force-quiescent-state 扫描。

## 5.4\_强制静止状态扫描不是强制结束读者

Force-QS 路径不会粗暴地杀死或跳过旧读者。它主要：

- 重新检查某 CPU 是否已经穿越 EQS。
- 检查 CPU 是否已下线。
- 对长时间在内核运行的 CPU 发出 urgent-QS/重调度提示。
- 对 NO_HZ_FULL CPU 使用远程 reschedule/irq_work 促进观测点出现。
- 如实等待仍在 `blkd_tasks` 中的旧 PREEMPT_RCU 读者。

## 5.5\_为什么回调列表要分段

`rcu_segcblist` 把回调划分为四个逻辑段：

| 段 | 含义 |
| --- | --- |
| `RCU_DONE_TAIL` | 已经等过目标 GP，可以执行 |
| `RCU_WAIT_TAIL` | 已绑定某个正在等待的 GP |
| `RCU_NEXT_READY_TAIL` | 下一次 GP 开始后可进入 WAIT |
| `RCU_NEXT_TAIL` | 刚加入，尚未与具体 GP 建立关联 |

这个结构使回调可以批量共享 GP，并在 GP 序列推进时通过调整尾指针快速前移，而不是遍历每个回调对象重新判定。

## 5.6\_call\_rcu()到回调执行

`call_rcu()` 调用 `__call_rcu_common()` 将 `rcu_head` 放入当前 CPU 的回调系统。后续路径大致为：

```text
call_rcu()
  → 排入 rcu_data.cblist
  → rcu_accelerate_cbs() 为回调关联 GP
  → GP 完成后 rcu_advance_cbs() 推进分段
  → rcu_core()
  → rcu_do_batch()
  → func(struct rcu_head *)
```

`rcu_core()` 还会处理静止状态、检查 GP 进度和回调负载。当 `CONFIG_RCU_NOCB_CPU` 启用时，部分 CPU 的回调可被 offload 给 nocb GP/CB 线程，减少对被隔离 CPU 的干扰。

## 5.7\_同步等待与回调屏障

- `synchronize_rcu()` 等待一个覆盖调用前旧读者的 GP。
- `call_rcu()` 使回调在相关 GP 后异步执行。
- `rcu_barrier()` 等待调用前已排队的 RCU 回调真正执行完毕。

GP 完成不等于所有回调代码已执行，这是 `synchronize_rcu()` 与 `rcu_barrier()` 不能混用的根本原因。

## 5.8\_源码入口

- [`tree.h`](../../../../research/source_reading/linux/kernel/rcu/tree.h)：三层数据结构。
- [`tree.c`](../../../../research/source_reading/linux/kernel/rcu/tree.c)：GP 线程、QS 聚合、`rcu_core()` 和回调执行。
- [`rcu_segcblist.h`](../../../../research/source_reading/linux/include/linux/rcu_segcblist.h) 与 [`rcu_segcblist.c`](../../../../research/source_reading/linux/kernel/rcu/rcu_segcblist.c)：回调分段。
- [`tree_nocb.h`](../../../../research/source_reading/linux/kernel/rcu/tree_nocb.h)：回调 offload。

上一篇：[Tree RCU 读侧与静止状态](P04_Tree_RCU_读侧与静止状态.md)。

下一篇：[SRCU 私有域与双 index 运行机制](P06_SRCU_私有域与双_index_运行机制.md)。
