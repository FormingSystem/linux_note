---
id: research.source_reading.waiting_notification.linux_6_12_completion_implementation
title: "Linux 6.12 completion.c 令牌与等待源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, completion, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第2章\_Linux\_6.12\_completion\_c令牌与等待源码实现

## 2.1\_实现讲解边界

本章展开 completion 对象、`complete_all()` 和 `do_wait_for_common()`。`complete()` 在该版本转交内部 helper；本文只解释它在同一 swait 锁下饱和增加 done 并唤醒一个 waiter 的语义，不复制 helper 的所有 flag 分支。

## 2.2\_源码符号覆盖账本

| 标题 | 上游位置 | 原理 |
| --- | --- | --- |
| [对象与初始化](#2.3_completion对象与初始化) | `include/linux/completion.h:26-100` | done、swait、init/reinit |
| [等待消费](#2.4_do_wait_for_common等待与消费) | `kernel/sched/completion.c:80-121` | 入队、调度、令牌递减 |
| [广播完成](#2.5_complete_all发布永久完成) | `kernel/sched/completion.c:51-77` | `UINT_MAX` 与 wake all |

## 2.3\_completion对象与初始化

```c
/** @brief 完成令牌与 simple waitqueue 的组合对象。 */
struct completion {
    unsigned int done;           /* 普通令牌数或 UINT_MAX 广播状态。 */
    struct swait_queue_head wait;
};

static inline void init_completion(struct completion *x)
{
    x->done = 0;
    init_swait_queue_head(&x->wait);
}

static inline void reinit_completion(struct completion *x)
{
    x->done = 0;                 /* 不重建队列，也不等待旧 waiter。 */
}
```

reinit 的短小实现恰好说明安全责任在调用者：源码没有替你证明旧 waiter 和 complete 路径已经离开。

## 2.4\_do\_wait\_for\_common等待与消费

```c
/**
 * @brief 在已持有 x->wait.lock 时等待 done，成功后消费普通令牌。
 * @param action schedule_timeout 或 io_schedule_timeout。
 */
static inline long do_wait_for_common(struct completion *x,
                                      long (*action)(long),
                                      long timeout, int state)
{
    if (!x->done) {
        DECLARE_SWAITQUEUE(wait);
        do {
            if (signal_pending_state(state, current)) {
                timeout = -ERESTARTSYS;
                break;
            }
            __prepare_to_swait(&x->wait, &wait);
            __set_current_state(state);
            raw_spin_unlock_irq(&x->wait.lock);
            timeout = action(timeout);
            raw_spin_lock_irq(&x->wait.lock);
        } while (!x->done && timeout);
        __finish_swait(&x->wait, &wait);
        if (!x->done)
            return timeout;
    }
    if (x->done != UINT_MAX)
        x->done--;
    return timeout ?: 1;
}
```

锁同时保护 done 检查和 swait 登记，提前 complete 留下 done；广播状态不递减，后续 waiter 持续直接通过。

## 2.5\_complete\_all发布永久完成

```c
/** @brief 把 completion 置为持续完成并唤醒所有现有 waiter。 */
void complete_all(struct completion *x)
{
    unsigned long flags;

    raw_spin_lock_irqsave(&x->wait.lock, flags);
    x->done = UINT_MAX;
    swake_up_all_locked(&x->wait);
    raw_spin_unlock_irqrestore(&x->wait.lock, flags);
}
```

`completion_done()` 看到 UINT_MAX 会一直为真，却无法证明被唤醒任务已经完成返回；复用必须靠外部阶段协议。

## 2.6\_复核问题

- done 与 swait 为什么必须由同一锁保护复合检查？
- timeout 为 0 时，源码有没有取消完成者？
- complete_all 后普通 wait 为什么不消耗 done？

模块导读：[Linux 6.12 completion 模块源码概念导读](../navigation/P03_Linux_6.12_completion模块源码概念导读.md#3.2_状态所有权)。

总索引：[Linux 6.12 等待与完成量源码总阅读索引](../navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.5_建议阅读顺序)。

上一篇：[wait.c 入队与唤醒源码实现](P01_Linux_6.12_wait_c入队与唤醒源码实现.md)。
