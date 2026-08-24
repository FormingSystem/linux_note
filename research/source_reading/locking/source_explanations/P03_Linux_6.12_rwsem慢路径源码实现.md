---
id: research.source_reading.locking.linux_6_12_rwsem_slowpath_implementation
title: "Linux 6.12 rwsem 慢路径源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, rwsem, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第3章\_Linux\_6.12\_rwsem慢路径源码实现

## 3.1\_实现讲解边界

本章展开 `struct rw_semaphore`、`struct rwsem_waiter` 和 `rwsem_mark_wake()` 的关键状态。读/写慢路径的所有优化分支不逐句复制，只说明它们怎样登记 waiter、等待所有权并处理失败退出。

## 3.2\_源码符号覆盖账本

| 标题 | 上游位置 | 解释 |
| --- | --- | --- |
| [rwsem对象](#3.3_rw_semaphore对象布局) | `include/linux/rwsem.h:36-67` | count、owner、OSQ、wait_lock/list |
| [waiter类型](#3.4_rwsem_waiter区分读写请求) | `kernel/locking/rwsem.c:331-365` | READ/WRITE 与唤醒批次 |
| [mark wake](#3.5_rwsem_mark_wake先记账再唤醒) | `kernel/locking/rwsem.c:395-523` | writer 单醒、reader 批量 |

## 3.3\_rw\_semaphore对象布局

```c
/**
 * @brief 非 RT rwsem 的功能状态与慢路径队列。
 * @note 中文说明由仓库补充；裁剪自 include/linux/rwsem.h。
 */
struct rw_semaphore {
    atomic_long_t count;      /* 读份额、写锁与等待标志的编码。 */
    atomic_long_t owner;      /* 写 owner 或读 owner 提示及标志。 */
#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
    struct optimistic_spin_queue osq;
#endif
    raw_spinlock_t wait_lock; /* 只保护慢路径 wait_list 修改。 */
    struct list_head wait_list;
    /* 省略：debug 与 Lockdep 字段。 */
};
```

count 决定功能占有，owner 辅助自旋/诊断，wait_list 保存调度等待者。任意单一字段都不足以还原完整状态。

## 3.4\_rwsem\_waiter区分读写请求

```c
enum rwsem_waiter_type {
    RWSEM_WAITING_FOR_WRITE,
    RWSEM_WAITING_FOR_READ,
};

/** @brief wait_lock 保护下登记的一次 rwsem 慢路径请求。 */
struct rwsem_waiter {
    struct list_head list;
    struct task_struct *task;
    enum rwsem_waiter_type type;
    unsigned long timeout;
    bool handoff_set;
};
```

唤醒者必须先读队首 type 才能决定交付一个写者还是连续读者。`MAX_READERS_WAKEUP` 还限制单轮 reader 批量，避免唤醒工作和计数调整无界扩大。

## 3.5\_rwsem\_mark\_wake先记账再唤醒

```c
/**
 * @brief 在 wait_lock 下选择可获权 waiter，并加入 wake_q。
 * @param sem 目标 rwsem，调用者已持有 wait_lock。
 * @param wake_type 允许唤醒任意、仅读者或读持有场景。
 * @param wake_q 暂存要在锁外实际唤醒的任务。
 */
static void rwsem_mark_wake(struct rw_semaphore *sem,
                            enum rwsem_wake_type wake_type,
                            struct wake_q_head *wake_q)
{
    struct rwsem_waiter *waiter = rwsem_first_waiter(sem);

    if (waiter->type == RWSEM_WAITING_FOR_WRITE) {
        if (wake_type == RWSEM_WAKE_ANY)
            wake_q_add(wake_q, waiter->task);
        return;
    }

    /* 队首是 reader：计算可批量放行数量并原子调整 count。 */
    /* 省略：最多 MAX_READERS_WAKEUP、handoff 与 waiter->task 发布。 */
    /* 对每个已取得读份额的 waiter 调用 wake_q_add_safe()。 */
}
```

writer 分支只标记一个队首任务；reader 分支先把一批读份额计入 count，再让 waiter task 可被唤醒。实际 `wake_up_q()` 在释放 wait_lock 后执行，缩短 raw 锁持有时间。

## 3.6\_慢路径退出边界

`rwsem_down_read_slowpath()` 和 `rwsem_down_write_slowpath()` 各自在栈上创建 waiter，wait_lock 下入队，循环检查任务字段/所有权后 schedule。可中断失败必须删除 waiter 并修正 WAITERS/HANDOFF 标志。获得所有权或失败返回以前，栈上 waiter 不能失效。

## 3.7\_复核问题

- 为什么 count 和 owner 要放在相邻热点位置，代价是什么？
- reader 批量唤醒为什么必须先调整 count 再唤醒任务？
- wake_q 将哪一段工作移出了 wait_lock 临界区？

模块导读：[Linux 6.12 mutex 与 rwsem 模块源码概念导读](../navigation/P03_Linux_6.12_mutex与rwsem模块源码概念导读.md#3.4_rwsem完整调用链)。

总索引：[Linux 6.12 锁源码总阅读索引](../navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.6_建议阅读顺序)。

上一篇：[mutex 慢路径源码实现](P02_Linux_6.12_mutex慢路径源码实现.md)。
