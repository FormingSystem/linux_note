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

# 第7章\_Linux\_6.12\_Lockdep依赖图与规则引擎源码实现

## 7.1\_关联入口

| 入口 | 本文提供的实现证据 |
| --- | --- |
| [Lockdep 总阅读索引](../navigation/P01_Linux_6.12_Lockdep源码导读.md#1.4_一次acquire的主调用链) | 完整规则链位置 |
| [依赖图与规则引擎模块导读](../navigation/P03_Linux_6.12_Lockdep依赖图与规则引擎模块导读.md#3.2_规则链而不是一个环检测函数) | 模块职责和状态传播 |
| [稳定机制：递归、依赖环、IRQ 与读写规则](../../../../knowledge/linux/synchronization/lockdep/P05_递归_依赖环_IRQ与读写规则.md#5.1_先检查同类递归) | 抽象阻塞规则与误修边界 |

基线为 Linux 6.12.20，提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。所有 Doxygen 和中文行内注释均为仓库补充，非上游原文。

## 7.2\_check\_deadlock同类递归检查

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
	struct held_lock *prev;
	struct held_lock *nest = NULL;
	int i;

	for (i = 0; i < curr->lockdep_depth; i++) {
		prev = curr->held_locks + i;
		if (prev->instance == next->nest_lock)
			nest = prev;
		if (hlock_class(prev) != hlock_class(next))
			continue;

		if ((next->read == 2) && prev->read)
			continue; /* 递归读可嵌套在已有读取得中。 */
		if (hlock_class(prev)->cmp_fn &&
		    hlock_class(prev)->cmp_fn(prev->instance, next->instance) < 0)
			continue; /* 类提供的实例自然顺序允许该方向。 */
		if (nest)
			return 2;

		print_deadlock_bug(curr, prev, next);
		return 0;
	}
	return 1;
}
```

**实现原理：** 同类递归不依赖一条已经写入图的 `class → class` 边，必须扫描 current held records 单独判断。读类型值 2 表示允许同实例递归的共享读；`nest_lock` 或 class comparator 是显式协议，不是自动推导。

## 7.3\_mark\_usage锁类上下文状态

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 把本次取得的IRQ上下文与开关状态写入锁类usage_mask。
 *
 * 仓库补充，非上游原文。只展示主要hardirq/softirq分支。
 */
static int mark_usage(struct task_struct *curr,
		      struct held_lock *hlock, int check)
{
	if (!check)
		goto lock_used;

	if (!hlock->trylock) {
		if (hlock->read) {
			if (lockdep_hardirq_context() &&
			    !mark_lock(curr, hlock, LOCK_USED_IN_HARDIRQ_READ))
				return 0;
		} else if (lockdep_hardirq_context() &&
			   !mark_lock(curr, hlock, LOCK_USED_IN_HARDIRQ)) {
			return 0;
		}
		/* softirq对应分支省略。 */
	}

	if (!hlock->hardirqs_off && !hlock->sync) {
		if (hlock->read) {
			if (!mark_lock(curr, hlock, LOCK_ENABLED_HARDIRQ_READ))
				return 0;
		} else if (!mark_lock(curr, hlock, LOCK_ENABLED_HARDIRQ)) {
			return 0;
		}
		/* softirq开启状态对应分支省略。 */
	}

lock_used:
	return mark_lock(curr, hlock, LOCK_USED);
}
```

**状态副作用：** `mark_lock()` 在 `graph_lock` 下设置 `lock_class.usage_mask`，为第一次使用状态保存 trace，并立即检查新状态与既有状态/依赖图是否冲突。已存在的位直接返回，避免重复写全局缓存行。

**语义边界：** safe 表示曾在 IRQ 上下文取得，unsafe 表示曾在相应 IRQ 开启时取得；它们是观察到的类使用事实，不是锁类型声明。trylock 和 `lock_sync()` 有不同标记规则，因为其等待/临界区语义不同。

## 7.4\_check\_prev\_add新依赖验证

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 验证并按需提交一条prev到next的锁类依赖。
 *
 * 仓库补充，非上游原文。调用时graph_lock由链缓存未命中路径持有。
 */
static int check_prev_add(struct task_struct *curr,
			  struct held_lock *prev,
			  struct held_lock *next,
			  u16 distance,
			  struct lock_trace **const trace)
{
	enum bfs_result ret;

	if (!hlock_class(prev)->key || !hlock_class(next)->key)
		return 2; /* 锁类生命期异常的诊断代码省略。 */

	/* 从next沿前向图搜索prev；能到达就会被候选prev→next闭环。 */
	ret = check_noncircular(next, prev, trace);
	if (unlikely(bfs_error(ret) || ret == BFS_RMATCH))
		return 0;

	if (!check_irq_usage(curr, prev, next))
		return 0;

	/* 已有直接边更新距离和依赖类型；冗余间接边可不再添加。 */
	/* ... 已有边和check_redundant()分支省略 ... */

	if (!*trace) {
		*trace = save_trace();
		if (!*trace)
			return 0;
	}

	ret = add_lock_to_list(hlock_class(next), hlock_class(prev),
			       &hlock_class(prev)->locks_after,
			       distance, calc_dep(prev, next), *trace);
	if (!ret)
		return 0;

	ret = add_lock_to_list(hlock_class(prev), hlock_class(next),
			       &hlock_class(next)->locks_before,
			       distance, calc_depb(prev, next), *trace);
	return ret ? 2 : 0;
}
```

**执行前后：** 执行前候选边只存在于 current 的取得关系中；环和 IRQ 规则通过以后，函数才向 prev 的 `locks_after` 与 next 的 `locks_before` 各写一条互为反向索引的边。保存的 trace 用于以后另一条路径闭环时解释本边来源。

**搜索边界：** `check_noncircular()` 使用受固定 circular queue 容量限制的图搜索；BFS 内部错误同样使本次验证失败，不能把“搜索队列满”解释成无环。

## 7.5\_check\_irq\_usageIRQ依赖传播检查

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 检查新边是否把IRQ-safe反向子图连接到IRQ-unsafe正向子图。
 *
 * 仓库补充，非上游原文；保留四阶段主线。
 */
static int check_irq_usage(struct task_struct *curr,
			   struct held_lock *prev,
			   struct held_lock *next)
{
	unsigned long usage_mask = 0, forward_mask, backward_mask;
	enum lock_usage_bit forward_bit = 0, backward_bit = 0;
	struct lock_list *target_entry1;
	struct lock_list *target_entry;
	struct lock_list this, that;
	enum bfs_result ret;

	bfs_init_rootb(&this, prev);
	ret = __bfs_backwards(&this, &usage_mask,
			      usage_accumulate, usage_skip, NULL);
	if (bfs_error(ret))
		return 0;
	usage_mask &= LOCKF_USED_IN_IRQ_ALL; /* 收集prev反向子图中的safe事实。 */
	if (!usage_mask)
		return 1;

	forward_mask = exclusive_mask(usage_mask);
	bfs_init_root(&that, next);
	ret = find_usage_forwards(&that, forward_mask, &target_entry1);
	if (bfs_error(ret))
		return 0;
	if (ret == BFS_RNOMATCH)
		return 1; /* next正向子图没有对应unsafe使用。 */

	backward_mask = original_mask(
		target_entry1->class->usage_mask & LOCKF_ENABLED_IRQ_ALL);
	ret = find_usage_backwards(&this, backward_mask, &target_entry);
	if (bfs_error(ret)) {
		print_bfs_bug(ret); /* 搜索内部失败会停止证明。 */
		return 0;
	}
	if (DEBUG_LOCKS_WARN_ON(ret == BFS_RNOMATCH))
		return 1;

	/* 缩小到一对不兼容使用位并打印两端依赖路径。 */
	ret = find_exclusive_match(target_entry->class->usage_mask,
				   target_entry1->class->usage_mask,
				   &backward_bit, &forward_bit);
	if (DEBUG_LOCKS_WARN_ON(ret == -1))
		return 1;

	print_bad_irq_dependency(curr, &this, &that,
				 target_entry, target_entry1,
				 prev, next, backward_bit, forward_bit,
				 state_name(backward_bit));
	return 0;
}
```

**实现原理：** 先向后汇聚所有可能位于 prev 以前的 IRQ-safe 使用，再向前寻找 next 以后与之排斥的 IRQ-enabled 使用；命中后反查一对具体状态以生成可解释报告。它检查的是整条图连接，不限于候选边两个端点。

## 7.6\_validate\_chain链缓存门控

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
	} else if (unlikely(!debug_locks)) {
		return 0;
	}
	return 1;
}
```

**状态副作用：** `lookup_chain_cache_add()` 未命中时在 `graph_lock` 下添加链缓存并让调用者继续验证；命中时返回 0，跳过重复图搜索。trylock 和 `check=0` 也跳过完整依赖增加，但外层 `__lock_acquire()` 仍可提交 held record。

**故障边界：** 函数最后再次检查 `debug_locks`，因为链缓存或验证途中可能发现内部错误并关闭检查器。不能把任何早退都解释为“链已经安全”。

## 7.7\_证据闭环

| 稳定结论 | 6.12.20 源码落点 | 可观察证据 |
| --- | --- | --- |
| 同类递归需独立检查 | `check_deadlock()` 扫描 current held records | 递归告警列出 prev/next 取得点 |
| 新边不能闭合旧路径 | `check_prev_add()` 调 `check_noncircular(next, prev)` | circular dependency 报告包含反向历史链 |
| IRQ状态是类历史 | `mark_usage()` → `lock_class.usage_mask` | inconsistent state / irq inversion 报告 |
| 重复链不重复图搜索 | `lookup_chain_cache_add()` | `CONFIG_DEBUG_LOCKDEP` 统计 hit/miss |
| 验证通过才写双向边 | 两次 `add_lock_to_list()` | `/proc/lockdep` 与 stats 依赖计数 |

下一篇：[Lockdep 查询注解与配置源码实现](P08_Linux_6.12_Lockdep查询注解与配置源码实现.md#8.1_关联入口)。
