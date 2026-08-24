---
id: research.source_reading.locking.linux_6_12_mutex_slowpath_implementation
title: "Linux 6.12 mutex 慢路径源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, mutex, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第2章\_Linux\_6.12\_mutex慢路径源码实现

## 2.1\_实现讲解边界

本章唯一展开 owner 低位标志、`__mutex_lock_common()` 的阶段和 `__mutex_unlock_slowpath()` handoff。完整条件分支以 `kernel/locking/mutex.c` 为准；裁剪代码中的中文 Doxygen 和注释由仓库补充。

## 2.2\_源码符号覆盖账本

| 标题 | 上游位置 | 解释 |
| --- | --- | --- |
| [owner标志](#2.3_owner指针与三个位标志) | `kernel/locking/mutex.c:60-145` | WAITERS/HANDOFF/PICKUP |
| [获取慢路径](#2.4_mutex_lock_common的阶段) | `kernel/locking/mutex.c:574-749` | 自旋、入队、睡眠、pickup |
| [释放慢路径](#2.5_mutex_unlock_slowpath的交接) | `kernel/locking/mutex.c:906-956` | 清 owner、handoff 与 wake |

## 2.3\_owner指针与三个位标志

```c
/**
 * @brief mutex.owner 低位的竞争协议标志。
 * @note task_struct 对齐留下低位；裁剪自 kernel/locking/mutex.c。
 */
#define MUTEX_FLAG_WAITERS 0x01 /* wait_list 非空，unlock 必须检查唤醒。 */
#define MUTEX_FLAG_HANDOFF 0x02 /* 队首请求释放者定向交接。 */
#define MUTEX_FLAG_PICKUP  0x04 /* 交接已写入，等待目标任务确认接收。 */

static inline struct task_struct *__mutex_owner(struct mutex *lock)
{
    return (struct task_struct *)
        (atomic_long_read(&lock->owner) & ~0x07UL);
}
```

WAITERS 连接 owner 原子字与 wait_list；HANDOFF/PICKUP 把“请求定向交接”和“目标确认接收”拆成两个状态，避免新到达者在窗口中无限插队。

## 2.4\_mutex\_lock\_common的阶段

```c
/**
 * @brief mutex 获取慢路径的阅读骨架。
 * @return 0 表示取得锁，负值表示信号/ww 规则导致失败。
 * @note 仅保留状态阶段，不是可编译替代实现。
 */
static int __mutex_lock_common(/* 省略参数 */)
{
    /* 1. 先尝试 owner optimistic spinning，成功则直接取得。 */
    if (mutex_optimistic_spin(lock, ww_ctx, NULL))
        return 0;

    raw_spin_lock(&lock->wait_lock);
    /* 2. 创建 waiter，加入 wait_list 尾部并设置 WAITERS。 */
    __mutex_add_waiter(lock, &waiter, &lock->wait_list);

    for (;;) {
        /* 3. 队首尝试取得；等待过久时可请求 handoff。 */
        if (__mutex_trylock_or_handoff(lock, first))
            break;
        /* 4. 可中断状态检查信号，失败要从队列移除。 */
        raw_spin_unlock(&lock->wait_lock);
        schedule_preempt_disabled();
        raw_spin_lock(&lock->wait_lock);
    }
    /* 5. 删除 waiter，清理 WAITERS，并成为 owner。 */
    return 0;
}
```

实际函数还包含 Lockdep、WW mutex、OSQ 和调度状态细节。阅读重点是 wait_lock 只保护队列修改，任务在释放 wait_lock 后 schedule，醒来再重新竞争/接收交接。

## 2.5\_mutex\_unlock\_slowpath的交接

```c
/**
 * @brief 在存在 waiter 或 handoff 标志时完成释放和唤醒。
 * @param lock 当前任务拥有的 mutex。
 */
static noinline void __mutex_unlock_slowpath(struct mutex *lock,
                                              unsigned long ip)
{
    unsigned long owner = atomic_long_read(&lock->owner);

    for (;;) {
        /* 无 handoff 时尝试把 owner 指针清空，并保留必要标志。 */
        if (!(owner & MUTEX_FLAG_HANDOFF) &&
            atomic_long_try_cmpxchg_release(&lock->owner, &owner,
                                            owner & MUTEX_FLAG_WAITERS))
            break;
        /* handoff 分支由队首 waiter 身份决定交接目标。 */
        /* 省略：wait_lock、首 waiter、PICKUP 与 wake_q 处理。 */
    }
}
```

release cmpxchg 发布临界区写入；wake 只让目标任务可运行。handoff 时 owner 不必先经历“完全空闲”，而是直接编码目标任务与 PICKUP 状态。

## 2.6\_复核问题

- WAITERS 为什么必须同时出现在 owner 字和 wait_list 语义中？
- 任务 schedule 前为什么不能继续持有 wait_lock？
- handoff 与普通清 owner 后竞争分别牺牲什么、改善什么？

模块导读：[Linux 6.12 mutex 与 rwsem 模块源码概念导读](../navigation/P03_Linux_6.12_mutex与rwsem模块源码概念导读.md#3.3_mutex完整调用链)。

总索引：[Linux 6.12 锁源码总阅读索引](../navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.6_建议阅读顺序)。

上一篇：[spinlock 包装与 raw 路径源码实现](P01_Linux_6.12_spinlock包装与raw路径源码实现.md)。

下一篇：[rwsem 慢路径源码实现](P03_Linux_6.12_rwsem慢路径源码实现.md)。
