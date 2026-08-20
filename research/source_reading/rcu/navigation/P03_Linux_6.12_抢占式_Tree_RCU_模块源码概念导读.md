---
id: research.source_reading.rcu.linux_6_12_preempt_tree
title: "Linux 6.12 抢占式 Tree RCU 模块源码概念导读"
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
source_project: linux
source_version: "6.12.20"
---

# 第3章\_Linux\_6.12\_抢占式\_Tree\_RCU\_模块源码概念导读

## 3.1\_取证问题

本章逐段验证这个具体交错：

```text
R-old在CPU1取得old_obj
    → 写者把入口换成new_obj并请求GP=N
    → R-old在读侧内被抢占
    → CPU1报告QS
    → R-old迁移到CPU2并继续使用old_obj
    → R-old最外层unlock
    → GP=N才允许完成
```

抽象证明见[抢占式 Tree RCU 的问题与任务跟踪模型](../../../../knowledge/linux/synchronization/rcu/P07_抢占式_Tree_RCU_问题与任务跟踪模型.md)，稳定机制正文见[抢占式 Tree RCU 源码同步机制](../../../../knowledge/linux/synchronization/rcu/P08_抢占式_Tree_RCU_源码同步机制.md)。本章只保存 Linux 6.12.20 的函数、字段、锁和分支证据。

## 3.2\_任务状态的准确位置

