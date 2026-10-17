---
id: research.source_reading.locking.spinlock_types_h
title: "Linux 6.12 spinlock_types.h 类型与配置"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, spinlock, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_spinlock\_types.h类型与配置

## 1.1\_为什么先读类型

调用者声明 `spinlock_t lock`，随后把 `&lock` 交给公共接口，看上去一直在操作同一种锁。可是沿源码追踪时，非 RT 包装会访问 `rlock`，RT 对象却没有这个成员。读操作函数以前，必须先确定编译配置选中了哪一份对象表示，否则连成员地址都可能找错。

上游 `include/linux/spinlock_types.h` 根据 PREEMPT_RT 选择表示；下面以固定 dfaf2136deb2af2e60b994421281ba42f1c087e0、Linux 6.12.20 为边界。中文 Doxygen 和行内中文是仓库阅读补充。本页把类型选择、共享存储视图和初始化连接起来，不推演 RT 调度算法。

## 1.2\_spinlock\_t的配置映射

先读非RT的完整typedef。union把功能raw对象与检查器dep_map视图放在同一片存储；填充长度定位到raw对象内的dep_map，不能理解成两份独立业务锁。

具体看两条成员路径：`lock.rlock.dep_map` 从 raw 对象内取得检查信息，`lock.dep_map` 从匿名结构体的视图取得同一位置。`offsetof` 计算前一条路径的偏移，`__padding` 把后一条路径推进到该位置。这个安排供检查代码访问，不会复制一份功能锁状态；相关配置关闭时，第二个视图也就不再存在。

```c
/**
 * @brief 仓库阅读说明：非PREEMPT_RT分支的普通锁表示。
 * @note 下列typedef来自该分支；dep_map视图仅在调试锁分配配置存在。
 */
typedef struct spinlock {
    union {
        struct raw_spinlock rlock;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define LOCK_PADSIZE (offsetof(struct raw_spinlock, dep_map))
        struct {
            u8 __padding[LOCK_PADSIZE];
            struct lockdep_map dep_map;
        };
#endif
    };
} spinlock_t;
```

同一文件的RT分支在包含rtmutex.h后使用下面的替代typedef。两个定义由条件编译二选一，不可拼在同一个C作用域一起编译。成员lock是RT基础状态；调试map仍是检查身份，不代替互斥状态。

```c
/**
 * @brief 仓库阅读说明：PREEMPT_RT分支的普通锁表示。
 * @note 其功能成员为lock，不再提供非RT的rlock成员。
 */
typedef struct spinlock {
    struct rt_mutex_base lock;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map dep_map;
#endif
} spinlock_t;
```

因此“普通锁总可直接转成raw锁”不成立；严格raw类型本身则另由spinlock_types_raw.h定义。此处只展开普通类型，raw类型的调试字段和架构布局不重复抄入。

## 1.3\_非RT初始状态

类型确定了存储空间，还没有解释初值从哪里来。以 `DEFINE_SPINLOCK(queue_lock)` 为例，最外层要声明一个对象，内层要分别给架构状态、可选调试状态和检查身份赋初值。下面四个宏从内向外完成这件事，既没有申请另一份内存，也没有替调用者取得临界区。

```c
/**
 * @brief 仓库阅读说明：非RT静态初始化逐层填入架构、调试与检查状态。
 * @note 这些宏位于非RT分支，宏参数还用于检查身份命名。
 */
#define ___SPIN_LOCK_INITIALIZER(lockname) \
    { \
    .raw_lock = __ARCH_SPIN_LOCK_UNLOCKED, \
    SPIN_DEBUG_INIT(lockname) \
    SPIN_DEP_MAP_INIT(lockname) }

#define __SPIN_LOCK_INITIALIZER(lockname) \
    { { .rlock = ___SPIN_LOCK_INITIALIZER(lockname) } }

#define __SPIN_LOCK_UNLOCKED(lockname) \
    (spinlock_t) __SPIN_LOCK_INITIALIZER(lockname)

#define DEFINE_SPINLOCK(x) spinlock_t x = __SPIN_LOCK_UNLOCKED(x)
```

最内层初始化raw状态，外层放入普通类型的union/rlock，再构造带类型初始值；DEFINE_SPINLOCK最终声明对象。它不是一次运行时获取。RT分支拥有另一套初始宏，本页不展开其rt_mutex内部初始化；不要把非RT成员初始化器套到RT对象。

两个辅助宏来自 `include/linux/spinlock_types_raw.h`。`SPIN_DEBUG_INIT` 在 DEBUG_SPINLOCK 下填 magic、owner_cpu 和调试 owner，关闭时展开为空；`SPIN_DEP_MAP_INIT` 在 DEBUG_LOCK_ALLOC 下填检查名称和等待类别，关闭时同样为空。`__ARCH_SPIN_LOCK_UNLOCKED` 由所选架构或 UP 类型提供。由此可以预测：关闭诊断后，检查字段及其初始化消失，但架构锁的初值仍须存在；不能把整个初始化删掉。

## 1.4\_阅读回路与边界

练习：删除dep_map视图是否意味着锁失去功能互斥？不是，功能状态仍在rlock；但直接改源删除检查状态会改变诊断能力，不能视为无影响优化。把RT对象传给直接读取rlock的代码又会怎样？字段根本不属于该分支，必须选择与类型一致的操作实现。

公共初始化和获取/释放包装见[spinlock.h](spinlock.h.md#1.3_初始化与对象地址)。模块阅读回到[spinlock导读](../../../navigation/P02_Linux_6.12_spinlock模块源码概念导读.md#2.2_接口层次与状态地址)，完整路线见[锁总索引](../../../navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.3_三条实现分支)。本页是固定源码静态解释，未执行RT或目标编译。
