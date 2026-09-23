---
id: research.kref.implementation.atomic_initializer
title: "types.h原子成员初始化"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_types.h原子成员初始化

固定来源为 NXP linux-imx，发布 lf-6.12.20-2.0.0，提交 dfaf2136deb2af2e60b994421281ba42f1c087e0（Linux 6.12.20）。上游位置 include/linux/types.h，blob 2bc8766ba20cab014a380f02e5644bd0d772ec67。以下中文 Doxygen 为仓库补充。

## 1.1\_整数外还有一层结构

```c
/** @brief 仓库阅读说明：原子计数存储是包含 counter 的结构，不是 int 别名。 */
typedef struct {
	int counter;
} atomic_t;

/** @brief 仓库阅读说明：为这一层结构提供带花括号的成员初始值。 */
#define ATOMIC_INIT(i) { (i) }
```

在本版本，[refcount_t](refcount_types.h.md#1.1_原子存储字段) 的 refs 是 atomic_t，最终整数位于 ref.refcount.refs.counter。ATOMIC_INIT 不执行原子读改写，也没有发布屏障；它只是让 C 初始化器与存储层次对应。实际并发操作要通过原子接口，不能因为看到了 counter 就直接写它来修改活对象。

[REFCOUNT_INIT](refcount.h.md#1.4_逐层构造初始值) 再包住本层，最外层由 [KREF_INIT](kref.h.md#1.6_定义对象时建立计数) 提供。初始化不会分配内存、选择清理函数或延长局部变量寿命。返回[初始化模块](../../../navigation/P02_普通引用与归零回调导读.md#2.6_初始化形式与存储寿命)或[总索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
