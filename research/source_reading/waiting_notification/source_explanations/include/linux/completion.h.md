---
id: research.source_reading.waiting_notification.completion_header_implementation
title: "Linux 6.12 completion.h 对象与初始化实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [completion, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_completion.h对象与初始化实现

## 1.1\_先建立可以共享的完成对象

完成量把完成事实保存在对象里，不能只为它取一个变量名就开始等待。本页沿S0初始化与S6复用解释结构和写入范围。固定NXP linux-imx提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20；上游include/linux/completion.h，blob fb291567657432083162031ddb949a7e581e2848。中文Doxygen与中文注释是仓库补充。

从[总索引](../../../navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.1_版本边界与阅读任务)进入[模块状态所有权](../../../navigation/P03_Linux_6.12_completion模块源码概念导读.md#3.2_状态所有权)，再用本页核对对象构造；通知和消费见[completion.c](../../kernel/sched/completion.c.md#1.2_源码符号覆盖账本)。

## 1.2\_completion对象与初始化

```c
/** @brief 仓库阅读说明：完成计数与简化任务等待队列。 */
struct completion {
	unsigned int done;
	struct swait_queue_head wait;
};
```

结构体completion包含unsigned int计数done和简化等待队列头wait。done为0表示没有可取得的完成事实，普通正数可以消费，UINT_MAX是持续通过状态；wait保存任务登记及保护复合操作的原始自旋锁。它没有请求编号、业务结果、取消位或引用计数。那些状态必须位于外围请求对象，不能从done反推。

```c
/**
 * @brief 仓库阅读说明：对象发布前建立初始计数和队列。
 * @param x 当前有效的完成量对象。
 */
static inline void init_completion(struct completion *x)
{
	x->done = 0;
	init_swait_queue_head(&x->wait);
}
```

S0同时建立done=0、队列锁和空链，然后调用者才可以把对象发布给其他任务。只写done=0而不初始化wait不是首次初始化。栈对象应使用DECLARE_COMPLETION_ONSTACK：CONFIG_LOCKDEP这一锁依赖检查配置开启时，它走运行时初始化；否则该宏退到静态式初始化。这一区别服务锁依赖记账，不延长栈寿命。宏的机械包装不在本页逐个展开。

```c
/**
 * @brief 仓库阅读说明：调用者证明旧轮退出后清零计数。
 * @param x 当前有效的完成量对象。
 */
static inline void reinit_completion(struct completion *x)
{
	x->done = 0;
}
```

S6只清零done，不取得wait.lock，不清空或等待旧链，也不取消旧生产者。调用者先证明上一轮所有可能访问者已经退出，才可以执行这一步。否则旧生产者晚到的complete会给新轮次增加令牌，旧等待者也可能错过广播事实。改成init_completion重新建立链同样不能解决，反而可能丢掉仍活动的登记。

## 1.3\_acquire与release名称不等于功能动作

```c
/** @brief 仓库阅读说明：本版本的两个空内联钩子。 */
static inline void complete_acquire(struct completion *x) {}
static inline void complete_release(struct completion *x) {}
```

固定版本中这两个内联函数为空。completion.c的等待包装调用它们，但实际互斥与顺序来自wait.lock等同步路径；不能仅按名称把这两个调用解释成CPU acquire/release指令，也不能由空函数反推出整个完成量无同步。其他版本或工具注入需独立取证。

## 1.4\_可修改性检查

修改字段或初始化流程时，先回答发布前谁独占对象、等待期间谁可能持有地址、新轮次怎样排除旧访问者。关系与S0～S6阶段见[模块图与阶段表](../../../navigation/P03_Linux_6.12_completion模块源码概念导读.md#3.2_状态所有权)。本页完成固定源码静态核对，未在内核中编译或执行栈初始化、锁依赖检查和复用竞态。
