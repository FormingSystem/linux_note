---
id: research.source_reading.lockdep.linux_6_12_queries_configuration_implementation
title: "Linux 6.12 Lockdep 查询注解与配置源码实现"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
topics:
  - locking
  - lockdep
  - debugging
---

# 第4章\_Linux\_6.12\_Lockdep查询注解与配置源码实现

## 4.1\_关联入口

设备辅助函数要求调用者持有 state_lock，回调又要求这段保护连续存在。实现中分别由查询、断言与 pin 消费已有检查记录；如果检查器已经停检，沉默还必须与配置和 proc 状态一起解读。本页追踪这些消费者怎样取数、怎样决定告警，以及它们为何不能代替功能加锁。

| 入口 | 本文提供的实现证据 |
| --- | --- |
| [Lockdep 总阅读索引](../navigation/P01_Linux_6.12_Lockdep源码导读.md#1.6_建议阅读顺序) | 查询、配置和 proc 的阅读位置 |
| [查询适配与诊断模块导读](../navigation/P04_Linux_6.12_Lockdep查询适配与诊断模块导读.md#4.2_查询链) | current 查询到 RCU/诊断的数据流 |
| [稳定机制：查询、断言、pin 与自定义原语接入](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P06_查询_断言_pin与自定义原语接入.md#6.1_先确认自己在哪一层使用Lockdep) | API 语义与误用边界 |
| [稳定机制：配置、亲手实验与报告解读](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P08_配置_亲手实验与报告解读.md#8.1_先把使用资格变成环境检查) | 配置和运行态核对 |
| [稳定机制：成本、覆盖边界与工程选择](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P09_成本_覆盖边界与工程选择.md#9.1_先把无告警写成条件命题) | 容量、停检和无告警结论的边界 |

基线为 Linux 6.12.20，提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。所有 Doxygen 和中文行内注释均为仓库补充，非上游原文。

## 4.2\_lock\_is\_held\_type当前持锁查询

查询要回答的是 current 的某个持有区间是否仍在，而不是锁字是否忙。宏先把业务锁转换成 dep_map，公共函数隔离本地重入，再遍历有效记录；不能可靠查询时返回第三种状态，避免把诊断者不知道误报成调用者未持锁。

**上游相对位置：** [`include/linux/lockdep.h`](../../linux/include/linux/lockdep.h)、[`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
#define LOCK_STATE_UNKNOWN  (-1)
#define LOCK_STATE_NOT_HELD 0
#define LOCK_STATE_HELD     1

static inline int lock_is_held(const struct lockdep_map *lock)
{
	return lock_is_held_type(lock, -1); /* -1表示任意取得类型。 */
}

#define lockdep_is_held(lock) \
	lock_is_held(&(lock)->dep_map)

/**
 * @brief 查询current是否持有指定map，并可限制独占/读类型。
 *
 * 仓库补充，非上游原文。
 */
noinstr int lock_is_held_type(const struct lockdep_map *lock, int read)
{
	unsigned long flags;
	int ret = LOCK_STATE_NOT_HELD;

	if (unlikely(!lockdep_enabled()))
		return LOCK_STATE_UNKNOWN; /* 避免断言在停检时误报未持锁。 */

	raw_local_irq_save(flags);
	check_flags(flags);
	lockdep_recursion_inc();
	ret = __lock_is_held(lock, read);
	lockdep_recursion_finish();
	raw_local_irq_restore(flags);
	return ret;
}

static __always_inline
int __lock_is_held(const struct lockdep_map *lock, int read)
{
	struct task_struct *curr = current;
	int i;

	for (i = 0; i < curr->lockdep_depth; i++) {
		struct held_lock *hlock = curr->held_locks + i;

		if (match_held_lock(hlock, lock)) {
			if (read == -1 || !!hlock->read == read)
				return LOCK_STATE_HELD;
			return LOCK_STATE_NOT_HELD;
		}
	}
	return LOCK_STATE_NOT_HELD;
}
```

**实现原理：** 宏先取得指定锁实例的 `dep_map`，查询函数在关闭本地 IRQ 和增加递归保护后遍历 current 的有效 held records。普通路径按 `hlock->instance == lock` 精确匹配；特殊引用式 nested 记录才可能按 class 辅助匹配。它不读 mutex owner，不扫描其他任务或全局类图。

**状态副作用：** 只读 current 账本；临时修改的是 Lockdep 自身递归保护和本地 IRQ 状态。返回 UNKNOWN 代表检查器无法可靠回答，不代表业务保护已经成立。

## 4.3\_lockdep\_assert系列断言展开

查询返回三态以后，断言必须决定哪些状态足以报告违反契约。普通持锁与不持锁断言只否定明确的相反事实；读写断言还筛选记录的取得类型。下面的宏只表达诊断条件，不创建临界区。

**上游相对位置：** [`include/linux/lockdep.h`](../../linux/include/linux/lockdep.h)

```c
/**
 * @brief 只有检查器可用且条件明确不成立时发出WARN。
 *
 * 仓库补充，非上游原文。
 */
#define lockdep_assert(cond) \
	do { WARN_ON(debug_locks && !(cond)); } while (0)

#define lockdep_assert_held(lock) \
	lockdep_assert(lockdep_is_held(lock) != LOCK_STATE_NOT_HELD)

#define lockdep_assert_not_held(lock) \
	lockdep_assert(lockdep_is_held(lock) != LOCK_STATE_HELD)

#define lockdep_assert_held_write(lock) \
	lockdep_assert(lockdep_is_held_type(lock, 0))

#define lockdep_assert_held_read(lock) \
	lockdep_assert(lockdep_is_held_type(lock, 1))
```

**实现原理：** 普通持锁断言把 UNKNOWN 和 HELD 都视为“不明确违反”，仅 NOT_HELD 报警；不持锁断言只在明确 HELD 时报警。读/写断言直接消费类型查询结果：`write` 要求 `hlock->read == 0`，`read` 要求 `hlock->read != 0`。它们虽没有写出普通持锁断言的比较式，UNKNOWN 的值 -1 在 C 布尔条件中仍为真，因而也不会仅因 UNKNOWN 告警；外层还受 `debug_locks` 门控。没有告警始终不能反推功能保护成立，调用者必须选择与所需保护强度一致的版本。

`CONFIG_LOCKDEP=n` 时这些宏只引用参数或为空操作。注解是测试构建中的可执行契约，不提供功能锁，也不能成为功能分支判断。

## 4.4\_lockdep\_pin\_lock锁保持注解

回调进入与返回时都持锁，仍可能在中间释放并重取。pin 因而给当前记录增加一笔配对计数，使 release 当场检查连续性；cookie 随后交给 unpin 扣回，不是对象引用或功能锁所有权。

**上游相对位置：** [`include/linux/lockdep.h`](../../linux/include/linux/lockdep.h)、[`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 给current中已经持有的指定实例增加pin计数并返回cookie。
 *
 * 仓库补充，非上游原文。
 */
static struct pin_cookie __lock_pin_lock(struct lockdep_map *lock)
{
	struct pin_cookie cookie = NIL_COOKIE;
	struct task_struct *curr = current;
	int i;

	if (unlikely(!debug_locks))
		return cookie;

	for (i = 0; i < curr->lockdep_depth; i++) {
		struct held_lock *hlock = curr->held_locks + i;

		if (match_held_lock(hlock, lock)) {
			cookie.val = 1 + (sched_clock() & 0xffff);
			hlock->pin_count += cookie.val;
			return cookie;
		}
	}
	WARN(1, "pinning an unheld lock\n");
	return cookie;
}
```

**状态副作用：** 只修改已有 held record 的 `pin_count`。`__lock_release()` 释放时若该值非零会告警；unpin 用 cookie 减回。pin 不让功能 unlock 失败，它只发现“上层要求连续持锁，而下层曾中途释放”的协议违例。

头文件的入口转换如下，调用者传入业务锁，检查函数接收其中的 map：

```c
/* 仓库补充：两个宏都只取得检查身份，不操作功能锁字。 */
#define lockdep_pin_lock(l)	lock_pin_lock(&(l)->dep_map)
#define lockdep_unpin_lock(l,c)	lock_unpin_lock(&(l)->dep_map, (c))
```

公共 `lock_pin_lock()` / `lock_unpin_lock()` 像查询入口一样检查 `lockdep_enabled()`，保存本地 IRQ 状态、增加递归保护后调用内部函数，最后恢复。pin 内部从零值 `NIL_COOKIE` 开始；找到匹配记录才用时钟低 16 位加一生成非零值，并累加到 `pin_count`。没有记录则告警，不能因此替调用者建立持锁区间。cookie 可能重复，是配对记账值，不是安全句柄。

与增加相配的是下面的扣回。这里保留固定版本全部可执行语句：

```c
/**
 * @brief 从当前匹配记录扣回本次 cookie，并诊断空计数或异常扣减。
 * @note 仓库补充，非上游原文；不执行功能 unlock。
 */
static void __lock_unpin_lock(struct lockdep_map *lock, struct pin_cookie cookie)
{
	struct task_struct *curr = current;
	int i;

	if (unlikely(!debug_locks))
		return;

	for (i = 0; i < curr->lockdep_depth; i++) {
		struct held_lock *hlock = curr->held_locks + i;

		if (match_held_lock(hlock, lock)) {
			if (WARN(!hlock->pin_count, "unpinning an unpinned lock\n"))
				return;

			hlock->pin_count -= cookie.val;

			if (WARN((int)hlock->pin_count < 0, "pin count corrupted\n"))
				hlock->pin_count = 0;

			return;
		}
	}

	WARN(1, "unpinning an unheld lock\n");
}
```

找不到记录与找到记录但计数为零是两类不同违规。扣减以后若转换为有符号数为负，会告警并把计数清零；这不是对任意错误 cookie 都能检出的数学证明。嵌套 pin 只要正确配对，归还一次只扣回自己的一份。用两个 cookie 预测计数“0 → a → a+b → b → 0”，再把其中一次换成错误值，就能看出为什么它是诊断辅助，而不是强制阻止解锁的机制。

## 4.5\_PROVE\_LOCKING\_DEBUG\_LOCK\_ALLOC与LOCKDEP

代码里有注解，不等于当前构建已经包含全部证明规则。配置选择先决定锁对象是否带 map、事件是否被编译以及完整图验证是否存在，运行态才能在此基础上提供证据。下面节选相关 Kconfig 条目，省略帮助文字和其他不影响本节选择关系的条目，不作为一份独立可构建配置。

**上游相对位置：** [`lib/Kconfig.debug`](../../linux/lib/Kconfig.debug)

```kconfig
# 仓库行内说明，非上游原文。
config PROVE_LOCKING
	bool "Lock debugging: prove locking correctness"
	depends on DEBUG_KERNEL && LOCK_DEBUGGING_SUPPORT
	select LOCKDEP
	select DEBUG_SPINLOCK
	select DEBUG_MUTEXES if !PREEMPT_RT
	select DEBUG_RWSEMS if !PREEMPT_RT
	select DEBUG_LOCK_ALLOC
	select TRACE_IRQFLAGS
	default n

config DEBUG_LOCK_ALLOC
	bool "Lock debugging: detect incorrect freeing of live locks"
	depends on DEBUG_KERNEL && LOCK_DEBUGGING_SUPPORT
	select DEBUG_SPINLOCK
	select DEBUG_MUTEXES if !PREEMPT_RT
	select LOCKDEP

config LOCKDEP
	bool
	depends on DEBUG_KERNEL && LOCK_DEBUGGING_SUPPORT
	select STACKTRACE
	select KALLSYMS
	select KALLSYMS_ALL
```

**实现原理：** `LOCKDEP` 是隐藏基础设施，通常由用户可见的 `PROVE_LOCKING`、`DEBUG_LOCK_ALLOC` 或 `LOCK_STAT` 选择。完整锁依赖闭包还需要 `PROVE_LOCKING` 和 IRQ flags 等分支；看到 `CONFIG_LOCKDEP=y` 不能自动推出所有规则都已启用。

`DEBUG_KERNEL` 与 `LOCK_DEBUGGING_SUPPORT` 是这些选项的构建依赖；`DEBUG_SPINLOCK`、`DEBUG_MUTEXES` 和 `DEBUG_RWSEMS` 为相应原语启用额外调试，带 `if !PREEMPT_RT` 的选择受实时内核配置限制。`TRACE_IRQFLAGS` 提供中断开关跟踪；`STACKTRACE`、`KALLSYMS` 和 `KALLSYMS_ALL` 提供堆栈及符号解析能力，供报告指出调用位置。这些选择各自承担证据收集职责，不表示只要日志能打印符号就已经执行了完整图证明。

**关闭分支：** `include/linux/lockdep.h` 把 acquire/release、map 初始化、断言和 pin 大多展开为空操作；`lockdep_types.h` 让 map/key 变成空结构。关闭只移除检查，不取消标准锁功能和源码中的协议义务。

## 4.6\_lockdep\_proc\_init与proc接口

运行者需要读取已经积累的历史与检查器生命状态。下面的初始化函数按构建能力创建 proc 文件；文件出现只说明对应代码存在，不能证明目标分支已执行。先核对创建条件，再解释读取到的计数。

权限常量 `S_IRUSR` 表示文件拥有者可读，`S_IWUSR` 表示拥有者可写；lock_stat 额外开放写入控制，不能把所有 proc 入口都当成普通只读统计文件。

**上游相对位置：** [`kernel/locking/lockdep_proc.c`](../../linux/kernel/locking/lockdep_proc.c)

```c
/**
 * @brief 根据已编译能力创建Lockdep与lockstat的proc只读/控制入口。
 *
 * 仓库补充，非上游原文。
 */
static int __init lockdep_proc_init(void)
{
	proc_create_seq("lockdep", S_IRUSR, NULL, &lockdep_ops);
#ifdef CONFIG_PROVE_LOCKING
	proc_create_seq("lockdep_chains", S_IRUSR, NULL, &lockdep_chains_ops);
#endif
	proc_create_single("lockdep_stats", S_IRUSR, NULL, lockdep_stats_show);
#ifdef CONFIG_LOCK_STAT
	proc_create("lock_stat", S_IRUSR | S_IWUSR,
		    NULL, &lock_stat_proc_ops);
#endif
	return 0;
}
__initcall(lockdep_proc_init);
```

`lockdep_stats_show()` 输出锁类、直接/间接依赖、链、链元素、堆栈、IRQ 分类、最大深度和 `debug_locks`。这些数字来自全局知识库，可判断容量和检查器生命状态；它们不能证明业务测试已经覆盖所有调用分支。

## 4.7\_容量常量与停检边界

类槽和当前记录都使用有限存储。耗尽后若只丢弃后续事件却继续声称完整验证，就会把缺失证据误写成安全。因此容量边界同时决定检查器何时停检；下面两个常量分别限制全局身份空间和单任务记录深度。

**上游相对位置：** [`include/linux/lockdep_types.h`](../../linux/include/linux/lockdep_types.h)、[`include/linux/sched.h`](../../linux/include/linux/sched.h)、[`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)、[`lib/Kconfig.debug`](../../linux/lib/Kconfig.debug)

```c
/**
 * @brief 给锁类索引和current持锁账本规定本版本的容量边界。
 *
 * 仓库补充，非上游原文；下面合并展示两个上游头文件中的定义。
 */
/* include/linux/lockdep_types.h：锁类索引占13位。 */
#define MAX_LOCKDEP_KEYS_BITS 13
#define MAX_LOCKDEP_KEYS      (1UL << MAX_LOCKDEP_KEYS_BITS)

/* include/linux/sched.h：每个任务的当前持锁账本深度。 */
#define MAX_LOCK_DEPTH 48UL
struct held_lock held_locks[MAX_LOCK_DEPTH];
```

`MAX_LOCKDEP_KEYS` 给锁类数组和 `class_idx` 提供同一索引空间；`MAX_LOCK_DEPTH` 则限制单个任务可记录的当前嵌套深度。依赖边、链、链元素和堆栈条目的容量由 `lockdep_internals.h` 与 `CONFIG_LOCKDEP_*_BITS` 继续约束。它们属于不同状态池，不能把一个 `[max: ...]` 当作全部容量。

**实现原理：** 锁类耗尽路径会在图锁保护下调用关闭检查的辅助函数，打印 `MAX_LOCKDEP_KEYS too low` 后返回失败；持锁深度越界同样通过 `DEBUG_LOCKS_WARN_ON()` 进入诊断。这样做避免在部分状态已写、部分状态未写时继续给出看似完整的图证明。容量失败后的无告警不再代表后续路径通过。

**可观察证据：** `/proc/lockdep_stats` 同时给出当前使用量、对应最大值和 `debug_locks`。判断检查器仍有效时必须三者一起看：配置让接口存在、容量没有耗尽、`debug_locks` 仍为 `1`。

## 4.8\_与RCU实现的唯一分工

本篇只展开通用 `lock_is_held_type()`。RCU 自己的四个虚拟 maps、wait type、事件配对、`rcu_read_lock_held()` 和 callback 消费由 [Linux 6.12 RCU Lockdep适配层源码实现](../../rcu/source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.1_实现所有权与读者目标)唯一维护；`RCU_LOCKDEP_WARN()` 宏体仍由 [RCU 公共接口与检查机制源码详解](../../rcu/source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.6_RCU_LOCKDEP_WARN检查适配层)维护。本专题解释 Lockdep 状态怎样产生，RCU 专题解释 RCU 怎样接入和消费，双方不复制同一源码实现。

返回：[Linux 6.12 Lockdep 源码导读](../navigation/P01_Linux_6.12_Lockdep源码导读.md#1.1_基线与阅读目标)。
