---
id: research.source_reading.waiting_notification.wait_header_implementation
title: "Linux 6.12 wait.h 结构与等待宏实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, waitqueue, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_wait.h结构与等待宏实现

## 1.1\_从条件表达式进入循环

调用者写下wait_event_interruptible，真正需要理解的是一次条件观察怎样连接到登记、调度和清理。本页只展开等待结构与宏编排，不复制wait.c的函数体。固定NXP linux-imx提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20，上游include/linux/wait.h，blob为2b322a9b88a2bd122d30e70a3d6eaa12c5cec244。中文Doxygen和行内注释均由仓库补充。

| 关联阅读 | 任务 |
| --- | --- |
| [总索引](../../../navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.1_版本边界与阅读任务) | 确认版本与模块 |
| [普通等待模块](../../../navigation/P02_Linux_6.12_普通等待队列模块源码概念导读.md#2.3_等待侧调用链) | 看宏与函数合作 |
| [wait.c实现](../../kernel/sched/wait.c.md#1.2_源码符号覆盖账本) | 查询prepare、wake和finish体 |
| [知识调用链](../../../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/P04_wait_event入队与唤醒调用链.md#4.2_等待侧状态落点) | 先理解S0～S7的职责 |

## 1.2\_队列头与等待项

```c
/**
 * @brief 仓库阅读说明：通知项与共享队列头，业务条件不在这些字段中。
 */
struct wait_queue_entry {
	unsigned int		flags;
	void			*private; /* 默认等待时关联任务，自定义回调可另用。 */
	wait_queue_func_t	func;
	struct list_head	entry;
};

struct wait_queue_head {
	spinlock_t		lock;
	struct list_head	head;
};
```

head保存共享链，lock保护这条通知关系；每个entry保存自己的链表节点、回调func、private对象与flags。wait_queue_func_t是返回int的函数指针类型，参数为等待项、状态mode、唤醒flags与事件key。默认条件等待由init_wait_entry设置private=current，poll等路径可设置不同回调与私有对象。WQ_FLAG_EXCLUSIVE是独占额度标志，不是业务锁；其他标志服务不同等待接口，本页不逐项展开无关路径。

对象应先初始化再发布，栈上entry在离开等待函数前必须脱离共享链。声明宏和初始化辅助宏只负责构造初始锁/链表，并不证明调用者重置时已无旧等待者。字段的写入/读取及状态所有权见[对象关系图](../../../navigation/P02_Linux_6.12_普通等待队列模块源码概念导读.md#2.2.1_队列头与栈上entry)。

## 1.3\_wait\_event宏循环与出口

```c
/**
 * @brief 仓库阅读说明：登记后重检，再决定调度或退出。
 * @param wq_head 队列对象。
 * @param condition 可反复求值的业务条件。
 * @param state 等待态。
 * @param exclusive 是否独占登记。
 * @param ret 初始返回状态。
 * @param cmd 每轮调度动作，可更新局部返回值。
 * @return 外层宏约定的正常或错误状态。
 */
#define ___wait_event(wq_head, condition, state, exclusive, ret, cmd)		\
({										\
	__label__ __out;							\
	struct wait_queue_entry __wq_entry;					\
	long __ret = ret;	/* 局部返回状态 */				\
										\
	init_wait_entry(&__wq_entry, exclusive ? WQ_FLAG_EXCLUSIVE : 0);	\
	for (;;) {								\
		long __int = prepare_to_wait_event(&wq_head, &__wq_entry, state);\
										\
		if (condition)							\
			break;							\
										\
		if (___wait_is_interruptible(state) && __int) {			\
			__ret = __int;						\
			goto __out;						\
		}								\
										\
		cmd;								\
	}									\
	finish_wait(&wq_head, &__wq_entry);					\
__out:	__ret;									\
})
```

wq_head参数是对象，内部取地址；condition会被反复求值，state选择等待状态，exclusive决定初始化标志，ret提供初始返回值，cmd是调度动作。局部标签__out和局部__ret使宏能表达循环及错误出口，而不借用调用者的标签或状态。

先由init_wait_entry建立局部项，循环中prepare处理登记或信号；condition先于信号错误判断。条件真经finish正常退出，条件假且可中断错误有效则直接去__out，摘链由prepare信号分支完成；其余执行cmd后再循环。超时变体可通过cmd更新__ret，并用条件包装共同决定停止，因此不能只从这一个底层宏推断所有外层接口的返回类型。

___wait_is_interruptible是控制信号错误出口的辅助宏：state为编译期常量时，检查TASK_INTERRUPTIBLE或TASK_WAKEKILL位；不是编译期常量时，辅助宏保守返回真，保留对prepare返回值的检查。实际状态是否允许信号打断，由prepare中的signal_pending_state结合state判断。辅助宏不检查业务条件，也不产生信号。具体prepare、finish和默认回调见[wait.c对应标题](../../kernel/sched/wait.c.md#1.3_prepare_to_wait_event登记与信号分支)。

## 1.4\_外层快查与超时边界

```c
/**
 * @brief 仓库阅读说明：可中断等待的快查入口。
 * @param wq_head 队列对象。
 * @param condition 快查及后续等待共同使用的条件。
 */
#define wait_event_interruptible(wq_head, condition)				\
({										\
	int __ret = 0;								\
	might_sleep();								\
	if (!(condition))							\
		__ret = __wait_event_interruptible(wq_head, condition);		\
	__ret;									\
})
```

外层先调用might_sleep检查可睡眠调用环境，然后快速求值condition；已经成立时直接返回0，否则才进入内部可中断等待。might_sleep是诊断检查，不是调度动作，检查未报警也不替调用者证明持锁和上下文一定合法。__wait_event_interruptible将TASK_INTERRUPTIBLE、非独占标志、初始0和schedule动作交给上节底层宏；这里不再复制机械参数转发宏。

```c
/**
 * @brief 仓库阅读说明：超时到点与条件成功同时发生时仍保留成功值1。
 * @param condition 本次只求值一次的业务条件。
 */
#define ___wait_cond_timeout(condition)						\
({										\
	bool __cond = (condition);						\
	if (__cond && !__ret)							\
		__ret = 1;							\
	__cond || !__ret;							\
})
```

超时条件包装只求值一次condition并保存到__cond；条件成立而剩余返回状态为0时，将__ret改为1，从而保留“成功至少1”的边界。条件假且预算为0时也结束循环，但返回0。可中断变体仍另有负错误码，业务调用者必须使用匹配的返回类型和分支。

## 1.5\_可修改性与验证边界

本实现簇的关系与时序复用[模块正常与信号出口图](../../../navigation/P02_Linux_6.12_普通等待队列模块源码概念导读.md#2.3.1_正常与信号出口时序)：本页编排快查、循环和出口，wait.c兑现队列与任务状态动作。不要把宏的初始快查移到“只查一次后直接睡眠”的自制协议，也不要把condition放到prepare之前替代登记后的重检。

修改任一外层参数、条件包装或错误出口时，应同时核对正常初始成立、登记后成立、信号退出和超时边界。需要实际内核配置与运行证据才能验证调度和上下文，本次只比对固定源码，未编译加载。通用自旋锁、链表和调度器的具体实现不在本页重复展开。
