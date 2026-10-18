---
id: research.uboot.arm_entry_navigation
title: "ARM入口与异常向量导读"
kind: source
status: evolving
domains:
  - boot
  - arm
  - source_reading
---

# 第6章\_ARM入口与异常向量导读

[入口正文](../../../../knowledge/system_software/uboot/P06_从复位入口到异常向量与主核交接.md) 已经区分复位、镜像交接与异常向量，也说明系统寄存器和共享释放地址是不同状态载体。这里沿 [固定版本总索引](README.md) 将这些对象放回 U-Boot v2022.10 的源文件，目标是判断“当前分支到底建立了什么前提”，不是按汇编文件行号背诵初始化顺序。

## 6.1\_从E0到E3找到实际状态写入者

先固定镜像角色和编译配置，再读入口文件。`CONFIG_SPL_BUILD` 可以省去异常向量设置，`ARMV8_MULTIENTRY` 改变选核过程，前级是否已做低级初始化又影响跳过分支。一个标签出现在原文件里，不证明当前镜像包含或执行了它。

| 正文阶段 | 要追踪的状态与源文件 | 唯一实现说明 |
| --- | --- | --- |
| [E0～E1](../../../../knowledge/system_software/uboot/P06_从复位入口到异常向量与主核交接.md#6.4_入口环境是多个独立条件共同成立)：当前权限 | `start.S` 调 `macro.h`，读取 CurrentEL；真正降级另由 `transition.S` 和宏完成 | [分支与降级](../source_explanations/P06_ARM入口与异常向量实现.md#6.1_读取当前异常级与改变异常级是两种操作) |
| E1：异常进入后保存到哪里 | VBAR 指向 `exceptions.S`，硬件返回寄存器与栈帧共同承载现场 | [固定槽位与软件保存](../source_explanations/P06_ARM入口与异常向量实现.md#6.2_固定向量槽位如何连接软件保存现场) |
| E1：入口环境由谁保证 | ARMv7 与 ARMv8 `start.S` 的条件路径和前级合同 | [缓存、模式与专用控制](../source_explanations/P06_ARM入口与异常向量实现.md#6.3_不能把ARMv7的清理动作套到ARMv8入口) |
| E2～E3：谁继续，谁等待 | 主核进入 crt0；等待桩读释放单元，`spin_table.c` 将地址和保留范围写进设备树 | [共享释放状态的消费者](../source_explanations/P06_ARM入口与异常向量实现.md#6.4_等待核怎样消费共享释放地址) |

各行都映射到正文同一组 E0～E3，E3 再回到 [栈与 GD 交接模块](P04_栈与GD及分配器交接导读.md)。不要在看到 `_main` 时就认为所有 C 状态已建立；它恰好是继续准备这些状态的入口。

## 6.2\_规范布局与产品入口如何分别核对

异常向量的槽位来自 Arm 规范，当前运行时填了哪些槽位则来自 U-Boot 源码。检查“少了后八项”时，要同时阅读该文件对运行范围的声明；检查“ARMv8 都设置这个位”时，要分辨架构寄存器与实现定义寄存器。规范解决允许的行为，源码解决选定版本做了什么，板级运行证据才解决当前机器实际上走了哪条路。

正文已经本地列出 CurrentEL、SPSel、DAIF、SCR/HCR、VBAR、ARMv7 入口控制和 MPIDR 的必要字段，以及十六槽向量表。规范身份为 DDI0487 G.b（2021）与 DDI0406 C.d；原件登记沿 [ARM 资料索引](../../../../reference/external_resources/arm/README.md)。布局实验只验证可重定位对象中的偏移和对齐，没有执行 ARM 指令，也没有验证缓存、异常返回或多核发布。

下列原件均来自官方 `u-boot/u-boot` 的提交 `4debc57a3da6c3f4d3f89a637e99206f4cea0a96`，按上游目录保留；字节摘要用于核对，没有把中文注释写回原件。两个 `start.S` 已在前序模块登记，本模块补齐共用宏、向量与等待实现。

| 上游相对路径与本地原件 | SHA-256 |
| --- | --- |
| [arch/arm/include/asm/macro.h](../upstream/v2022.10/arch/arm/include/asm/macro.h) | `184b19fe0cbfc2bf9160f5ec05947a9dad60c3d08bbbfcfe6e5d404ba1f6b7a1` |
| [arch/arm/cpu/armv8/exceptions.S](../upstream/v2022.10/arch/arm/cpu/armv8/exceptions.S) | `8cd758ece064f47a20647bd0db60cffa8f484f2b56359a9ebe20e4715e9df8ce` |
| [arch/arm/cpu/armv8/transition.S](../upstream/v2022.10/arch/arm/cpu/armv8/transition.S) | `5307a668b66e689342eac3b72dc858b3ac1f93f8272251e66191ee5f7bb7d9c6` |
| [arch/arm/cpu/armv8/spin_table_v8.S](../upstream/v2022.10/arch/arm/cpu/armv8/spin_table_v8.S) | `91f2ce8a160fcda23574d7387022f5a6f96a9f6d6c006ca0f6776f9e1590d9c1` |
| [arch/arm/cpu/armv8/spin_table.c](../upstream/v2022.10/arch/arm/cpu/armv8/spin_table.c) | `0b6bd38855ff8ebaa11e3668fa5ba601862641f9f0bb0cee67761c68b2e21503` |

对应上游入口为 [固定提交的 arch/arm](https://github.com/u-boot/u-boot/tree/4debc57a3da6c3f4d3f89a637e99206f4cea0a96/arch/arm)。遇到厂商 2016/2017 SDK 的类似片段时，先核对其配置与真实文件，不用上游 2022.10 的函数名替它证明分支一致。
