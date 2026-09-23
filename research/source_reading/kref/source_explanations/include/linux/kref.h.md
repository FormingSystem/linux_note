---
id: research.kref.implementation.kref
title: "kref.h普通引用与回调实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_kref.h普通引用与回调实现

固定来源为 NXP linux-imx，发布 lf-6.12.20-2.0.0，提交 dfaf2136deb2af2e60b994421281ba42f1c087e0（Linux 6.12.20）。以下中文 Doxygen 为仓库补充，函数或宏主体保持该提交内容。

上游位置 include/linux/kref.h，blob d32e21a2538c292452db99b915b1bb6c3ab15e53。本页只展开结构及普通 init/get/put/read；条件取得、静态宏和锁组合不是本页已覆盖的实现。

## 1.1\_计数成员

```c
/** @brief 仓库阅读说明：仅嵌入引用原语，不保存对象类型或回调。 */
struct kref {
	refcount_t refcount;
};
```

外层对象决定本成员在哪里，也决定每份责任归谁。字段的存储定义见[refcount 类型](refcount_types.h.md#1.1_原子存储字段)；kref 本身没有 owner 表或 release 成员。

## 1.2\_建立初始引用

```c
/** @brief 仓库阅读说明：S0 将未发布对象的计数设为 1，不是对现有计数加一。 */
static inline void kref_init(struct kref *kref)
{
	refcount_set(&kref->refcount, 1);
}
```


它调用[设置原语](refcount.h.md#1.1_设置与观察)，不分配内存、不初始化业务字段、不发布对象；不能用来把已有责任重置为一份。

## 1.3\_为独立使用追加引用

```c
/** @brief 仓库阅读说明：S1 在有效持有仍成立时，为接收者预留一份。 */
static inline void kref_get(struct kref *kref)
{
	refcount_inc(&kref->refcount);
}
```


没有返回值不表示可以对任意地址试探。调用者必须已经满足有效对象和正引用前提；下层[普通增加](refcount.h.md#1.2_普通增加与异常检测)的异常告警不是这个接口的成功/失败分支。S2 发布责任及业务字段由队列或容器的协议完成，本函数没有代替它。

## 1.4\_最后归还调用清理

```c
/** @brief 仓库阅读说明：S3/S4 归还一份，S5 只在下层返回真时同步调用传入回调。 */
static inline int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
	if (refcount_dec_and_test(&kref->refcount)) {
		release(kref);
		return 1;
	}
	return 0;
}
```


这里直接消费[减并检测](refcount.h.md#1.3_旧值决定归零与异常分支)的返回值：true 才调用 release，返回 1 表示这次已经调用；返回 0 不能证明自己还拥有引用，也不承诺对象仍可访问。饱和或异常情况下下层也可能返回 false，因此不能把 0 总解释成“剩余计数为正”。

release 收到的是原 kref 参数，地址不被转换；外层类型封装通过已知成员还原对象。回调可安排进一步清理，返回 1 不是通用“物理内存此刻必定已 free”断言。此函数没有 __must_check 属性，因为归零回调已在内部兑现；底层的 bool 判定则由本函数正确消费。

## 1.5\_读取快照不新增责任

```c
/** @brief 仓库阅读说明：将底层瞬时值返回给观察者，不取得引用或独占权。 */
static inline unsigned int kref_read(const struct kref *kref)
{
	return refcount_read(&kref->refcount);
}
```


const 只限制此参数的写法，不阻止其他执行路径更新。调用前仍要保活成员地址，返回值不能用于“先看大于零再 get”的无保护查找。底层读取见[设置与观察](refcount.h.md#1.1_设置与观察)。

返回[普通引用模块](../../../navigation/P02_普通引用与归零回调导读.md#2.2_把S0到S5落到状态地址)或[源码总索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
