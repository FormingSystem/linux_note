---
id: research.source_reading.error_pointer.index
title: "Linux 6.12 错误指针源码阅读索引"
kind: source
status: evolving
domains:
  - linux
  - source_reading
---

# 第1章\_Linux\_6.12\_错误指针源码阅读索引

当一个资源获取函数返回失败时，至少有三处源码值得分开看：它怎样表示失败，调用者怎样据此选择路径，已取得的资源怎样释放。本索引把这三个问题连接起来，避免从 `ERR_PTR` 的名字直接跳到“整个驱动已经回滚”的结论。

## 1.1\_固定版本与证据粒度

本组证据来自 NXP 官方 `https://github.com/nxp-imx/linux-imx.git`，来源分支 `lf-6.12.y`，发布标签 `lf-6.12.20-2.0.0`，固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，Linux 6.12.20。证据身份见[源码基线](../../linux/SOURCE_BASELINE.md#1.1_当前来源)。本地实验 HEAD 不作为实现依据。

`err.h` 的编码属于通用内核代码；数值比较采用目标内核数据模型，不能代替其他体系结构的地址布局证明。本次目标配置启用 ARM、MODULES、GPIOLIB、COMMON_CLK、REGULATOR、PINCTRL、I2C，未启用 KASAN；这只是当前工作树配置，不是发布标签携带的配置。实验源码不依赖这些硬件子系统，只依赖平台设备模型、模块和 devres。

## 1.2\_按问题进入源码

| 问题 | 模块导读或实现入口 | 上游位置与阅读边界 |
| --- | --- | --- |
| 值中有没有错误对象 | [编码与还原](../source_explanations/P01_err.h_错误值编码与检查.md#1.2_编码与还原) | `include/linux/err.h`；只转换值，无对象分配 |
| NULL、-4096 与错误范围怎样区分 | [区间识别](../source_explanations/P01_err.h_错误值编码与检查.md#1.3_区间识别与空值组合) | 同一头文件；不读取地址内容 |
| 便利转换保留或丢掉了什么 | [传播帮助接口](../source_explanations/P01_err.h_错误值编码与检查.md#1.4_传播帮助接口与类型注解) | 同一头文件；不自动检查所有权 |
| devm 的失败表示与释放有何关系 | [返回与登记](P02_返回值与清理路径导读.md#2.2_比较两种申请路径) | `drivers/base/devres.c` 与 `drivers/gpio/gpiolib-devres.c` |
| probe 失败在哪一层变号、何时清理 | [失败路径](P02_返回值与清理路径导读.md#2.3_跟随驱动失败而不混用返回类型) | `drivers/base/dd.c` 与 `drivers/base/core.c` |
| Rust 包装与日志能证明什么 | [其他表示与观测](P02_返回值与清理路径导读.md#2.4_其他表示与观测入口) | `rust/kernel/error.rs`、`lib/vsprintf.c` |

`lib/errname.c` 的错误名查询与 `lib/errseq.c` 的错误序列记录是其他职责，不能把它们拼成不存在的指针转换实现 `lib/err.c`。本组没有为它们建立虚假的源码讲解。

## 1.3\_回到应用与验证

需要先重建稳定模型时返回[教材大纲](../../../../knowledge/linux/error_handling/error_pointer/大纲.md)。[完整实验](../../../../knowledge/linux/error_handling/error_pointer/P04_观察返回值与释放顺序.md)用四条路径观察控制流和 devres 释放标记；源码核对与静态编译不等于目标内核实验已经执行。
