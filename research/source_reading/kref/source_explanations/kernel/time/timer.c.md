---
id: research.kref.impl.timer_c
title: "timer.c重启与同步退出实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_timer.c重启与同步退出实现

固定 NXP 提交 dfaf2136deb2af2e60b994421281ba42f1c087e0 的 kernel/time/timer.c，blob 7835f9b376e76a010926c3c2036c9a458b1f553c。这里只展开 kref 退出组合需要的改期、运行检测和同步删除/关闭链，不承担完整定时轮教学。先读[定时器退出模块](../../../navigation/P05_定时器重启与退出导读.md#5.2_从排队到最终关闭)，再按 S2 改期、S3 关闭、S4 等待顺序核对。以下保留相应函数主体，去掉部分上游注释；中文 Doxygen 是仓库补充。

## 1.1\_改期不等于追加一次回调

```c
/** @brief 仓库补充阅读说明：传 options=0 进入统一改期；返回旧 pending 状态，不是引用票据。 */
int mod_timer(struct timer_list *timer, unsigned long expires)
{
	return __mod_timer(timer, expires, 0);
}
```
```c
/** @brief 仓库补充阅读说明：在 base 保护下重新安排同一个 timer；关闭后不再发布，计数责任由外层提供。 */
static inline int
__mod_timer(struct timer_list *timer, unsigned long expires, unsigned int options)
{
	unsigned long clk = 0, flags, bucket_expiry;
	struct timer_base *base, *new_base;
	unsigned int idx = UINT_MAX;
	int ret = 0;

	debug_assert_init(timer);

	if (!(options & MOD_TIMER_NOTPENDING) && timer_pending(timer)) {
		long diff = timer->expires - expires;

		if (!diff)
			return 1;
		if (options & MOD_TIMER_REDUCE && diff <= 0)
			return 1;

		base = lock_timer_base(timer, &flags);
		if (!timer->function)
			goto out_unlock;

		forward_timer_base(base);

		if (timer_pending(timer) && (options & MOD_TIMER_REDUCE) &&
		    time_before_eq(timer->expires, expires)) {
			ret = 1;
			goto out_unlock;
		}

		clk = base->clk;
		idx = calc_wheel_index(expires, clk, &bucket_expiry);

		if (idx == timer_get_idx(timer)) {
			if (!(options & MOD_TIMER_REDUCE))
				timer->expires = expires;
			else if (time_after(timer->expires, expires))
				timer->expires = expires;
			ret = 1;
			goto out_unlock;
		}
	} else {
		base = lock_timer_base(timer, &flags);
		if (!timer->function)
			goto out_unlock;

		forward_timer_base(base);
	}

	ret = detach_if_pending(timer, base, false);
	if (!ret && (options & MOD_TIMER_PENDING_ONLY))
		goto out_unlock;

	new_base = get_timer_this_cpu_base(timer->flags);

	if (base != new_base) {
		if (likely(base->running_timer != timer)) {
			timer->flags |= TIMER_MIGRATING;

			raw_spin_unlock(&base->lock);
			base = new_base;
			raw_spin_lock(&base->lock);
			WRITE_ONCE(timer->flags,
				   (timer->flags & ~TIMER_BASEMASK) | base->cpu);
			forward_timer_base(base);
		}
	}

	debug_timer_activate(timer);

	timer->expires = expires;
	if (idx != UINT_MAX && clk == base->clk)
		enqueue_timer(base, timer, idx, bucket_expiry);
	else
		internal_add_timer(base, timer);

out_unlock:
	raw_spin_unlock_irqrestore(&base->lock, flags);

	return ret;
}
```
先看两个无需重新挂接的情况：已经 pending 且到期时刻相同可以直接返回 1；处于同一个定时轮槽时可只调整 expires 并返回 1。它们没有创建第二个 timer 节点，更不会替外层增加第二次 callback 责任。其余路径先 detach_if_pending 再重新安排，仍是同一个对象的节点。

涉及重新安排的路径由 lock_timer_base 取得该 timer 当前 base 锁，并在锁内检查 function 是否已被置空。退出函数也在这把锁下置空，因此关闭与重新入队有共同串行化点。running_timer 的检查还禁止在回调执行时迁移到另一 base，使同步删除仍能在正确的 base 观察该执行者。时间轮索引、CPU 选择和迁移的完整算法在此不作跨配置推论。

对于未关闭的正常调用，返回 0 可表示原先 inactive、现在已启动；返回 1 可表示原先 pending、已改期或无需改期。关闭后的 function 为空也返回 0，但没有启动任何回调。不能按零/非零把它解释成 queue_work 的接收布尔值，更不能据此盲目决定预留引用由谁接走。

## 1.2\_等待执行与关闭重启

```c
/** @brief 仓库补充阅读说明：base 锁内观察执行者；shutdown 同时清空 function，阻止后续启动。 */
static int __try_to_del_timer_sync(struct timer_list *timer, bool shutdown)
{
	struct timer_base *base;
	unsigned long flags;
	int ret = -1;

	debug_assert_init(timer);

	base = lock_timer_base(timer, &flags);

	if (base->running_timer != timer)
		ret = detach_if_pending(timer, base, true);
	if (shutdown)
		timer->function = NULL;

	raw_spin_unlock_irqrestore(&base->lock, flags);

	return ret;
}
```
ret 初始为 -1；base->running_timer 指向本 timer 时不摘除，表示执行尚未结束。若已无该执行者，detach_if_pending 返回原来是否排队。shutdown 分支无论本次是否仍在执行都会清空 function；它禁止新的启动，不中断已经进入回调的函数体。

```c
/** @brief 仓库补充阅读说明：持续尝试，直到不再观察到运行中；保留上下文及调试配置检查。 */
static int __timer_delete_sync(struct timer_list *timer, bool shutdown)
{
	int ret;

#ifdef CONFIG_LOCKDEP
	unsigned long flags;

	local_irq_save(flags);
	lock_map_acquire(&timer->lockdep_map);
	lock_map_release(&timer->lockdep_map);
	local_irq_restore(flags);
#endif
	WARN_ON(in_hardirq() && !(timer->flags & TIMER_IRQSAFE));

	if (IS_ENABLED(CONFIG_PREEMPT_RT) && !(timer->flags & TIMER_IRQSAFE))
		lockdep_assert_preemption_enabled();

	do {
		ret = __try_to_del_timer_sync(timer, shutdown);

		if (unlikely(ret < 0)) {
			del_timer_wait_running(timer);
			cpu_relax();
		}
	} while (ret < 0);

	return ret;
}
```
ret<0 触发等待运行者及重试；最后只返回 0/1。CONFIG_LOCKDEP 的映射检查和硬中断条件告警属于诊断，不替调用者解开等待循环。PREEMPT_RT 分支另有可调度要求；当前非 RT 配置的检查不能证明其他配置。调用方不得持有妨碍回调退出的锁，也不能在该 timer 回调中等自己。

```c
/** @brief 仓库补充阅读说明：普通同步删除不清空 function，调用者须防止竞争重启。 */
int timer_delete_sync(struct timer_list *timer)
{
	return __timer_delete_sync(timer, false);
}
```
```c
/** @brief 仓库补充阅读说明：最终同步关闭清空 function 并等待旧执行结束。 */
int timer_shutdown_sync(struct timer_list *timer)
{
	return __timer_delete_sync(timer, true);
}
```
两个包装仅在 shutdown 布尔参数上分流。timer_delete_sync 的注释要求调用者阻止重启；若另一路在其释放 base 锁后重新启动，返回瞬间的结果不能作为永久关闭证据。timer_shutdown_sync 则保证返回后该 timer 不排队、不执行且再启动被丢弃，直到另行重新初始化；它不撤销外部保存的对象裸地址，也不自动等待本 timer 曾经投递的 work。

固定注释给出 timer 与 work 相互启动时先 shutdown timer、再退出 workqueue 的顺序；实际 API 名称是 destroy_workqueue。外层必须保留对象到两者退出，保证剩余清理不再依赖一个能触发的 timer，并停止其他外部生产者。shutdown 的返回值仍只描述最终删除观察到的 pending 状态，不是对已执行次数的汇总。

返回[模块导读](../../../navigation/P05_定时器重启与退出导读.md#5.2_从排队到最终关闭)或[总索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
