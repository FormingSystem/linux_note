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

# 第8章\_Linux\_6.12\_Lockdep查询注解与配置源码实现

## 8.1\_关联入口

| 入口 | 本文提供的实现证据 |
| --- | --- |
| [Lockdep 总阅读索引](../navigation/P01_Linux_6.12_Lockdep源码导读.md#1.6_建议阅读顺序) | 查询、配置和 proc 的阅读位置 |
| [查询适配与诊断模块导读](../navigation/P04_Linux_6.12_Lockdep查询适配与诊断模块导读.md#4.2_查询链) | current 查询到 RCU/诊断的数据流 |
| [稳定机制：查询、断言、pin 与自定义原语接入](../../../../knowledge/linux/synchronization/lockdep/P06_查询_断言_pin与自定义原语接入.md#6.1_三类接口不能互相替代) | API 语义与误用边界 |
| [稳定机制：配置、报告解读与验证方法](../../../../knowledge/linux/synchronization/lockdep/P08_配置_报告解读与验证方法.md#8.1_先确认检查能力确实存在) | 配置和运行态核对 |
| [稳定机制：成本、覆盖边界与工程选择](../../../../knowledge/linux/synchronization/lockdep/P09_成本_覆盖边界与工程选择.md#9.1_链缓存消除了什么成本) | 容量、停检和无告警结论的边界 |

基线为 Linux 6.12.20，提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。所有 Doxygen 和中文行内注释均为仓库补充，非上游原文。

## 8.2\_lock\_is\_held\_type当前持锁查询

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

static int __lock_is_held(const struct lockdep_map *lock, int read)
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

## 8.3\_lockdep\_assert\_held断言展开

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

**实现原理：** 普通持锁断言把 UNKNOWN 和 HELD 都视为“不明确违反”，仅 NOT_HELD 报警；不持锁断言只在明确 HELD 时报警。读/写断言直接消费类型查询结果：`write` 要求 `hlock->read == 0`，`read` 只要求 `hlock->read != 0`；这两个宏不使用 UNKNOWN 容错形式，只应在检查器可用的诊断语义下解读。调用者必须选择与所需保护强度一致的版本。

`CONFIG_LOCKDEP=n` 时这些宏只引用参数或为空操作。注解是测试构建中的可执行契约，不提供功能锁，也不能成为功能分支判断。

## 8.4\_lockdep\_pin\_lock锁保持注解

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

## 8.5\_PROVE\_LOCKING\_DEBUG\_LOCK\_ALLOC与LOCKDEP

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

**关闭分支：** `include/linux/lockdep.h` 把 acquire/release、map 初始化、断言和 pin 大多展开为空操作；`lockdep_types.h` 让 map/key 变成空结构。关闭只移除检查，不取消标准锁功能和源码中的协议义务。

## 8.6\_lockdep\_proc\_init与proc接口

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

## 8.7\_容量常量与停检边界

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

## 8.8\_与RCU实现的唯一分工

本篇只展开通用 `lock_is_held_type()`。RCU 自己的虚拟 maps、`rcu_read_lock_held()` 和 `RCU_LOCKDEP_WARN()` 继续由 [Linux 6.12 RCU 公共接口与检查机制源码详解](../../rcu/source_explanations/P05_Linux_6.12_RCU_公共接口与检查机制源码详解.md#5.6_RCU_Lockdep状态来源)唯一维护。本专题解释 Lockdep 状态怎样产生，RCU 专题解释 RCU 怎样接入和消费，双方不复制同一源码实现。

返回：[Linux 6.12 Lockdep 源码导读](../navigation/P01_Linux_6.12_Lockdep源码导读.md#1.1_基线与阅读目标)。
