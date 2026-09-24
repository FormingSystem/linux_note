---
id: research.source_reading.waiting_notification.linux_6_12_completion_implementation
title: "Linux 6.12 completion.c 令牌与等待源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, completion, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_completion.c令牌与等待源码实现

## 1.1\_实现讲解边界

本页接住[completion模块导读](../../../navigation/P03_Linux_6.12_completion模块源码概念导读.md#3.2_状态所有权)的S0～S6阶段，展开同一对象锁如何连接令牌发布、任务登记和成功消费。固定NXP linux-imx提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20，上游kernel/sched/completion.c，blob 3561ab533dd4e33ddb5284bcab51736f9b9ab6bf。中文Doxygen和注释由仓库补充，保留所选函数控制流，省去原英文长注释及导出语句。

[总阅读索引](../../../navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.4_模块与实现入口)负责模块选择；[头文件实现](../../include/linux/completion.h.md#1.2_completion对象与初始化)负责对象构造，不在本页重复结构体和init/reinit函数体。

## 1.2\_源码符号覆盖账本

先记住两个值：done为0时没有令牌，UINT_MAX是unsigned int的最大值，在这里表示持续完成。S编号引用模块阶段表，不是另设一组状态。后面的__sched是函数属性标记，保留上游声明，不改变本页的计数规则。

| 标题 | 状态变化与执行者 | 仍需调用者保证 |
| --- | --- | --- |
| [计数发布](#1.3_complete计数发布) | S3完成者锁内增加done并通知 | 业务结果先发布，合法对象地址 |
| [等待消费](#1.4_do_wait_for_common等待与消费) | S1/S2等待者登记、解锁调度；S4清理后消费 | 可睡眠上下文、对象有效 |
| [广播](#1.5_complete_all发布永久完成) | S5设置UINT_MAX并通知现有链 | 满足实时上下文约束，复用前收束旧轮 |
| [try与观察](#1.6_try与done观察) | try锁内取得；done只观察和同步锁区间 | 观察不预留令牌，不证明所有访问者退出 |

## 1.3\_complete计数发布

```c
/**
 * @brief 仓库阅读说明：锁内增加计数并按wake_flags通知一个任务。
 * @param x 当前有效的完成量对象。
 */
static void complete_with_flags(struct completion *x, int wake_flags)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&x->wait.lock, flags);

	if (x->done != UINT_MAX)
		x->done++;
	swake_up_locked(&x->wait, wake_flags);
	raw_spin_unlock_irqrestore(&x->wait.lock, flags);
}
```

```c
/**
 * @brief 仓库阅读说明：使用普通唤醒标志发布一次完成。
 * @param x 当前有效的完成量对象。
 */
void complete(struct completion *x)
{
	complete_with_flags(x, 0);
}
```

helper函数complete_with_flags把done变化和swait通知放在同一wait.lock临界区：未达到UINT_MAX才增加，有无等待者都保存计数，然后尝试通知一个已登记任务。普通complete传wake_flags=0；同CPU偏好接口传WF_CURRENT_CPU，仍使用相同令牌规则。本页不展开调度器如何落实这种偏好。

提前到达的完成者可能找不到等待者，但done留给后续S1检查，因此不会像裸通知那样消失。已经饱和时仍可通知，但不再增加计数。队列顺序不等于令牌已私分给某个任务：醒后仍在S4锁内检查和消费，另一个等待者或try路径可能先取得。完成量也不替调用者写入业务结果，更不替调用者持有请求引用。

## 1.4\_do\_wait\_for\_common等待与消费

先读取得锁的包装，再读可能放锁睡眠的核心。核心使用DECLARE_SWAITQUEUE声明并初始化栈上等待项；符合等待态的信号出现时，以-ERESTARTSYS这个内核错误结果退出，外层再按具体接口约定返回。

```c
/**
 * @brief 仓库阅读说明：在可睡眠环境取得锁并调用等待核心；action为调度函数，timeout为预算，state为等待态。
 * @param x 当前有效的完成量对象。
 */
static inline long __sched
__wait_for_common(struct completion *x,
		  long (*action)(long), long timeout, int state)
{
	might_sleep();

	complete_acquire(x);

	raw_spin_lock_irq(&x->wait.lock);
	timeout = do_wait_for_common(x, action, timeout, state);
	raw_spin_unlock_irq(&x->wait.lock);

	complete_release(x);

	return timeout;
}
```

包装函数先做might_sleep上下文诊断，再取得原始自旋锁调用内部等待函数。complete_acquire/release在本固定头文件是空内联函数，见[头文件说明](../../include/linux/completion.h.md#1.3_acquire与release名称不等于功能动作)，不能把名称当作硬件屏障证据。这里使用lock_irq/unlock_irq，和完成方保存恢复IRQ标志的irqsave版本不同；等待接口要求可睡眠且中断开启的调用环境。

```c
/**
 * @brief 仓库阅读说明：已持锁进入；action负责调度，timeout记录预算，state决定信号资格。
 * @param x 当前有效的完成量对象。
 */
static inline long __sched
do_wait_for_common(struct completion *x,
		   long (*action)(long), long timeout, int state)
{
	if (!x->done) {
		DECLARE_SWAITQUEUE(wait);

		do {
			if (signal_pending_state(state, current)) {
				timeout = -ERESTARTSYS;
				break;
			}
			__prepare_to_swait(&x->wait, &wait);
			__set_current_state(state);
			raw_spin_unlock_irq(&x->wait.lock);
			timeout = action(timeout);
			raw_spin_lock_irq(&x->wait.lock);
		} while (!x->done && timeout);
		__finish_swait(&x->wait, &wait);
		if (!x->done)
			return timeout;
	}
	if (x->done != UINT_MAX)
		x->done--;
	return timeout ?: 1;
}
```

action参数是schedule_timeout或io_schedule_timeout调度函数，timeout是剩余等待预算，state选择不可中断、可中断或killable等待态。DECLARE_SWAITQUEUE在需要登记的分支建立栈上任务项。S1先在锁内看done：已有令牌时跳过登记与调度，直接S4。没有令牌时检查信号，登记后设态，S2解锁才调用action；恢复执行后重取同一锁判断done和预算。

循环结束先__finish_swait清理，再判断是否取得令牌。若仍为0，返回超时0或信号负值；若非0，普通计数减一，UINT_MAX不变。timeout为0但锁内已经取得完成时，GNU省略中间操作数表达式timeout ?: 1返回1，保留成功与超时的区别。无超时可中断外层另把成功转换成0，超时外层保留正剩余量，因此应用必须按所选接口解读返回值。

这里没有停止完成者的语句。超时只结束本次等待；对象取消、工作同步退出和S6复用是外围协议。修改循环时应检查提前完成、登记后完成、信号退出、预算到点和令牌被其他等待者消费的交错，不能仅测一次正常睡眠。

## 1.5\_complete\_all发布永久完成

```c
/**
 * @brief 仓库阅读说明：设置持续完成并持锁广播。
 * @param x 当前有效的完成量对象。
 */
void complete_all(struct completion *x)
{
	unsigned long flags;

	lockdep_assert_RT_in_threaded_ctx();

	raw_spin_lock_irqsave(&x->wait.lock, flags);
	x->done = UINT_MAX;
	swake_up_all_locked(&x->wait);
	raw_spin_unlock_irqrestore(&x->wait.lock, flags);
}
```

S5在同一原始自旋锁下设置UINT_MAX，并调用专用于此路径的持锁广播函数。它不是普通swake_up_all的分段解锁实现。RT断言是检查而非保护机制，不能把无告警或不睡眠当成所有配置所有中断上下文都安全的证明。

持续完成指后续wait也不递减此值，不是对象可以永久复用或立刻释放。completion_done会持续观察到真，却不能证明所有已唤醒任务走完清理；S6仍须外围协议收束旧生产者和等待者。

## 1.6\_try与done观察

两个接口都以READ_ONCE访问宏读取done，防止这次标记读取被编译器任意合并或重取；这不等于取得令牌，也不是对象保活操作。关键区别在进入锁以后是否重检并更新。

```c
/**
 * @brief 仓库阅读说明：非阻塞尝试锁内消费，返回是否取得完成事实。
 * @param x 当前有效的完成量对象。
 */
bool try_wait_for_completion(struct completion *x)
{
	unsigned long flags;
	bool ret = true;

	
	if (!READ_ONCE(x->done))
		return false;

	raw_spin_lock_irqsave(&x->wait.lock, flags);
	if (!x->done)
		ret = false;
	else if (x->done != UINT_MAX)
		x->done--;
	raw_spin_unlock_irqrestore(&x->wait.lock, flags);
	return ret;
}
```

非阻塞尝试先用READ_ONCE做零值快速失败，有值时还必须锁内重检，因为另一个消费者可能已经取走。只有锁内仍有令牌才返回真，并消费普通计数；广播状态不减。它不会登记任务，也不会调度睡眠。READ_ONCE单独观察不预留计数，锁内检查与更新才完成取得。

```c
/**
 * @brief 仓库阅读说明：观察完成并经过锁区间，不消费或预留计数。
 * @param x 当前有效的完成量对象。
 */
bool completion_done(struct completion *x)
{
	unsigned long flags;

	if (!READ_ONCE(x->done))
		return false;

	
	raw_spin_lock_irqsave(&x->wait.lock, flags);
	raw_spin_unlock_irqrestore(&x->wait.lock, flags);
	return true;
}
```

观察函数同样先看done，但进入锁后不再重检或消费，只与正在使用这把锁的路径完成同步。因此初次读到正数到函数返回之间，其他等待者可能已消费它；返回真不能保证随后try成功。等待一次锁区间也不禁止未来访问，不证明所有完成者和等待者已经退出，更不能单独充当引用计数或join。

## 1.7\_对照阶段与验证范围

[模块阶段表及端到端时序](../../../navigation/P03_Linux_6.12_completion模块源码概念导读.md#3.2_状态所有权)用同一对象串起各函数。结构初始化承担S0；本页实现S1～S5；安全S6依赖外层。所选函数与固定源码静态比对，不声称已经编译加载或运行内核、实时配置、信号及超时竞态。知识侧令牌C模型验证顺序规则，不能替代这些运行证据。
