---
id: governance.migration.uboot_textbook_migration
title: "U-Boot长稿拆分与信息单元迁移记录"
kind: reference
status: evolving
domains:
  - governance
  - boot
---

# 第1章\_U-Boot长稿拆分与信息单元迁移记录

本记录对应全仓教材重构中的 U-Boot 单元。旧长稿在提交 `d53d5a936389ed6dda3976168ef8321529121fb2` 中保持可追溯，原路径为 `knowledge/system_software/uboot/uboot-makefile.md`。原文件 8081 行，夹有重复转录、提问、生成回答与不同版本源码；新正文按学习问题拆分，不能把“文件变短”作为完成依据。

旧文件的稳定 ID 保留给 [专题大纲](../../knowledge/system_software/uboot/大纲.md)，不保留第二份权威长教程。原有人工评审标记按已有节点继承，不因本次自动或 AI 检查升级新章节；新增章节与历史证据记录均从未校正开始。

## 1.1\_每组有效内容迁往哪里

以下行号仅用于定位上述不可变提交中的原文。目的位置都是真实正文或研究记录，不能用工作缓存替代长期入口。

| 旧稿范围与内容 | 目的位置 | 迁移处理与关键修正 |
| --- | --- | --- |
| 1～218：学习目标、SoC 资源与早期内存 | [大纲](../../knowledge/system_software/uboot/大纲.md)、[P01](../../knowledge/system_software/uboot/P01_从链接布局到启动阶段的交接.md)、[历史记录](../../research/investigations/uboot_vendor_baselines.md) | 保留重新建立完整运行认识的目标及两张资源图；不再断言 ROM 不可能初始化 DDR，也不泛化 SRAM 或 SPL 的固定大小 |
| 220～1078：来源、工具链、配置和 Make 基础 | [P02](../../knowledge/system_software/uboot/P02_从构建上下文找到实际进入镜像的函数.md)、[P03](../../knowledge/system_software/uboot/P03_从板配置到可核对的构建产物.md) | 以可运行 Make 实验说明变量和阶段上下文；区分宿主与目标工具，去掉按源码年份选择编译器的规则 |
| 1079～2100：目标、入口对象、链接脚本与镜像 | [P01](../../knowledge/system_software/uboot/P01_从链接布局到启动阶段的交接.md)、[P03](../../knowledge/system_software/uboot/P03_从板配置到可核对的构建产物.md)、[历史构建图](../../research/investigations/uboot_vendor_baselines.md#1.3.3_构建依赖图不能读成处理器执行时序) | 保留对象集合、链接入口、DTB 和产物依赖；构建期 STATIC_RELA 与运行时重定位分开 |
| 2102～3230：ARM32 入口、栈/GD、板初始化与加载 | [P04](../../knowledge/system_software/uboot/P04_先建立栈与GD再进入可用的C环境.md)、[P05](../../knowledge/system_software/uboot/P05_为什么需要SPL以及它怎样交出下一阶段.md)、[P06](../../knowledge/system_software/uboot/P06_从复位入口到异常向量与主核交接.md) | 保留保存前级参数、弱强定义、早期可用对象、BSS、候选设备、失败与入口协议；芯片启动寄存器摘录另在 [平台差量](../../research/investigations/uboot_vendor_baselines.md#1.4.1_启动寄存器说明的是来源线索而不是加载成功) 限定，不把 IMX6DQRM 分支当作 6ULL 能力 |
| 3232～4602：AArch64 入口、异常、低级初始化与搬迁 | [P06](../../knowledge/system_software/uboot/P06_从复位入口到异常向量与主核交接.md)、[P07](../../knowledge/system_software/uboot/P07_镜像搬迁为什么不等于改变内存映射.md)、[旧厂商条件](../../research/investigations/uboot_vendor_baselines.md#1.4_旧启动摘录有哪些不能跨平台套用的差量) | 区分复位标签/地址、EL 分支/降级、路由/屏蔽/访问许可、架构向量/产品槽位；补 CPTR/CPACR 字段，限制 SMPEN 与勘误结论，不虚构厂商提交 |
| 4603～5292：Rockchip 板初始化、SPL 与 BROM | [P05 实现](../../research/source_reading/uboot/source_explanations/P05_SPL装载与下一阶段交接实现.md)、[历史差量](../../research/investigations/uboot_vendor_baselines.md#1.4_旧启动摘录有哪些不能跨平台套用的差量) | 保留计时、早期串口、框架/RAM 获取、板钩子、返回 ROM 的职责；失败 return、hang 和成功交接分开，不按函数名字补写其内部动作 |
| 5294～6259：TPL/SPL、DDR、执行位置与地址视角 | [P05](../../knowledge/system_software/uboot/P05_为什么需要SPL以及它怎样交出下一阶段.md)、[P07](../../knowledge/system_software/uboot/P07_镜像搬迁为什么不等于改变内存映射.md) | 合并重复问答并保留因果问题；解释 DRAM/DDR 分类、当前可执行存储与下一阶段预算；用 6ULL 手册校正 DDR 基址和 OCRAM 别名，避免“搬迁即改映射” |
| 6263～6470：同名函数、阶段宏与实际入口 | [P02 及模块导读](../../research/source_reading/uboot/navigation/P02_构建上下文与入口选择导读.md) | 从源码候选追到配置、对象与 ELF；无阶段宏不等于 proper 专属，子 Make 上下文不能混用 |
| 6474～7968：命令字符串、记录与回调 | [P08](../../knowledge/system_software/uboot/P08_命令名怎样找到镜像里的实现函数.md)、[P09](../../knowledge/system_software/uboot/P09_命令字符串怎样跨过缓冲区与失败边界.md) | 分开登记/解析/执行、返回值/重复能力、分号/换行/环境循环；修正帮助文本是函数、无换行会改只读原串、所有错误都停止等错误 |
| 7977～8048：早期分配区与 GD 位置 | [P04](../../knowledge/system_software/uboot/P04_先建立栈与GD再进入可用的C环境.md) | 用实际保留区公式和宿主实验替代把 `base +=` 当作原函数的示意伪码，区分预算、指针、偏移和完整堆 |

## 1.2\_图片附件和源码没有被同一化

原长稿引用的十一张图片全部保留，并在 [历史证据正文](../../research/investigations/uboot_vendor_baselines.md) 按资源位置、资料定位、配置搜索和构建依赖重新解释。三个配置附件保持原始文件；2022 附件原先没有在长稿正文链接，本次也纳入对照。逐张查看后明确：指南截图可见 `lf_v2025.04`、编辑器截图可见 2022.10、配置头自报 2022.04，三者不能互相替代版本证据。

新源码说明以官方提交 `4debc57a3da6c3f4d3f89a637e99206f4cea0a96` 为界，原件摘要在各模块导读登记。历史摘录只用于描述旧记录的差量，不混入该上游原件目录。重复转录的同一函数不再多处维护，唯一实现入口由 [源码总索引](../../research/source_reading/uboot/navigation/README.md) 连接；硬件寄存器语义同时用对应架构或 SoC 手册核对。

## 1.3\_完成判断和验证限制

此次核对覆盖旧正文、代码块清单、图片、配置和实际入链；各新单元已分别执行内容冷读及适用的宿主实验，详细结果保存在 [本轮执行记录](repository_textbook_refactor.md) 与逐篇库存中。总入口只组织真实章节，不用一个新提纲替代旧内容的重写。

实验包含 Make 上下文与构建依赖、早期分配区、加载回退、向量布局、相对重定位、命令分派和失败传播；它们各自证明宿主模型、对象布局或固定源码局部行为。没有据此宣称已在真实 ARM 板上完成复位、DDR 训练、权限切换、多核释放或操作系统启动。历史厂商 SDK 的完整提交身份也没有被补造。
