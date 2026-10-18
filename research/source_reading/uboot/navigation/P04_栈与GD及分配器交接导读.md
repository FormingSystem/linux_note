---
id: research.uboot.early_runtime_navigation
title: "栈与GD及分配器交接导读"
kind: source
status: evolving
domains:
  - boot
  - arm
  - memory
---

# 第4章\_栈与GD及分配器交接导读

本模块回答“第一次 C 调用凭什么成立，以及它留下的状态怎样交给后期环境”。[正文](../../../../knowledge/system_software/uboot/P04_先建立栈与GD再进入可用的C环境.md) 已用地址图和实验建立模型；这里从 [源码总索引](README.md) 的固定基线，沿 U-Boot v2022.10、提交 `4debc57a3da6c3f4d3f89a637e99206f4cea0a96` 追踪真实写入者和消费者。

## 4.1\_先跟踪地址的主人\_再看函数调用

主阶段早期使用一份位于当前可写 RAM 的 GD。汇编保存其地址，C 函数写字段，后来的汇编再读取这些字段切换环境。这里的通信依赖同一启动路径对内存与寄存器的有序读写，没有消息队列或“GD 后台线程”。最容易漏掉的边界是：计算出地址、写出内容、切换指针，分别是三个动作。

| 公共阶段 | 要追踪的状态与动作 | 源码入口 | 唯一实现 |
| --- | --- | --- | --- |
| [S0](../../../../knowledge/system_software/uboot/P01_从链接布局到启动阶段的交接.md#1.3_完整_U-Boot_的数据为什么分几次才能使用) | 旧 SP、返回的 GD 基址、新 SP、r9/x18 | `crt0.S` / `crt0_64.S` → `common/init/board_init.c` | [计算和真正切栈](../source_explanations/P04_早期运行环境与GD交接实现.md#4.1_S0_计算边界与实际修改SP必须分开) |
| [S1](../../../../knowledge/system_software/uboot/P01_从链接布局到启动阶段的交接.md#1.3_完整_U-Boot_的数据为什么分几次才能使用) | `malloc_base/limit/ptr` 从初始化走到分配消费 | `common/dlmalloc.c` → `common/malloc_simple.c` | [游标和完整堆](../source_explanations/P04_早期运行环境与GD交接实现.md#4.2_S1与S5_分配游标和完整堆分别初始化) |
| [S1 → S2](../../../../knowledge/system_software/uboot/P01_从链接布局到启动阶段的交接.md#1.3_完整_U-Boot_的数据为什么分几次才能使用) | 新 GD 的预留、内容复制、SP/GD 指针切换 | `common/board_f.c` → ARM crt0 | [复制与切换分离](../source_explanations/P04_早期运行环境与GD交接实现.md#4.3_S1到S2_复制GD与切换GD是两个动作) |
| [S5](../../../../knowledge/system_software/uboot/P01_从链接布局到启动阶段的交接.md#1.3_完整_U-Boot_的数据为什么分几次才能使用) | 标志改变、完整堆范围建立、后期分配 | `common/board_r.c` → `common/dlmalloc.c` | [分流与初始化窗口](../source_explanations/P04_早期运行环境与GD交接实现.md#4.2_S1与S5_分配游标和完整堆分别初始化) |

读第一行时先在汇编中找到保留区函数的返回点，再进入函数体，避免把函数名里的 alloc 当成 SP 已改变。读第二行时固定三个字段的量纲：基址是地址，limit 是长度，ptr 是偏移。读第三行时同时观察旧、新 GD 的地址以及仍存活的指针指向谁。读第四行时按初始化项的真实先后检查，不以标志名称代替调用顺序。

初始化项返回错误时，序列停止，调用者进入 `hang()`；已经执行的外设配置不会自动回滚。SPL 另有 `board_init_f/r` 和可选栈/GD 迁移，需先从 [构建上下文导读](P02_构建上下文与入口选择导读.md) 确认选中哪个定义，再判断当前环境。主体复制、地址修正与 BSS 的完整过程继续引用 [布局模块](P01_ARM32链接布局与启动交接阅读索引.md)，不把 GD 的迁移冒充全部启动工作。

## 4.2\_原件身份与实验边界

新增原件取自 [固定提交树](https://github.com/u-boot/u-boot/tree/4debc57a3da6c3f4d3f89a637e99206f4cea0a96)，以原始字节保存。已有 `board_f.c`、`board_r.c`、`board_init.c`、ARM32 crt0 及 SPL 公共代码在前两模块登记；这里补足本轮实际依赖的类型、接口和分配实现。

| 上游相对路径及本地原件 | SHA-256 |
| --- | --- |
| [include/asm-generic/global_data.h](../upstream/v2022.10/include/asm-generic/global_data.h) | `d416fd596169f7d97b91f42a92956e520274ea7c047ccc7e2221898c2db270b4` |
| [arch/arm/include/asm/global_data.h](../upstream/v2022.10/arch/arm/include/asm/global_data.h) | `f7a3fd9058e075a6f78f35bf1de99044706a8f15cc0463dab050788e9b7c9f3f` |
| [common/malloc_simple.c](../upstream/v2022.10/common/malloc_simple.c) | `d541abdea01c0ae381c274e0d852c78f74b9a2e7a4f40da788f5cb6d3323810b` |
| [common/dlmalloc.c](../upstream/v2022.10/common/dlmalloc.c) | `e1e5526b5a4196d99773f28f2a0292f063244a06b0a78fb180aa97603f0a03d2` |
| [arch/arm/lib/crt0_64.S](../upstream/v2022.10/arch/arm/lib/crt0_64.S) | `1920ac699a072ac1f15d74ad614663ab38112122d0b2d750b696420443a9d676` |
| [include/init.h](../upstream/v2022.10/include/init.h) | `5cc40b2c0b8fa7845115c04b100f8f95d90229720ea61f35c8489172e40802d1` |
| [include/initcall.h](../upstream/v2022.10/include/initcall.h) | `3aefb87b9399c40d4d3db292f8c7398a54d28700ac3febac84cef4236189a17f` |
| [include/malloc.h](../upstream/v2022.10/include/malloc.h) | `266fd5e95927e720c1ae0a5e6fe29e34ddfce799c903523080813b2d683f6363` |
| [include/env_internal.h](../upstream/v2022.10/include/env_internal.h) | `64f90217db92003b25d118b71226102166884e75d300d78c6e5d6df30f518cca` |

正文的宿主 C 实验验证地址区间、局部清零、对齐消耗和容量失败，额外加入了范围检查。它没有执行 ARM 的 SP/GD 寄存器切换，没有调用上游完整分配器，也不证明目标板 RAM 已可靠初始化。调试真实启动时，应在保留区函数返回、GD 复制完成、寄存器切换后及完整堆建立后分别观察地址与字段，避免只看最后一次串口输出猜测失败位置。
