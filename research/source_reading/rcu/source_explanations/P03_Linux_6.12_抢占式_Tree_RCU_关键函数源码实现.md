---
id: research.source_reading.rcu.linux_6_12_preempt_tree_implementation
title: "Linux 6.12 抢占式 Tree RCU 关键函数源码实现"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
topics:
  - synchronization
  - rcu
  - preemption
  - implementation
source_project: linux
source_version: "6.12.20"
---

# 第3章\_Linux\_6.12\_抢占式\_Tree\_RCU\_关键函数源码实现

## 3.1\_实现讲解边界与入口

本章只讲解抢占式 Tree RCU 中的具体状态定义和函数实现：读侧快路怎样写 `task_struct`，调度钩子怎样把债务转移到 `rcu_node`，任务退出时怎样删链、推进游标并恢复树形上报。整体交错、状态轴和模块结论位于 navigation 文档。

| 阅读入口 | 职责 |
| --- | --- |
| [RCU 源码总导航](../navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.9_建议的源码阅读顺序) | 先区分 RCU 家族，再选择功能模块和阅读顺序 |
| [RCU 实现家族与内核配置](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P22_RCU_实现家族与内核配置.md#22.2_三个正交维度) | 先确认 `CONFIG_PREEMPT_RCU` 改变的是普通 Tree RCU 的读侧方式 |
| [抢占式 Tree RCU 问题与任务跟踪模型](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P07_抢占式_Tree_RCU_问题与任务跟踪模型.md#7.1_先制造非抢占模型无法解释的现场) | 在进入函数前建立 CPU 债务转为任务债务的抽象证明 |
| [抢占式 Tree RCU 模块源码概念导读](../navigation/P03_Linux_6.12_抢占式_Tree_RCU_模块源码概念导读.md#3.1_取证问题) | 说明这些实现如何组成“CPU 债务转任务债务”的端到端闭环 |
| [抢占式 Tree RCU 稳定机制正文](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P08_抢占式_Tree_RCU_源码同步机制.md#8.1_版本_配置与源码边界) | 解释跨版本稳定的任务跟踪模型 |
| [普通 GP 全局生命周期源码实现](P05_Linux_6.12_Tree_RCU_GP全局生命周期源码实现.md#5.9_rcu_gp_init开始代际并建立证明债务) | 说明 GP init 怎样接管已有 blocked task，以及根债务清零后怎样进入 cleanup；本章只展开任务侧债务 |

下列 `/** ... */` 是本仓库补充的中文 Doxygen 阅读说明，不是上游原注释。裁剪代码保留影响状态所有权、等待边界和上报顺序的语句；完整函数以链接的 Linux 6.12.20 版本化源文件为准。

## 3.2\_任务与节点的共享状态实现

`task_struct` 中的 `rcu_read_lock_nesting`、`rcu_read_unlock_special`、`rcu_node_entry` 和 `rcu_blocked_node` 声明位于上游 `include/linux/sched.h`。当前仓库尚未保存该文件快照，因此本章不伪造结构体摘录；后续只使用已保存 [`tree_plugin.h`](../../linux/kernel/rcu/tree_plugin.h) 中对这些字段的真实读写作为实现证据。节点侧声明已经保存在 [`tree.h`](../../linux/kernel/rcu/tree.h)，可以直接摘录：

```c
/**
 * @brief 叶 rcu_node 用于保存已被抢占 reader 及不同 GP 等待边界的字段。
 * @note 字段按 tree.h 中的原顺序摘录；中文注释由仓库补充。
 */
struct rcu_node {
	/* 省略：节点锁、代际、掩码、父子关系等其他字段。 */
	struct list_head blkd_tasks; /* 在 RCU 读侧内被抢占的任务集合。 */
	struct list_head *gp_tasks;  /* 当前普通 GP 等待的第一个任务，NULL 表示无债务。 */
	struct list_head *exp_tasks; /* 当前 expedited GP 的等待游标。 */
	struct list_head *boost_tasks; /* 需要优先级提升的第一个任务。 */
};
```

**实现原理：** `task_struct` 保存“这个 reader 是谁、欠的债务属于哪个叶节点”，`rcu_node` 保存“节点共享地等待哪些任务以及每类 GP 的起始游标”。任务迁移只改变运行 CPU，不改变 `rcu_blocked_node` 指向的债务所有者。

## 3.3\_\_\_rcu\_read\_lock与\_\_rcu\_read\_unlock实现

```c
/**
 * @brief 进入抢占式 RCU 读侧临界区。
 * @note 快路径只修改当前 task_struct，不锁 rcu_node。
 */
void __rcu_read_lock(void)
{
	rcu_preempt_read_enter(); /* current->rcu_read_lock_nesting++。 */
	if (IS_ENABLED(CONFIG_PROVE_LOCKING))
		WARN_ON_ONCE(rcu_preempt_depth() > RCU_NEST_PMAX);
	if (IS_ENABLED(CONFIG_RCU_STRICT_GRACE_PERIOD) && rcu_state.gp_kthread)
		WRITE_ONCE(current->rcu_read_unlock_special.b.need_qs, true);
	barrier(); /* 防止临界区内访问被编译器移到进入操作之前。 */
}

/**
 * @brief 退出抢占式 RCU 读侧，必要时进入任务债务清理慢路径。
 */
void __rcu_read_unlock(void)
{
	struct task_struct *t = current;

	barrier(); /* 临界区内访问必须先于退出操作。 */
	if (rcu_preempt_read_exit() == 0) {
		barrier();
		/* 只有最外层退出且存在 special 债务时才支付节点锁成本。 */
		if (unlikely(READ_ONCE(t->rcu_read_unlock_special.s)))
			rcu_read_unlock_special(t);
	}
	if (IS_ENABLED(CONFIG_PROVE_LOCKING)) {
		int rrln = rcu_preempt_depth();

		WARN_ON_ONCE(rrln < 0 || rrln > RCU_NEST_PMAX);
	}
}
```

**实现原理：** 没有被抢占的 reader 始终只写当前任务。只有真正发生状态归属变化时，`blocked` 或 `need_qs` 才把最外层 unlock 导向共享清理路径。

## 3.4\_rcu\_note\_context\_switch转移读侧债务

```c
/**
 * @brief 在任务债务已经共享登记后，锁存本 CPU 的普通 QS。
 * @pre 调用者必须禁止抢占。
 * @note 本 Doxygen 说明由仓库补充；函数体裁剪自 tree_plugin.h。
 */
static void rcu_qs(void)
{
	RCU_LOCKDEP_WARN(preemptible(),
			  "rcu_qs() invoked with preemption enabled!!!\n");
	if (__this_cpu_read(rcu_data.cpu_no_qs.b.norm)) {
		/* 省略：trace；下面只修改当前 CPU 和 current 的本地状态。 */
		__this_cpu_write(rcu_data.cpu_no_qs.b.norm, false);
		barrier();
		WRITE_ONCE(current->rcu_read_unlock_special.b.need_qs, false);
	}
}

/**
 * @brief 在调度切换前，把读侧内被抢占任务从 CPU 本地债务转成叶节点共享债务。
 * @param preempt true 表示被动抢占；false 且仍在普通读侧内会触发警告。
 * @pre 调用者已关闭本地中断。
 */
void rcu_note_context_switch(bool preempt)
{
	struct task_struct *t = current;
	struct rcu_data *rdp = this_cpu_ptr(&rcu_data);
	struct rcu_node *rnp;

	/* 省略：利用率 trace 和中断关闭断言。 */
	WARN_ONCE(!preempt && rcu_preempt_depth() > 0,
		    "Voluntary context switch within RCU read-side critical section!");
	if (rcu_preempt_depth() > 0 &&
	    !t->rcu_read_unlock_special.b.blocked) {
		rnp = rdp->mynode;
		raw_spin_lock_rcu_node(rnp);
		/* 先声明任务已转为共享 blocked 状态。 */
		t->rcu_read_unlock_special.b.blocked = true;
		/* 保存原叶节点，使迁移后的 unlock 仍能找回债务所有者。 */
		t->rcu_blocked_node = rnp;
		/* 省略：CPU 在线性、链表状态和抢占事件 trace 检查。 */
		rcu_preempt_ctxt_queue(rnp, rdp);
	} else {
		rcu_preempt_deferred_qs(t);
	}
	/* 任务债务已经全局可见后，才允许本 CPU 清 QS 债务。 */
	rcu_qs();
	/* 省略：expedited CPU 报告、Tasks RCU QS 和结束 trace。 */
}
```

**实现原理：** 顺序是正确性的核心。如果先清 CPU QS，再挂 `blkd_tasks`，GP 可能在两步之间看到“CPU 已报告且没有任务债务”的伪窗口。当前实现先使共享债务可见，再清本地位，因而债务始终至少有一个载体。

## 3.5\_rcu\_preempt\_ctxt\_queue建立任务等待边界

```c
/**
 * @brief 根据普通/expedited GP 的当前游标和 CPU 掩码，把被抢占 reader 插入 blkd_tasks 的正确位置。
 * @param rnp 当前 CPU 的叶 rcu_node，进入时已持有节点锁。
 * @param rdp 当前 CPU 的 per-CPU RCU 状态。
 * @post 函数返回前释放 rnp->lock，但保持中断关闭。
 */
static void rcu_preempt_ctxt_queue(struct rcu_node *rnp,
				   struct rcu_data *rdp)
{
	int blkd_state = (rnp->gp_tasks ? RCU_GP_TASKS : 0) +
			 (rnp->exp_tasks ? RCU_EXP_TASKS : 0) +
			 (rnp->qsmask & rdp->grpmask ? RCU_GP_BLKD : 0) +
			 (rnp->expmask & rdp->grpmask ? RCU_EXP_BLKD : 0);
	struct task_struct *t = current;

	raw_lockdep_assert_held_rcu_node(rnp);
	WARN_ON_ONCE(rdp->mynode != rnp);
	WARN_ON_ONCE(!rcu_is_leaf_node(rnp));
	/* 省略：检查新上线 CPU 不应被当前 GP 等待。 */

	/* 上游使用完整状态表决定插入位置，不能压缩成真假二分。 */
	switch (blkd_state) {
	case 0:
	case                RCU_EXP_TASKS:
	case                RCU_EXP_TASKS + RCU_GP_BLKD:
	case RCU_GP_TASKS:
	case RCU_GP_TASKS + RCU_EXP_TASKS:
		/* 不阻塞已在等待的 GP，放到链表头。 */
		list_add(&t->rcu_node_entry, &rnp->blkd_tasks);
		break;

	case                                              RCU_EXP_BLKD:
	case                                RCU_GP_BLKD:
	case                                RCU_GP_BLKD + RCU_EXP_BLKD:
	case RCU_GP_TASKS +                               RCU_EXP_BLKD:
	case RCU_GP_TASKS +                 RCU_GP_BLKD + RCU_EXP_BLKD:
	case RCU_GP_TASKS + RCU_EXP_TASKS + RCU_GP_BLKD + RCU_EXP_BLKD:
		/* 首个阻塞某类 GP 的任务放到链表尾，避免旧 GP 多等无关任务。 */
		list_add_tail(&t->rcu_node_entry, &rnp->blkd_tasks);
		break;

	case                RCU_EXP_TASKS +               RCU_EXP_BLKD:
	case                RCU_EXP_TASKS + RCU_GP_BLKD + RCU_EXP_BLKD:
	case RCU_GP_TASKS + RCU_EXP_TASKS +               RCU_EXP_BLKD:
		/* 后续 expedited 阻塞者插到 exp_tasks 之后。 */
		list_add(&t->rcu_node_entry, rnp->exp_tasks);
		break;

	case RCU_GP_TASKS +                 RCU_GP_BLKD:
	case RCU_GP_TASKS + RCU_EXP_TASKS + RCU_GP_BLKD:
		/* 后续普通 GP 阻塞者插到 gp_tasks 之后。 */
		list_add(&t->rcu_node_entry, rnp->gp_tasks);
		break;

	default:
		WARN_ON_ONCE(1);
		break;
	}

	if (!rnp->gp_tasks && (blkd_state & RCU_GP_BLKD)) {
		WRITE_ONCE(rnp->gp_tasks, &t->rcu_node_entry);
		WARN_ON_ONCE(rnp->completedqs == rnp->gp_seq);
	}
	if (!rnp->exp_tasks && (blkd_state & RCU_EXP_BLKD))
		WRITE_ONCE(rnp->exp_tasks, &t->rcu_node_entry);
	WARN_ON_ONCE(!(blkd_state & RCU_GP_BLKD) !=
		     !(rnp->qsmask & rdp->grpmask));
	WARN_ON_ONCE(!(blkd_state & RCU_EXP_BLKD) !=
		     !(rnp->expmask & rdp->grpmask));
	raw_spin_unlock_rcu_node(rnp);

	/* 中断仍关闭；若 expedited GP 正等待本 CPU，在这里完成它的 CPU 报告。 */
	if (blkd_state & RCU_EXP_BLKD && rdp->cpu_no_qs.b.exp)
		rcu_report_exp_rdp(rdp);
	else
		WARN_ON_ONCE(rdp->cpu_no_qs.b.exp);
}
```

**实现原理：** `switch (blkd_state)` 明确区分链表头、链表尾、`gp_tasks` 之后和 `exp_tasks` 之后四类插入位置。游标只指向真正阻塞相应 GP 的第一项；更晚开始、与当前 GP 无关的 reader 可以排在游标之前。该算法允许普通 GP 多等一些 reader，但不允许漏掉应等 reader。

## 3.6\_rcu\_preempt\_check\_blocked\_tasks接管旧任务

```c
/**
 * @brief 在新 GP 初始化时，把 GP 开始前已挂入 blkd_tasks 的任务纳入新等待边界。
 * @param rnp 正在初始化的 rcu_node。
 * @pre 调用者持有 rnp->lock，且禁止抢占。
 */
static void rcu_preempt_check_blocked_tasks(struct rcu_node *rnp)
{
	struct task_struct *t;

	RCU_LOCKDEP_WARN(preemptible(),
			  "rcu_preempt_check_blocked_tasks() invoked with preemption enabled!!!\n");
	raw_lockdep_assert_held_rcu_node(rnp);
	/* 新 GP 开始时不应遗留上一轮 gp_tasks 游标。 */
	if (WARN_ON_ONCE(rcu_preempt_blocked_readers_cgp(rnp)))
		dump_blkd_tasks(rnp, 10);
	if (rcu_preempt_has_tasks(rnp) &&
	    (rnp->qsmaskinit || rnp->wait_blkd_tasks)) {
		/* 链表中旧 reader 都早于新 GP，所以第一项就是新等待边界。 */
		WRITE_ONCE(rnp->gp_tasks, rnp->blkd_tasks.next);
		t = container_of(rnp->gp_tasks, struct task_struct,
				 rcu_node_entry);
		trace_rcu_unlock_preempted_task(TPS("rcu_preempt-GPS"),
						rnp->gp_seq, t->pid);
	}
	WARN_ON_ONCE(rnp->qsmask);
}
```

**实现原理：** `rcu_preempt_ctxt_queue()` 解决“GP 先开始、任务后被抢占”，本函数解决“任务先被抢占、GP 后开始”。两条路径共同覆盖负债转移与 GP 边界的两种时间顺序。

## 3.7\_节点汇聚同时等待CPU与任务

```c
/**
 * @brief 查询指定节点是否仍有阻塞当前普通 GP 的 reader 任务。
 * @param rnp 要检查的 rcu_node。
 * @return gp_tasks 非空返回真。
 */
static int rcu_preempt_blocked_readers_cgp(struct rcu_node *rnp)
{
	return READ_ONCE(rnp->gp_tasks) != NULL;
}
```

共享的 `rcu_report_qs_rnp()` 已在[非抢占式 Tree RCU 关键函数实现](P02_Linux_6.12_非抢占式_Tree_RCU_关键函数源码实现.md#2.6_rcu_report_qs_rdp与rcu_report_qs_rnp汇聚证据)逐行展开，本章不复制同一函数。该实现清除当前 `qsmask` 位以后，只有 `rnp->qsmask == 0` 且这里的 `rcu_preempt_blocked_readers_cgp(rnp)` 也为假，才继续清父节点位。

**实现原理：** `qsmask` 表示 CPU/子树债务，`gp_tasks` 表示任务 reader 债务。抢占式 RCU 不把两者强行编码进同一掩码；它们通过 `rcu_report_qs_rnp()` 的同一个退出条件汇合，同时保持具体函数只在一个文档中展开。

## 3.8\_最外层退出删除任务并恢复传播

```c
/**
 * @brief 在安全的上下文中清理任务的 deferred QS 和 blocked-reader 债务。
 * @param t 刚退出最外层 RCU 读侧的任务。
 * @param flags 恢复本地中断状态所需的标志。
 */
static notrace void
rcu_preempt_deferred_qs_irqrestore(struct task_struct *t, unsigned long flags)
{
	struct rcu_node *rnp;
	struct list_head *np;
	bool empty_norm;
	struct rcu_data *rdp;
	union rcu_special special;

	/* 先快照并清除 special；中断关闭保证快照期间不会被并发改写。 */
	special = t->rcu_read_unlock_special;
	rdp = this_cpu_ptr(&rcu_data);
	if (!special.s && !rdp->cpu_no_qs.b.exp) {
		local_irq_restore(flags);
		return;
	}
	t->rcu_read_unlock_special.s = 0;

	/* 省略：need_qs 与 expedited CPU 报告，保留 blocked reader 清理主线。 */
	if (special.b.blocked) {
		/* 使用入队时保存的原叶节点，不使用当前 CPU 的叶节点。 */
		rnp = t->rcu_blocked_node;
		raw_spin_lock_rcu_node(rnp);
		WARN_ON_ONCE(rnp != t->rcu_blocked_node);
		WARN_ON_ONCE(!rcu_is_leaf_node(rnp));
		empty_norm = !rcu_preempt_blocked_readers_cgp(rnp);
		smp_mb(); /* 使 expedited fast path 观察到读侧已结束。 */
		np = rcu_next_node_entry(t, rnp);
		list_del_init(&t->rcu_node_entry);
		t->rcu_blocked_node = NULL;
		/* 若游标正指向本任务，把它推进到下一个债务。 */
		if (&t->rcu_node_entry == rnp->gp_tasks)
			WRITE_ONCE(rnp->gp_tasks, np);
		if (&t->rcu_node_entry == rnp->exp_tasks)
			WRITE_ONCE(rnp->exp_tasks, np);
		/* 省略：CONFIG_RCU_BOOST 的 boost_tasks 游标处理。 */
		/* 该任务是最后一个普通 GP 债务，且 CPU 位已清时，恢复树形传播。 */
		if (!empty_norm && !rcu_preempt_blocked_readers_cgp(rnp))
			rcu_report_unblock_qs_rnp(rnp, flags);
		else
			raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
		/* 省略：expedited 层级上报与解除 priority boost。 */
	} else {
		local_irq_restore(flags);
	}
}

/**
 * @brief 在节点的最后一个 blocked reader 退出后，把“子树已完成”恢复上报到父节点。
 * @param rnp 任务债务刚归零的叶节点。
 * @param flags 最终释放锁时恢复的中断状态。
 */
static void __maybe_unused
rcu_report_unblock_qs_rnp(struct rcu_node *rnp, unsigned long flags)
{
	unsigned long gps;
	unsigned long mask;
	struct rcu_node *rnp_p;

	raw_lockdep_assert_held_rcu_node(rnp);
	/* 任务债务或 CPU 债务任一存在，都不能恢复上报。 */
	if (WARN_ON_ONCE(!IS_ENABLED(CONFIG_PREEMPT_RCU)) ||
	    WARN_ON_ONCE(rcu_preempt_blocked_readers_cgp(rnp)) ||
	    rnp->qsmask != 0) {
		raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
		return;
	}

	rnp->completedqs = rnp->gp_seq;
	rnp_p = rnp->parent;
	if (rnp_p == NULL) {
		rcu_report_qs_rsp(flags);
		return;
	}

	/* 释放子节点锁，转而持有父节点锁，再复用普通树形汇聚函数。 */
	gps = rnp->gp_seq;
	mask = rnp->grpmask;
	raw_spin_unlock_rcu_node(rnp);
	raw_spin_lock_rcu_node(rnp_p);
	rcu_report_qs_rnp(mask, rnp_p, gps, flags);
}
```

**实现原理：** 退出任务必须在原债务节点锁下同时完成“删链、游标推进、所有权指针清空”。如果这是最后一个任务债务，`rcu_report_unblock_qs_rnp()` 才把先前被任务截断的节点传播接回树形汇聚主线。

## 3.9\_实现复核问题

1. 为什么未被抢占的 reader 不需要修改 `rcu_node`？
2. `rcu_note_context_switch()` 为什么必须先挂任务，再调用 `rcu_qs()`？
3. `rcu_blocked_node` 为什么不能在任务迁移后改为新 CPU 的叶节点？
4. `gp_tasks` 为什么是链表游标，而不是普通任务计数器？
5. `rcu_preempt_check_blocked_tasks()` 补齐了哪一种事件先后顺序？
6. 为什么 `qsmask==0` 仍不足以让抢占式 RCU 节点向上清位？
7. 最后一个 blocked reader 退出时，哪个函数把任务债务重新接回树形汇聚？

模块概念导读：[Linux 6.12 抢占式 Tree RCU 模块源码概念导读](../navigation/P03_Linux_6.12_抢占式_Tree_RCU_模块源码概念导读.md)。
