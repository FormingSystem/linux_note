---
id: research.source_reading.workqueue.linux_6_12_queue_activation_implementation
title: "Linux 6.12 工作队列投递与激活源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [asynchrony, workqueue, queueing, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第2章\_Linux\_6.12\_工作队列投递与激活源码实现

## 2.1\_实现讲解边界

本章唯一展开 `insert_work()` 与 `__queue_work()` 的关键分流。PENDING 原子抢占发生在公开 `queue_work_on()` 外围，完整 bit 编码与 disable API 不在这里重复。

## 2.2\_源码符号覆盖账本

| 标题 | 上游位置 | 原理 |
| --- | --- | --- |
| [insert](#2.3_insert_work写入归属并链接) | `kernel/workqueue.c:2165-2189` | set pwq、link、引用 |
| [pool选择](#2.4_queue_work选择pool并保持非重入) | `kernel/workqueue.c:2235-2328` | bound/unbound、last_pool、color |
| [active分流](#2.5_active与inactive分流) | `kernel/workqueue.c:2329-2344` | 并发额度与 inactive list |

## 2.3\_insert\_work写入归属并链接

```c
/**
 * @brief 在已持有 pool lock 时把 work 归属到 pwq 并插入目标链表。
 */
static void insert_work(struct pool_workqueue *pwq,
                        struct work_struct *work,
                        struct list_head *head,
                        unsigned int extra_flags)
{
    debug_work_activate(work);
    kasan_record_aux_stack_noalloc(work);
    set_work_pwq(work, pwq, extra_flags); /* data 编码 pwq 与 flags。 */
    list_add_tail(&work->entry, head);
    get_pwq(pwq);                        /* work 在途期间保活连接对象。 */
}
```

写 data、链表插入和 pwq 引用在同一 pool 锁域中完成，使 worker 和 cancel 路径能从 work 找回一致归属。

## 2.4\_queue\_work选择pool并保持非重入

```c
/** @brief PENDING 已设置、IRQ 已关闭时选择 pwq/pool 并排队。 */
static void __queue_work(int cpu, struct workqueue_struct *wq,
                         struct work_struct *work)
{
    /* 1. bound 取当前/指定 CPU；unbound 按允许 mask 选 CPU。 */
    pwq = rcu_dereference(*per_cpu_ptr(wq->cpu_pwq, cpu));
    pool = pwq->pool;

    /* 2. 若 work 仍在旧 pool 执行，回到旧 pool/current_pwq 排队，避免重入。 */
    last_pool = get_work_pool(work);
    /* 省略：锁 last_pool、find_worker_executing_work 与重试。 */

    pwq->nr_in_flight[pwq->work_color]++;
    work_flags = work_color_to_flags(pwq->work_color);
    /* 3. 按 active 额度选择目标链。 */
}
```

`__WQ_DESTROYING/__WQ_DRAINING` 还会拒绝不合法的新外部提交；这属于生命周期防御，不能替代调用者先停止生产者。

## 2.5\_active与inactive分流

```c
if (list_empty(&pwq->inactive_works) &&
    pwq_tryinc_nr_active(pwq, false)) {
    insert_work(pwq, work, &pool->worklist, work_flags);
    kick_pool(pool);
} else {
    work_flags |= WORK_STRUCT_INACTIVE;
    insert_work(pwq, work, &pwq->inactive_works, work_flags);
}
```

只有 active 分支进入物理 pool worklist 并可能唤醒 worker。inactive work 已 pending、已计入 in-flight，却不占 active 额度；完成路径释放额度后再激活。

## 2.6\_复核问题

- get_pwq() 保活的是业务对象、workqueue 还是连接对象？
- 为什么 work 正在旧 pool 执行时要继续使用它的 current_pwq？
- inactive work 是否已经属于本轮 flush 的 in-flight？

模块导读：[工作队列对象与投递模块源码概念导读](../navigation/P02_Linux_6.12_工作队列对象与投递模块源码概念导读.md#2.3_从queue_work到insert_work)。

总索引：[Linux 6.12 工作队列源码总阅读索引](../navigation/P01_Linux_6.12_工作队列源码总阅读索引.md#1.6_建议阅读顺序)。

上一篇：[工作队列对象布局源码实现](P01_Linux_6.12_工作队列对象布局源码实现.md)。

下一篇：[worker、flush 与取消源码实现](P03_Linux_6.12_worker_flush与取消源码实现.md)。
