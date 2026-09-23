---
id: research.kref.implementation.refcount
title: "refcount.h引用增减实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_refcount.h引用增减实现

固定来源为 NXP linux-imx，发布 lf-6.12.20-2.0.0，提交 dfaf2136deb2af2e60b994421281ba42f1c087e0（Linux 6.12.20）。以下中文 Doxygen 为仓库补充，函数或宏主体保持该提交内容。

上游位置 include/linux/refcount.h，blob 35f039ecb2725618ca098e3515c6e19e2aece3ee。覆盖普通单份与条件取得 kref 路径及其 helper；原子底层采用明确契约，不在此推定目标指令。

## 1.1\_设置与观察

```c
/** @brief 仓库阅读说明：S0 直接设置 refs，生命周期前提由初始化者保证。 */
static inline void refcount_set(refcount_t *r, int n)
{
	atomic_set(&r->refs, n);
}
```

```c
/** @brief 仓库阅读说明：取得当前 refs 值，没有为观察者新增责任。 */
static inline unsigned int refcount_read(const refcount_t *r)
{
	return atomic_read(&r->refs);
}
```


set 没有判断旧值是否已发布，因此重新初始化错误不能靠本函数检测。read 返回 unsigned int；异常区的位也可以作为大无符号数观察，不意味着真有这么多合法持有者。

## 1.2\_普通增加与异常检测

```c
/** @brief 仓库阅读说明：先取得增加前的旧值，再按零、负值或符号跨界分类异常。 */
static inline __signed_wrap
void __refcount_add(int i, refcount_t *r, int *oldp)
{
	int old = atomic_fetch_add_relaxed(i, &r->refs);

	if (oldp)
		*oldp = old;

	if (unlikely(!old))
		refcount_warn_saturate(r, REFCOUNT_ADD_UAF);
	else if (unlikely(old < 0 || old + i < 0))
		refcount_warn_saturate(r, REFCOUNT_ADD_OVF);
}
```

```c
/** @brief 仓库阅读说明：把普通单份增加交给 add，并透传可选的旧值输出。 */
static inline void __refcount_inc(refcount_t *r, int *oldp)
{
	__refcount_add(1, r, oldp);
}
```

```c
/** @brief 仓库阅读说明：普通公开增加固定为一份，不给调用者提供成功布尔值。 */
static inline void refcount_inc(refcount_t *r)
{
	__refcount_inc(r, NULL);
}
```


