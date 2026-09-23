---
id: research.source_reading.rbtree.container_of_implementation
title: "container_of.h 成员地址还原实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_container\_of.h成员地址还原实现

上游位置 `include/linux/container_of.h`，证据为 NXP `linux-imx` 固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`（Linux 6.12.20），[原文件](../../../../linux/include/linux/container_of.h)保持原文。先读[布局与所有者导读](../../../navigation/P07_节点布局与编码状态导读.md#7.4_从嵌入成员回到所有者)，再对应本页 GNU C 语句。这里的编译期设施不受 Tiny/Tree RCU 选择影响，也不提供任何一种读侧保护。

## 1.1\_一次还原中的求值与类型检查

以下中文 Doxygen 为仓库补充；宏体保持固定版本写法。

```c
/**
 * container_of - 仓库阅读说明：从真实嵌入成员的地址还原外围对象。
 * @ptr: 仍存活对象的成员地址；不能凭这个宏验证寿命或所属类型。
 * @type: 实际外围结构体类型。
 * @member: ptr 实际指向的那个成员名。
 * GNU C 语句表达式返回最后一个表达式；普通入口会丢失 const。
 */
#define container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	static_assert(__same_type(*(ptr), ((type *)0)->member) ||	\
		      __same_type(*(ptr), void),			\
		      "pointer type mismatch in container_of()");	\
	((type *)(__mptr - offsetof(type, member))); })
```

实际地址先保存到局部 `__mptr`。类型检查中的 `__same_type` 比较指向对象与成员的类型，另允许 void 情形；它不搜索所有者登记表，也不读取一枚运行时类型标签。`((type *)0)->member` 在这里用于获得成员类型，不是允许运行时解引用空指针。普通结构成员情形下 ptr 的运行时求值发生在赋值处，不能把类型检查中的外形误读为又执行了两次取指针操作。

随后 `offsetof` 给出目标类型的成员偏移，GNU C 的 void 指针算术按字节减去它，再转换到外围指针类型。这是本版本的 GNU C 实现，不能直接宣称为任意 ISO C/C++ 编译器可移植代码。偏移必须与传入成员匹配，外层对象必须仍存活；若两个 rb_node 成员类型相同，写错成员名不会触发这项类型检查。

调用上下文：rbtree 的 [rb_entry](rbtree.h.md#1.11_父地址与业务地址的两种还原)转发到这里。宏不读取节点父色或孩子字段，不改树、不取得引用、不验证成员属于哪棵树；T0 私有对象和 T2 在树对象都可以还原，只要对象与成员前提成立。把节点摘除不会使还原失效，释放外围对象则会使旧地址失效。

另一调用上下文是[kref 回调地址](../../../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.9_container_of_是理解_kref_的关键)：归零回调收到 ref 地址，工作回调收到 work 地址，分别用实际成员偏移还原同一请求。它们共用本标题的宏体；非首成员布局进一步排除把成员指针直接当分配块起点的错误。

可修改性：不能把 offsetof 换成“前面字段大小的和”，因为结构体可能插入对齐填充；不能把成员类型检查当成所有者验证；也不能用去掉断言的改写掩盖实参类型错误。正文[双成员实验](../../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_两个嵌入成员还原同一个任务)直接包含固定宏，宿主适配只提供其编译期依赖。

## 1.2\_只读限定由哪个入口保留

以下中文 Doxygen 同样为仓库补充。

```c
/**
 * container_of_const - 仓库阅读说明：按传入指针类型保留外围 const。
 * @ptr: 成员地址，寿命和成员匹配前提与普通入口相同。
 * @type: 实际外围类型。
 * @member: 实际成员名。
 * _Generic 选择结果类型；这不等于给对象取得引用或冻结并发写入。
 */
#define container_of_const(ptr, type, member)				\
	_Generic(ptr,							\
		const typeof(*(ptr)) *: ((const type *)container_of(ptr, type, member)),\
		default: ((type *)container_of(ptr, type, member))	\
	)
```

普通入口先转成 void 指针，最后返回 `type *`；固定源码对此明确警告 const 丢失。这个变体用 `_Generic` 根据传入指针的类型选择返回 `const type *` 或 `type *`，选中分支仍复用 1.1 的地址计算。它不表示对所有限定符进行任意传播，也不让一个已经释放的只读指针恢复有效。

如果把该入口换回普通 rb_entry，编译器看到的结果可以是非 const；这不能作为修改实际 const 对象的许可。调用方的同步、引用与成员身份仍需另行证明。返回[模块导读](../../../navigation/P07_节点布局与编码状态导读.md#7.4_从嵌入成员回到所有者)或[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。
