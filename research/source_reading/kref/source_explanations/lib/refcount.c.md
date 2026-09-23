---
id: research.kref.implementation.refcount_saturation
title: "refcount.c异常与归零锁实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_refcount.c异常与归零锁实现

固定来源为 NXP linux-imx，发布 lf-6.12.20-2.0.0，提交 dfaf2136deb2af2e60b994421281ba42f1c087e0（Linux 6.12.20）。以下中文 Doxygen 为仓库补充，函数或宏主体保持该提交内容。

上游位置 lib/refcount.c，blob a207a8f22b3ca35890671e51c480266d89e4d8d6。

## 1.1\_告警之前先收敛到饱和

```c
/** @brief 仓库阅读说明：按告警点报告异常；不是每个对象专有的告警记账。 */
#define REFCOUNT_WARN(str)	WARN_ONCE(1, "refcount_t: " str ".\n")
```

```c
/** @brief 仓库阅读说明：先写 REFCOUNT_SATURATED，再按调用方提供的异常类别报告。 */
void refcount_warn_saturate(refcount_t *r, enum refcount_saturation_type t)
{
	refcount_set(r, REFCOUNT_SATURATED);

	switch (t) {
	case REFCOUNT_ADD_NOT_ZERO_OVF:
		REFCOUNT_WARN("saturated; leaking memory");
		break;
	case REFCOUNT_ADD_OVF:
		REFCOUNT_WARN("saturated; leaking memory");
		break;
	case REFCOUNT_ADD_UAF:
		REFCOUNT_WARN("addition on 0; use-after-free");
		break;
	case REFCOUNT_SUB_UAF:
		REFCOUNT_WARN("underflow; use-after-free");
		break;
	case REFCOUNT_DEC_LEAK:
		REFCOUNT_WARN("decrement hit 0; leaking memory");
		break;
	default:
		REFCOUNT_WARN("unknown saturation event!?");
	}
}
```


调用方已经完成对应原子操作，这里先设置标记再发出告警。ADD_UAF 与 SUB_UAF 的分类来自调用方观察到的数值，不是一次分配器对象身份鉴定。WARN_ONCE 还意味着不能按日志条数推算发生过多少个错误对象；是否能观察到日志以及告警后的执行行为受运行配置和环境影响。

饱和以保守保留资源为代价，不能修复配对错误。并发操作可能在检查与设值之间穿插，固定 refcount.h 的头注释讨论其异常区与规模约束；不能套用教学模型“一进入就额外布尔位永久截断所有写入”的实现。

返回[普通引用模块](../../navigation/P02_普通引用与归零回调导读.md#2.4_正常退出与异常收敛)及[引用原语](../include/linux/refcount.h.md#1.3_旧值决定归零与异常分支)。

## 1.2\_快路径保留最后一份

```c
/**
 * @brief 仓库阅读说明：S4 优先完成非最后减少；遇到一份时保留它并返回 false。
 * @return 正常非最后减少或异常保守退出返回 true；值为一时 false。
 */
bool refcount_dec_not_one(refcount_t *r)
{
	unsigned int new, val = atomic_read(&r->refs);

	do {
		if (unlikely(val == REFCOUNT_SATURATED))
			return true;

		if (val == 1)
			return false;

		new = val - 1;
		if (new > val) {
			WARN_ONCE(new > val, "refcount_t: underflow; use-after-free.\n");
			return true;
		}

	} while (!atomic_try_cmpxchg_release(&r->refs, &val, new));

	return true;
}
```

val/new 在此使用 unsigned int。遇到 REFCOUNT_SATURATED 直接返回 true，不减少；零值减一形成无符号回绕，new>val 时告警并返回 true，也不把回绕值写入共享计数。普通大于一时用 release 比较交换递减；失败更新 val 后重查。它的 true 不应统一解释为“已经成功减一”，异常出口也会返回 true。

从[锁交接模块](../../navigation/P04_最后归还与锁交接导读.md#4.2_把最后减少留在锁内)看调用者怎样解释 false：尚未消费最后一份，因此可以先取锁，再决定本次是否归零。

## 1.3\_取得锁后再次减少判断

```c
/** @brief 仓库阅读说明：S4 可能非最后时直接退出；慢路径取 mutex 后真正减少，S5 归零才把锁交出去。 */
bool refcount_dec_and_mutex_lock(refcount_t *r, struct mutex *lock)
{
	if (refcount_dec_not_one(r))
		return false;

	mutex_lock(lock);
	if (!refcount_dec_and_test(r)) {
		mutex_unlock(lock);
		return false;
	}

	return true;
}
```

等待锁期间别的查找者可能新增引用，故取锁后仍要 refcount_dec_and_test。若并非最后，本函数自行解锁返回 false；若归零，则返回 true 并保留锁，供上层回调完成索引不可见转换。调用者必须未持有这把非递归 mutex，且允许该路径取得 mutex。

```c
/** @brief 仓库阅读说明：相同分支结构采用普通 spin_lock，不保存或关闭本地中断状态。 */
bool refcount_dec_and_lock(refcount_t *r, spinlock_t *lock)
{
	if (refcount_dec_not_one(r))
		return false;

	spin_lock(lock);
	if (!refcount_dec_and_test(r)) {
		spin_unlock(lock);
		return false;
	}

	return true;
}
```

这条 spinlock 链不是同文件中的 irqsave 变体；不能因变量叫 lock 就推断中断已关闭。当前基线的非 PREEMPT_RT 配置下，持普通 spinlock 的回调区间不能睡眠。平台或实时配置改变时须重审锁及上下文契约，不把本段推广为所有配置的锁实现。

两函数的最终减少复用[普通减并检测](../include/linux/refcount.h.md#1.3_旧值决定归零与异常分支)，kref 回调层见[两种锁入口](../include/linux/kref.h.md#1.8_归零时把锁交给回调)。锁生命周期必须覆盖取锁、回调和解锁；教材完整例子使用对象外的索引锁，不在回收后触碰对象内的锁。

[异常诊断与覆盖导读](../../navigation/P02_普通引用与归零回调导读.md#2.16_异常报告与检查覆盖)区分这里的数值条件与应用拥有者责任；不告警不等于交付协议正确。
