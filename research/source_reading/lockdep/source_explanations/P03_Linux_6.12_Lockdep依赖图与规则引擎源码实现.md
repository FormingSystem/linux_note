---
id: research.source_reading.lockdep.linux_6_12_graph_rules_implementation
title: "Linux 6.12 Lockdep 依赖图与规则引擎源码实现"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
topics:
  - locking
  - lockdep
  - interrupt
---

# 第3章\_Linux\_6.12\_Lockdep依赖图与规则引擎源码实现

## 3.1\_关联入口

取得路径已经组织好候选，但尚未增加 current 的有效深度。现在要判断历史中的 config→state 会不会与本次反向候选闭合，以及 IRQ 事实是否让没有普通环的路径也变成不可恢复等待。本页沿使用位更新、历史搜索和缓存门控核对实现；返回成功、登记缓存与写入依赖边是不同事件。

| 入口 | 本文提供的实现证据 |
| --- | --- |
| [Lockdep 总阅读索引](../navigation/P01_Linux_6.12_Lockdep源码导读.md#1.4_一次acquire的主调用链) | 完整规则链位置 |
| [依赖图与规则引擎模块导读](../navigation/P03_Linux_6.12_Lockdep依赖图与规则引擎模块导读.md#3.2_规则链而不是一个环检测函数) | 模块职责和状态传播 |
| [稳定机制：递归、依赖环、IRQ 与读写规则](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P05_递归_依赖环_IRQ与读写规则.md#5.1_先明确闭环搜索要回答什么) | 抽象阻塞规则与误修边界 |

基线为 Linux 6.12.20，提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。所有 Doxygen 和中文行内注释均为仓库补充，非上游原文。

## 3.2\_check\_deadlock同类递归检查

当前已有同类记录时，验证器首先判断是否会自己阻塞自己。它读取 current 数组，不必先向全局图写自环；只有递归读、实例比较关系或显式外层锁提供合法协议时，才能沿相应分支继续。

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 在current账本中检查新取得是否重复已有锁类。
 *
 * 仓库补充，非上游原文。
 * @return 0检测到递归死锁；1正常；2由nest_lock序列化同类嵌套。
 */
static int check_deadlock(struct task_struct *curr, struct held_lock *next)
{
	struct lock_class *class;
	struct held_lock *prev;
	struct held_lock *nest = NULL;
	int i;

	for (i = 0; i < curr->lockdep_depth; i++) {
		prev = curr->held_locks + i;

		if (prev->instance == next->nest_lock)
			nest = prev;

		if (hlock_class(prev) != hlock_class(next))
			continue;

		/* 仓库补充：允许的递归读不按普通独占递归判断。 */
	if ((next->read == 2) && prev->read)
			continue;

		class = hlock_class(prev);

		if (class->cmp_fn &&
		    class->cmp_fn(prev->instance, next->instance) < 0)
			continue;

		if (nest)
			return 2;

		print_deadlock_bug(curr, prev, next);
		return 0;
	}
	return 1;
}
```

**实现原理：** 同类递归不依赖一条已经写入图的 `class → class` 边，必须扫描 current held records 单独判断。读类型值 2 表示允许同实例递归的共享读；`nest_lock` 或 class comparator 是显式协议，不是自动推导。

## 3.3\_mark\_usage锁类上下文状态

一个锁类可能先在进程路径出现，稍后才在硬中断出现。第二个事件需要与第一次的事实组合，因此上下文不能只存在于 current 记录，必须累积到类的 usage 位。下面同时保留 hardirq 与 softirq 分支：所在上下文和是否开启中断是两组事实，trylock 与 sync 又会改变适用的标记。

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 把本次取得的IRQ上下文与开关状态写入锁类usage_mask。
 *
 * 仓库补充，非上游原文。完整保留hardirq/softirq及通用使用标记分支。
 */
static int mark_usage(struct task_struct *curr, struct held_lock *hlock, int check)
{
	if (!check)
		goto lock_used;

	/* 仓库补充：阻塞取得记录所在 IRQ 上下文；trylock 不走此分支。 */
	if (!hlock->trylock) {
		if (hlock->read) {
			if (lockdep_hardirq_context())
				if (!mark_lock(curr, hlock,
						LOCK_USED_IN_HARDIRQ_READ))
					return 0;
			if (curr->softirq_context)
				if (!mark_lock(curr, hlock,
						LOCK_USED_IN_SOFTIRQ_READ))
					return 0;
		} else {
			if (lockdep_hardirq_context())
				if (!mark_lock(curr, hlock, LOCK_USED_IN_HARDIRQ))
					return 0;
			if (curr->softirq_context)
				if (!mark_lock(curr, hlock, LOCK_USED_IN_SOFTIRQ))
					return 0;
		}
	}

	/* 仓库补充：可被中断的持有区间另记 enabled 使用事实。 */
	if (!hlock->hardirqs_off && !hlock->sync) {
		if (hlock->read) {
			if (!mark_lock(curr, hlock,
					LOCK_ENABLED_HARDIRQ_READ))
				return 0;
			if (curr->softirqs_enabled)
				if (!mark_lock(curr, hlock,
						LOCK_ENABLED_SOFTIRQ_READ))
					return 0;
		} else {
			if (!mark_lock(curr, hlock,
					LOCK_ENABLED_HARDIRQ))
				return 0;
			if (curr->softirqs_enabled)
				if (!mark_lock(curr, hlock,
						LOCK_ENABLED_SOFTIRQ))
					return 0;
		}
	}

lock_used:

	if (!mark_lock(curr, hlock, LOCK_USED))
		return 0;

	return 1;
}
```

**状态副作用：** `mark_lock()` 在 `graph_lock` 下设置 `lock_class.usage_mask`，为第一次使用状态保存 trace，并立即检查新状态与既有状态/依赖图是否冲突。已存在的位直接返回，避免重复写全局缓存行。

**语义边界：** safe 表示曾在 IRQ 上下文取得，unsafe 表示曾在相应 IRQ 开启时取得；它们是观察到的类使用事实，不是锁类型声明。trylock 和 `lock_sync()` 有不同标记规则，因为其等待/临界区语义不同。

代码中的 `LOCK_USED_IN_*` 记录在指定中断上下文使用，`LOCK_ENABLED_*` 记录在相应中断开启条件下使用，后缀 `_READ` 区分读取得。末尾 `LOCK_USED` 是通用“曾使用”标记，即使 `check=0` 跳过完整上下文规则，也会到达该标记路径。每一次 `mark_lock()` 都可能因新事实冲突而返回失败，所以不能只保留最后一个置位后声称流程等价。

## 3.4\_check\_prev\_add新依赖验证

前驱 prev 和新后继 next 已经确定。函数必须先排除闭环和 IRQ 冲突，再判断本次应更新已有直接边、复用已有路径，还是分配新边。无条件追加会重复消耗固定池，也无法准确合并同一对类后来出现的阻塞类型。

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 验证并按需提交一条prev到next的锁类依赖。
 *
 * 仓库补充，非上游原文。调用时graph_lock由链缓存未命中路径持有。
 */
static int check_prev_add(struct task_struct *curr, struct held_lock *prev,
	       struct held_lock *next, u16 distance,
	       struct lock_trace **const trace)
{
	struct lock_list *entry;
	enum bfs_result ret;

	if (!hlock_class(prev)->key || !hlock_class(next)->key) {

		WARN_ONCE(!debug_locks_silent && !hlock_class(prev)->key,
			  "Detected use-after-free of lock class %px/%s\n",
			  hlock_class(prev),
			  hlock_class(prev)->name);
		WARN_ONCE(!debug_locks_silent && !hlock_class(next)->key,
			  "Detected use-after-free of lock class %px/%s\n",
			  hlock_class(next),
			  hlock_class(next)->name);
		return 2;
	}

	if (prev->class_idx == next->class_idx) {
		struct lock_class *class = hlock_class(prev);

		if (class->cmp_fn &&
		    class->cmp_fn(prev->instance, next->instance) < 0)
			return 2;
	}

	/* 仓库补充：从后继沿历史寻找前驱，候选边尚未提交。 */
	ret = check_noncircular(next, prev, trace);
	if (unlikely(bfs_error(ret) || ret == BFS_RMATCH))
		return 0;

	if (!check_irq_usage(curr, prev, next))
		return 0;

	list_for_each_entry(entry, &hlock_class(prev)->locks_after, entry) {
		if (entry->class == hlock_class(next)) {
			if (distance == 1)
				entry->distance = 1;
			entry->dep |= calc_dep(prev, next);

			list_for_each_entry(entry, &hlock_class(next)->locks_before, entry) {
				if (entry->class == hlock_class(prev)) {
					if (distance == 1)
						entry->distance = 1;
					entry->dep |= calc_depb(prev, next);
					return 1;
				}
			}

			return 0;
		}
	}

	ret = check_redundant(prev, next);
	if (bfs_error(ret))
		return 0;
	else if (ret == BFS_RMATCH)
		return 2;

	if (!*trace) {
		*trace = save_trace();
		if (!*trace)
			return 0;
	}

	ret = add_lock_to_list(hlock_class(next), hlock_class(prev),
			       &hlock_class(prev)->locks_after, distance,
			       calc_dep(prev, next), *trace);

	if (!ret)
		return 0;

	ret = add_lock_to_list(hlock_class(prev), hlock_class(next),
			       &hlock_class(next)->locks_before, distance,
			       calc_depb(prev, next), *trace);
	if (!ret)
		return 0;

	return 2;
}
```

**执行前后：** 执行前候选边只存在于 current 的取得关系中；环和 IRQ 规则通过以后，函数才向 prev 的 `locks_after` 与 next 的 `locks_before` 各写一条互为反向索引的边。保存的 trace 用于以后另一条路径闭环时解释本边来源。

**搜索边界：** `check_noncircular()` 使用受固定 circular queue 容量限制的图搜索；BFS 内部错误同样使本次验证失败，不能把“搜索队列满”解释成无环。

`BFS_RMATCH` 表示找到符合本次搜索条件的目标，`BFS_RNOMATCH` 表示没有找到，`bfs_error()` 则识别搜索内部失败。是否“找到”是好事由搜索问题决定：环检查找到返回前驱的路径是违规，冗余检查找到已有替代路径则可能省去新增边。不能把同一个枚举统一翻译成“成功所以安全”。

已有直接边时，`distance==1` 把两侧距离更新为直接关系，`dep` 合入本次取得类型；找到正向边却缺少对应反向边时返回 0。没有直接边时，冗余路径检查可能使本次无需分配，否则保存 trace 并分别加入正向和反向索引。第二次分配失败不会自动撤回第一次写入，这类错误必须结合下层停检路径理解，不能宣称是事务式提交。

返回 1 表示更新已有直接关系；返回 2 包含新边成功、已有冗余路径及实例比较允许等出口。入口发现类 key 失效时也会诊断并返回 2，所以该数值不能独立充当“业务锁有效”的证书。读调用者时还要看前驱扫描是否继续、检查器生命状态和实际副作用。

## 3.5\_check\_irq\_usageIRQ依赖传播检查

只比较 prev 和 next 的使用位会漏掉更远的参与者。若历史有 H→prev 和 next→U，新边可能把 hardirq-safe 的 H 连到 hardirq-unsafe 的 U。实现先汇聚前驱反向子图的使用位，再到后继正向子图寻找冲突，最后定位具体两端以打印完整反例。

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 检查新边是否把IRQ-safe反向子图连接到IRQ-unsafe正向子图。
 *
 * 仓库补充，非上游原文；保留四阶段主线。
 */
static int check_irq_usage(struct task_struct *curr, struct held_lock *prev,
			   struct held_lock *next)
{
	unsigned long usage_mask = 0, forward_mask, backward_mask;
	enum lock_usage_bit forward_bit = 0, backward_bit = 0;
	struct lock_list *target_entry1;
	struct lock_list *target_entry;
	struct lock_list this, that;
	enum bfs_result ret;

	/* 仓库补充：先汇聚前驱的反向子图使用位，再寻找后继的正向冲突。 */
	bfs_init_rootb(&this, prev);

	ret = __bfs_backwards(&this, &usage_mask, usage_accumulate, usage_skip, NULL);
	if (bfs_error(ret)) {
		print_bfs_bug(ret);
		return 0;
	}

	usage_mask &= LOCKF_USED_IN_IRQ_ALL;
	if (!usage_mask)
		return 1;

	forward_mask = exclusive_mask(usage_mask);

	bfs_init_root(&that, next);

	ret = find_usage_forwards(&that, forward_mask, &target_entry1);
	if (bfs_error(ret)) {
		print_bfs_bug(ret);
		return 0;
	}
	if (ret == BFS_RNOMATCH)
		return 1;

	backward_mask = original_mask(target_entry1->class->usage_mask & LOCKF_ENABLED_IRQ_ALL);

	ret = find_usage_backwards(&this, backward_mask, &target_entry);
	if (bfs_error(ret)) {
		print_bfs_bug(ret);
		return 0;
	}
	if (DEBUG_LOCKS_WARN_ON(ret == BFS_RNOMATCH))
		return 1;

	ret = find_exclusive_match(target_entry->class->usage_mask,
				   target_entry1->class->usage_mask,
				   &backward_bit, &forward_bit);
	if (DEBUG_LOCKS_WARN_ON(ret == -1))
		return 1;

	print_bad_irq_dependency(curr, &this, &that,
				 target_entry, target_entry1,
				 prev, next,
				 backward_bit, forward_bit,
				 state_name(backward_bit));

	return 0;
}
```

**实现原理：** 先向后汇聚所有可能位于 prev 以前的 IRQ-safe 使用，再向前寻找 next 以后与之排斥的 IRQ-enabled 使用；命中后反查一对具体状态以生成可解释报告。它检查的是整条图连接，不限于候选边两个端点。

## 3.6\_validate\_chain链缓存门控

同样的 current 类序列可能高频重复。此处用链缓存决定是否再次执行上述规则：首次新链的返回路径仍持有图锁，把详细验证责任交给本函数；已有项与登记失败都可能使缓存辅助函数返回 0，必须继续结合生命状态区分。

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 只对首次出现的可阻塞检查链执行完整图验证。
 *
 * 仓库补充，非上游原文。
 */
static int validate_chain(struct task_struct *curr,
			  struct held_lock *hlock,
			  int chain_head, u64 chain_key)
{

	/* 仓库补充：首次新链登记缓存后保持图锁，规则验证继续在锁内进行。 */
	if (!hlock->trylock && hlock->check &&
	    lookup_chain_cache_add(curr, hlock, chain_key)) {

		int ret = check_deadlock(curr, hlock);

		if (!ret)
			return 0;

		if (!chain_head && ret != 2) {
			if (!check_prevs_add(curr, hlock))
				return 0;
		}

		graph_unlock();
	} else {

		if (unlikely(!debug_locks))
			return 0;
	}

	return 1;
}
```

**状态副作用：** `lookup_chain_cache_add()` 未命中时在 `graph_lock` 下先添加链缓存，再让调用者继续做 `check_deadlock()` 和依赖图验证；命中时返回 0，跳过重复图搜索。新 cache 项的写入早于详细规则完成，若随后发现违规，报告路径会关闭 `debug_locks`，不能把 cache 项单独当作成功证明。trylock 和 `check=0` 也跳过完整依赖增加，但外层 `__lock_acquire()` 仍可提交 held record。

**故障边界：** `else` 分支里的检查只在没有进入首次新链验证分支时读取 `debug_locks`，覆盖缓存命中、trylock、`check=0` 或缓存登记未成功等情形；它不是对所有分支统一执行的函数末尾检查。新链分支由各检查返回值和对应错误出口控制失败。不能把任何早退都解释为“链已经安全”。

## 3.7\_证据闭环

| 稳定结论 | 6.12.20 源码落点 | 可观察证据 |
| --- | --- | --- |
| 同类递归需独立检查 | `check_deadlock()` 扫描 current held records | 递归告警列出 prev/next 取得点 |
| 新边不能闭合旧路径 | `check_prev_add()` 调 `check_noncircular(next, prev)` | circular dependency 报告包含反向历史链 |
| IRQ状态是类历史 | `mark_usage()` → `lock_class.usage_mask` | inconsistent state / irq inversion 报告 |
| 重复链不重复图搜索 | `lookup_chain_cache_add()` | `CONFIG_DEBUG_LOCKDEP` 统计 hit/miss |
| 验证通过才写双向边 | 两次 `add_lock_to_list()` | `/proc/lockdep` 与 stats 依赖计数 |

下一篇：[Lockdep 查询注解与配置源码实现](P04_Linux_6.12_Lockdep查询注解与配置源码实现.md#4.1_关联入口)。
