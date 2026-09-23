---
id: research.kref.implementation.must_check
title: "compiler_attributes.h返回值检查属性"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_compiler\_attributes.h返回值检查属性

固定来源为 NXP linux-imx，发布 lf-6.12.20-2.0.0，提交 dfaf2136deb2af2e60b994421281ba42f1c087e0（Linux 6.12.20）。以下中文 Doxygen 为仓库补充，函数或宏主体保持该提交内容。

上游位置 include/linux/compiler_attributes.h，blob c16d4199bf9231b8aa8e08d6c8174247b11da82c。

## 1.1\_返回值诊断不是自动清理

```c
/** @brief 仓库阅读说明：要求编译器诊断丢弃结果的调用点。 */
#define __must_check                    __attribute__((__warn_unused_result__))
```

编译器支持且相应诊断有效时，未消费返回值会产生警告；是否作为错误还受编译选项影响。它不会把返回 true 自动变成 free，也不验证调用方 if 分支里的清理代码正确。

refcount_dec_and_test 的 bool 是后续是否清理的重要证据。kref_put 内部已经检查并执行传入回调，因此它自己的 int 返回没有该属性。不能把“底层必须检查”推广成“所有带返回值的 kref 调用都必须再写 if”。

返回[普通引用模块](../../../navigation/P02_普通引用与归零回调导读.md#2.2_把S0到S5落到状态地址)或[源码总索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
