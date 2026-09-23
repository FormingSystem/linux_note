---
id: research.kref.implementation.refcount_saturation
title: "refcount.c异常收敛实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_refcount.c异常收敛实现

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