atomic_fetch_add_relaxed 返回旧值，计数存储已经发生增加；oldp 非空时才保存旧值。普通 kref 调用链一直传 NULL，因此不会额外维护一份持有者记录。old 为零走 ADD_UAF，原有异常值或 old+i 落入负区走 ADD_OVF；最终由[异常收敛](../../lib/refcount.c.md#1.1_告警之前先收敛到饱和)设置标记。不能把检测误读成“先拒绝坏增加，再决定是否写计数”。

relaxed 不为取得对象另添发布排序；对象地址与字段可见性由此前锁或发布/读取协议建立。[signed_wrap 属性](compiler_types.h.md#1.1_检查器属性与构建选项分工)只管理特定溢出插桩，不独立决定这些算术的编译语义。

## 1.3\_旧值决定归零与异常分支

```c
/** @brief 仓库阅读说明：先减并返回旧值，只在合法正值恰等于减量时返回真。 */
static inline __must_check __signed_wrap
bool __refcount_sub_and_test(int i, refcount_t *r, int *oldp)
{
	int old = atomic_fetch_sub_release(i, &r->refs);

	if (oldp)
		*oldp = old;

	if (old > 0 && old == i) {
		smp_acquire__after_ctrl_dep();
		return true;
	}

	if (unlikely(old <= 0 || old - i < 0))
		refcount_warn_saturate(r, REFCOUNT_SUB_UAF);

	return false;
}
```

```c
/** @brief 仓库阅读说明：普通单份归还把减量固定为 1。 */
static inline __must_check bool __refcount_dec_and_test(refcount_t *r, int *oldp)
{
	return __refcount_sub_and_test(1, r, oldp);
}
```

```c
/** @brief 仓库阅读说明：向调用者返回是否本次完成正常归零，要求消费结果。 */
static inline __must_check bool refcount_dec_and_test(refcount_t *r)
{
	return __refcount_dec_and_test(r, NULL);
}
```


按同一轮 S3～S5 比较：旧值 2、减量 1 时存储变为 1，返回 false；旧值 1、减量 1 时变为 0，执行 acquire 补充后返回 true；旧值 1、减量 2 的通用 helper 会进入异常分支，不是一次合法最后归还。普通 kref 只减 1，不能把批量示例解释成它会任意减量。

原子减提供 release 顺序；正常归零分支再经 smp_acquire__after_ctrl_dep 提供规定的 acquire 顺序。这里讲的是访问的顺序约束，不是声称调用时所有处理器的物理总线事务已完全结束。随后 kref_put 才调用清理。

old<=0 包括零和已有饱和/异常值，不能仅凭这一项断言对象物理内存早已释放；old-i<0 则包含过量归还。异常处理后返回 false，真实责任不再能由数值可靠还原，不能擅自 reset 或仍走正常回收。[must_check 属性](compiler_attributes.h.md#1.1_返回值诊断不是自动清理)提示调用者不要丢弃这个判断；它不是运行时的自动释放机制。

返回[普通引用模块](../../../navigation/P02_普通引用与归零回调导读.md#2.2_把S0到S5落到状态地址)或[源码总索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。

## 1.4\_逐层构造初始值

```c
/** @brief 仓库阅读说明：为 refcount_t 的 refs 原子成员提供初始化器。 */
#define REFCOUNT_INIT(n)	{ .refs = ATOMIC_INIT(n), }
```

下层的 [atomic_t 与 ATOMIC_INIT](types.h.md#1.1_整数外还有一层结构) 还有一层结构。把三层实际宏代换进去，KREF_INIT(n) 得到如下初始化器；这是展开结果，不是第二份宏定义：

```c
{ .refcount = { .refs = { (n) }, }, }
```

从外到内分别对应 kref、refcount_t、atomic_t，最内层的 n 初始化 counter。写成 `.refcount.refs = n` 不是这些宏的实际展开。C 在部分聚合初始化位置允许省略内层花括号，所以仅凭“它还能编译”不能确认抄写忠于源码；严格启用 missing-braces 诊断可帮助看见被省略的层次。

该宏不验证 n 的所有权来源，也不检查它是否适合作为新生命周期的初值。自动对象可使用运行时整数初始化；静态存储对象的初始化还须满足 C 对常量表达式的要求。不能把定义时填值与运行中 refcount_set 混为一谈，更不能以覆盖计数来回避旧引用尚未归还的问题。

返回[初始化模块](../../../navigation/P02_普通引用与归零回调导读.md#2.6_初始化形式与存储寿命)或[总索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。

## 1.5\_条件增加与失败重试

以下按被调用顺序之外的定义依赖列出三个函数体；调用阅读顺序由[条件模块](../../../navigation/P03_条件取得与查找窗口导读.md#3.2_从观察到自己持有)组织。上游相对路径、提交与 blob 与本页开头一致。

```c
/**
 * @brief 仓库阅读说明：S2 观察非零后比较并更新，失败获取新旧值后重试。
 * @param i 本链由上层传入 1，不在此讨论其他批量增加调用。
 * @param r 调用者保证其存储在整个尝试期间有效的计数对象。
 * @param oldp 非空时写回最终观察/比较所依据的旧值。
 * @return 正常零值退出为 false；正常成功为 true，异常饱和也可能为 true。
 */
static inline __must_check __signed_wrap
bool __refcount_add_not_zero(int i, refcount_t *r, int *oldp)
{
	int old = refcount_read(r);

	do {
		if (!old)
			break;
	} while (!atomic_try_cmpxchg_relaxed(&r->refs, &old, old + i));

	if (oldp)
		*oldp = old;

	if (unlikely(old < 0 || old + i < 0))
		refcount_warn_saturate(r, REFCOUNT_ADD_NOT_ZERO_OVF);

	return old;
}
```

初始 read 不是取得。atomic_try_cmpxchg_relaxed 比较失败时更新 old，循环再次检查新值：变为 0 就停止，仍为正数就以它为基准重试；成功使用的是本次比较的旧值。oldp 写回发生在异常检测前，便于内部调用者区分旧值，但 kref 这条链传 NULL。

old 为负或 old+1 为负时进入[饱和告警](../../lib/refcount.c.md#1.1_告警之前先收敛到饱和)，然后 return old 转为 bool。因而非零返回不统一表示计数恰好增加一份：异常态会饱和并可能泄漏，不能当正常保活建立的替代方案。此处有符号运算的构建约束见[编译边界](compiler_types.h.md#1.1_检查器属性与构建选项分工)。

```c
/** @brief 仓库阅读说明：把增加量固定为一份，保留可选旧值出口。 */
static inline __must_check bool __refcount_inc_not_zero(refcount_t *r, int *oldp)
{
	return __refcount_add_not_zero(1, r, oldp);
}
```

```c
/** @brief 仓库阅读说明：供 kref 使用的单份包装，不接收旧值。 */
static inline __must_check bool refcount_inc_not_zero(refcount_t *r)
{
	return __refcount_inc_not_zero(r, NULL);
}
```

上游注释明确该取得链不提供 acquire 式发布读取保证，依赖调用者已经稳定对象存储；条件控制依赖用于约束后续写入，不应推广成后续所有读写的通用屏障。宿主顺序夹具只验证分支、重试与旧值，不验证体系结构屏障或 Linux 内存模型。

[额外归还实验](../../../navigation/P07_引用错误的动态诊断导读.md#7.7_额外归还与快照的检测界限)把应用责任错误与本节old值检测分开：old为1仍走正常归零，即使该次调用在应用层已经重复消费。
