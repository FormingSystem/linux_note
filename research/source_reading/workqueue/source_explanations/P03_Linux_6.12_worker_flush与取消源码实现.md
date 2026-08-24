---
id: research.source_reading.workqueue.linux_6_12_worker_flush_cancel_implementation
title: "Linux 6.12 worker、flush 与取消源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [asynchrony, workqueue, worker_pool, flush, cancel, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第3章\_Linux\_6.12\_worker\_flush与取消源码实现

## 3.1\_实现讲解边界

本章唯一展开 worker 主循环/执行边界、wq barrier、workqueue color 和 cancel sync 骨架。完整 `process_one_work()` 包含大量 trace、Lockdep、CPU-intensive 和链式 work 细节，本章只保留状态所有权转换。

## 3.2\_源码符号覆盖账本

| 标题 | 上游位置 | 原理 |
| --- | --- | --- |
| [worker执行](#3.3_worker_thread与process_one_work执行边界) | `kernel/workqueue.c:3115-3384` | current 状态、func、in-flight 完成 |
| [barrier](#3.4_flush颜色与barrier) | `kernel/workqueue.c:3715-3802` | 目标后插 completion work |
| [wq flush](#3.4_flush颜色与barrier) | `kernel/workqueue.c:3805-4062` | color 与 pwq 汇聚 |
| [cancel sync](#3.5_cancel_work_sync撤销与等待) | `kernel/workqueue.c:4325-4380` | disable、cancel、flush、enable |

## 3.3\_worker\_thread与process\_one\_work执行边界

```c
/** @brief worker 主循环的状态骨架。 */
static int worker_thread(void *arg)
{
    struct worker *worker = arg;
    struct worker_pool *pool = worker->pool;

    set_pf_worker(true);
    raw_spin_lock_irq(&pool->lock);
    for (;;) {
        if (!need_more_worker(pool))
            goto sleep;
        if (!may_start_working(pool) && manage_workers(worker))
            continue;
        /* 从 pool->worklist 移入 worker->scheduled。 */
        process_scheduled_works(worker);
    }
}

/** @brief 单 work 执行边界的概念裁剪。 */
static void process_one_work(struct worker *worker,
                             struct work_struct *work)
{
    /* pool lock 下记录 current_work/current_pwq/current_color。 */
    worker->current_work = work;
    worker->current_pwq = get_work_pwq(work);
    /* 省略：从链表移除、清 pending/设置执行状态。 */

    raw_spin_unlock_irq(&worker->pool->lock);
    worker->current_func(work);          /* 工作函数在 pool lock 外执行。 */
    cond_resched();
    raw_spin_lock_irq(&worker->pool->lock);

    worker->current_work = NULL;
    worker->current_pwq = NULL;
    pwq_dec_nr_in_flight(pwq, work_data); /* 最后推进 flush/active 账本。 */
}
```

真实源码要求 `pwq_dec_nr_in_flight()` 是清 current 状态后的最后步骤之一，因为它可能释放 pwq 并唤醒 flusher。

## 3.4\_flush颜色与barrier

```c
/** @brief 跟在目标 work 后执行，以 completion 返回局部越过证据。 */
struct wq_barrier {
    struct work_struct work;
    struct completion done;
    struct task_struct *task;
};

static void wq_barrier_func(struct work_struct *work)
{
    struct wq_barrier *barr = container_of(work, struct wq_barrier, work);
    complete(&barr->done);
}
```

`insert_wq_barrier()` 若目标正在执行，就把 barrier 接到 worker scheduled；否则插到目标 entry 后，并继承目标 color。barrier 不参与 normal nr_active，却增加对应 color 的 in-flight。

```c
/** @brief workqueue flush 的开始阶段概念裁剪。 */
void __flush_workqueue(struct workqueue_struct *wq)
{
    struct wq_flusher this_flusher = { .flush_color = -1 };

    mutex_lock(&wq->mutex);
    this_flusher.flush_color = wq->work_color; /* 封存旧代际。 */
    wq->work_color = work_next_color(wq->work_color);
    flush_workqueue_prep_pwqs(wq, this_flusher.flush_color,
                              wq->work_color);
    mutex_unlock(&wq->mutex);
    wait_for_completion(&this_flusher.done);
    /* 省略：first flusher 级联同色、后续和 overflow 队列。 */
}
```

`flush_workqueue_prep_pwqs()` 只为目标 color 仍有 in-flight 的 pwq 设置 flush_color，并用 `nr_pwqs_to_flush` 汇聚局部完成。

## 3.5\_cancel\_work\_sync撤销与等待

```c
/** @brief 撤销 pending work，并等待可能正在执行的实例结束。 */
static bool __cancel_work_sync(struct work_struct *work, u32 cflags)
{
    bool ret;

    ret = __cancel_work(work, cflags | WORK_CANCEL_DISABLE);
    if (*work_data_bits(work) & WORK_OFFQ_BH)
        WARN_ON_ONCE(in_hardirq());
    else
        might_sleep();

    if (wq_online)
        __flush_work(work, true);
    if (!(cflags & WORK_CANCEL_DISABLE))
        enable_work(work);
    return ret;
}
```

函数注释明确：只有不存在 racing enqueue 时，返回后才保证 work 不 pending、不执行。停止生产者是调用者的前置责任。

## 3.6\_复核问题

- work function 为什么要在 pool lock 外调用？
- `pwq_dec_nr_in_flight()` 为什么能触发 active 激活和 flush 完成？
- barrier 的 completion 证明的是哪一个局部顺序？
- cancel sync 内部为何临时 disable work，返回前又可能 enable？

worker 导读：[Linux 6.12 worker 管理模块源码概念导读](../navigation/P03_Linux_6.12_worker管理模块源码概念导读.md#3.3_worker_thread与process_one_work)。

flush 导读：[Linux 6.12 flush、取消与生命周期模块源码概念导读](../navigation/P04_Linux_6.12_flush取消与生命周期模块源码概念导读.md#4.2_三类完成证明)。

总索引：[Linux 6.12 工作队列源码总阅读索引](../navigation/P01_Linux_6.12_工作队列源码总阅读索引.md#1.6_建议阅读顺序)。

上一篇：[工作队列投递与激活源码实现](P02_Linux_6.12_工作队列投递与激活源码实现.md)。
