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

current 已持有配置锁，现在准备以可中断方式取得状态锁。功能路径还没有成为状态锁的 owner，检查账本却必须先记录这段可能等待的区间，否则两个任务可能先卡死，验证器再也没有报告机会。本页核对谁隔离检查器重入、谁提交候选，以及失败或释放撤回什么；读完应能指出深度改变的语句，并区分检查返回值与功能取得结果。

| 入口 | 本文提供的实现证据 |
| --- | --- |
| [Lockdep 总阅读索引](../navigation/P01_Linux_6.12_Lockdep源码导读.md#1.4_一次acquire的主调用链) | acquire/release 主链 |
| [身份与事件接入模块导读](../navigation/P02_Linux_6.12_Lockdep身份与事件接入模块导读.md#2.4_取得与释放调用链) | 状态写入者与失败回退 |
| [稳定机制：持锁账本、依赖图与状态闭环](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P04_持锁账本_依赖图与状态闭环.md#4.1_本章只追踪一个问题) | 当前事实与全局历史分工 |

基线为 Linux 6.12.20，提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。所有 Doxygen 和中文行内注释均为仓库补充，非上游原文。

## 2.2\_task\_struct持锁账本与held\_lock

新取得需要找到 current 尚未结束的前缀，释放又要找回一次取得的身份和旧链键。因此任务保存固定数组，记录同时保留本次实例与全局类身份。下面裁剪两份头文件中的相关字段，不是完整结构定义：可选统计时间戳及其他无关字段没有展开。任务中的 lockdep_recursion 与每 CPU 同名变量是不同存储，不能合并理解。

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

标准锁上报事件也可能触发日志和其他检查，检查器不能因此无限检查自己。本入口先判断全局和当前递归资格，再保存本地 IRQ 状态并增加每 CPU 递归保护。全局图的跨 CPU 同步仍由后续图锁负责，关本地 IRQ 不代替图锁。

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

		if (lockdep_nmi() && !trylock) {
			struct held_lock hlock;

			hlock.acquire_ip = ip;
			hlock.instance = lock;
			hlock.nest_lock = nest_lock;
			hlock.irq_context = 2;
			hlock.trylock = trylock;
			hlock.read = read;
			hlock.check = check;
			hlock.hardirqs_off = true;
			hlock.references = 0;

			verify_lock_unused(lock, &hlock, subclass);
		}
		return;
	}

	/* 仓库补充：隔离本 CPU 普通中断重入，再进入检查器递归保护。 */
	raw_local_irq_save(flags);
	check_flags(flags);

	lockdep_recursion_inc();
	__lock_acquire(lock, subclass, trylock, read, check,
		       irqs_disabled_flags(flags), nest_lock, ip, 0, 0, 0);
	lockdep_recursion_finish();
	raw_local_irq_restore(flags);
}
```

**调用上下文：** 公共入口自己保存本地 IRQ 并增加 per-CPU 递归保护，再调用要求 IRQ 已关闭的内部状态机。阻塞 mutex 路径在等待前上报，以便记录潜在等待依赖；取得失败会用 `mutex_release()` 回退影子记录。

`trace_lock_acquire()` 位于全局停检判断以前，所以存在 trace 不证明本次完成依赖检查。普通入口受抑制时大部分调用直接返回；NMI（不可屏蔽中断）的非 trylock 路径还有特例：用栈上临时 hlock 调用 `verify_lock_unused()` 核对使用事实，不走常规 current 记录提交。不能把该分支说成 NMI 内也执行了一次完整的普通获取检查。

## 2.4\_内部取得函数的状态提交

入口已经隔离重入，本函数现在要解决候选何时可被后续查询看见。它先取得类身份、检查容量并填写数组尾部，依次核对上下文、使用状态和依赖链，最后才增加有效深度。源码还包含合并记录与 sync 注解等不增加新深度的出口，因而保留全部可执行语句，不能只截取最后两条赋值。

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 建立候选held record、验证链并提交current状态。
 *
 * 仓库补充，非上游原文；以下保留该版本完整可执行语句，省去上游英文说明注释。
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

	if (unlikely(!debug_locks))
		return 0;

	if (unlikely(lock->key == &__lockdep_no_track__))
		return 0;

	if (!prove_locking || lock->key == &__lockdep_no_validate__)
		check = 0;

	if (subclass < NR_LOCKDEP_CACHING_CLASSES)
		class = lock->class_cache[subclass];

	if (unlikely(!class)) {
		class = register_lock_class(lock, subclass, 0);
		if (!class)
			return 0;
	}

	debug_class_ops_inc(class);

	if (very_verbose(class)) {
		nbcon_cpu_emergency_enter();
		printk("\nacquire class [%px] %s", class->key, class->name);
		if (class->name_version > 1)
			printk(KERN_CONT "#%d", class->name_version);
		printk(KERN_CONT "\n");
		dump_stack();
		nbcon_cpu_emergency_exit();
	}

	/* 仓库补充：候选放在当前有效区间之后，先不提交深度。 */
	depth = curr->lockdep_depth;

	if (DEBUG_LOCKS_WARN_ON(depth >= MAX_LOCK_DEPTH))
		return 0;

	class_idx = class - lock_classes;

	/* 仓库补充：同类且由 nest_lock 串行化时，可能合并到栈顶引用。 */
	if (depth && !sync) {

		hlock = curr->held_locks + depth - 1;
		if (hlock->class_idx == class_idx && nest_lock) {
			if (!references)
				references++;

			if (!hlock->references)
				hlock->references++;

			hlock->references += references;

			if (DEBUG_LOCKS_WARN_ON(hlock->references < references))
				return 0;

			return 2;
		}
	}

	hlock = curr->held_locks + depth;

	if (DEBUG_LOCKS_WARN_ON(!class))
		return 0;
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
#ifdef CONFIG_LOCK_STAT
	hlock->waittime_stamp = 0;
	hlock->holdtime_stamp = lockstat_clock();
#endif
	hlock->pin_count = pin_count;

	if (check_wait_context(curr, hlock))
		return 0;

	if (!mark_usage(curr, hlock, check))
		return 0;

	if (DEBUG_LOCKS_WARN_ON(!test_bit(class_idx, lock_classes_in_use)))
		return 0;

	chain_key = curr->curr_chain_key;
	if (!depth) {

		if (DEBUG_LOCKS_WARN_ON(chain_key != INITIAL_CHAIN_KEY))
			return 0;
		chain_head = 1;
	}

	hlock->prev_chain_key = chain_key;
	if (separate_irq_context(curr, hlock)) {
		chain_key = INITIAL_CHAIN_KEY;
		chain_head = 1;
	}
	chain_key = iterate_chain_key(chain_key, hlock_id(hlock));

	if (nest_lock && !__lock_is_held(nest_lock, -1)) {
		print_lock_nested_lock_not_held(curr, hlock);
		return 0;
	}

	if (!debug_locks_silent) {
		WARN_ON_ONCE(depth && !hlock_class(hlock - 1)->key);
		WARN_ON_ONCE(!hlock_class(hlock)->key);
	}

	if (!validate_chain(curr, hlock, chain_head, chain_key))
		return 0;

	if (hlock->sync)
		return 1;

	/* 仓库补充：验证通过且不是 sync 事件，才提交当前链与深度。 */
	curr->curr_chain_key = chain_key;
	curr->lockdep_depth++;
	check_chain_key(curr);
#ifdef CONFIG_DEBUG_LOCKDEP
	if (unlikely(!debug_locks))
		return 0;
#endif
	if (unlikely(curr->lockdep_depth >= MAX_LOCK_DEPTH)) {
		debug_locks_off();
		nbcon_cpu_emergency_enter();
		print_lockdep_off("BUG: MAX_LOCK_DEPTH too low!");
		printk(KERN_DEBUG "depth: %i  max: %lu!\n",
		       curr->lockdep_depth, MAX_LOCK_DEPTH);

		lockdep_print_held_locks(current);
		debug_show_all_locks();
		dump_stack();
		nbcon_cpu_emergency_exit();

		return 0;
	}

	if (unlikely(curr->lockdep_depth > max_lockdep_depth))
		max_lockdep_depth = curr->lockdep_depth;

	return 1;
}
```

**状态副作用：** class 可能被首次登记，类使用状态可能增加；新 chain cache 项在未命中时先登记，详细规则通过后才把新依赖边写入图。只有 `validate_chain()` 整体返回成功以后，`lockdep_depth` 才增加。候选区位于数组尾部但未计入有效深度，避免失败路径把半成品暴露给查询。

**配置分支：** `prove_locking=0` 或 novalidate 类把 `check` 清零，保留基础跟踪而跳过完整图验证； notrack 类连 held record 都不建立。二者都不应作为普通告警修复手段。

先看两个正常但不增加深度的出口。当前栈顶同类且提供 `nest_lock` 时，函数可能增加栈顶 `references` 后返回 2，表示合并检查记录；`sync` 事件验证后返回 1，但不形成实际临界区。普通路径才把候选变成 S5 的有效记录。

返回 0 也不统一表示“状态没变”。类登记和 usage 写入可能已发生，合并引用可能在递增后发现溢出；S5 提交后还要核对链键和容量。深度达到 `MAX_LOCK_DEPTH` 时，代码已经写入最后一个数组槽并增加深度，随后停检、报告并返回 0，不会回滚成调用前快照。业务能否进入临界区仍由功能 mutex 的返回值决定。

`separate_irq_context()` 使新上下文从初始链键开始，旧记录仍在任务数组中；`nest_lock` 还必须存在于当前检查记录。LOCK_STAT 时间戳与 verbose 输出服务统计和诊断，不是功能 owner。修改时应分别追踪普通新记录、合并引用、sync、候选失败及提交后容量停检，不能只验证末尾成功返回。

## 2.5\_内部释放函数的链回退

状态锁的功能获取可能因信号失败，也可能成功后走到 unlock；两者都要结束此前上报的等待/持有区间。release 按实例寻找当前记录，而不是从功能 owner 反推。普通栈顶记录可以直接回退；中间记录和合并引用则必须保留其他尚未结束的区间。

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

	if (hlock->instance == lock)
		lock_release_holdtime(hlock);

	WARN(hlock->pin_count, "releasing a pinned lock\n");

	/* 仓库补充：合并记录先减引用，仍有引用时不删除整个槽。 */
	if (hlock->references) {
		hlock->references--;
		if (hlock->references) {

			return 1;
		}
	}

	/* 仓库补充：退回目标以前，再按需重放后续检查记录。 */
	curr->lockdep_depth = i;
	curr->curr_chain_key = hlock->prev_chain_key;

	if (i == depth-1)
		return 1;

	if (reacquire_held_locks(curr, depth, i + 1, &merged))
		return 0;

	DEBUG_LOCKS_WARN_ON(curr->lockdep_depth != depth - merged);

	return 0;
}
```

**实现原理：** `find_held_lock()` 从当前上下文的尾部向前按实例匹配，不能跨 IRQ context。栈顶释放直接恢复 `prev_chain_key`；非栈顶释放先截断，再用原记录属性调用 `__lock_acquire()` 重建后半段。全局锁类图和链缓存不随这次 release 删除。

`references` 非零时先减一，仍非零就返回，整条记录继续有效；只有引用归零或普通记录才进入深度回退。pin 检查在这些出口以前发生。末尾返回 0 不统一表示释放失败：非栈顶重建完成也会到达这里，必须结合分支和调用者理解。重建只重放检查记录，不松开或重新取得后面的功能锁。

## 2.6\_功能路径与检查路径

| 动作 | mutex 功能状态 | Lockdep 影子状态 |
| --- | --- | --- |
| 进入阻塞取得 | 尝试 owner 竞争、必要时等待 | 在等待前上报候选依赖 |
| 取得成功 | current 成为功能 owner | held record 已存在，另可记录 acquired 统计 |
| 可中断取得失败 | 从等待队列退出，未成为 owner | release 注解撤销 S5 已正式提交的当前记录；不是撤销尚未提交的候选 |
| unlock | 清除 owner并唤醒等待者 | release 删除 current 实例记录 |

下一篇：[Lockdep 依赖图与规则引擎源码实现](P03_Linux_6.12_Lockdep依赖图与规则引擎源码实现.md#3.1_关联入口)。
