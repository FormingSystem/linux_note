---
id: research.kref.implementation.signed_wrap
title: "compiler_types.h有符号回绕插桩属性"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_compiler\_types.h有符号回绕插桩属性

固定来源为 NXP linux-imx，发布 lf-6.12.20-2.0.0，提交 dfaf2136deb2af2e60b994421281ba42f1c087e0（Linux 6.12.20）。以下中文 Doxygen 为仓库补充，函数或宏主体保持该提交内容。

上游位置 include/linux/compiler_types.h，blob 639be0f30b455d7b42adc26701fb47093012a1b8。

## 1.1\_检查器属性与构建选项分工

```c
/** @brief 仓库阅读说明：仅在相应配置下禁止此函数的有符号溢出检查插桩。 */
#ifdef CONFIG_UBSAN_SIGNED_WRAP
# define __signed_wrap __attribute__((no_sanitize("signed-integer-overflow")))
#else
# define __signed_wrap
#endif
```

启用 CONFIG_UBSAN_SIGNED_WRAP 时，属性告诉支持它的编译器对该函数不做 signed-integer-overflow sanitizer 检查；未启用时宏为空。它不做计数运算，不替对象加锁，也不是把任意标准 C 有符号溢出改为合法回绕的语言开关。

内核的相关编译语义还依赖[顶层构建选项](../../Makefile.md#1.1_优化选项与函数属性分开核对)。把引用 helper 单独复制到默认宿主编译命令，然后只定义这个属性，不能宣称已经复现内核构建。对于编译器不支持、配置未启用或其他检查器，不能从此宏名字推断所有检查被关闭。

返回[普通引用模块](../../../navigation/P02_普通引用与归零回调导读.md#2.2_把S0到S5落到状态地址)或[源码总索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
