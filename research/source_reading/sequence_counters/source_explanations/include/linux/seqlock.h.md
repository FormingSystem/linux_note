---
id: research.source_reading.sequence_counters.linux_6_12_seqlock_header_implementation
title: "Linux 6.12 seqlock.h 读写与 latch 源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, seqcount, seqlock, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_seqlock.h读写与latch源码实现

## 1.1\_实现讲解边界

本页按固定NXP linux-imx提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20展开include/linux/seqlock.h；blob 5298765d6ca4827eb7bf8a9dca020f1383d3b901。中文Doxygen由仓库补充，省去原英文注释、相邻未选择接口，保留所选宏和函数的真实控制流。概念性假类型和associated_lock占位函数不再作为实现证据。

[总索引](../../../navigation/P01_Linux_6.12_序列计数器源码总阅读索引.md#1.4_阅读入口)组织阅读目标，[模块导读](../../../navigation/P02_Linux_6.12_seqcount与seqlock模块源码概念导读.md#2.1_模块问题与职责分支)组织状态及协作；[类型实现](seqlock_types.h.md#1.2_plain计数与条件检查字段)解释声明和配置。以下实现回到模块中的普通、关联锁与latch阶段，不把它们混成同一个状态机。

## 1.2\_源码符号覆盖账本

| 入口 | 写读关系 | 修改时必须联查 |
| --- | --- | --- |
| [初始化](#1.2.1_初始化与关联建立) | 发布前清sequence，构造检查状态及关联地址 | 外部锁初始化、对象寿命、配置裁剪 |
| [普通读侧](#1.3_普通读侧begin与retry) | acquire取证、复制后rmb与版本比较 | 奇数资格、检查器覆盖、raw差异 |
| [普通写侧](#1.4_普通写侧begin与end) | 调用者串行下开窗/关窗，按属性处理抢占 | KCSAN/Lockdep动作不是功能锁 |
| [关联属性](#1.5_关联锁属性与RT补偿) | 配置及类型选择真实锁慢路径 | 释放后重读，不保护整个读区 |
| [latch](#1.6_latch重定向与双副本更新) | 两次重定向与完整计数验证 | 业务数据的两次更新、依赖选址及寿命 |
| [seqlock](#1.7_seqlock封装与加锁读者) | 内嵌锁包围写窗口或加锁读区 | 重试读者与加锁读者不能混同 |

这些是本次所选实现，不宣称覆盖每个IRQ/BH包装、嵌套子类接口或raw_write_seqcount_barrier。通用spinlock、调度器和KCSAN内部实现不在这里重复展开。

### 1.2.1\_初始化与关联建立

```c
/**
 * @brief 仓库阅读说明：__seqcount_init：s为待构造对象，name和key用于锁类检查，发布前调用。
 */
static inline void __seqcount_init(seqcount_t *s, const char *name,
					  struct lock_class_key *key)
{

	lockdep_init_map(&s->dep_map, name, key, 0);
	s->sequence = 0;
}
```

__seqcount_init先初始化Lockdep映射，再清sequence。公共seqcount_init在CONFIG_DEBUG_LOCK_ALLOC下生成锁类键，其他配置传空名称/键；检查器宏按配置消去不适用的字段访问。调用者必须在对象发布前执行，不能在仍有旧访问者时“清零重来”。

```c
/**
 * @brief 仓库阅读说明：seqcount_LOCKNAME_init：s为关联对象，_lock为已初始化外部锁地址，lockname选择类型。
 */
#define seqcount_LOCKNAME_init(s, _lock, lockname)			\
	do {								\
		seqcount_##lockname##_t *____s = (s);			\
		seqcount_init(&____s->seqcount);			\
		__SEQ_LOCK(____s->lock = (_lock));			\
	} while (0)
```

关联初始化宏先初始化内嵌seqcount，再经__SEQ_LOCK保存外部锁地址。具体seqcount_mutex_init等只是选择对应lockname的包装；外部锁本身要由调用者先初始化。这里没有取得锁，也不排除旧访问者。

## 1.3\_普通读侧begin与retry

```c
/**
 * @brief 仓库阅读说明：__read_seqcount_begin：s为plain或关联对象，返回本轮起点。
 */
#define __read_seqcount_begin(s)					\
({									\
	unsigned __seq;							\
									\
	while ((__seq = seqprop_sequence(s)) & 1)			\
		cpu_relax();						\
									\
	kcsan_atomic_next(KCSAN_SEQLOCK_REGION_MAX);			\
	__seq;								\
})
```

```c
/**
 * @brief 仓库阅读说明：raw_read_seqcount_begin：s为plain或关联对象，返回本轮起点。
 */
#define raw_read_seqcount_begin(s) __read_seqcount_begin(s)
```

```c
/**
 * @brief 仓库阅读说明：read_seqcount_begin：s为plain或关联对象，返回本轮起点。
 */
#define read_seqcount_begin(s)						\
({									\
	seqcount_lockdep_reader_access(seqprop_const_ptr(s));		\
	raw_read_seqcount_begin(s);					\
})
```

```c
/**
 * @brief 仓库阅读说明：do___read_seqcount_retry：s为底层对象，start为本轮起点，返回是否必须丢弃候选。
 */
static inline int do___read_seqcount_retry(const seqcount_t *s, unsigned start)
{
	kcsan_atomic_next(0);
	return unlikely(READ_ONCE(s->sequence) != start);
}
```

```c
/**
 * @brief 仓库阅读说明：do_read_seqcount_retry：s为底层对象，start为本轮起点，返回是否必须丢弃候选。
 */
static inline int do_read_seqcount_retry(const seqcount_t *s, unsigned start)
{
	smp_rmb();
	return do___read_seqcount_retry(s, start);
}
```

普通入口的seqcount_lockdep_reader_access对锁依赖检查进行短暂获取/释放记账，不是给业务读区加读锁。__read_seqcount_begin反复调用属性读取，奇数时cpu_relax后重试。属性读取的acquire实现在[1.5](#1.5_关联锁属性与RT补偿)，返回偶数才建立本轮start。KCSAN标记接下来的有限访问预算，普通retry终止这一范围并在rmb后比较sequence；这些检查动作不能替代内存顺序。

```c
/**
 * @brief 仓库阅读说明：raw_read_seqcount：s为目标对象，返回值的奇偶处理不同。
 */
#define raw_read_seqcount(s)						\
({									\
	unsigned __seq = seqprop_sequence(s);				\
									\
	kcsan_atomic_next(KCSAN_SEQLOCK_REGION_MAX);			\
	__seq;								\
})
```

```c
/**
 * @brief 仓库阅读说明：raw_seqcount_begin：s为目标对象，返回值的奇偶处理不同。
 */
#define raw_seqcount_begin(s)						\
({									\
									\
	raw_read_seqcount(s) & ~1;					\
})
```

raw_read_seqcount保留属性读取和KCSAN标记，但不等待或掩掉奇数。raw_seqcount_begin则清最低位：读到11会记录10，使末尾读11或12都比较失败，前提仍是没有完整回绕。raw_read_seqcount_begin仅省略普通Lockdep读访问入口，仍等偶数；__read_seqcount_retry省略普通retry的rmb。不能把这些不同名字统称为“没有屏障的读”。

修改时先画出S1取证、字段复制和S5末尾取证之间的顺序；删除稳定循环或rmb分别改变不同前提。默认读者不登记共享名单，版本失败只丢弃局部副本，不撤销已经发生的I/O或失效地址访问。

## 1.4\_普通写侧begin与end

```c
/**
 * @brief 仓库阅读说明：write_seqcount_begin：s的写者串行协议由调用者先建立。
 */
#define write_seqcount_begin(s)						\
do {									\
	seqprop_assert(s);						\
									\
	if (seqprop_preemptible(s))					\
		preempt_disable();					\
									\
	do_write_seqcount_begin(seqprop_ptr(s));			\
} while (0)
```

```c
/**
 * @brief 仓库阅读说明：write_seqcount_end：s的写者串行协议由调用者先建立。
 */
#define write_seqcount_end(s)						\
do {									\
	do_write_seqcount_end(seqprop_ptr(s));				\
									\
	if (seqprop_preemptible(s))					\
		preempt_enable();					\
} while (0)
```

```c
/**
 * @brief 仓库阅读说明：do_write_seqcount_begin：s为底层序号，嵌套接口的subclass用于检查器记账，不是功能锁。
 */
static inline void do_write_seqcount_begin(seqcount_t *s)
{
	do_write_seqcount_begin_nested(s, 0);
}
```

```c
/**
 * @brief 仓库阅读说明：do_write_seqcount_begin_nested：s为底层序号，嵌套接口的subclass用于检查器记账，不是功能锁。
 */
static inline void do_write_seqcount_begin_nested(seqcount_t *s, int subclass)
{
	seqcount_acquire(&s->dep_map, subclass, 0, _RET_IP_);
	do_raw_write_seqcount_begin(s);
}
```

```c
/**
 * @brief 仓库阅读说明：do_raw_write_seqcount_begin：s为底层序号，嵌套接口的subclass用于检查器记账，不是功能锁。
 */
static inline void do_raw_write_seqcount_begin(seqcount_t *s)
{
	kcsan_nestable_atomic_begin();
	s->sequence++;
	smp_wmb();
}
```

```c
/**
 * @brief 仓库阅读说明：do_write_seqcount_end：s为底层序号，嵌套接口的subclass用于检查器记账，不是功能锁。
 */
static inline void do_write_seqcount_end(seqcount_t *s)
{
	seqcount_release(&s->dep_map, _RET_IP_);
	do_raw_write_seqcount_end(s);
}
```

```c
/**
 * @brief 仓库阅读说明：do_raw_write_seqcount_end：s为底层序号，嵌套接口的subclass用于检查器记账，不是功能锁。
 */
static inline void do_raw_write_seqcount_end(seqcount_t *s)
{
	smp_wmb();
	s->sequence++;
	kcsan_nestable_atomic_end();
}
```

最外层write_seqcount_begin先按类型断言已有串行/抢占条件，需要时关闭抢占，再进入Lockdep记账和raw核心。raw核心开启KCSAN范围、sequence变奇，wmb后才轮到调用者字段更新。end经对应记账，先wmb后sequence变偶，再结束KCSAN范围；外层按属性恢复抢占。plain类型属性返回不可由包装自动补偿，责任由调用者实现。

这些函数不自行取得业务写者锁。raw_write_seqcount_begin/end包装仍使用同一raw核心和抢占属性，只省略相应Lockdep路径；不能因为raw便删除字段顺序。保持所有写者串行、窗口短和上下文有效，才可以把奇偶计数当作稳定证据。

```c
/**
 * @brief 仓库阅读说明：do_write_seqcount_invalidate：s为目标序号，推进两代使尚在进行的旧读区失效。
 */
static inline void do_write_seqcount_invalidate(seqcount_t *s)
{
	smp_wmb();
	kcsan_nestable_atomic_begin();
	s->sequence+=2;
	kcsan_nestable_atomic_end();
}
```

失效操作先写屏障，再把sequence加2。它让尚在进行的旧读区比较失败，不是任意多字段写入的开窗/关窗，也不追回已交付的副本。KCSAN嵌套范围只包住这里的计数动作，不扩大成外围业务事务。

## 1.5\_关联锁属性与RT补偿

```c
/**
 * @brief 仓库阅读说明：SEQCOUNT_LOCKNAME：lockname/locktype决定属性族，preemptible和lockbase选择补偿行为。
 */
#define SEQCOUNT_LOCKNAME(lockname, locktype, preemptible, lockbase)	\
static __always_inline seqcount_t *					\
__seqprop_##lockname##_ptr(seqcount_##lockname##_t *s)			\
{									\
	return &s->seqcount;						\
}									\
									\
static __always_inline const seqcount_t *				\
__seqprop_##lockname##_const_ptr(const seqcount_##lockname##_t *s)	\
{									\
	return &s->seqcount;						\
}									\
									\
static __always_inline unsigned						\
__seqprop_##lockname##_sequence(const seqcount_##lockname##_t *s)	\
{									\
	unsigned seq = smp_load_acquire(&s->seqcount.sequence);		\
									\
	if (!IS_ENABLED(CONFIG_PREEMPT_RT))				\
		return seq;						\
									\
	if (preemptible && unlikely(seq & 1)) {				\
		__SEQ_LOCK(lockbase##_lock(s->lock));			\
		__SEQ_LOCK(lockbase##_unlock(s->lock));			\
									\
									\
		seq = smp_load_acquire(&s->seqcount.sequence);		\
	}								\
									\
	return seq;							\
}									\
									\
static __always_inline bool						\
__seqprop_##lockname##_preemptible(const seqcount_##lockname##_t *s)	\
{									\
	if (!IS_ENABLED(CONFIG_PREEMPT_RT))				\
		return preemptible;					\
									\
			\
	return false;							\
}									\
									\
static __always_inline void						\
__seqprop_##lockname##_assert(const seqcount_##lockname##_t *s)		\
{									\
	__SEQ_LOCK(lockdep_assert_held(s->lock));			\
}
```

```c
/**
 * @brief 仓库阅读说明：四种锁的实际实例化参数。
 */
#define __SEQ_RT	IS_ENABLED(CONFIG_PREEMPT_RT)

SEQCOUNT_LOCKNAME(raw_spinlock, raw_spinlock_t,  false,    raw_spin)
SEQCOUNT_LOCKNAME(spinlock,     spinlock_t,      __SEQ_RT, spin)
SEQCOUNT_LOCKNAME(rwlock,       rwlock_t,        __SEQ_RT, read)
SEQCOUNT_LOCKNAME(mutex,        struct mutex,    true,     mutex)
#undef SEQCOUNT_LOCKNAME
```

宏生成底层指针、const指针、sequence读取、抢占属性和持锁断言。sequence用acquire读取；非RT直接返回。RT下只有传入preemptible属性为真且读到奇数时，才沿外部锁地址取得并释放锁，再acquire重读。raw_spinlock实例的属性为false，不走该慢路径；spinlock/rwlock受RT配置选择，mutex实例为true。类型实例化参数与types.h的结构布局相配合。

__seqprop_LOCKNAME_preemptible在RT下返回false，避免写包装沿同一规则统一禁抢占，读侧依赖上面的锁补偿；非RT则返回该锁类型属性。__SEQ_LOCK在RT下即使没有Lockdep也保留实际锁调用。读者释放后新writer仍能进入，所以重新读到奇数并不矛盾，稳定循环会继续。

```c
/**
 * @brief 仓库阅读说明：__seqprop_sequence：s为plain计数对象，没有外部锁地址。
 */
static inline unsigned __seqprop_sequence(const seqcount_t *s)
{
	return smp_load_acquire(&s->sequence);
}
```

```c
/**
 * @brief 仓库阅读说明：__seqprop_preemptible：s为plain计数对象，没有外部锁地址。
 */
static inline bool __seqprop_preemptible(const seqcount_t *s)
{
	return false;
}
```

```c
/**
 * @brief 仓库阅读说明：__seqprop_assert：s为plain计数对象，没有外部锁地址。
 */
static inline void __seqprop_assert(const seqcount_t *s)
{
	lockdep_assert_preemption_disabled();
}
```

plain读取同样用acquire，但没有关联锁地址；其断言检查抢占已经关闭，不替调用者串行多个写者。seqprop_*经_Generic按实际类型分派：这是编译期选择正确属性族，不是运行时把不同锁强制转换成一种锁。不能伪造类型来获取某种属性。

## 1.6\_latch重定向与双副本更新

```c
/**
 * @brief 仓库阅读说明：仅存储序号，业务双副本由调用者提供。
 */
typedef struct {
	seqcount_t seqcount;
} seqcount_latch_t;
```

latch类型只包含seqcount，业务data[2]由调用者提供并在发布前初始化。普通奇偶“写中”含义在这里改成最低位选择；奇数也可以是合法读起点。

```c
/**
 * @brief 仓库阅读说明：raw_read_seqcount_latch：s为latch序号；retry的start必须是完整起点计数。
 */
static __always_inline unsigned raw_read_seqcount_latch(const seqcount_latch_t *s)
{

	return READ_ONCE(s->seqcount.sequence);
}
```

```c
/**
 * @brief 仓库阅读说明：read_seqcount_latch：s为latch序号；retry的start必须是完整起点计数。
 */
static __always_inline unsigned read_seqcount_latch(const seqcount_latch_t *s)
{
	kcsan_atomic_next(KCSAN_SEQLOCK_REGION_MAX);
	return raw_read_seqcount_latch(s);
}
```

```c
/**
 * @brief 仓库阅读说明：raw_read_seqcount_latch_retry：s为latch序号；retry的start必须是完整起点计数。
 */
static __always_inline int
raw_read_seqcount_latch_retry(const seqcount_latch_t *s, unsigned start)
{
	smp_rmb();
	return unlikely(READ_ONCE(s->seqcount.sequence) != start);
}
```

```c
/**
 * @brief 仓库阅读说明：read_seqcount_latch_retry：s为latch序号；retry的start必须是完整起点计数。
 */
static __always_inline int
read_seqcount_latch_retry(const seqcount_latch_t *s, unsigned start)
{
	kcsan_atomic_next(0);
	return raw_read_seqcount_latch_retry(s, start);
}
```

```c
/**
 * @brief 仓库阅读说明：raw_write_seqcount_latch：s为latch序号；retry的start必须是完整起点计数。
 */
static __always_inline void raw_write_seqcount_latch(seqcount_latch_t *s)
{
	smp_wmb();
	s->seqcount.sequence++;
	smp_wmb();
}
```

```c
/**
 * @brief 仓库阅读说明：write_seqcount_latch_begin：s为latch序号；retry的start必须是完整起点计数。
 */
static __always_inline void write_seqcount_latch_begin(seqcount_latch_t *s)
{
	kcsan_nestable_atomic_begin();
	raw_write_seqcount_latch(s);
}
```

```c
/**
 * @brief 仓库阅读说明：write_seqcount_latch：s为latch序号；retry的start必须是完整起点计数。
 */
static __always_inline void write_seqcount_latch(seqcount_latch_t *s)
{
	raw_write_seqcount_latch(s);
}
```

```c
/**
 * @brief 仓库阅读说明：write_seqcount_latch_end：s为latch序号；retry的start必须是完整起点计数。
 */
static __always_inline void write_seqcount_latch_end(seqcount_latch_t *s)
{
	kcsan_nestable_atomic_end();
}
```

读起点使用READ_ONCE取得完整计数，随后以最低位产生对业务副本的依赖选择；这里不是普通seqcount的acquire稳定循环。raw_write_seqcount_latch在增量前后各放wmb：先约束此前副本写完成，再把重定向排在下一份副本写入之前。读末尾rmb后比较完整计数，不能只比较最低位。

begin和write分别调用一次重定向，调用者在两者之间更新data0，之后更新data1；end只结束KCSAN写范围，没有第三次翻转。对应模块latch S1/S3与S2/S4业务动作。旧读者仍可能选中随后被改的副本，只有完整计数验证才能拒绝其候选；动态对象仍需要独立回收协议。

## 1.7\_seqlock封装与加锁读者

```c
/**
 * @brief 仓库阅读说明：seqlock_init：sl为尚未发布的组合对象。
 */
#define seqlock_init(sl)						\
	do {								\
		spin_lock_init(&(sl)->lock);				\
		seqcount_spinlock_init(&(sl)->seqcount, &(sl)->lock);	\
	} while (0)
```

```c
/**
 * @brief 仓库阅读说明：read_seqbegin：sl为组合对象；重试接口的start是之前取得的计数。
 */
static inline unsigned read_seqbegin(const seqlock_t *sl)
{
	return read_seqcount_begin(&sl->seqcount);
}
```

```c
/**
 * @brief 仓库阅读说明：read_seqretry：sl为组合对象；重试接口的start是之前取得的计数。
 */
static inline unsigned read_seqretry(const seqlock_t *sl, unsigned start)
{
	return read_seqcount_retry(&sl->seqcount, start);
}
```

```c
/**
 * @brief 仓库阅读说明：write_seqlock：sl为组合对象；重试接口的start是之前取得的计数。
 */
static inline void write_seqlock(seqlock_t *sl)
{
	spin_lock(&sl->lock);
	do_write_seqcount_begin(&sl->seqcount.seqcount);
}
```

```c
/**
 * @brief 仓库阅读说明：write_sequnlock：sl为组合对象；重试接口的start是之前取得的计数。
 */
static inline void write_sequnlock(seqlock_t *sl)
{
	do_write_seqcount_end(&sl->seqcount.seqcount);
	spin_unlock(&sl->lock);
}
```

```c
/**
 * @brief 仓库阅读说明：read_seqlock_excl：sl为组合对象；重试接口的start是之前取得的计数。
 */
static inline void read_seqlock_excl(seqlock_t *sl)
{
	spin_lock(&sl->lock);
}
```

```c
/**
 * @brief 仓库阅读说明：read_sequnlock_excl：sl为组合对象；重试接口的start是之前取得的计数。
 */
static inline void read_sequnlock_excl(seqlock_t *sl)
{
	spin_unlock(&sl->lock);
}
```

初始化同时构造实际spinlock与关联seqcount。write_seqlock先取得内嵌锁再开窗，write_sequnlock先关窗后解锁；内部直接调用do_write核心，避免对已由包装取得的同一锁重复作通用属性断言。read_seqbegin/retry转交关联seqcount读路径，因此RT属性仍可参与进展。

read_seqlock_excl/对应解锁只取得/释放内嵌锁，不改变sequence。它排斥写者和其他加锁读者，却不让普通重试读者登记或等待这把锁。不能拿着这个读锁修改受保护字段而不推进sequence；普通读者可能接受未被标记的变化。中断/BH包装另有上下文契约，本页不假称逐个展开。

## 1.8\_复核与实际验证范围

先沿模块普通状态链定位每次sequence写入，再分别加入RT慢路径和latch双副本，检查字段地址、写者锁、读者候选与检查器状态是否仍区分清楚。尝试解释：删去raw核心的wmb、把latch末尾换成最低位比较、提前释放写者锁，分别破坏哪一个阶段约束。

所选实现完成与固定源码的静态比对，知识侧C模型只验证有序候选规则。未编译目标内核、运行RT配置、NMI、弱内存模型或检查器异常路径；本页不把源码相同等同于这些运行验证。
