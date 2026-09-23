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

上游位置 include/linux/kref.h，blob d32e21a2538c292452db99b915b1bb6c3ab15e53。本页展开结构、普通 init/get/put/read 与定义时初始化宏；条件取得和锁组合尚未覆盖。

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

[P03 初始责任](../../../../../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.4.2_kref_init_阶段_创建初始引用)把写入 1 与接口交付谁来归还分开：本函数不记录创建者身份，也不设置业务许可。

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

这里消费的是本次调用传入的 release，并不保存此前调用的函数指针；[回调选择对照](../../../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.24_kref_不保存_release_函数)比较两种结束顺序。release 收到的是原 kref 参数，地址不被转换；外层类型封装通过已知成员还原对象。[部分初始化模块](../../../navigation/P02_普通引用与归零回调导读.md#2.7_新对象在发布之前失败)展示先释放拥有资源再释放外壳，回调必须能处理该类型的失败状态。[容器状态模块](../../../navigation/P02_普通引用与归零回调导读.md#2.8_容器入口与引用状态协作)另解释槽锁与责任配合；本函数没有自动摘除、等待 work 或等待 RCU 的动作。[清理诊断前提](../../../../../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.7.1_release_阶段_对象销毁点)另由类型的节点与资源协议决定。回调可安排进一步清理，返回 1 不是通用“物理内存此刻必定已 free”断言。此函数没有 __must_check 属性，因为归零回调已在内部兑现；底层的 bool 判定则由本函数正确消费。

## 1.5\_读取快照不新增责任

```c
/** @brief 仓库阅读说明：将底层瞬时值返回给观察者，不取得引用或独占权。 */
static inline unsigned int kref_read(const struct kref *kref)
{
	return refcount_read(&kref->refcount);
}
```


const 只限制此参数的写法，不阻止其他执行路径更新。调用前仍要保活成员地址；[快照对照程序](../../../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.17.1_运行快照与持有的对照程序)把正数快照与实际归还责任分开观察。返回值不能用于“先看大于零再 get”的无保护查找。底层读取见[设置与观察](refcount.h.md#1.1_设置与观察)。

返回[普通引用模块](../../../navigation/P02_普通引用与归零回调导读.md#2.2_把S0到S5落到状态地址)或[源码总索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。

## 1.6\_定义对象时建立计数

```c
/** @brief 仓库阅读说明：生成 kref 聚合初始化器；不执行获取、发布或分配。 */
#define KREF_INIT(n)	{ .refcount = REFCOUNT_INIT(n), }
```

调用上下文是对象定义的初始化部分，下一层由 [REFCOUNT_INIT](refcount.h.md#1.4_逐层构造初始值) 填入。宏不保存 release；每次 put 仍由调用者传入符合外层对象清理协议的回调。

初始化值 1 应对应一份真实的初始责任；其他正值要求外层协议提前建立相同数量的责任。值 0 不提供普通 get 的存活前提。此宏也能初始化自动存储对象，宏名不决定存储期；自动对象离开作用域时不会因计数仍为正而保留内存。

裸宏展开为花括号初始化器，不能作为普通赋值表达式右侧使用。C 复合字面量可以构成表达式，但这不使覆盖正在使用的计数成为合法生命周期操作。完整静态模块和归零后的边界见[正文实验](../../../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.14.1_初始化器沿着成员层次填值)，模块状态映射见[初始化形式](../../../navigation/P02_普通引用与归零回调导读.md#2.6_初始化形式与存储寿命)。
