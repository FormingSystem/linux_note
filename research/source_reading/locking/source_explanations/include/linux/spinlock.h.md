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

类型页已经说明非 RT 普通锁内嵌 raw 对象。现在跟随一次 `spin_lock(&queue_lock)`：调用者只提供普通锁地址，公共包装必须把它交给正确的 raw 成员，再由通用上下文包装和架构实现分别约束本地执行、争取共享资格。短小的外层函数只完成其中一段，不能据其行数推断整个获取过程。

本章按上游位置 `include/linux/spinlock.h` 展开普通锁初始化、公共包装与 raw 到架构边界，固定提交为 dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20。类型定义在[spinlock_types.h](spinlock_types.h.md#1.2_spinlock_t的配置映射)唯一展开。本章中文 Doxygen 和注释均为仓库补充，不是上游注释。

先读模块导读建立地址与阶段，再读以下裁剪；SMP 通用 API 内部、GENERIC_LOCKBREAK、调试实现和 ARM 汇编函数体不在本页覆盖。函数上的静态上下文属性属于真实声明，摘录保留这些属性，避免把检查语义从源码中抹掉。

## 1.2\_源码符号覆盖账本

按对象的使用顺序，先在发布前初始化，再取得与归还，最后深入 raw 操作。以下入口表只定位本页已讲的单元；它不表示 CPU 会在一次获取中依次执行所有初始化与释放函数。

| 标题 | 上游位置 | 解释 |
| --- | --- | --- |
| [初始化与对象地址](#1.3_初始化与对象地址) | `include/linux/spinlock.h` | 非RT初始化，debug分支与地址转换 |
| [公共到 raw 包装](#1.4_spin_lock到raw包装) | `include/linux/spinlock.h:349-391` | 普通、bh、irq 包装 |
| [raw 到架构边界](#1.5_do_raw_spin_lock到架构边界) | `include/linux/spinlock.h:184-205` | arch 与 mmiowb 边界 |

## 1.3\_初始化与对象地址

```c
/**
 * @brief 仓库阅读说明：将普通锁转换为其内嵌raw对象地址。
 * @param lock 已有效存在的非RT普通锁对象。
 */
static __always_inline raw_spinlock_t *spinlock_check(spinlock_t *lock)
{
    return &lock->rlock;
}
```

这里没有分配新锁或检测“当前是否持有”，只是返回同一对象内成员地址，并让类型签名约束调用者。下面初始化分支要在对象对外可见之前执行；对已在使用的锁重新初始化会破坏其他参与者的状态。

```c
/**
 * @brief 仓库阅读说明：非RT普通锁初始化的调试与非调试分支。
 * @note 静态key服务检查器；非调试分支赋予该类型的初始状态。
 */
#ifdef CONFIG_DEBUG_SPINLOCK
# define spin_lock_init(lock) \
do { \
    static struct lock_class_key __key; \
    __raw_spin_lock_init(spinlock_check(lock), \
                         #lock, &__key, LD_WAIT_CONFIG); \
} while (0)
#else
# define spin_lock_init(_lock) \
do { \
    spinlock_check(_lock); \
    *(_lock) = __SPIN_LOCK_UNLOCKED(_lock); \
} while (0)
#endif
```

这说明初始化同时建立功能状态与可能存在的检查身份，不是向操作系统注册一个运行时服务。调试分支中，块作用域 `static __key` 为该初始化展开位置提供可持续使用的锁类别身份，`#lock` 把参数拼成检查名称，`LD_WAIT_CONFIG` 标记按配置解释的等待类别；这些都不替代 raw 功能状态。非调试分支先用 `spinlock_check` 核对传入类型，再把初值赋给原对象。初始值的成员布局见[非 RT 初始状态](spinlock_types.h.md#1.3_非RT初始状态)；调试初始化函数体不在本页展开。

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

下面补上同一层的释放配对。它们仍只转交给raw入口；本地上下文恢复发生在后续层，不能把这些短包装的行数当成完整开销。

后续通用层的[irqsave配对](spinlock_api_smp.h.md#1.3_irqsave把哪份状态带回调用者)和[普通获取释放](spinlock_api_smp.h.md#1.2_普通获取和释放的顺序)展示本地状态与检查事件的实际先后。

```c
/**
 * @brief 仓库阅读说明：非RT普通锁的三个释放配对入口。
 * @note irqrestore使用此次获取保存的flags，不无条件打开中断。
 */
static __always_inline void spin_unlock(spinlock_t *lock)
{
    raw_spin_unlock(&lock->rlock);
}

static __always_inline void spin_unlock_bh(spinlock_t *lock)
{
    raw_spin_unlock_bh(&lock->rlock);
}

static __always_inline void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
    raw_spin_unlock_irqrestore(&lock->rlock, flags);
}
```

## 1.5\_do\_raw\_spin\_lock到架构边界

通用层建立本地约束以后，还需要实际争取共享资格。这里进入未启用 DEBUG_SPINLOCK 的分支：`arch_spin_lock` 接收的是 raw 对象内部的架构锁地址；成功后才登记 MMIO 写顺序辅助。释放则在放开架构资格之前处理该辅助，避免把两者的先后颠倒。

```c
/**
 * @brief 取得 raw 锁，并进入架构 MMIO 锁顺序域。
 * @param lock 内含 arch_spinlock_t 的 raw 锁。
 */
static inline void do_raw_spin_lock(raw_spinlock_t *lock) __acquires(lock)
{
    __acquire(lock);                    /* 静态上下文注解。 */
    arch_spin_lock(&lock->raw_lock);    /* 具体原子算法由体系结构提供。 */
    mmiowb_spin_lock();                 /* 记录 MMIO 写顺序域。 */
}

static inline void do_raw_spin_unlock(raw_spinlock_t *lock) __releases(lock)
{
    mmiowb_spin_unlock();
    arch_spin_unlock(&lock->raw_lock);
    __release(lock);
}
```

`__acquire/__release` 是编译期上下文标记，不完成硬件互斥；`arch_spin_*` 才操作架构锁字；`mmiowb_spin_*` 又承担 I/O 写顺序辅助。三个动作不能混为“锁指令”。

这两段内联函数属于 `CONFIG_DEBUG_SPINLOCK` 未启用的分支；调试分支使用外部声明及对应实现。获取时先架构取得、再进入MMIO锁顺序辅助，释放时先结束辅助、再架构释放，最后更新静态上下文标记。辅助是否产生实际机器指令还由架构与配置决定，不能据此承诺设备写已经完成。

## 1.6\_复核问题

- RT 分支为什么不能继续访问 `lock->rlock`？
- `_bh/_irqsave` 增加的是哪一类本地状态？
- `__acquire()`、`arch_spin_lock()`、`mmiowb_spin_lock()` 分别属于检查、互斥还是 I/O 顺序？

模块导读：[Linux 6.12 spinlock 模块源码概念导读](../../../navigation/P02_Linux_6.12_spinlock模块源码概念导读.md#2.2_接口层次与状态地址)。

总索引：[Linux 6.12 锁源码总阅读索引](../../../navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.6_建议阅读顺序)。

下一篇：[mutex 慢路径源码实现](../../kernel/locking/mutex.c.md)。
