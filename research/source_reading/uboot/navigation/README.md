---
id: research.uboot.navigation.index
title: "U-Boot源码阅读索引"
kind: source
status: evolving
domains:
  - boot
  - source_reading
---

# 第1章\_U-Boot源码阅读索引

这里组织带版本的源码阅读，知识正文先建立问题、状态和交接关系；本目录再把这些关系落到特定版本的目录、对象规则和执行路径。当前模块固定上游 U-Boot v2022.10、提交 `4debc57a3da6c3f4d3f89a637e99206f4cea0a96`。它不是所有厂商 SDK 的共同版本，也没有以一个年份替代完整提交身份。

## 1.1\_从读者问题选择模块

| 现在要解释什么 | 模块导读 | 知识正文 |
| --- | --- | --- |
| 链接边界怎样连接 GD、主体复制、重定位与 BSS | [ARM32 布局与启动交接](P01_ARM32链接布局与启动交接阅读索引.md) | [从链接布局到启动阶段的交接](../../../../knowledge/system_software/uboot/P01_从链接布局到启动阶段的交接.md) |
| 同名函数如何被选进不同镜像，入口对象从哪里来 | [构建上下文与入口选择](P02_构建上下文与入口选择导读.md) | [从构建上下文找到实际进入镜像的函数](../../../../knowledge/system_software/uboot/P02_从构建上下文找到实际进入镜像的函数.md) |
| 板模板如何产生配置，ELF 如何继续成为镜像输入 | [配置工具与镜像产物](P03_配置工具与镜像产物导读.md) | [从板配置到可核对的构建产物](../../../../knowledge/system_software/uboot/P03_从板配置到可核对的构建产物.md) |
| 第一段 C 代码依赖谁准备栈，GD 与分配器怎样交接 | [栈与 GD 及分配器交接](P04_栈与GD及分配器交接导读.md) | [先建立栈与 GD 再进入可用的 C 环境](../../../../knowledge/system_software/uboot/P04_先建立栈与GD再进入可用的C环境.md) |
| 早期内存为何促成加载阶段，成功结果如何交给下一镜像 | [SPL 装载与下一阶段交接](P05_SPL装载与下一阶段交接导读.md) | [为什么需要 SPL 以及它怎样交出下一阶段](../../../../knowledge/system_software/uboot/P05_为什么需要SPL以及它怎样交出下一阶段.md) |
| 进入 reset 以后还要建立哪些处理器条件 | [ARM 入口与异常向量](P06_ARM入口与异常向量导读.md) | [从复位入口到异常向量与主核交接](../../../../knowledge/system_software/uboot/P06_从复位入口到异常向量与主核交接.md) |
| 搬镜像与改映射怎样区分，AArch64 指针怎样避免重复修正 | [地址视图与 AArch64 重定位](P07_地址视图与AArch64重定位导读.md) | [镜像搬迁为什么不等于改变内存映射](../../../../knowledge/system_software/uboot/P07_镜像搬迁为什么不等于改变内存映射.md) |
| 用户命令名怎样找到当前镜像的实现函数 | [命令注册与分派](P08_命令注册与分派导读.md) | [命令名怎样找到镜像里的实现函数](../../../../knowledge/system_software/uboot/P08_命令名怎样找到镜像里的实现函数.md) |
| 字符串由谁改写，失败在哪一层阻止后续命令 | [字符串解析与失败边界](P09_命令字符串解析与失败边界导读.md) | [命令字符串怎样跨过缓冲区与失败边界](../../../../knowledge/system_software/uboot/P09_命令字符串怎样跨过缓冲区与失败边界.md) |
| 没有人输入时谁选择命令，返回后是否还有维护入口 | [自动引导策略](P10_自动引导选择与交互退路导读.md) | [自动引导与维护入口](../../../../knowledge/system_software/uboot/P10_自动引导如何选择命令与保留维护入口.md) |
| 已读入的镜像怎样变成内核可接手的字节与参数 | [镜像解析与 Linux 交接](P11_镜像解析与Linux交接导读.md) | [内存镜像与入口现场](../../../../knowledge/system_software/uboot/P11_内存镜像如何变成Linux入口现场.md) |

模块导读保存状态/对象关系、阅读顺序与原件摘要；`source_explanations/` 保存唯一的裁剪实现解释；`upstream/v2022.10/` 保存不加注释的上游原件。要找函数体时从模块的精确锚点进入，不把本索引变成第二份源码讲义。

## 1.2\_证据怎样用于自己的工程

先核对官方来源、不可变提交与板配置，再查看构建产物和运行日志。已有阶段相同、文件名相同或函数名相同，都不能证明两个 SDK 走同一分支。各模块明确区分主机实验、源码求值和目标执行；当前没有完整 U-Boot 或硬件启动验收。[专题大纲](../../../../knowledge/system_software/uboot/大纲.md) 给出连续阅读顺序；[历史配置与图片记录](../../../investigations/uboot_vendor_baselines.md) 单独保存厂商材料的身份和差量，不属于本索引的 v2022.10 原件。

## 1.3\_补充原件的本地查阅入口

以下原件补充启动环境、地址映射与命令分派的查证上下文，仍使用本索引开头的同一固定提交。它们保留上游内容，不添加教学注释；机制说明与裁剪实现继续从上面的模块入口阅读。2026-10-18 已逐字节对照官方固定提交；[来源与 SHA-256 清单](../supplemental_source_manifest.json) 保存这八份文件的地址、字节数和摘要，便于复核。

| 查证上下文 | 本地上游原件 |
| --- | --- |
| ARMv7 低级初始化入口 | [arch/arm/cpu/armv7/lowlevel_init.S](../upstream/v2022.10/arch/arm/cpu/armv7/lowlevel_init.S) |
| ARMv8 地址映射接口 | [arch/arm/include/asm/armv8/mmu.h](../upstream/v2022.10/arch/arm/include/asm/armv8/mmu.h) |
| ARM 处理器系统接口声明 | [arch/arm/include/asm/system.h](../upstream/v2022.10/arch/arm/include/asm/system.h) |
| C 布局生成汇编偏移 | [arch/arm/lib/asm-offsets.c](../upstream/v2022.10/arch/arm/lib/asm-offsets.c) |
| ARM SPL 阶段交接 | [arch/arm/lib/spl.c](../upstream/v2022.10/arch/arm/lib/spl.c) |
| help 命令注册实例 | [cmd/help.c](../upstream/v2022.10/cmd/help.c) |
| 公共功能配置依赖 | [common/Kconfig](../upstream/v2022.10/common/Kconfig) |
| 上游命令开发接口说明 | [doc/develop/commands.rst](../upstream/v2022.10/doc/develop/commands.rst) |
