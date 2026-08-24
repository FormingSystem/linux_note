---
id: research.source_reading.workqueue.linux_6_12_object_queue_navigation
title: "Linux 6.12 工作队列对象与投递模块源码概念导读"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [asynchrony, workqueue, queueing, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第2章\_Linux\_6.12\_工作队列对象与投递模块源码概念导读

## 2.1\_模块问题

本章追踪一个 work 从提交前的 off-queue 状态，到 pending、pwq 归属、active/inactive 和 pool worklist。worker 如何执行和 flush 如何等待分别留给 P03/P04。

## 2.2\_五层对象与状态所有权

| 对象 | 关键字段/概念 | 主要锁域 |
| --- | --- | --- |
| `work_struct` | `data`、`entry`、`func` | 原子 data + 当前 pool lock |
| `pool_workqueue` | pool/wq、color、in-flight、nr_active、inactive | pool lock；部分 wq 管理锁 |
| `worker_pool` | lock、worklist、worker/idle、nr_running | `pool->lock` |
| `workqueue_struct` | pwqs、mutex、colors、attrs、rescuer、flags | `wq->mutex` 与专用锁域 |
| `worker` | task、current_work/pwq、scheduled | pool lock 与 worker 自身执行上下文 |

固定提交在结构注释中用 I/L/WQ/MD 等缩写标记访问规则。先读文件顶部锁规则，再读字段，避免把所有状态都误认为由 pool lock 保护。

## 2.3\_从queue\_work到insert\_work

```text
queue_work(wq, work)
  → queue_work_on(WORK_CPU_UNBOUND, wq, work)
    → test_and_set_bit(PENDING)
    → __queue_work(cpu, wq, work)
      → 选择 bound/unbound CPU 与 pwq
      → 检查 last_pool，保持同一 work 非重入
      → pwq->nr_in_flight[work_color]++
      → pwq_tryinc_nr_active()
      → insert_work(pool->worklist 或 pwq->inactive_works)
      → kick_pool()
```

具体函数体见[投递与激活源码实现](../source_explanations/P02_Linux_6.12_工作队列投递与激活源码实现.md#2.3_insert_work写入归属并链接)。

## 2.4\_active与inactive

`pwq->nr_active >= max_active` 或前面已有 inactive work 时，新的普通 work 插入 `pwq->inactive_works` 并带 INACTIVE 标志。活动 work 完成后 `pwq_dec_nr_active()` 触发 `pwq_activate_first_inactive()`，再把工作移到 pool worklist。barrier work 也可能带 INACTIVE 标志但不位于 inactive list，因此源码注释特别警告不能仅看标志推断链表位置。

## 2.5\_unbound选择与RCU生命期

unbound queue 根据允许 CPU 与 pod 选择 pwq；`cpu_pwq` 指针通过 RCU 读取。若选中的 pwq refcnt 已归零，`__queue_work()` 释放 pool lock 后重试。旧 pwq 的释放也经 RCU/kthread work 延迟，避免热路径读取悬空连接对象。

## 2.6\_delayed与RCU\_work边界

`delayed_work` 在 work 之外保存 timer、目标 wq 和 CPU；timer 到期才调用 `__queue_work()`。`rcu_work` 在 RCU callback 之后排队内含 work。两者都增加一个前置异步生产者，cancel/destroy 必须覆盖 timer/RCU 阶段，不能只看 pool worklist。

## 2.7\_复核问题

- work.data 在 queued 与 off-queue 时分别编码什么？
- last_pool 检查为什么与“同一 work 不并发执行”有关？
- INACTIVE 标志为什么不能直接推断 work 位于 inactive_works？
- unbound pwq 更换时，哪一层负责旧连接对象生命期？

总索引：[工作队列源码总阅读索引](P01_Linux_6.12_工作队列源码总阅读索引.md#1.6_建议阅读顺序)。

上一篇：[工作队列源码总阅读索引](P01_Linux_6.12_工作队列源码总阅读索引.md)。

下一篇：[worker 管理模块源码概念导读](P03_Linux_6.12_worker管理模块源码概念导读.md)。
