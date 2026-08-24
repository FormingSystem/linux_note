---
id: research.source_reading.waiting_notification.linux_6_12_wait_implementation
title: "Linux 6.12 wait.c 入队与唤醒源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, waitqueue, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_wait\_c入队与唤醒源码实现

## 1.1\_实现讲解边界

本章唯一展开 `___wait_event` 骨架、`prepare_to_wait_event()`、`__wake_up_common()` 和 `finish_wait()`。中文 Doxygen/注释由仓库补充，代码省略超时和特殊回调细节。

## 1.2\_源码符号覆盖账本

| 标题 | 上游位置 | 原理 |
| --- | --- | --- |
| [wait宏骨架](#1.3_prepare_to_wait_event登记与信号分支) | `include/linux/wait.h:300-322` | 循环 prepare、重检、调度、finish |
| [prepare](#1.3_prepare_to_wait_event登记与信号分支) | `kernel/sched/wait.c:270-304` | 队列锁下登记/信号竞态 |
| [wake扫描](#1.4_wake_up_common按回调与exclusive额度扫描) | `kernel/sched/wait.c:73-110` | mode/key/callback/独占数量 |
| [finish](#1.5_finish_wait恢复任务并移除栈上entry) | `kernel/sched/wait.c:347-380` | TASK_RUNNING 与安全删除 |

## 1.3\_prepare\_to\_wait\_event登记与信号分支

```c
/**
 * @brief 在 waitqueue 锁下登记当前任务，或处理可中断失败。
 * @return 0 表示可继续检查/等待，-ERESTARTSYS 表示信号退出。
 */
long prepare_to_wait_event(struct wait_queue_head *wq_head,
                           struct wait_queue_entry *wq_entry,
                           int state)
{
    unsigned long flags;
    long ret = 0;

    spin_lock_irqsave(&wq_head->lock, flags);
    if (signal_pending_state(state, current)) {
        list_del_init(&wq_entry->entry); /* 退出者不能再消费后续exclusive事件。 */
        ret = -ERESTARTSYS;
    } else {
        if (list_empty(&wq_entry->entry)) {
            if (wq_entry->flags & WQ_FLAG_EXCLUSIVE)
                __add_wait_queue_entry_tail(wq_head, wq_entry);
            else
                __add_wait_queue(wq_head, wq_entry);
        }
        set_current_state(state);       /* 入队后再发布可睡状态。 */
    }
    spin_unlock_irqrestore(&wq_head->lock, flags);
    return ret;
}
```

wait 宏随后才重新求值 condition 并决定 schedule，使事件发生在入队后、schedule 前时也能把 task 恢复为 runnable。

## 1.4\_wake\_up\_common按回调与exclusive额度扫描

```c
/**
 * @brief 在持有 wq_head.lock 时扫描 waiter 并调用各自唤醒回调。
 * @return 尚未消费的 exclusive 唤醒额度。
 */
static int __wake_up_common(struct wait_queue_head *wq_head,
                            unsigned int mode, int nr_exclusive,
                            int wake_flags, void *key)
{
    wait_queue_entry_t *curr, *next;

    list_for_each_entry_safe(curr, next, &wq_head->head, entry) {
        unsigned flags = curr->flags;
        int ret = curr->func(curr, mode, wake_flags, key);
        if (ret < 0)
            break;
        if (ret && (flags & WQ_FLAG_EXCLUSIVE) && !--nr_exclusive)
            break;
    }
    return nr_exclusive;
}
```

非独占 waiter 即使回调成功也不减少 exclusive 额度；回调返回 0 表示该 entry 没有成功唤醒，扫描应继续。

## 1.5\_finish\_wait恢复任务并移除栈上entry

```c
/** @brief 在 waiter 离开循环前恢复 TASK_RUNNING 并清理队列节点。 */
void finish_wait(struct wait_queue_head *wq_head,
                 struct wait_queue_entry *wq_entry)
{
    unsigned long flags;

    __set_current_state(TASK_RUNNING);
    if (!list_empty_careful(&wq_entry->entry)) {
        spin_lock_irqsave(&wq_head->lock, flags);
        list_del_init(&wq_entry->entry);
        spin_unlock_irqrestore(&wq_head->lock, flags);
    }
}
```

entry 常在当前栈上；只有 finish 与并发 wake 完成同步后，函数返回和栈内存失效才安全。

## 1.6\_复核问题

- signal 分支为什么要在同一队列锁下删除 exclusive waiter？
- condition 在 prepare 后求值关闭了哪一个交错窗口？
- wake 成功与任务实际运行之间还隔着什么状态？

模块导读：[Linux 6.12 普通等待队列模块源码概念导读](../navigation/P02_Linux_6.12_普通等待队列模块源码概念导读.md#2.3_等待侧调用链)。

总索引：[Linux 6.12 等待与完成量源码总阅读索引](../navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.5_建议阅读顺序)。

下一篇：[completion.c 令牌与等待源码实现](P02_Linux_6.12_completion_c令牌与等待源码实现.md)。
