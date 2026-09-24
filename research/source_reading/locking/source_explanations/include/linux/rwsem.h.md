---
id: research.source_reading.locking.rwsem_h
title: "rwsem.h 对象布局与观察边界"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [rwsem, configuration, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_rwsem.h对象布局与观察边界

## 1.1\_先确定正在解释哪个对象

[mutex 与 rwsem 模块导读](../../../navigation/P03_Linux_6.12_mutex与rwsem模块源码概念导读.md#3.4_rwsem完整调用链)把读份额、写独占和等待队列串成一条获取链。本篇先检查承载它们的对象，再读几个名字容易使人误判的观察函数。观察到“忙”与自己取得所有权，是两种不同证据。

上游位置 include/linux/rwsem.h；证据来自 NXP 官方 linux-imx 的固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，发布标签 lf-6.12.20-2.0.0、Linux 6.12.20，blob c8b543d428b0a8d4662183f3342e88ec61d10189。详见[源码基线](../../../../linux/SOURCE_BASELINE.md)。下面是原文件的完整类型或函数单元，中文 Doxygen 为仓库补充，英文注释保留。公共 down/up 函数体与初始化实现不在本头文件，本篇不冒充整份头文件的逐项展开。

## 1.2\_非RT对象的状态落点

先选择 !CONFIG_PREEMPT_RT 分支。count 是功能计账字，owner 保存写持有者或某位读持有者的提示及状态标志；多个读者并存时，后者不是所有读者身份的集合。wait_lock 保护慢路径队列及相关状态协调，wait_list 才保存具体等待记录。不能把 wait_lock 理解成保护临界区业务数据的那把读写锁。

```c
/** @brief 非 RT 读写信号量的完整对象布局（仓库补充阅读说明）。 */
struct rw_semaphore {
	atomic_long_t count;
	/*
	 * Write owner or one of the read owners as well flags regarding
	 * the current state of the rwsem. Can be used as a speculative
	 * check to see if the write owner is running on the cpu.
	 */
	atomic_long_t owner;
#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
	struct optimistic_spin_queue osq; /* spinner MCS lock */
#endif
	raw_spinlock_t wait_lock;
	struct list_head wait_list;
#ifdef CONFIG_DEBUG_RWSEMS
	void *magic;
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map	dep_map;
#endif
};
```

CONFIG_RWSEM_SPIN_ON_OWNER 启用时，osq 才存在。它是乐观自旋者的排队结构，不是睡眠等待者名单；睡眠请求在 wait_list 中。magic 用于调试身份检查，dep_map 用于锁依赖检查，两者按配置出现，不能拿某次 sizeof 结果当作跨配置固定 ABI。

上游把 count 和 owner 放在相邻位置，是因为无竞争获取通常只需触及这两个字段，让它们可能落在同一缓存行可以减少取数范围。相邻并不保证任意布局下都在同一行。竞争时，其他 CPU 的乐观等待者可能反复读取 owner；若外围对象的高频写字段也挤入同一缓存行，写入就可能使等待者缓存副本失效，随后读取又要取得该行。被拖入一致性往返的是整个缓存行，不只是被写的那个字段。因此上游建议嵌入 rwsem 时让外围热点字段离它远些；这是布局选择依据，不是已经测出的性能增益。

## 1.3\_RT分支换了状态载体

启用 CONFIG_PREEMPT_RT 后，同名类型的基础部分变成 rwbase。不能继续按上节的 count、owner 与 wait_list 偏移读这个对象，也不能把普通路径的 owner 位协议直接套进去。

```c
/** @brief RT 配置下由 rwbase 承载功能状态（仓库补充阅读说明）。 */
struct rw_semaphore {
	struct rwbase_rt	rwbase;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map	dep_map;
#endif
};
```

本篇只核对这个替代布局；rwbase 内部读者计数、等待与优先级处理要另沿 linux/rwbase_rt.h 阅读。当前工作配置为非 SMP、PREEMPT_NONE、TINY_RCU，未启用 RT 或 rwsem owner 自旋，不能把以下静态分支比较说成两种配置的运行测试。

## 1.4\_is\_locked与is\_contended没有替你加锁

下面两个函数只展开非 RT 分支。RWSEM_UNLOCKED_VALUE 是 0UL。先预测：若 count 暂时只剩等待协议标志而没有读份额或写占有，is_locked 会怎样回答？

```c
/** @brief 观察 count 是否偏离完全空闲编码（仓库补充阅读说明）。 */
static inline int rwsem_is_locked(struct rw_semaphore *sem)
{
	return atomic_long_read(&sem->count) != RWSEM_UNLOCKED_VALUE;
}
```
```c
/** @brief 以等待队列非空作为竞争启发式（仓库补充阅读说明）。 */
static inline int rwsem_is_contended(struct rw_semaphore *sem)
{
	return !list_empty(&sem->wait_list);
}
```

is_locked 检查整个 count 是否非零，没有单独计算活跃持有者数量。因此前面的问题答案是“非零”：它不能证明当前任务持锁，甚至不能仅凭名字把所有非零值理解为某个任务正在执行临界区。一次读取以后状态也可能马上变化。若调用者想访问业务数据，仍须走 down_read 或 down_write，不能先 if (!rwsem_is_locked(...)) 再无锁访问。

is_contended 的上游契约更窄：供已经持有 rwsem 的调用者使用，作为是否有人等待的启发式提示。函数不获取 wait_lock，也不预留后续队列状态。它返回后，新等待者可以加入，旧等待者也可能离开。这个结果适合辅助策略，不适合证明“绝无竞争者，所以可以销毁对象”。

## 1.5\_断言能证明到哪一层

没有 Lockdep 时，断言只检查共享功能字。RWSEM_WRITER_LOCKED 是 count 中的最低写占有位。下面分别观察“不是完全空闲”与“有写占有位”，都没有比较 current 的身份。

```c
/** @brief 在完全空闲编码上发出告警（仓库补充阅读说明）。 */
static inline void rwsem_assert_held_nolockdep(const struct rw_semaphore *sem)
{
	WARN_ON(atomic_long_read(&sem->count) == RWSEM_UNLOCKED_VALUE);
}
```
```c
/** @brief 在没有写占有位时发出告警（仓库补充阅读说明）。 */
static inline void rwsem_assert_held_write_nolockdep(const struct rw_semaphore *sem)
{
	WARN_ON(!(atomic_long_read(&sem->count) & RWSEM_WRITER_LOCKED));
}
```

假设 A 持有写锁，B 错误调用一个要求自己持写锁的辅助函数。非 Lockdep 写断言看到写位为 1，并不会因此识别“真正持有者是 A”。这不是给 B 的访问授权，只是说明这一降级检查能力有限。WARN_ON 也不是一个获取操作，不能修复错误调用。

公共包装选择检查器路径还是上述降级路径：

```c
/** @brief 按配置选择持锁断言（仓库补充阅读说明）。 */
static inline void rwsem_assert_held(const struct rw_semaphore *sem)
{
	if (IS_ENABLED(CONFIG_LOCKDEP))
		lockdep_assert_held(sem);
	else
		rwsem_assert_held_nolockdep(sem);
}
```
```c
/** @brief 按配置选择写持锁断言（仓库补充阅读说明）。 */
static inline void rwsem_assert_held_write(const struct rw_semaphore *sem)
{
	if (IS_ENABLED(CONFIG_LOCKDEP))
		lockdep_assert_held_write(sem);
	else
		rwsem_assert_held_write_nolockdep(sem);
}
```

CONFIG_LOCKDEP 启用时，包装转入 Lockdep 的持锁检查，而不是继续用 count 的非零状态代替当前任务的持锁记录。检查器自身必须有效、相关操作必须正确登记，“没告警”才有有限意义。这里不展开 Lockdep 宏体；RT 的降级函数也有不同实现，不能把上面的非 RT 函数体当作公共包装在全部配置中的最终目标。

## 1.6\_用三个反例复核理解

1. count 非零、当前任务尚未执行 down_read：能否凭 is_locked 返回真读取数据？指出缺少的是哪个动作。
2. A 持写锁，B 调用非 Lockdep 写断言：为什么可能不告警？若要求验证 B 自己的持锁前提，应区分哪两类状态？
3. 把一个每次采样都写入的统计字段紧贴 rwsem：为何可能影响远端自旋者？若没有启用 owner 自旋，这条特定因果链还能照搬吗？

对照要点：第 1 题缺少成功取得与相应顺序保证；第 2 题共享写位不是当前任务的持锁记录；第 3 题应追踪缓存行失效与重取，未启用该自旋分支时不能继续声称发生了同一类等待者读写往返。实际布局效果仍需要对应负载测量。

下一步进入[rwsem 慢路径中的等待与授予](../../kernel/locking/rwsem.c.md#1.3_等待记录与登记删除)，观察队列中的每个请求怎样变成 count 中的份额。

总索引：[锁源码总阅读索引](../../../navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.6_建议阅读顺序)。