`include/linux/sched.h:894-899` 在 `CONFIG_PREEMPT_RCU` 下把 `rcu_read_lock_nesting`、`rcu_read_unlock_special`、`rcu_node_entry` 和 `rcu_blocked_node` 放入每个 `task_struct`。字段定义、读写者和中文注释见 [任务与节点的共享状态实现](../source_explanations/P03_Linux_6.12_抢占式_Tree_RCU_关键函数源码实现.md#3.2_任务与节点的共享状态实现)。

这是任务可以跨 CPU 保存旧读侧状态的物理载体。它们不是 `rcu_data` 字段，因为 `rcu_data` 随 CPU 固定，任务可能迁移。

叶节点共享记录位于 [`kernel/rcu/tree.h`](../../linux/kernel/rcu/tree.h) 的 `struct rcu_node`，由 `blkd_tasks`、`gp_tasks`、`exp_tasks` 和 `boost_tasks` 表达任务集合及不同等待边界；具体字段摘录同样集中在 [任务与节点的共享状态实现](../source_explanations/P03_Linux_6.12_抢占式_Tree_RCU_关键函数源码实现.md#3.2_任务与节点的共享状态实现)。

访问这些链表和游标的关键路径持有 `rnp->lock`，并在需要时关闭本地中断，防止同 CPU 调度/RCU 路径重入破坏状态。

## 3.3\_读侧进入\_没有节点登记

[`kernel/rcu/tree_plugin.h:411-445`](../../linux/kernel/rcu/tree_plugin.h) 实现抢占分支：

```text
__rcu_read_lock()
    → rcu_preempt_read_enter()
    → current->rcu_read_lock_nesting++
    → barrier()

__rcu_read_unlock()
    → barrier()
    → current->rcu_read_lock_nesting--
    → 只有最外层且special非零时调用rcu_read_unlock_special()
```

所以快路径的通信范围首先只到当前 `task_struct`。节点链表登记是读侧内真正发生 context switch 后才支付的慢路径成本。

## 3.4\_调度钩子\_先转移债务再报告CPU\_QS

`kernel/sched/core.c:6615::__schedule()` 调用 `rcu_note_context_switch(preempt)`。抢占实现先把读侧债务从当前 CPU 转移到任务和原叶节点，然后才允许 `rcu_qs()` 清本 CPU 位；逐句实现见 [`rcu_note_context_switch()` 转移读侧债务](../source_explanations/P03_Linux_6.12_抢占式_Tree_RCU_关键函数源码实现.md#3.4_rcu_note_context_switch转移读侧债务)。

顺序提供的证明是：

```text
任务旧读侧仍存在
    → 先把task_struct挂入原叶节点共享状态
    → 共享状态可继续阻塞GP
    → 原CPU才清本地QS债务
```

调度入口还检查 `!preempt && rcu_preempt_depth() > 0` 并警告主动 context switch，证明 PREEMPT_RCU 的常规契约是处理被动抢占，不是允许普通读侧任意阻塞。

## 3.5\_入队决策\_一个链表怎样保存多条GP边界

[`tree_plugin.h:128-278`](../../linux/kernel/rcu/tree_plugin.h) 的 `rcu_preempt_ctxt_queue()` 先组合 `gp_tasks`、`exp_tasks`、`qsmask` 和 `expmask` 四类状态。决策表、链表插入位置与游标更新的实现见 [`rcu_preempt_ctxt_queue()` 建立任务等待边界](../source_explanations/P03_Linux_6.12_抢占式_Tree_RCU_关键函数源码实现.md#3.5_rcu_preempt_ctxt_queue建立任务等待边界)。

它回答四个不同问题：

1. 普通 GP 是否已经等待某个链表边界；
2. 加速 GP 是否已经等待某个边界；
3. 当前 CPU 是否仍欠普通 GP 的位；
4. 当前 CPU 是否仍欠加速 GP 的位。

根据组合结果，任务被插到链表头、尾、`gp_tasks` 后或 `exp_tasks` 后。若它是第一项阻塞当前普通 GP 的任务，实现会让 `gp_tasks` 指向该任务的 `rcu_node_entry`。

源码注释明确说这是一种保守近似，可能让普通 GP 多等，但必须避免漏等。也正因为存在游标，不能把 `blkd_tasks` 的每个成员都无条件解释成当前普通 GP 的旧 reader。

## 3.6\_GP开始以前已被抢占的任务怎样纳入

任务可能在没有普通 GP 时就进入 `blkd_tasks`。`kernel/rcu/tree.c:1920-1928::rcu_gp_init()` 遍历节点时，在设置新 `qsmask` 和节点 `gp_seq` 前调用 `rcu_preempt_check_blocked_tasks()`。公共 [`rcu_gp_init()` 控制实现](../source_explanations/P05_Linux_6.12_Tree_RCU_GP全局生命周期源码实现.md#5.9_rcu_gp_init开始代际并建立证明债务)与抢占配置的 [`rcu_preempt_check_blocked_tasks()` 任务接管实现](../source_explanations/P03_Linux_6.12_抢占式_Tree_RCU_关键函数源码实现.md#3.6_rcu_preempt_check_blocked_tasks接管旧任务)分别由两个唯一标题负责。

[`tree_plugin.h:704-721`](../../linux/kernel/rcu/tree_plugin.h) 的抢占分支若发现已有 blocked tasks，并且节点属于本轮需要跟踪的在线/离线边界，就让 `gp_tasks` 指向旧任务边界。

因此两种到达顺序都有闭环：

```text
GP先开始、任务后被抢占
    → rcu_preempt_ctxt_queue()按当前qsmask建立gp_tasks

任务先被抢占、GP后开始
    → rcu_preempt_check_blocked_tasks()在GP初始化接管旧任务
```

## 3.7\_CPU报告为何停在叶节点

任务入队以后，`rcu_note_context_switch()` 调用 `rcu_qs()`，清 `rdp->cpu_no_qs.b.norm`。每 CPU `rcu_core()` 以后通过 `rcu_report_qs_rdp()` 报告到叶节点。

[`kernel/rcu/tree.c:2289-2344`](../../linux/kernel/rcu/tree.c) 的 `rcu_report_qs_rnp()` 清当前位后，同时检查 `qsmask` 和 `rcu_preempt_blocked_readers_cgp()`。只要 CPU 债务或当前 GP 的任务债务仍有一项存在，本节点就停止向父节点传播。实现见 [节点汇聚同时等待 CPU 与任务](../source_explanations/P03_Linux_6.12_抢占式_Tree_RCU_关键函数源码实现.md#3.7_节点汇聚同时等待CPU与任务)。

于是 CPU1 的位可以清零，但叶节点代表自身的父级位仍保持一。任务债务没有伪装成 CPU1 位，而是截断该叶节点继续向上汇聚的动作。

## 3.8\_任务迁移为何不会丢记录

调度入队时把原叶节点保存到 `t->rcu_blocked_node`，这个写入点位于 [`rcu_note_context_switch()` 转移读侧债务](../source_explanations/P03_Linux_6.12_抢占式_Tree_RCU_关键函数源码实现.md#3.4_rcu_note_context_switch转移读侧债务)。

任务后来即使在 CPU2 恢复，特殊退出仍读取 `t->rcu_blocked_node` 并锁住原节点。若错误地使用 `this_cpu_ptr(&rcu_data)->mynode`，就会去 CPU2 的叶节点寻找一条实际挂在 CPU1 叶节点的链表项。

这个字段把状态归属说得很清楚：

```text
任务的运行CPU可以变化
任务的blocked记录在退出前仍归最初叶节点所有
```

## 3.9\_最外层unlock与延迟特殊处理

`__rcu_read_unlock()` 只有在 nesting 变为零且 `rcu_read_unlock_special.s != 0` 时调用 `rcu_read_unlock_special()`。

[`tree_plugin.h:637-692`](../../linux/kernel/rcu/tree_plugin.h) 先判断当前是否仍处在不适合完整清理的 NMI、关中断、禁 BH 或禁抢占上下文。需要延迟时，它可能：

- raise `RCU_SOFTIRQ`；
- 设置 `TIF_NEED_RESCHED` / preempt need-resched；
- 必要时向本 CPU 排 `irq_work`，让调度器重新评价。

安全后进入 [`tree_plugin.h:477-586`](../../linux/kernel/rcu/tree_plugin.h) 的 `rcu_preempt_deferred_qs_irqrestore()`。这说明最外层 unlock 的“通知”不是固定的一次函数直达树根，而是依赖当前执行上下文选择立即共享清理或延迟交付。

## 3.10\_删除任务并推进gp\_tasks

特殊退出在原叶节点锁下执行：

```text
rnp = t->rcu_blocked_node
    → 记录node_entry的next
    → list_del_init(node_entry)
    → t->rcu_blocked_node = NULL
    → 若gp_tasks正指向本任务则推进到next
    → 同理处理exp_tasks/boost_tasks
```

删除前的 `smp_mb()` 用于使 expedited fast path 观察读侧结束。它是这条特殊算法中的顺序约束，不应被扩写成“`rcu_read_unlock()` 总会执行一个全局硬件屏障”。

若删除前节点有普通 GP 阻塞者，删除后 `gp_tasks` 变空，并且 `qsmask==0`，路径调用 `rcu_report_unblock_qs_rnp()` 恢复向父节点传播。链表删除、游标推进和恢复上报的实现见 [最外层退出删除任务并恢复传播](../source_explanations/P03_Linux_6.12_抢占式_Tree_RCU_关键函数源码实现.md#3.8_最外层退出删除任务并恢复传播)。

## 3.11\_任务清债怎样恢复树形传播

[`kernel/rcu/tree.c:2354`](../../linux/kernel/rcu/tree.c) 的 `rcu_report_unblock_qs_rnp()` 首先确认：

```text
启用了PREEMPT_RCU
该节点不再有当前GP blocked reader
该节点qsmask已经为零
```

然后取叶节点的 `grpmask`，进入父节点清位流程。它与 CPU QS 路径的区别是触发源：

```text
CPU最后清债
    → rcu_report_qs_rnp()

任务最后清债且CPU债务早已清空
    → rcu_report_unblock_qs_rnp()
```

两条路径最终都把一个叶节点的完整证明向根汇聚。

## 3.12\_GP完成处的双重检查

`tree.c:1969` 的 FQS 唤醒检查要求根 `qsmask` 为空且没有当前 GP blocked reader。`tree.c:2052-2060` 的循环退出也检查同一逻辑；源码注释说明多层树可依靠根 `qsmask` 汇聚叶任务债务，而单节点树需要显式检查根/叶的 `gp_tasks`。

`tree.c:2142-2144::rcu_gp_cleanup()` 在推进每个节点的完成代际前还用警告验证；完整公共 cleanup 实现见 [`rcu_gp_cleanup()` 发布完成并承接下一代](../source_explanations/P05_Linux_6.12_Tree_RCU_GP全局生命周期源码实现.md#5.11_rcu_gp_cleanup发布完成并承接下一代)：

```text
当前GP blocked reader必须为空
qsmask必须为零
```

这不是超时分支。FQS 和 stall 只催促、诊断或 boost，不能绕过这些完成条件。

## 3.13\_端到端字段变化表

| 时刻 | `R-old.nesting` | `R-old.blocked` | `R-old.blocked_node` | 叶 `qsmask` CPU1 位 | 叶 `gp_tasks` | 结论 |
| --- | ---: | ---: | --- | ---: | --- | --- |
| 读者进入 | 1 | 0 | `NULL` | 取决于 GP 是否开始 | `NULL` 或既有边界 | 旧引用仅在运行现场 |
| GP=N开始 | 1 | 0 | `NULL` | 1 | 取决于既有 blocked tasks | CPU1 欠证据 |
| 抢占登记完成 | 1 | 1 | 原 CPU1 叶节点 | 仍可为 1 | 覆盖 R-old | 任务债务已共享 |
| CPU QS 上报 | 1 | 1 | 原 CPU1 叶节点 | 0 | 仍覆盖 R-old | 叶节点不能向上完成 |
| CPU2 恢复 | 1 | 1 | 仍是原 CPU1 叶节点 | 0 | 仍覆盖 R-old | 仍可安全使用 old_obj |
| 最外层 unlock | 0 | 清零 | `NULL` | 0 | 推进或 `NULL` | 若为最后债务则恢复上报 |
| GP cleanup | 0 | 0 | `NULL` | 0 | `NULL` | 写者才可释放 old_obj |

## 3.14\_trace与动态验证入口

源码中与本交错直接对应的 tracepoint 是：

| tracepoint | 调用位置 | 可观察事件 |
| --- | --- | --- |
| `rcu_preempt_task` | `rcu_note_context_switch()` 入队后 | 哪个 PID 在读侧内被抢占、关联哪个 GP |
| `rcu_unlock_preempted_task` | 特殊退出删除任务，或 GP 初始化建立等待边界 | blocked task 退出/游标变化 |
| `rcu_quiescent_state_report` | CPU或任务债务向节点报告 | mask 清除前后与 `gp_tasks` 是否存在 |
| `rcu_grace_period` | GP 生命周期各阶段 | GP 开始、等待、FQS、结束 |

可复现模块、CPU 绑定与 trace 命令见[晚到读者与抢占读者的对象回收实验](../../../../labs/kernel/rcu/P01_晚到读者与抢占读者/README.md)。实验必须在启用 PREEMPT_RCU 的多 CPU 内核上运行，不能用当前源码树中的非抢占分支静态结论替代运行配置检查。

## 3.15\_Linux\_5.10对照边界

Linux 5.10 已有相同的核心字段与债务转移设计，所以下列结论跨版本成立：

```text
任务nesting先保持局部
    → 读侧内被抢占才挂到叶blkd_tasks
    → gp_tasks区分当前普通GP等待边界
    → CPU QS和任务退出分别清两类债务
```

但 `rcu_read_unlock_special()` 的延迟触发、expedited/boost 辅助逻辑以及 EQS 状态位置会随版本调整。对 5.10 做逐行分析时，应重新从该版本 `tree_plugin.h` 与 `tree.c` 取证，不能把 6.12 的 softirq/irq_work 条件或 context-tracking 路径逐字套回去。

## 3.16\_复核问题

1. 为什么 `rcu_read_lock_nesting` 放在 `task_struct` 而不是 `rcu_data`？
2. 为什么必须在 `rcu_qs()` 以前完成 `rcu_preempt_ctxt_queue()`？
3. `blkd_tasks` 与 `gp_tasks` 的集合语义有什么区别？
4. GP 开始前、GP 开始后才被抢占的任务分别由哪里接入当前代际？
5. CPU1 的 `qsmask` 位清零后，什么条件仍阻止叶节点清父节点位？
6. 任务迁移到 CPU2 后，哪个字段把退出路径带回原叶节点？
7. 为什么最外层 unlock 可能只安排一次延迟处理，而不立即锁节点？
8. CPU 最后清债和任务最后清债分别通过哪个上报函数恢复传播？
9. FQS、stall 和 boost 为什么不能成为“超时后忽略旧读者”的出口？

上一篇：[Linux 6.12 非抢占式 Tree RCU 模块源码概念导读](P02_Linux_6.12_非抢占式_Tree_RCU_模块源码概念导读.md)。

下一篇：[Linux 6.12 Tasks RCU 与 Tiny RCU 模块源码概念导读](P04_Linux_6.12_Tasks_RCU与Tiny_RCU模块源码概念导读.md)。

阅读索引：[Linux 6.12 RCU 源码总阅读索引](P01_Linux_6.12_RCU源码总阅读索引.md#1.9_建议的源码阅读顺序)。
