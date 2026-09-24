---
id: research.source_reading.waiting_notification.linux_6_12_wait_implementation
title: "Linux 6.12 wait.c 入队与唤醒源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, waitqueue, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_wait.c入队与唤醒源码实现

## 1.1\_实现讲解边界

本页沿一次普通可中断条件等待，解释栈上等待项怎样进入共享队列、信号怎样撤销登记、唤醒怎样按回调结果推进、退出怎样保护栈寿命。固定NXP linux-imx提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20；上游路径kernel/sched/wait.c，blob为51e38f5f47018c953e31c834dc6385c182359359。正文中的中文Doxygen与行内注释均为仓库补充，不是上游注释译文。

| 关联阅读 | 当前任务 |
| --- | --- |
| [总索引](../../../navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.1_版本边界与阅读任务) | 定位两类等待机制 |
| [普通等待模块](../../../navigation/P02_Linux_6.12_普通等待队列模块源码概念导读.md#2.3_等待侧调用链) | 看函数怎样协作 |
| [wait.h结构与宏](../../include/linux/wait.h.md#1.3_wait_event宏循环与出口) | 看S0～S4和S7出口由谁编排 |
| [四窗口正文](../../../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/P03_条件等待的统一状态机.md#3.5_逐个关闭检查睡眠窗口) | 检查机制前提，不把源代码当成业务资源协议 |

## 1.2\_源码符号覆盖账本

| 标题 | 初始化与写读关系 | 生命期及边界 |
| --- | --- | --- |
| [prepare](#1.3_prepare_to_wait_event登记与信号分支) | 在队列锁下处理entry链和任务等待态；随后宏读condition | 信号退出先摘链，条件成功仍须走正确出口 |
| [wake扫描](#1.4_wake_up_common按回调与exclusive额度扫描) | 调用func，把返回值和flags汇入独占额度 | 只在持队列锁时运行；不读设备业务条件 |
| [finish](#1.5_finish_wait恢复任务并移除栈上entry) | 恢复TASK_RUNNING，必要时锁内摘链 | 栈项不能在仍可被共享队列访问时失效 |
| [初始化与自动移除](#1.6_初始化与默认自动摘链) | init_wait_entry设private/func；成功回调移除entry | 解释为什么再次prepare可能需要重新入队 |

以下保留所选函数的控制流，省去导出声明、相邻接口和原英文长注释。链表和自旋锁内部算法属于其各自框架，本页使用其同步契约，不在此重复展开。默认任务唤醒继续进入调度器，不在本页证明处理器间中断（IPI，Inter-Processor Interrupt）或调度策略。

## 1.3\_prepare\_to\_wait\_event登记与信号分支

等待项的WQ_FLAG_EXCLUSIVE标志选择独占额度登记：它影响加入位置和后续扫描计数，不是业务资源所有权。准备函数先在队列锁下判断信号，再选择摘链退出或登记设态；先沿这两个分支阅读。

```c
/**
 * @brief 仓库阅读说明：队列锁下登记当前任务，或撤销信号退出者。
 * @param wq_head 与生产者共享的队列头。
 * @param wq_entry 本次等待项。
 * @param state 本次等待任务状态。
 * @return 0表示可继续重检，-ERESTARTSYS交给宏结合条件判断。
 */
long prepare_to_wait_event(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry, int state)
{
	unsigned long flags;
	long ret = 0;

	spin_lock_irqsave(&wq_head->lock, flags);
	if (signal_pending_state(state, current)) {

		list_del_init(&wq_entry->entry); /* 信号退出先从共享队列摘除。 */
		ret = -ERESTARTSYS;
	} else {
		if (list_empty(&wq_entry->entry)) {
			if (wq_entry->flags & WQ_FLAG_EXCLUSIVE)
				__add_wait_queue_entry_tail(wq_head, wq_entry);
			else
				__add_wait_queue(wq_head, wq_entry);
		}
		set_current_state(state);
	}
	spin_unlock_irqrestore(&wq_head->lock, flags);

	return ret;
}
```

进入函数时，当前等待项可能尚未入队，也可能在前一轮唤醒中已被自动摘除。无待处理信号时，list_empty决定是否重新加入，独占项进入尾部，普通项使用普通加入路径；随后set_current_state发布等待状态。队列锁使这些动作与同队列的wake扫描串行。

信号分支在同一锁下摘链并返回-ERESTARTSYS，防止后来的独占通知再把额度花在已选择错误退出的项上。但宏仍先检查condition：若条件已成立，可以正常完成等待；不能只凭函数返回负值就跳过其调用者的控制流。业务条件本身不由wq_head.lock保护，仍需业务锁或明确的无锁协议。

修改时必须同时检查宏的condition优先、信号错误出口和独占回调计数。把信号检测搬到锁外，或只返回错误不摘链，都会改变与并发通知的配对关系；测试应覆盖信号先到、通知先到和两者紧邻，而不是只测正常睡眠。

## 1.4\_wake\_up\_common按回调与exclusive额度扫描

```c
/**
 * @brief 仓库阅读说明：已持队列锁时扫描回调并计算独占额度。
 * @param wq_head 共享队列头。
 * @param mode 传给回调的任务状态匹配模式。
 * @param nr_exclusive 独占成功额度，0采用不限额度停止的约定。
 * @param wake_flags 传递给唤醒路径的标志。
 * @param key 由具体回调解释的事件键。
 * @return 扫描后的剩余额度；不是业务资源消费数。
 */
static int __wake_up_common(struct wait_queue_head *wq_head, unsigned int mode,
			int nr_exclusive, int wake_flags, void *key)
{
	wait_queue_entry_t *curr, *next;

	lockdep_assert_held(&wq_head->lock);

	curr = list_first_entry(&wq_head->head, wait_queue_entry_t, entry);

	if (&curr->entry == &wq_head->head)
		return nr_exclusive;

	list_for_each_entry_safe_from(curr, next, &wq_head->head, entry) {
		unsigned flags = curr->flags; /* 回调之前保存标志。 */
		int ret;

		ret = curr->func(curr, mode, wake_flags, key);
		if (ret < 0)
			break;
		if (ret && (flags & WQ_FLAG_EXCLUSIVE) && !--nr_exclusive)
			break;
	}

	return nr_exclusive;
}
```

锁断言核对调用者已经持有队列锁，空链快速返回保留原额度。safe_from遍历允许回调移除当前项；flags在回调前保存，随后用回调结果和这份标志决定额度变化。ret为负终止，ret为0继续且不扣额度，正值且独占才扣额度并可能结束扫描。额度耗尽后，后面的非独占项不保证被访问。

mode是任务状态匹配信息，wake_flags是调度唤醒标志，key交给回调解释，例如poll的事件键。核心不统一解释key，更不会为消费者取得设备锁或取走记录。外层__wake_up_common_lock负责irqsave加锁/解锁，并由初始额度与剩余额度计算返回统计；本段只展示实际扫描，不把接口返回统计和业务消费数混为一谈。

lockdep_assert_held是诊断，不是加锁动作。关闭相应检查不会消除持锁契约；其查询机制见[Lockdep查询与断言](../../../../lockdep/source_explanations/P04_Linux_6.12_Lockdep查询注解与配置源码实现.md#4.3_lockdep_assert系列断言展开)。本固定版本没有bookmark分段；改变长队列扫描策略时需同时论证回调摘链、并发增删和锁区间，不可只加一个循环阈值。

## 1.5\_finish\_wait恢复任务并移除栈上entry

```c
/**
 * @brief 仓库阅读说明：正常退出恢复运行态，并与并发摘链同步。
 * @param wq_head 该等待项登记的共享队列。
 * @param wq_entry 即将离开作用域的等待项。
 */
void finish_wait(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry)
{
	unsigned long flags;

	__set_current_state(TASK_RUNNING); /* 退出等待态。 */

	if (!list_empty_careful(&wq_entry->entry)) {
		spin_lock_irqsave(&wq_head->lock, flags);
		list_del_init(&wq_entry->entry);
		spin_unlock_irqrestore(&wq_head->lock, flags);
	}
}
```

先恢复TASK_RUNNING，再观察entry是否已被其他路径摘除。list_empty_careful不是任意无锁遍历许可：它与其他修改者持队列锁及相应摘链操作共同使用。若仍非空，finish取得同一队列锁执行list_del_init，和并发wake扫描完成同步。

它处理正常条件出口留下的项；信号出口的摘链由prepare负责。调用者不能在任一出口仍留下可被共享队列找到的栈项，也不能因为“成功唤醒通常会自动移除”便删除正常退出清理。可修改性检查要覆盖未实际睡眠、被唤醒后条件仍假、正常退出和信号退出四条路径。

## 1.6\_初始化与默认自动摘链

初始化要建立三个关系：private指向当前任务，func指向默认回调，entry通过INIT_LIST_HEAD链表初始化宏成为空链。此时还没有把局部项发布到共享队列。

```c
/**
 * @brief 仓库阅读说明：建立当前任务关联与默认自动摘链回调。
 * @param wq_entry 尚未发布到共享队列的局部项。
 * @param flags 本次等待标志。
 */
void init_wait_entry(struct wait_queue_entry *wq_entry, int flags)
{
	wq_entry->flags = flags;
	wq_entry->private = current; /* 默认关联当前任务。 */
	wq_entry->func = autoremove_wake_function;
	INIT_LIST_HEAD(&wq_entry->entry);
}
```

这里初始化的是一次等待的entry，不是设备的队列头。private把局部项关联到当前任务，func选择成功后自动摘链的默认回调，INIT_LIST_HEAD建立尚未登记的空链状态。队列头应在对象发布之前通过其初始化接口建立锁与链表，不能对仍服务等待者的共享头反复初始化。

```c
/**
 * @brief 仓库阅读说明：默认任务唤醒成功以后自动移除当前项。
 * @param wq_entry 当前回调所属等待项。
 * @param mode 任务状态模式。
 * @param sync 传递给默认唤醒函数的标志参数。
 * @param key 回调的事件键。
 * @return 默认任务唤醒的结果。
 */
int autoremove_wake_function(struct wait_queue_entry *wq_entry, unsigned mode, int sync, void *key)
{
	int ret = default_wake_function(wq_entry, mode, sync, key);

	if (ret) /* 只有成功唤醒才自动摘链。 */
		list_del_init_careful(&wq_entry->entry);

	return ret;
}
```

default_wake_function把entry关联的任务交给调度唤醒逻辑；本函数只在返回成功时用list_del_init_careful摘链。它与finish侧的careful检查共同解释“已经唤醒但尚未最终退出”的中间状态。自定义回调可能采用不同协议，不能把自动摘链假设推广到所有wait_queue_entry。

## 1.7\_把函数放回一条状态链

对象关系使用[队列头与栈上entry图](../../../navigation/P02_Linux_6.12_普通等待队列模块源码概念导读.md#2.2.1_队列头与栈上entry)，端到端交错见[正常与信号出口时序](../../../navigation/P02_Linux_6.12_普通等待队列模块源码概念导读.md#2.3.1_正常与信号出口时序)。prepare承担登记/信号箭头，默认回调承担成功通知后的摘链箭头，finish承担正常出口箭头。宏把这些箭头接起来，任何一个函数都不单独拥有整个等待协议。

本页是固定源码静态解释，未编译或运行真实等待路径。知识侧C模型可检查动作顺序与扫描额度，不能证明调度器、弱内存、真实信号或设备退出。复核时应能预测：把flags保存移到回调之后有什么生命期风险；跳过finish会留下哪条共享引用；条件观察不加业务同步为何不由队列锁补救。
