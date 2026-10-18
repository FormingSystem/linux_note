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

## 1.4\_读书笔记保留交接任务\_不再复制完整启动教材

随后重写 [读书笔记 P04](../../publications/books/running_linux_kernel/P04_uboot启动流程说明.md)。其旧稿保存在提交 `2cc51748` 的同一路径，共 981 行；重复分成七轮生成回答，夹有未成立的通用化结论。文件、稳定 ID 和人工评审状态不变，不把重写等同于新的人工确认。

| 原内容单元 | 改后去向与处理 |
| --- | --- |
| BootROM、TPL、SPL、proper 的五视角与多幅重复总图 | 本篇 4.1 用资源约束和可选拓扑保留桥接认识；具体构建、栈/GD、加载与入口进入 U-Boot 权威专题，不再复写源码教材 |
| proper 搬迁、PIE、RELA 与地址解释 | 本篇 4.2 保留交接需要的最小结论，精确链接 P07；删除把 `_TEXT_BASE` 写成运行时目标、所有 SPL 固定 SRAM、所有 proper 必须搬迁等错误 |
| Falcon 与 Linux 参数交接 | 本篇 4.3～4.4 新增固定 Linux 协议的本地表、主次 CPU 区分、参数快照和恢复选择；不再混用 `r0` 与 `x0`，不把关闭 MMU 扩大成整个引导期间的常态 |
| 32 位用户范围、LPAE、大应用与 AArch64 | 本篇 4.5 保留虚拟/物理范围、多进程、窗口映射和配置选择的因果解释；纠正 `mmap` 自动突破虚拟范围、固定 3 GiB、AArch64 固定 128 TiB 用户区和用多进程解释所有应用设计等说法 |
| 排障建议、启动选择与总结 | 本篇 4.2 的 H0～H3、4.6 判断题及解析；把无条件关缓存/刷缓存改为按入口协议定位，未执行硬件启动不作成功证明 |

本次没有从旧稿继承未经来源支持的 Android 产品要求，也没有用别的架构地址数值替代 ARM 配置。Linux 协议和配置原件保存到既有源码基线，Falcon 文档保存到固定 U-Boot 原件目录；原稿可由上述提交恢复用于逐项比较。

## 1.5\_内核引导旧长稿的独有信息逐项归位

[读书笔记 P03](../../publications/books/running_linux_kernel/P03_内核引导和初始化.md) 原件以提交 `773dd8312cc738b54589f05166186f083e61fd65` 为界，共 4031 行，SHA-256 为 `876646157e7ec59dcc9ec1f1928e90515beecc01b951a1e8bd920ce4ebb5280e`。重构保留文件、文档 ID 与既有人工评审状态；正文承担从复位到 PID 1 的桥接，独有内容先落地再收拢重复问答。

| 原稿行区间与内容 | 完整说明去向 | 本次处理 |
| --- | --- | --- |
| 1～625、1709～2455：入口、异常级、早期 C 与 GD | U-Boot [P02](../../knowledge/system_software/uboot/P02_从构建上下文找到实际进入镜像的函数.md)、[P04](../../knowledge/system_software/uboot/P04_先建立栈与GD再进入可用的C环境.md)、[P06](../../knowledge/system_software/uboot/P06_从复位入口到异常向量与主核交接.md) | 固定源码和配置语境；不继承未证实的 2018 年版本归属、固定零入口、伪造打印或不受支持的返回流程 |
| 626～1363、3337～4031：地址空间、搬迁与 GD 重复讨论 | U-Boot [P05](../../knowledge/system_software/uboot/P05_为什么需要SPL以及它怎样交出下一阶段.md)、[P07](../../knowledge/system_software/uboot/P07_镜像搬迁为什么不等于改变内存映射.md)、读书 [P04](../../publications/books/running_linux_kernel/P04_uboot启动流程说明.md) | 区分装载另一镜像、自身复制、重定位、地址映射与内存生命周期；固定 RELA 数值模型验证重复加偏移错误 |
| 1364～1708：32 位地址范围、Windows、GPU、多进程与窗口 | [地址空间与容量](../../knowledge/foundations/computer_architecture/address_space_and_capacity.md) | 独立说明并提供 C 窗口模型；纠正固定 3 GiB、虚拟范围与物理容量混用等结论 |
| 2456～2860：主循环、延时、命令选择与执行 | U-Boot [P10](../../knowledge/system_software/uboot/P10_自动引导如何选择命令与保留维护入口.md) 及对应源码单元 | 保留无人值守与维护窗口的问题，核对负延时、零延时、菜单键、控制树及 CLI 返回边界 |
| 2861～2916：bootm 与内核交接调用图 | U-Boot [P11](../../knowledge/system_software/uboot/P11_内存镜像如何变成Linux入口现场.md)、[历史图说明](../../research/investigations/uboot_vendor_baselines.md#1.6_读书笔记的旧启动图能证明到哪一步) | 新增镜像范围、设备树修订、清理与跳转的完整周期；保留原图并限定版本证据 |
| 2917～2941：未展开的内核初始化标题 | 读书 P03 的 K0～K4 | 补公共初始化、完成量交接、initcall、初始化内存生命周期及用户入口选择；不是用新提纲替代原空缺 |
| 2942～3036：NOR、NAND、eMMC、DDR 比较 | [存储介质与取指路径](../../knowledge/foundations/computer_architecture/boot_storage_paths.md) | 从接口与访问过程推出 XIP、装载和工作内存，补协议条件，不保留无依据的绝对速度及成本排行 |
| 3037～3244：PIC、PIE、GOT、PLT、REL/RELA | [7.3.1 位置无关构建的责任边界](../../knowledge/system_software/uboot/P07_镜像搬迁为什么不等于改变内存映射.md#7.3.1_PIC与PIE减少哪些固定引用_又留下哪些修正责任) 与 7.4 | 分开代码生成、链接产物、相对寻址与槽内地址；补内核 RELOCATABLE 和 RANDOMIZE_BASE 的能力/策略区别 |
| 3245～3336：WFI/WFE 与旧空闲代码 | U-Boot [P06](../../knowledge/system_software/uboot/P06_从复位入口到异常向量与主核交接.md) | 按等待条件、事件和允许的实现行为解释，不把指令名直接当作固定睡眠深度、缓存关闭或功耗保证 |

新正文的启动时序明确分开 PID 1 先等待和完成先发布两条交错。Linux 公共流程使用仓库固定 `init/main.c`，未重复维护既有 RCU 单元中相同函数的实现裁剪。模型、局部源码用例和文档检查不替代真实板上的固件、驱动或用户空间启动验证。
