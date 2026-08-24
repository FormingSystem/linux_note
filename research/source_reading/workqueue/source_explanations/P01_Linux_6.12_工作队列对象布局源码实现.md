---
id: research.source_reading.workqueue.linux_6_12_object_layout_implementation
title: "Linux 6.12 工作队列对象布局源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [asynchrony, workqueue, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_工作队列对象布局源码实现

## 1.1\_实现讲解边界

本章只裁剪 `worker_pool`、`pool_workqueue`、`wq_flusher` 和 `workqueue_struct` 的功能字段。文件顶部的锁规则是理解字段的前置证据；中文 Doxygen/注释由仓库补充。

## 1.2\_源码符号覆盖账本

| 标题 | 上游位置 | 原理 |
| --- | --- | --- |
| [worker_pool](#1.3_worker_pool拥有物理执行资源) | `kernel/workqueue.c:184-231` | worklist、worker、idle、manager |
| [pool_workqueue](#1.4_pool_workqueue连接逻辑队列与物理pool) | `kernel/workqueue.c:250-298` | color、in-flight、active/inactive |
| [workqueue_struct](#1.5_workqueue_struct拥有属性与flush域) | `kernel/workqueue.c:331-383` | pwqs、colors、flusher、rescuer、attrs |

## 1.3\_worker\_pool拥有物理执行资源

```c
/** @brief 共享的物理执行池；多数 L 标记字段由 pool->lock 保护。 */
struct worker_pool {
    raw_spinlock_t lock;
    int cpu;
    int node;
    unsigned int flags;
    int nr_running;
    struct list_head worklist;
    int nr_workers;
    int nr_idle;
    struct list_head idle_list;
    struct timer_list mayday_timer;
    struct worker *manager;
    struct list_head workers;
    struct workqueue_attrs *attrs;
    struct rcu_head rcu;
};
```

pool 只拥有执行能力和工作链，不保存某个用户队列的全部属性/flush 语义。

## 1.4\_pool\_workqueue连接逻辑队列与物理pool

```c
/** @brief 一个 workqueue 到一个 worker_pool 的连接与局部账本。 */
struct pool_workqueue {
    struct worker_pool *pool;
    struct workqueue_struct *wq;
    int work_color;
    int flush_color;
    int nr_in_flight[WORK_NR_COLORS];
    bool plugged;
    int nr_active;
    struct list_head inactive_works;
    struct list_head mayday_node;
    struct rcu_head rcu;
};
```

work queued 时 data 高位可指向 pwq；pwq 对齐保证低位可继续编码 work flags。`nr_in_flight[color]` 是 flush 的局部证据，`nr_active/inactive_works` 是并发限制状态，不能互相替代。

## 1.5\_workqueue\_struct拥有属性与flush域

```c
/** @brief 用户可见逻辑 workqueue 的属性、pwq 集合和全局完成状态。 */
struct workqueue_struct {
    struct list_head pwqs;
    struct mutex mutex;
    int work_color;
    int flush_color;
    atomic_t nr_pwqs_to_flush;
    struct wq_flusher *first_flusher;
    struct list_head flusher_queue;
    struct list_head maydays;
    struct worker *rescuer;
    int max_active;
    struct workqueue_attrs *unbound_attrs;
    unsigned int flags;
    struct pool_workqueue __rcu * __percpu *cpu_pwq;
};
```

一个 wq 通过多个 pwq 连接多个 pool，因此 flush 必须汇聚 `nr_pwqs_to_flush`；rescuer 属于 wq 的回收前进保证，而非每个 pool 固有 worker。

## 1.6\_复核问题

- 哪个对象同时指向 wq 与 pool，并保存局部 in-flight？
- pool worklist 与 pwq inactive list 分别受哪种条件驱动？
- 为什么 wq 的 flush 完成必须跨所有相关 pwq 汇聚？

模块导读：[工作队列对象与投递模块源码概念导读](../navigation/P02_Linux_6.12_工作队列对象与投递模块源码概念导读.md#2.2_五层对象与状态所有权)。

总索引：[Linux 6.12 工作队列源码总阅读索引](../navigation/P01_Linux_6.12_工作队列源码总阅读索引.md#1.6_建议阅读顺序)。

下一篇：[工作队列投递与激活源码实现](P02_Linux_6.12_工作队列投递与激活源码实现.md)。
