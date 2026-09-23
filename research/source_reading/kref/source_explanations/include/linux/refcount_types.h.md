---
id: research.kref.implementation.refcount_types
title: "refcount_types.h引用存储定义"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_refcount\_types.h引用存储定义

固定来源为 NXP linux-imx，发布 lf-6.12.20-2.0.0，提交 dfaf2136deb2af2e60b994421281ba42f1c087e0（Linux 6.12.20）。以下中文 Doxygen 为仓库补充，函数或宏主体保持该提交内容。

上游位置 include/linux/refcount_types.h，blob 162004f06edf7c3049bac7c960e2e50a190595d6。

## 1.1\_原子存储字段

```c
/** @brief 仓库阅读说明：引用计数类型包含 atomic_t 存储；规则由操作函数执行。 */
typedef struct refcount_struct {
	atomic_t refs;
} refcount_t;
```

这只是存储层，不含单独的饱和布尔值。正常值、零和异常区由引用操作解释；类型定义本身不会跟踪指针、检查所有者或恢复失效地址。P02 八位模型另有显式布尔位，是为了隔离教学问题，不能拿它代替这里的实际布局。

[普通引用原语](refcount.h.md#1.2_普通增加与异常检测)在这个 refs 地址上执行原子操作。返回[普通引用模块](../../../navigation/P02_普通引用与归零回调导读.md#2.2_把S0到S5落到状态地址)或[源码总索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
