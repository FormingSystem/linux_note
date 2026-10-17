---
id: research.source_reading.sequence_counters.seqlock_types_implementation
title: "Linux 6.12 seqlock_types.h 类型与配置实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [seqcount, seqlock, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_seqlock\_types.h类型与配置实现

## 1.1\_从状态地址开始

普通 seqcount 需要保存版本，却不保存每个读者或实际写者锁；关联类型要能找到外部锁；seqlock 则把锁本身也放进对象。这三种存储关系决定后续函数该访问哪个成员。先把它们拆开，才不会把可选检查字段误认为功能锁，或以为传入关联指针就取得了锁。

固定NXP linux-imx提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20；上游include/linux/seqlock_types.h，blob dfdf43e3fa3de3acfd294807cc207d6464db0d11。本页解释序号、调试映射、关联指针与内嵌锁分别存在哪里。中文Doxygen和注释是仓库补充，原英文说明省略，声明与条件分支保留。

从[总索引](../../../navigation/P01_Linux_6.12_序列计数器源码总阅读索引.md#1.1_版本边界与阅读任务)进入[模块对象与检查状态](../../../navigation/P02_Linux_6.12_seqcount与seqlock模块源码概念导读.md#2.2_公共对象与检查状态)，操作实现另见[seqlock.h](seqlock.h.md#1.2_源码符号覆盖账本)。

## 1.2\_plain计数与条件检查字段

最小对象只有一个功能计数。业务字段仍由调用者保存，读者起点和候选仍是各自的局部变量；若开启锁依赖检查，才在这个对象上增加检查身份。下面的条件声明把这两类状态直接分开。

```c
/**
 * @brief 仓库阅读说明：保存功能计数和按配置存在的检查映射。
 */
typedef struct seqcount {
	unsigned sequence;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map dep_map;
#endif
} seqcount_t;
```

sequence是功能计数。dep_map只在CONFIG_DEBUG_LOCK_ALLOC配置下存在，用于锁依赖建模，不能把它当作阻止其他写者进入的锁。初始化把sequence清零，调用者仍须在发布前构造业务字段并建立写者串行协议。字段宽度是unsigned，回绕分析按实际目标宽度计算，不能在教材中随意缩窄后仍沿用原工程假设。

## 1.3\_关联指针为何可能参与功能

写侧检查需要找到外部串行锁；RT 读者在适用条件下还要真正等待它。两种用途都需要地址，所以决定是否保留字段的条件不能只检查 Lockdep。先看控制字段保留的宏，再看它如何参与结构生成。

```c
/**
 * @brief 仓库阅读说明：检查或实时配置决定表达式是否保留。
 */
#if defined(CONFIG_LOCKDEP) || defined(CONFIG_PREEMPT_RT)
#define __SEQ_LOCK(expr)	expr
#else
#define __SEQ_LOCK(expr)
#endif
```

这条预处理分支同时服务检查与实时配置。两项配置都关闭时，传入表达式被省略；任一开启则保留。因此RT配置中的关联锁地址不能简单归为“调试信息”。

```c
/**
 * @brief 仓库阅读说明：按锁类型生成关联结构，外部锁由调用者管理。
 */
#define SEQCOUNT_LOCKNAME(lockname, locktype, preemptible, lockbase)	\
typedef struct seqcount_##lockname {					\
	seqcount_t		seqcount;				\
	__SEQ_LOCK(locktype	*lock);					\
} seqcount_##lockname##_t;

SEQCOUNT_LOCKNAME(raw_spinlock, raw_spinlock_t,  false,    raw_spin)
SEQCOUNT_LOCKNAME(spinlock,     spinlock_t,      __SEQ_RT, spin)
SEQCOUNT_LOCKNAME(rwlock,       rwlock_t,        __SEQ_RT, read)
SEQCOUNT_LOCKNAME(mutex,        struct mutex,    true,     mutex)
#undef SEQCOUNT_LOCKNAME
```

宏为每一种锁类型生成带内嵌seqcount和可选lock指针的结构体。raw_spinlock、spinlock、rwlock和mutex四种实例的差别由参数传入；这里宏体只用lockname和locktype生成布局，preemptible/lockbase的功能属性在seqlock.h同名生成宏中使用，不要误认这里在运行时取得锁。

初始化保存的是外部锁地址，外部锁必须比所有可能访问关联指针的路径活得更久。RT读者可沿指针进行一次真正lock/unlock，非RT的Lockdep可用它检查写者是否持锁；实际写者仍先自行取得锁。状态阶段见[关联锁模块](../../../navigation/P02_Linux_6.12_seqcount与seqlock模块源码概念导读.md#2.4_关联锁与PREEMPT_RT)。

## 1.4\_seqlock组合了什么

关联指针仍把实际锁的存储交给调用者。如果希望一个对象同时拥有计数和写者锁，seqlock 就把二者内嵌在一起。初始化还要把前者的关联地址指向后者，光有并排的两个成员不等于关联已经建立。

```c
/**
 * @brief 仓库阅读说明：将关联序号和实际自旋锁组合。
 */
typedef struct {

	seqcount_spinlock_t seqcount;
	spinlock_t lock;
} seqlock_t;
```

seqlock_t把seqcount_spinlock_t与实际spinlock_t放进同一对象；内部关联关系由初始化建立。普通重试读者仍不在整个复制区持这个锁，写者包装取得它，locking reader则直接使用它互斥。区别来自调用路径，不来自同一个对象名。latch类型在seqlock.h中声明，业务双副本仍由调用者提供。

## 1.5\_修改与验证边界

检查新增字段或配置裁剪时，必须同时核对初始化、读者慢路径和外部锁寿命；仅在非RT非Lockdep配置编译通过，不能证明被裁掉的字段在其他配置下仍正确。这里完成固定声明的静态比对，没有运行配置矩阵或目标内核。重新初始化sequence和锁也不能证明旧读者已经离开。
