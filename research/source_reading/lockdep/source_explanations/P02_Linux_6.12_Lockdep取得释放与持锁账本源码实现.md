---
id: research.source_reading.lockdep.linux_6_12_acquire_release_implementation
title: "Linux 6.12 Lockdep 取得释放与持锁账本源码实现"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
topics:
  - locking
  - lockdep
---

# 第2章\_Linux\_6.12\_Lockdep取得释放与持锁账本源码实现

## 2.1\_关联入口

| 入口 | 本文提供的实现证据 |
| --- | --- |
| [Lockdep 总阅读索引](../navigation/P01_Linux_6.12_Lockdep源码导读.md#1.4_一次acquire的主调用链) | acquire/release 主链 |
| [身份与事件接入模块导读](../navigation/P02_Linux_6.12_Lockdep身份与事件接入模块导读.md#2.4_取得与释放调用链) | 状态写入者与失败回退 |
| [稳定机制：持锁账本、依赖图与状态闭环](../../../../knowledge/linux/synchronization/lockdep/P04_持锁账本_依赖图与状态闭环.md#4.1_本章只追踪一个问题) | 当前事实与全局历史分工 |

基线为 Linux 6.12.20，提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。所有 Doxygen 和中文行内注释均为仓库补充，非上游原文。

## 2.2\_task\_struct持锁账本与held\_lock

**上游相对位置：** [`include/linux/sched.h`](../../linux/include/linux/sched.h)、[`include/linux/lockdep_types.h`](../../linux/include/linux/lockdep_types.h)

```c
/* task_struct在CONFIG_LOCKDEP下的字段。 */
u64 curr_chain_key;                       /* 当前未释放锁链的增量哈希。 */
int lockdep_depth;                        /* held_locks[]有效长度。 */
unsigned int lockdep_recursion;           /* 防止检查器递归进入自身。 */
struct held_lock held_locks[MAX_LOCK_DEPTH]; /* 本版本固定深度48。 */

/**
 * @brief current一次尚未释放的Lockdep取得记录。
 *
 * 仓库补充，非上游原文。它不是CPU调用栈，也不是功能锁owner。
 */
struct held_lock {
	u64 prev_chain_key;              /* 释放本记录时恢复的前一链键。 */
	unsigned long acquire_ip;        /* 取得注解位置。 */
	struct lockdep_map *instance;    /* 精确实例查询与释放匹配。 */
	struct lockdep_map *nest_lock;   /* 显式嵌套序列化依据。 */
	unsigned int class_idx:MAX_LOCKDEP_KEYS_BITS;
	unsigned int irq_context:2;
	unsigned int trylock:1;
	unsigned int read:2;             /* 0独占，1非递归读，2递归读。 */
	unsigned int check:1;
	unsigned int hardirqs_off:1;
	unsigned int sync:1;
	unsigned int references:11;
	unsigned int pin_count;
};
```

**实现原理：** `instance` 服务“current 是否持指定对象”的查询，`class_idx` 服务锁类图推理，二者同时保存。`prev_chain_key` 让栈顶释放常数时间回退；非栈顶释放则用其余字段重新取得后半段记录。`read`、IRQ、trylock 等位决定依赖是否具有相同阻塞语义。

## 2.3\_lock\_acquire事件入口

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 接收锁原语上报的取得尝试，并串行进入Lockdep状态机。
 *
 * 仓库补充，非上游原文。此函数名不表示功能锁已经成功，
 * 也不表示硬件acquire内存序。
 */
void lock_acquire(struct lockdep_map *lock, unsigned int subclass,
		  int trylock, int read, int check,
		  struct lockdep_map *nest_lock, unsigned long ip)
{
	unsigned long flags;

	trace_lock_acquire(lock, subclass, trylock, read, check, nest_lock, ip);
	if (!debug_locks)
		return;
	if (unlikely(!lockdep_enabled())) {
		/* NMI特殊检查分支省略。 */
		return;
	}

	raw_local_irq_save(flags); /* 防止本CPU中断重入Lockdep。 */
	check_flags(flags);
	lockdep_recursion_inc();
	__lock_acquire(lock, subclass, trylock, read, check,
		       irqs_disabled_flags(flags), nest_lock, ip, 0, 0, 0);
	lockdep_recursion_finish();
	raw_local_irq_restore(flags);
}
```

**调用上下文：** 公共入口自己保存本地 IRQ 并增加 per-CPU 递归保护，再调用要求 IRQ 已关闭的内部状态机。阻塞 mutex 路径在等待前上报，以便记录潜在等待依赖；取得失败会用 `mutex_release()` 回退影子记录。

## 2.4\_\_\_lock\_acquire取得状态提交

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 建立候选held record、验证链并提交current状态。
 *
 * 仓库补充，非上游原文；以下裁剪保留S1到S5主路径。
 */
static int __lock_acquire(struct lockdep_map *lock, unsigned int subclass,
			  int trylock, int read, int check, int hardirqs_off,
			  struct lockdep_map *nest_lock, unsigned long ip,
			  int references, int pin_count, int sync)
{
	struct task_struct *curr = current;
	struct lock_class *class = NULL;
	struct held_lock *hlock;
	unsigned int depth;
	int chain_head = 0;
	int class_idx;
	u64 chain_key;

	if (unlikely(!debug_locks) ||
	    unlikely(lock->key == &__lockdep_no_track__))
		return 0;
	if (!prove_locking || lock->key == &__lockdep_no_validate__)
		check = 0; /* 仍可跟踪，但不做完整依赖验证。 */

	if (subclass < NR_LOCKDEP_CACHING_CLASSES)
		class = lock->class_cache[subclass];
	if (unlikely(!class)) {
		class = register_lock_class(lock, subclass, 0);
		if (!class)
			return 0;
	}

	depth = curr->lockdep_depth;
	if (DEBUG_LOCKS_WARN_ON(depth >= MAX_LOCK_DEPTH))
		return 0;
	class_idx = class - lock_classes;

	hlock = curr->held_locks + depth; /* 只预留，尚未增加有效深度。 */
	hlock->class_idx = class_idx;
	hlock->acquire_ip = ip;
	hlock->instance = lock;
	hlock->nest_lock = nest_lock;
	hlock->irq_context = task_irq_context(curr);
	hlock->trylock = trylock;
	hlock->read = read;
	hlock->check = check;
	hlock->sync = !!sync;
	hlock->hardirqs_off = !!hardirqs_off;
	hlock->references = references;
	hlock->pin_count = pin_count;

	if (check_wait_context(curr, hlock) ||
	    !mark_usage(curr, hlock, check))
		return 0;

	chain_key = curr->curr_chain_key;
	if (!depth)
		chain_head = 1;
	hlock->prev_chain_key = chain_key;
	if (separate_irq_context(curr, hlock)) {
		chain_key = INITIAL_CHAIN_KEY;
		chain_head = 1;
	}
	chain_key = iterate_chain_key(chain_key, hlock_id(hlock));

	if (!validate_chain(curr, hlock, chain_head, chain_key))
		return 0;
	if (hlock->sync)
		return 1; /* 同步注解不建立真实持有区间。 */

	curr->curr_chain_key = chain_key;
	curr->lockdep_depth++; /* 验证通过后才正式提交候选记录。 */
	return 1;
}
```

**状态副作用：** class 可能被首次登记，类使用状态可能增加；新 chain cache 项在未命中时先登记，详细规则通过后才把新依赖边写入图。只有 `validate_chain()` 整体返回成功以后，`lockdep_depth` 才增加。候选区位于数组尾部但未计入有效深度，避免失败路径把半成品暴露给查询。

**配置分支：** `prove_locking=0` 或 novalidate 类把 `check` 清零，保留基础跟踪而跳过完整图验证； notrack 类连 held record 都不建立。二者都不应作为普通告警修复手段。

## 2.5\_\_\_lock\_release释放与链回退

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 移除current中的指定实例，并恢复剩余锁链状态。
 *
 * 仓库补充，非上游原文。
 */
static int __lock_release(struct lockdep_map *lock, unsigned long ip)
{
	struct task_struct *curr = current;
	unsigned int depth, merged = 1;
	struct held_lock *hlock;
	int i;

	if (unlikely(!debug_locks))
		return 0;
	depth = curr->lockdep_depth;
	if (depth <= 0) {
		print_unlock_imbalance_bug(curr, lock, ip);
		return 0;
	}

	hlock = find_held_lock(curr, lock, depth, &i);
	if (!hlock) {
		print_unlock_imbalance_bug(curr, lock, ip);
		return 0;
	}
	WARN(hlock->pin_count, "releasing a pinned lock\n");

	curr->lockdep_depth = i;
	curr->curr_chain_key = hlock->prev_chain_key; /* 先退回被删记录以前。 */
	if (i == depth - 1)
		return 1; /* 常见栈顶释放已经完成。 */

	if (reacquire_held_locks(curr, depth, i + 1, &merged))
		return 0; /* 非栈顶释放要重建后半段。 */
	DEBUG_LOCKS_WARN_ON(curr->lockdep_depth != depth - merged);
	return 0;
}
```

**实现原理：** `find_held_lock()` 从当前上下文的尾部向前按实例匹配，不能跨 IRQ context。栈顶释放直接恢复 `prev_chain_key`；非栈顶释放先截断，再用原记录属性调用 `__lock_acquire()` 重建后半段。全局锁类图和链缓存不随这次 release 删除。

## 2.6\_功能路径与检查路径

| 动作 | mutex 功能状态 | Lockdep 影子状态 |
| --- | --- | --- |
| 进入阻塞取得 | 尝试 owner 竞争、必要时等待 | 在等待前上报候选依赖 |
| 取得成功 | current 成为功能 owner | held record 已存在，另可记录 acquired 统计 |
| 可中断取得失败 | 从等待队列退出，未成为 owner | release 注解撤销候选记录 |
| unlock | 清除 owner并唤醒等待者 | release 删除 current 实例记录 |

下一篇：[Lockdep 依赖图与规则引擎源码实现](P03_Linux_6.12_Lockdep依赖图与规则引擎源码实现.md#3.1_关联入口)。
