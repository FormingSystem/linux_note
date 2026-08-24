---
id: research.source_reading.waiting_notification.linux_6_12_waitqueue_navigation
title: "Linux 6.12 普通等待队列模块源码概念导读"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, waitqueue, scheduler, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第2章\_Linux\_6.12\_普通等待队列模块源码概念导读

## 2.1\_模块问题与状态地址

本章回答 wait_event 如何关闭检查—睡眠窗口。`wait_queue_head.lock/head` 保存共享 waiter 链；栈上 `wait_queue_entry` 保存 flags、task/private、回调和链表节点；`current->__state` 保存睡眠状态；业务条件位于调用者对象，不在 waitqueue 中。

## 2.2\_结构与默认回调

`wait_queue_head` 只有自旋锁和链表头。普通 `init_wait_entry()` 把 `private=current`、`func=autoremove_wake_function`，因此 wake 扫描通过 entry callback 进入默认任务唤醒，并在成功时自动移除普通 entry。自定义 callback/poll key 让同一框架支持更多等待对象。

## 2.3\_等待侧调用链

```text
wait_event_interruptible(wq, condition)
  → ___wait_event(... TASK_INTERRUPTIBLE ... schedule())
    → init_wait_entry(entry, flags)
    → 循环 prepare_to_wait_event(wq, entry, state)
    → 重新求值 condition
    → 检查信号返回
    → schedule()
    → finish_wait(wq, entry)
```

`prepare_to_wait_event()` 在同一 `wq_head.lock` 下处理“信号退出时删除 entry”与“正常时入队并 set_current_state”，使 wake 与可中断失败不会各自消费同一 exclusive 事件。具体实现见[`prepare_to_wait_event()`](../source_explanations/P01_Linux_6.12_wait_c入队与唤醒源码实现.md#1.3_prepare_to_wait_event登记与信号分支)。

## 2.4\_唤醒侧调用链

```text
wake_up_interruptible(wq)
  → __wake_up(wq, TASK_INTERRUPTIBLE, nr_exclusive, key)
    → __wake_up_common_lock()
      → 持 wq_head.lock
      → __wake_up_common() 遍历 entry
        → entry->func(entry, mode, wake_flags, key)
        → 成功且 EXCLUSIVE 时递减额度
```

非独占 entry 不消耗 exclusive 额度。默认回调最终让匹配 task 进入调度器 runnable 状态；实际何时运行由调度器决定。

## 2.5\_bookmark与长队列

固定提交的 wake 实现还支持 bookmark 分段扫描路径，使超长队列可以在批次间释放锁。普通 `__wake_up_common()` 的核心循环仍按 flags、callback 返回和 exclusive 额度决定停止。bookmark 是锁持有时间优化，不改变业务条件必须重检的契约。

## 2.6\_waitqueue\_active屏障边界

`waitqueue_active()` 是无锁链表非空观察。头文件明确要求调用者持有队列锁，或在 waker 条件写之后使用额外 `smp_mb()`，与 waiter 的 `set_current_state()` 屏障配对。省掉无条件 wake 的微优化换来严格内存序责任，多数驱动应直接 wake。

## 2.7\_源码阅读核对

- entry 为什么通常在 waiter 栈上，何时才能失效？
- signal pending 与 exclusive wake 并发时，队列锁保护哪个决策？
- wake callback 返回 0 时为什么不能消耗 exclusive 额度？
- `finish_wait()` 为什么先恢复 TASK_RUNNING 再处理链表？

总索引：[等待与完成量源码总阅读索引](P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.5_建议阅读顺序)。

上一篇：[等待与完成量源码总阅读索引](P01_Linux_6.12_等待与完成量源码总阅读索引.md)。

下一篇：[completion 模块源码概念导读](P03_Linux_6.12_completion模块源码概念导读.md)。
