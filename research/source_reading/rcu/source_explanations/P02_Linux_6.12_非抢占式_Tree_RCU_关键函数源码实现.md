---
id: research.source_reading.rcu.linux_6_12_nonpreempt_tree_implementation
title: "Linux 6.12 非抢占式 Tree RCU 关键函数源码实现"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
topics:
  - synchronization
  - rcu
  - nonpreempt
  - implementation
source_project: linux
source_version: "6.12.20"
---

# 第2章\_Linux\_6.12\_非抢占式\_Tree\_RCU\_关键函数源码实现

## 2.1\_实现讲解边界与入口

本章不再解释“非抢占式 Tree RCU 整体怎样完成 GP”，而是专门回答“非抢占配置怎样把同步等待接入 callback、CPU 怎样感知代际、产生 QS 并把证据逐层上报”。普通 GP kthread、`rcu_gp_init()`、FQS 与 `rcu_gp_cleanup()` 已收敛到独立的 GP 实现文档，避免把全局公共控制误写成非抢占配置私有实现。

| 阅读入口 | 职责 |
| --- | --- |
| [RCU 源码总导航](../navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.9_建议的源码阅读顺序) | 先区分普通 Tree RCU、SRCU、Tasks 与 Tiny，再选择模块 |
| [RCU 实现家族与内核配置](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P22_RCU_实现家族与内核配置.md#22.2_三个正交维度) | 解释“非抢占式”与“Tree RCU”分别限定哪一个维度 |
| [非抢占式 Tree RCU 问题与证明模型](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P05_非抢占式_Tree_RCU_问题与证明模型.md#5.1.1_标题里的两个限定不是同义关系) | 在进入函数前建立 CPU QS 的抽象证明边界 |
| [Tree RCU GP 全局生命周期源码实现](P05_Linux_6.12_Tree_RCU_GP全局生命周期源码实现.md#5.2_源码符号覆盖账本) | 唯一展开 GP 请求、长期线程、init、FQS 与 cleanup |
| [非抢占式 Tree RCU 模块源码概念导读](../navigation/P02_Linux_6.12_非抢占式_Tree_RCU_模块源码概念导读.md#2.3_源码文件与状态所有权) | 说明这些函数如何组成同步等待桥、CPU QS 和树形汇聚 |
| [非抢占式 Tree RCU 稳定机制正文](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P06_非抢占式_Tree_RCU_源码同步机制.md#6.1_源码边界与贯穿场景) | 解释跨版本稳定的状态与通信模型 |

下列 `/** ... */` 块是本仓库为阅读补充的中文 Doxygen 说明，不是上游文件原注释。代码只裁剪支撑本章结论的语句，省略处明确标记；完整实现以链接的版本化源文件为准。

## 2.2\_函数实现索引

| 实现点 | 上游相对位置 | 本章解释的原理 |
| --- | --- | --- |
| [`wakeme_after_rcu()` / `__wait_rcu_gp()`](#2.3___wait_rcu_gp与wakeme_after_rcu连接等待者) | [`kernel/rcu/update.c`](../../linux/kernel/rcu/update.c) | 用栈上 `completion` 和 callback 把阻塞任务接到 GP 完成边界 |
| [`__note_gp_changes()`](#2.4___note_gp_changes让CPU识别新GP) | [`kernel/rcu/tree.c`](../../linux/kernel/rcu/tree.c) | 把节点代际和 CPU 本地债务对齐 |
| [`__rcu_read_lock()` / `__rcu_read_unlock()`](#2.5_rcu_note_context_switch与rcu_qs记录静止态) | [`include/linux/rcupdate.h`](../../linux/include/linux/rcupdate.h) | 非抢占读侧为什么能使用调度边界作为 QS |
| [`rcu_note_context_switch()` / `rcu_qs()`](#2.5_rcu_note_context_switch与rcu_qs记录静止态) | [`kernel/rcu/tree_plugin.h`](../../linux/kernel/rcu/tree_plugin.h) | 调度钩子如何只锁存本 CPU 的 QS 证据 |
| [`rcu_report_qs_rdp()` / `rcu_report_qs_rnp()`](#2.6_rcu_report_qs_rdp与rcu_report_qs_rnp汇聚证据) | [`kernel/rcu/tree.c`](../../linux/kernel/rcu/tree.c) | 本地证据如何校验代际并逐层清位 |

## 2.3\_\_\_wait\_rcu\_gp与wakeme\_after\_rcu连接等待者

```c
/**
 * @brief 在 RCU 回调可执行时唤醒对应的同步等待者。
 * @param head 嵌在栈上 struct rcu_synchronize 中的 rcu_head。
 * @return 无返回值。
 * @note 本 Doxygen 说明由仓库补充；函数体裁剪自 kernel/rcu/update.c。
 */
void wakeme_after_rcu(struct rcu_head *head)
{
	struct rcu_synchronize *rcu;

	/* 从回调头找回调用者栈上的等待对象。 */
	rcu = container_of(head, struct rcu_synchronize, head);
	/* callback 能执行已证明相关 GP 边界被跨过。 */
	complete(&rcu->completion);
}

/**
 * @brief 为一组 RCU flavor 排队回调，并等待每个唯一回调完成。
 * @param checktiny 是否应用 Tiny RCU 的特殊跳过规则。
 * @param state 等待 completion 时使用的任务状态。
 * @param n 回调函数数组的元素数量。
 * @param crcu_array call_rcu 类型的函数数组。
 * @param rs_array 调用者栈上的等待对象数组。
 */
void __wait_rcu_gp(bool checktiny, unsigned int state, int n,
		   call_rcu_func_t *crcu_array,
		   struct rcu_synchronize *rs_array)
{
	int i;
	int j;

	for (i = 0; i < n; i++) {
		/* Tiny RCU 的普通 call_rcu 路径无需另排等待回调。 */
		if (checktiny &&
		    (crcu_array[i] == call_rcu)) {
			might_sleep();
			continue;
		}
		/* 相同 flavor 只注册一次，避免重复等待同一回调函数。 */
		for (j = 0; j < i; j++)
			if (crcu_array[j] == crcu_array[i])
				break;
		if (j == i) {
			init_rcu_head_on_stack(&rs_array[i].head);
			init_completion(&rs_array[i].completion);
			(crcu_array[i])(&rs_array[i].head, wakeme_after_rcu);
		}
	}

	for (i = 0; i < n; i++) {
		if (checktiny &&
		    (crcu_array[i] == call_rcu))
			continue;
		for (j = 0; j < i; j++)
			if (crcu_array[j] == crcu_array[i])
				break;
		if (j == i) {
			/* 回调在 GP 后 complete()，这里才能继续。 */
			wait_for_completion_state(&rs_array[i].completion, state);
			destroy_rcu_head_on_stack(&rs_array[i].head);
		}
	}
}
```

**实现原理：** 同步调用者不需要被 GP kthread 单独记录。它只需要提交一个带 `completion` 的 callback；回调可执行性代表相关宽限期已结束，因此 callback 执行器调用 `complete()` 就能把结论返回原任务。完整实现还会去重相同的 `call_rcu` 函数，并处理 Tiny RCU 分支。

## 2.4\_\_\_note\_gp\_changes让CPU识别新GP

```c
/**
 * @brief 把叶 rcu_node 的 GP 状态同步到当前 CPU 的 rcu_data。
 * @param rnp 当前 CPU 所属的叶节点，调用者必须持有 rnp->lock。
 * @param rdp 当前 CPU 的 per-CPU RCU 状态。
 * @return 回调列表推进是否产生了后续工作。
 */
static bool __note_gp_changes(struct rcu_node *rnp, struct rcu_data *rdp)
{
	bool ret = false;
	bool need_qs;
	const bool offloaded = rcu_rdp_is_offloaded(rdp);

	raw_lockdep_assert_held_rcu_node(rnp);
	if (rdp->gp_seq == rnp->gp_seq)
		return false; /* CPU 已经观察到同一节点代际。 */

	/* 必须先处理旧 GP 结束，才能把回调推进到正确分段。 */
	if (rcu_seq_completed_gp(rdp->gp_seq, rnp->gp_seq) ||
	    unlikely(READ_ONCE(rdp->gpwrap))) {
		if (!offloaded)
			ret = rcu_advance_cbs(rnp, rdp);
		rdp->core_needs_qs = false;
	} else {
		if (!offloaded)
			ret = rcu_accelerate_cbs(rnp, rdp);
		if (rdp->core_needs_qs)
			rdp->core_needs_qs = !!(rnp->qsmask & rdp->grpmask);
	}
	/* 省略：上一轮结束和新一轮开始的 trace。 */

	if (rcu_seq_new_gp(rdp->gp_seq, rnp->gp_seq) ||
	    unlikely(READ_ONCE(rdp->gpwrap))) {
		/* 只有本 CPU 的叶掩码位尚未清零时，它才欠本轮 QS。 */
		need_qs = !!(rnp->qsmask & rdp->grpmask);
		rdp->cpu_no_qs.b.norm = need_qs;
		rdp->core_needs_qs = need_qs;
		zero_cpu_stall_ticks(rdp);
	}
	rdp->gp_seq = rnp->gp_seq;
	if (ULONG_CMP_LT(rdp->gp_seq_needed, rnp->gp_seq_needed) || rdp->gpwrap)
		WRITE_ONCE(rdp->gp_seq_needed, rnp->gp_seq_needed);
	/* 省略：CONFIG_PROVE_RCU 下记录 last_sched_clock。 */
	WRITE_ONCE(rdp->gpwrap, false);
	rcu_gpnum_ovf(rnp, rdp);
	return ret;
}
```

**实现原理：** CPU 不是在 GP 开始时被单独发消息，而是在 RCU core 路径中比较本地 `gp_seq` 与叶节点 `gp_seq`。只有叶掩码仍包含本 CPU 的位时，才把 `cpu_no_qs.b.norm` 和 `core_needs_qs` 设为待偿。

## 2.5\_rcu\_note\_context\_switch与rcu\_qs记录静止态

```c
/**
 * @brief 进入非抢占 RCU 读侧临界区。
 * @note 禁止抢占使普通调度切换不可能发生在合法读侧内部。
 */
static inline void __rcu_read_lock(void)
{
	preempt_disable(); /* 读侧与当前 CPU 绑定，不产生共享登记。 */
}

/**
 * @brief 退出非抢占 RCU 读侧临界区。
 */
static inline void __rcu_read_unlock(void)
{
	preempt_enable(); /* 恢复抢占；默认路径不直接上报 QS。 */
	if (IS_ENABLED(CONFIG_RCU_STRICT_GRACE_PERIOD))
		rcu_read_unlock_strict(); /* 严格测试配置的额外慢路径。 */
}

/**
 * @brief 在非抢占配置中锁存“本 CPU 已经越过 QS”的证据。
 * @pre 调用者必须已禁止抢占。
 */
static void rcu_qs(void)
{
	RCU_LOCKDEP_WARN(preemptible(),
			  "rcu_qs() invoked with preemption enabled!!!");
	if (!__this_cpu_read(rcu_data.cpu_no_qs.s))
		return; /* 本 CPU 当前没有待偿债务。 */
	trace_rcu_grace_period(TPS("rcu_sched"),
			       __this_cpu_read(rcu_data.gp_seq), TPS("cpuqs"));
	/* 只写 per-CPU 标志，不在这里获取 rcu_node 锁。 */
	__this_cpu_write(rcu_data.cpu_no_qs.b.norm, false);
	if (__this_cpu_read(rcu_data.cpu_no_qs.b.exp))
		rcu_report_exp_rdp(this_cpu_ptr(&rcu_data));
}

/**
 * @brief 在普通调度切换边界为非抢占 RCU 记录 QS。
 * @param preempt 调度器传入的抢占切换标志；本分支不需要利用它追踪任务。
 * @pre 调用者已关闭本地中断。
 */
void rcu_note_context_switch(bool preempt)
{
	rcu_qs(); /* 合法的调度切换证明旧普通 reader 已结束。 */
	/* 省略：紧急 QS、Tasks RCU 与 trace 处理。 */
}
```

**实现原理：** 读侧用“禁止抢占”维持本地不变量，因而调度边界可以证明 GP 开始前的普通 reader 不再存在。`rcu_qs()` 只锁存 per-CPU 证据，共享树清位由后续 `rcu_core()` 路径完成，把高频本地操作与较重的节点锁操作分开。

## 2.6\_rcu\_report\_qs\_rdp与rcu\_report\_qs\_rnp汇聚证据

```c
/**
 * @brief 把当前 CPU 已锁存的 QS 证据提交到它的叶 rcu_node。
 * @param rdp 当前 CPU 的 per-CPU RCU 状态。
 * @pre 必须在 rdp 所属 CPU 上调用。
 */
static void rcu_report_qs_rdp(struct rcu_data *rdp)
{
	unsigned long flags;
	unsigned long mask;
	struct rcu_node *rnp;

	WARN_ON_ONCE(rdp->cpu != smp_processor_id());
	rnp = rdp->mynode;
	raw_spin_lock_irqsave_rcu_node(rnp, flags);
	/* 本地仍欠 QS、代际已变或发生回绕时，不能把旧证据上报到新 GP。 */
	if (rdp->cpu_no_qs.b.norm || rdp->gp_seq != rnp->gp_seq || rdp->gpwrap) {
		rdp->cpu_no_qs.b.norm = true;
		raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
		return;
	}
	mask = rdp->grpmask;
	rdp->core_needs_qs = false;
	if ((rnp->qsmask & mask) == 0) {
		raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
	} else {
		/* 省略：非 NOCB CPU 先加速本地 callback。 */
		rcu_disable_urgency_upon_qs(rdp);
		rcu_report_qs_rnp(mask, rnp, rnp->gp_seq, flags);
	}
}

/**
 * @brief 在 rcu_node 树中清除一个参与者位，并在节点归零时逐层向上汇聚。
 * @param mask 本层应清除的 CPU 位或子节点位。
 * @param rnp 当前节点，进入时已持有它的锁。
 * @param gps 证据所属的 GP 代际。
 * @param flags 最终释放节点锁时恢复的中断状态。
 */
static void rcu_report_qs_rnp(unsigned long mask, struct rcu_node *rnp,
			      unsigned long gps, unsigned long flags)
{
	unsigned long oldmask = 0;
	struct rcu_node *rnp_c;

	raw_lockdep_assert_held_rcu_node(rnp);
	for (;;) {
		/* 位已清或代际不匹配，说明这份证据已被消费或已过期。 */
		if ((!(rnp->qsmask & mask) && mask) || rnp->gp_seq != gps) {
			raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
			return;
		}
		WARN_ON_ONCE(oldmask);
		WARN_ON_ONCE(!rcu_is_leaf_node(rnp) &&
			     rcu_preempt_blocked_readers_cgp(rnp));
		WRITE_ONCE(rnp->qsmask, rnp->qsmask & ~mask);
		/* 省略：记录本层掩码变化的 trace。 */
		/* 本层还有位或抢占 reader 债务时，不得清父节点位。 */
		if (rnp->qsmask != 0 || rcu_preempt_blocked_readers_cgp(rnp)) {
			raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
			return;
		}
		rnp->completedqs = rnp->gp_seq;
		mask = rnp->grpmask;
		if (rnp->parent == NULL)
			break;
		/* 先释放子节点锁，再获取父节点锁；同一时刻不持有整条路径。 */
		raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
		rnp_c = rnp;
		rnp = rnp->parent;
		raw_spin_lock_irqsave_rcu_node(rnp, flags);
		oldmask = READ_ONCE(rnp_c->qsmask);
	}
	rcu_report_qs_rsp(flags); /* 根节点归零，唤醒 GP 推进。 */
}
```

**实现原理：** `rcu_report_qs_rdp()` 先在叶节点锁下复核“本地证据仍属于当前 GP”；`rcu_report_qs_rnp()` 再把一个局部结论变成父节点的一个子树结论。这两层校验防止延迟上报把旧 GP 的 QS 错认为新 GP 证据。

## 2.7\_实现复核问题

1. `__wait_rcu_gp()` 为什么可以只排队 callback，而不需要让 GP kthread 记录等待任务？
2. `__note_gp_changes()` 怎样防止没有参与本轮 GP 的 CPU 被错设为欠 QS？
3. 为什么 `rcu_qs()` 只写 per-CPU 状态，而不立即锁叶节点？
4. `rcu_report_qs_rdp()` 为什么必须在上报前重新检查 GP 代际？
5. `rcu_report_qs_rnp()` 为什么只有本层 CPU/子树位和任务债务都清除以后才继续上报父节点？

模块概念导读：[Linux 6.12 非抢占式 Tree RCU 模块源码概念导读](../navigation/P02_Linux_6.12_非抢占式_Tree_RCU_模块源码概念导读.md)。
