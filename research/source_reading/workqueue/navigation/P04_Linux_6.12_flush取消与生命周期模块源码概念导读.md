---
id: research.source_reading.workqueue.linux_6_12_flush_cancel_lifecycle_navigation
title: "Linux 6.12 flush、取消与生命周期模块源码概念导读"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [asynchrony, workqueue, flush, cancel, lifecycle, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第4章\_Linux\_6.12\_flush取消与生命周期模块源码概念导读

## 4.1\_模块问题

本章区分四个结论：指定 work 的目标执行实例结束、workqueue 某个颜色代际结束、pending work 被取消、workqueue 不再接受外部提交并可销毁。它们由不同状态证明，不能都简称“清空”。

## 4.2\_三类完成证明

| 目标 | 核心状态 | 等待对象 |
| --- | --- | --- |
| 单 work flush | 目标后插入 `wq_barrier`，或连接正在执行 worker | barrier 内 completion |
| workqueue flush | wq/pwq work_color、flush_color、各颜色 in-flight | `wq_flusher.done` |
| cancel sync | 抢 pending/设置 cancel disable，再 flush 执行实例 | work 状态 + barrier |

`drain_workqueue()` 进一步设置 DRAINING，只允许当前队列 work 链式提交，并反复 flush；它比一次 flush 强，却仍需要调用者最终停止业务生产者。

## 4.3\_颜色汇聚调用链

```text
__flush_workqueue(wq)
  → wq->mutex 下选择 this_flusher.flush_color
  → 推进 wq->work_color
  → flush_workqueue_prep_pwqs()
    → 对有目标颜色 in-flight 的 pwq 设置 flush_color
    → wq->nr_pwqs_to_flush 计数
  → wait_for_completion(this_flusher.done)
  → 最后一个 pwq 归零时 complete first_flusher
  → first flusher 级联同色/后续/overflow flusher
```

后来 queue 的 work 使用新 work_color，不让本轮 flush 被持续输入活锁。实现见[flush 颜色与 barrier](../source_explanations/P03_Linux_6.12_worker_flush与取消源码实现.md#3.4_flush颜色与barrier)。

## 4.4\_cancel调用链

`__cancel_work_sync()` 先以 DISABLE 标志调用 `__cancel_work()`，尝试从 timer/pool 队列抢走 pending 状态；然后在 workqueue online 后调用 `__flush_work(work, true)` 等待正在执行实例，最后按接口恢复 enable。返回时只有在“没有并发重新 enqueue”前提下才保证 work 不 pending/不执行。

delayed work 必须用 delayed 专用 cancel，因为 timer 仍可能持有尚未进入 workqueue 的投递责任。

## 4.5\_destroy与对象生命期

`destroy_workqueue()` 标记 DESTROYING、drain/flush 在途 work、拆除 attrs/pwq/pool 连接并延迟释放 RCU 可见对象。调用前仍必须封住 IRQ、timer、用户入口和其他队列等外部生产者。销毁 wq 也不自动保活嵌入 work 的业务对象。

## 4.6\_依赖检查

workqueue Lockdep map 和 `check_flush_dependency()` 能发现部分 reclaim/flush 依赖；barrier work 使用独立 key 避免 BH 与线程队列假阳性。检查配置关闭、路径未执行或跨对象业务锁未接入时，未告警不构成完整死锁证明。

## 4.7\_复核问题

- 新提交为什么不会无限延长当前 flush color？
- barrier 对 target 提供什么局部顺序，不能证明什么？
- cancel_sync 的注释为什么明确保留“没有 racing enqueues”前提？
- destroy 前应由哪个层次停止所有外部生产者？

总索引：[工作队列源码总阅读索引](P01_Linux_6.12_工作队列源码总阅读索引.md#1.6_建议阅读顺序)。

上一篇：[worker 管理模块源码概念导读](P03_Linux_6.12_worker管理模块源码概念导读.md)。
