---
id: research.source_reading.locking.linux_6_12_spinlock_implementation
title: "Linux 6.12 spinlock 包装与 raw 路径源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, spinlock, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_spinlock包装与raw路径源码实现

## 1.1\_实现讲解边界

本章只展开 `spinlock_t` 配置映射、`spin_lock()` 到 raw 层、`do_raw_spin_lock/unlock()` 到架构层三处。中文 Doxygen 为仓库阅读补充，代码裁剪自固定提交；ARM 原子锁函数体受具体架构配置影响，不在此伪造唯一实现。

## 1.2\_源码符号覆盖账本

| 标题 | 上游位置 | 解释 |
| --- | --- | --- |
| [类型配置映射](#1.3_spinlock_t的配置映射) | `include/linux/spinlock_types.h:16-72` | 非 RT raw 与 RT rt_mutex 分叉 |
| [公共到 raw 包装](#1.4_spin_lock到raw包装) | `include/linux/spinlock.h:349-391` | 普通、bh、irq 包装 |
| [raw 到架构边界](#1.5_do_raw_spin_lock到架构边界) | `include/linux/spinlock.h:184-205` | arch 与 mmiowb 边界 |

## 1.3\_spinlock\_t的配置映射

```c
/**
 * @brief 展示普通 spinlock 在非 RT 与 RT 配置下的不同承载对象。
 * @note 仓库补充说明；裁剪自 include/linux/spinlock_types.h。
 */
#ifndef CONFIG_PREEMPT_RT
typedef struct spinlock {
    union {
        struct raw_spinlock rlock; /* 非 RT：普通锁直接承载 raw 状态。 */
        /* 省略：CONFIG_DEBUG_LOCK_ALLOC 视图。 */
    };
} spinlock_t;
#else
typedef struct spinlock {
    struct rt_mutex_base lock;     /* RT：可调度的 RT 锁基础。 */
    /* 省略：Lockdep map。 */
} spinlock_t;
#endif
```

类型分叉本身已经否定“所有配置的 `spinlock_t` 都严格忙等”。真正 raw 语义由 `raw_spinlock_t` 保持。

## 1.4\_spin\_lock到raw包装

```c
/**
 * @brief 非 RT 配置下把普通 spinlock 包装转交给内嵌 raw 锁。
 * @param lock 调用者持有生命周期保证的锁对象。
 */
static __always_inline void spin_lock(spinlock_t *lock)
{
    raw_spin_lock(&lock->rlock);
}

static __always_inline void spin_lock_bh(spinlock_t *lock)
{
    raw_spin_lock_bh(&lock->rlock); /* 同时约束本 CPU bottom half。 */
}

#define spin_lock_irqsave(lock, flags) \
do {                                  \
    raw_spin_lock_irqsave(spinlock_check(lock), flags); \
} while (0)
```

`flags` 是调用现场本地状态，raw lock 是共享状态；包装同时操作二者，但两者所有权不同。

## 1.5\_do\_raw\_spin\_lock到架构边界

```c
/**
 * @brief 取得 raw 锁，并进入架构 MMIO 锁顺序域。
 * @param lock 内含 arch_spinlock_t 的 raw 锁。
 */
static inline void do_raw_spin_lock(raw_spinlock_t *lock)
{
    __acquire(lock);                    /* 静态上下文注解。 */
    arch_spin_lock(&lock->raw_lock);    /* 具体原子算法由体系结构提供。 */
    mmiowb_spin_lock();                 /* 记录 MMIO 写顺序域。 */
}

static inline void do_raw_spin_unlock(raw_spinlock_t *lock)
{
    mmiowb_spin_unlock();
    arch_spin_unlock(&lock->raw_lock);
    __release(lock);
}
```

`__acquire/__release` 是编译期上下文标记，不完成硬件互斥；`arch_spin_*` 才操作架构锁字；`mmiowb_spin_*` 又承担 I/O 写顺序辅助。三个动作不能混为“锁指令”。

## 1.6\_复核问题

- RT 分支为什么不能继续访问 `lock->rlock`？
- `_bh/_irqsave` 增加的是哪一类本地状态？
- `__acquire()`、`arch_spin_lock()`、`mmiowb_spin_lock()` 分别属于检查、互斥还是 I/O 顺序？

模块导读：[Linux 6.12 spinlock 模块源码概念导读](../navigation/P02_Linux_6.12_spinlock模块源码概念导读.md#2.2_接口层次与状态地址)。

总索引：[Linux 6.12 锁源码总阅读索引](../navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.6_建议阅读顺序)。

下一篇：[mutex 慢路径源码实现](P02_Linux_6.12_mutex慢路径源码实现.md)。
