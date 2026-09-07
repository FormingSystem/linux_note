---
id: reference.external_resources.arm
title: "Arm、RK3588 与 i.MX6ULL 外部资料索引"
kind: reference
status: evolving
domains:
  - reference
  - arm
  - rk3588
  - imx6ull
---

# 第1章\_Arm\_RK3588\_与\_i.MX6ULL\_外部资料索引

## 1.1\_索引定位与存储边界

本索引登记学习 Arm A-profile 架构、通用中断控制器（Generic Interrupt Controller，GIC）、RK3588 和 i.MX6ULL 时使用的外部资料。仓库只保存书目信息、上游入口、预期文件名、字节数、页数、SHA-256 和下载工具，不保存这些厂商发布的 PDF 原件。

下载工具把文件写入仓库根目录的 `.cache/private_sources/arm/`。整个 `.cache/` 已被 Git 忽略，因此本地资料不会随提交进入 GitHub。规范正文、芯片手册和本仓库的学习笔记属于不同产物：规范负责定义硬件或架构契约，学习笔记负责解释和引用，二者不能用同一份许可证处理。

机器可读清单见 [manifest.json](manifest.json)，下载与校验工具见 [download_external_resources.py](../../../scripts/download_external_resources.py)。

### 1.1.1\_缓存目录与学习资料组

```text
.cache/private_sources/arm/
├── architecture/
│   ├── a_profile/          # A-profile 规范及历史版本
│   ├── armv7_ar/           # 旧 32 位架构规范
│   └── guides/             # 架构、异常入门资料
├── cores/
│   ├── cortex_a7/
│   ├── cortex_a55/
│   └── cortex_a76/
├── gic/
│   ├── specifications/    # v2、v3/v4 规范，每份只存一处
│   ├── guides/            # 概览、消息中断、虚拟化及历史指南
│   └── implementations/
│       └── gic600/        # 与 RK3588 对应的 r1p6 实现手册
├── soc/
│   ├── nxp/imx6ull/
│   └── rockchip/rk3588/
└── archive/
    ├── unregistered/      # 尚未进入来源清单的本地 PDF
    └── acquisition/       # 旧脚本、日志、元数据与未完成文件
```

目录回答“资料描述什么对象”，清单字段 `profiles` 回答“本次学习选哪些资料”。例如 GICv3 与 GICv4 共用一份规范，分组通过稳定文档编号引用它，不各存一份 PDF。资料分类不是读者必须预先掌握的概念；讲解仍从当前问题逐步引入对象。

2026-09-06 只整理了这批 Arm 缓存：20 份原清单文件迁入新位置，另 20 个本地文件按原相对路径保存在归档区，迁移前后逐文件核对 SHA-256，没有删除原始资料；仅移除了旧路径的空目录。未登记的 PDF 不因被归档就被判定为错误，也不自动成为已确认的来源证据，需要使用时再核对官方身份并登记。归档脚本只保留获取历史，不再作为下载入口。

其他课程资料等 `.cache/` 内容未改动。迁移核对记录仅保存在本地 `.cache/gic_resource_audit/migration_result.json`，不作为长期技术证据。

## 1.2\_版权\_许可与商标声明

本目录不是 Arm、NXP、Rockchip 或任何开发板厂商的官方镜像，也不代表这些权利人对本仓库提供认可、授权或背书。

- Arm 架构规范、GIC 规范、处理器核技术参考手册及相关学习资料的版权和其他权利归 Arm Limited 或其关联方所有。Arm 文档标注为 `Non-confidential` 只说明保密级别，**不等于采用允许任意再分发的开放许可证**；具体使用条件以每份文档内的 Proprietary Notice 和 Arm 官方条款为准。
- i.MX6ULL 数据手册、勘误和参考手册的版权及其他权利归 NXP B.V. 或其关联方所有，具体许可和免责声明以 NXP 官方页面及文档内声明为准。
- RK3588 数据手册和技术参考手册的版权及其他权利归 Rockchip Electronics Co., Ltd. 或相应权利人所有，具体许可和保密边界以取得文件时附带的声明为准。
- Arm、Cortex、NXP、i.MX、Rockchip、RK3588 等名称和商标仅用于识别资料来源及兼容平台，其权利归各自权利人所有。
- 仓库根目录的 `GPL-2.0-only` 只覆盖本仓库有权许可的原创脚本、索引和说明，**不会重新许可下载到 `.cache/` 的第三方文件**。

公开可访问、可以个人下载和允许本仓库再次分发原文件是三件不同的事。没有逐份取得明确再分发授权以前，本仓库不把这些 PDF 放入普通 Git、Git LFS、GitHub Release、Pages 或其他公共镜像。使用者应自行阅读并接受来源站点和文档内的条款；本索引不构成法律意见。

## 1.3\_来源等级与自动化边界

超文本传输安全协议（Hypertext Transfer Protocol Secure，HTTPS）是这里用于访问权利人站点的网络协议。资料清单区分两种获取方式：

| 来源类型 | 含义 | 下载工具行为 |
| --- | --- | --- |
| `official_direct` | 文件由权利人控制的 HTTPS 地址直接提供 | 可以下载到本地 `.cache/`，随后核对字节数、PDF 结构、页数和 SHA-256 |
| `official_portal` | 只登记权利人的产品或文档入口，实际文件可能需要账号、开发板资料包或额外条款 | 不代替用户登录、不绕过访问控制；文件由用户合法取得后只执行本地校验 |

公共清单不自动访问来源不明的网盘或第三方镜像。第三方文件即使与已知哈希一致，也只能证明字节内容相同，不能证明镜像具有再分发授权。

## 1.4\_版本选择与阅读顺序

先确认目标芯片真正采用的控制器，再选择该版本所需的资料；资料库收集得完整，不表示每份资料都要列为入门必读。

### 1.4.1\_先锁定\_RK3588\_实际使用的控制器

当前目标是 **RK3588 上的 GIC-600 r1p6，所实现的架构为 GICv3.0**。不是先遍历所有 Arm 架构，也不要求先读完 GICv2，再把 GICv3/v4 混合比较。已落地的十篇物理中断正文见 [GIC 专题大纲](../../../platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图)：概念可以独立学懂，每节旁边提供官方目录对照。后续虚拟化、源码与板上验证由[建设路线](../../../atlas/roadmaps/gic_learning_plan.md#1.6_建设落点与当前交付边界)维护。

| 名称 | 它是什么 | 能回答什么，不能回答什么 |
| --- | --- | --- |
| GICv3.0 | 通用中断控制器的软件可见架构规则 | 定义状态、路由和寄存器行为；不决定某块芯片的地址和集成数量 |
| GIC-600 r1p6 | Arm 的一款控制器硬件设计及其修订 | 把架构落实为具体组件和实现选项；“600”不是 GIC 的第六代 |
| RK3588 | Rockchip 集成处理器核、控制器和外设的系统级芯片（System on Chip，SoC） | 决定采用哪款控制器、怎样连接及启用哪些选项 |

处理器架构、处理器核、GIC 架构、控制器产品和 SoC 型号不是同一条版本序列。不能根据 Armv8-A/Armv9-A 名字推导 GIC 版本，也不能看到规范标题中的“version 3 and version 4”就认为某颗芯片同时实现两者。

### 1.4.2\_型号判断的证据与适用边界

2026-09-06 核对了清单中的以下原件。技术参考手册（Technical Reference Manual，TRM）描述实现细节；GIC600 是芯片手册对 GIC-600 产品名称的写法。页码给出原文定位，不靠文件名推断型号。

表中的：

* **LPI**（Locality-specific Peripheral Interrupt，特定局部外设中断）：是 GIC 管理的一类中断，常用于设备通过写事务发送的消息中断；
* **ITS**（Interrupt Translation Service，中断翻译服务）：把设备发来的标识翻译为目标中断及其投递位置。这里只解释为何收集对应资料，完整机制在物理中断主线建立后从零推演。

| 证据 | 定位 | 本次确认的事实 |
| --- | --- | --- |
| RK3588 TRM V1.0 Part1，2022-03-09，条目 `rockchip.rk3588.trm.1_0.part1` | 第 11 章，11.1，表 11-1；PDF 第 1312 页 | 控制器为 GIC600，修订为 `r1p6-00rel0`；启用 LPI，配置两个 ITS |
| [Arm GIC-600 TRM 100336 r1p6](https://developer.arm.com/documentation/100336/0106) | 1.3 Compliance；原文 1-17，PDF 第 17 页 | 遵循 GIC 架构版本 3.0 |
| 同一 RK3588 TRM | 1.1 地址表，PDF 第 18 页 | GIC600 的 SoC 地址窗口从 `0xFE600000` 开始；不等于每类 GIC 寄存器都使用同一基地址 |

**芯片手册也需要交叉核对。** RK3588 TRM 第 1.3 节与第 11.1 节关于 SPI 数量、PPI 触发方式的摘要并不完全一致；第 11.1 节还出现“所有寄存器均内存映射”的概括。不能据此跳过 Arm 对 CPU Interface 系统寄存器的定义，或直接把配置参数当成可用中断个数。正式讲解须交叉核对 GIC-600 TRM、架构寄存器定义、实际设备树与硬件能力寄存器。本次只确认上述型号、版本和集成选项，不照搬摘要中的全部句子。

这里尚未执行 RK3588 板上实验，也没有核对某套 RK3588 板级支持包（Board Support Package，BSP）的启动固件、内核配置和运行寄存器。手册证据不冒充实测结果。

### 1.4.3\_先读哪几份资料

`DDI`、`IHI`、`DAI` 是 Arm 的文档编号前缀，不是 GIC 组件。技术参考手册（Technical Reference Manual，TRM）描述具体硬件实现；架构规范描述规则；学习指南建立第一轮模型。**规范是查证依据，不意味着初学者必须从几千页规范第一页顺读。**

| 阅读任务 | 资料与入口 | 本轮怎样读 |
| --- | --- | --- |
| 补齐处理器与异常的最小背景 | 已有 Introducing the Arm architecture、AArch64 Exception model | 先解释处理器执行、外设事件、异常入口、特权与执行状态；不要求先学完整 Armv8/Armv9 |
| 建立 GICv3.0 物理中断模型 | [198123，概览 3.2-01](https://developer.arm.com/documentation/198123/0302) | 第 3～8 章讲组件、配置、处理中断和核间通知；标题虽含 v4，物理路径可独立学习 |
| 查状态和寄存器规则 | [IHI0069 H.b](https://developer.arm.com/documentation/ihi0069/hb) | 按问题查术语、状态机和寄存器，筛选 GICv3.0 条件；不把 v3.1 及以后特性套到 RK3588 |
| 查控制器实现与芯片集成 | [100336，GIC-600 r1p6](https://developer.arm.com/documentation/100336/0106) | 第 1～4 章配合 RK3588 TRM 第 11 章，区分产品能力与集成选择 |
| 进入设备消息中断 | [102923，LPI 指南 1.0-01](https://developer.arm.com/documentation/102923/0100) | 概览完成后读第 2～5 章；追踪内存中的配置、挂起状态、ITS 表和命令队列 |
| 后续选修 GICv3 虚拟化 | [107627，虚拟化指南 1.2-02](https://developer.arm.com/documentation/107627/0102) | 先补虚拟机、虚拟处理器和管理程序，再读第 1～3 章；这仍是 GICv3，不以 GICv4 为前置 |
| 再扩展 GICv4 | 同一 107627 与 IHI0069 | 掌握 GICv3 虚拟化后再学 v4.0，再进入指南第 5～6 章的 v4.1；指南不完整覆盖 v4.0，细节查规范 |

新版概览、LPI 指南、虚拟化指南及 GIC-600 r1p6 TRM 是本次新增的四份官方资料。原有 `arm.dai0492` 条目实际封面为 Overview 3.0，本次据此明确版本并保留为历史概览；默认主线使用 198123 的 3.2-01，不靠旧条目名称推断修订。

### 1.4.4\_其他资料何时使用

下面的 DDI0406 是 ARMv7 架构参考手册的文档编号，IHI0048 是 GICv2 架构规范的文档编号；这些不是读者要预先掌握的硬件术语。

| 资料 | 定位与边界 |
| --- | --- |
| DDI0487 M.c | 已登记的 A-profile 总规范基线；查询异常和系统寄存器，不是顺读前置，也不保证芯片具备书中所有扩展 |
| DDI0487 G.b | 已被取代的 Armv8-A 历史规范，保留作版本对照 |
| DDI0608 B.a | 已退役的早期 Armv9 增量资料，不纳入 RK3588 中断入门必修 |
| Cortex-A55、Cortex-A76 TRM | 核对 RK3588 处理器核侧接口；核的版本与 GIC 的版本分开记录 |
| DDI0406 C.d、Cortex-A7 TRM、IHI0048 B.b、i.MX6ULL 手册 | 供原有 ARMv7/GICv2 平台路线使用，不作为当前主线前置 |

Linux IRQ 子系统讲软件如何管理、映射和分派中断；独立 GIC 专题讲硬件如何接收、排队、路由、确认和完成中断。二者在 Linux 控制器驱动边界对齐，不互相复制完整正文。`irqchip` 是 Linux 中控制器驱动的源码目录名称，不是另一代 GIC。

## 1.5\_下载与校验

在仓库根目录运行：

```bash
# 查看学习资料组，不读取缓存也不访问网络
python3 scripts/download_external_resources.py --list-profiles

# 当前推荐：查看、下载或只校验 RK3588 的 GICv3.0 资料
python3 scripts/download_external_resources.py --profile rk3588_gicv3 --list
python3 scripts/download_external_resources.py --profile rk3588_gicv3 --yes
python3 scripts/download_external_resources.py --profile rk3588_gicv3 --verify-only

# 查看清单，不访问网络
python3 scripts/download_external_resources.py --list

# 下载所有具有官方直链的缺失文件；需要人工取得的文件只给出来源提示
python3 scripts/download_external_resources.py

# 已经审阅清单时，可在自动化环境中显式跳过交互确认
python3 scripts/download_external_resources.py --yes

# 只处理指定文档，可以多次使用 --select
python3 scripts/download_external_resources.py --select arm.ddi0487.mc --select arm.ihi0069.hb

# 不下载，只核对当前缓存中的全部清单文件
python3 scripts/download_external_resources.py --verify-only
```

其他组为 `gicv3_virtualization`（GICv3 虚拟化选修）、`gicv4_extension`（以后扩展）、`imx6ull_gicv2`（原有平台对照）。`--profile` 与 `--select` 互斥；不指定选择参数仍处理完整清单，不会静默只处理推荐组。组内保持清单顺序，共用文档不会复制到多个目录。

Windows 没有 `python3` 命令时使用 `python`。默认缓存根目录来自 `manifest.json`，也可以用 `--cache-root` 指向另一个本地目录。

官方页面和下载直链统一在 [manifest.json](manifest.json) 的 `documents[]` 条目中维护：`official_page` 是读者可以查看条款和版本信息的官方页面，`download_url` 是允许自动获取时使用的官方直链。工具启动界面会显示该清单的仓库相对路径，并明确提示这两个字段的修改位置。文档换版时不能只改链接；必须同时核对 `version`、`size_bytes`、`pages` 和 `sha256`，避免新链接与旧校验值混用。

工具在联网前会完整展示本次模式、清单位置、仓库相对缓存目录，以及每个条目的文档编号、版本、发布者、文件名、大小、页数、官方页面和最终保存位置。默认输出只使用相对于仓库根目录的位置，例如 `.cache/private_sources/arm/...`，不固化盘符、用户名、网络共享或其他机器专属信息；使用仓库外的 `--manifest` 或 `--cache-root` 时，界面只标明该位置来自对应参数，不把绝对路径写进可共享日志。

工具随后检查目标文件是否存在，再执行 PDF 结构、大小、页数和 SHA-256 校验：已有文件校验通过就跳过；文件缺失才下载；已有文件校验失败时，先把新内容写入 `.download` 临时文件，等新文件全部校验通过后才原子覆盖旧文件。校验失败的新文件不会破坏原文件。

只有确实存在待下载条目时，交互终端才要求输入 `y`；`--yes` 表示调用者已经审阅清单并主动跳过确认。标准输入不是终端且没有 `--yes` 时，工具会拒绝开始联网；持续集成或批处理必须显式添加 `--yes`，防止脚本在无人确认时产生网络访问和文件写入。

下载期间，交互终端在同一行更新进度条、完成比例、已接收字节、速度和预计剩余时间；日志重定向或非交互运行时改为间隔输出，不会用大量回车刷新污染日志。每个文件先写到带 `.download` 后缀的临时路径，只有 PDF 结构、大小、页数和 SHA-256 全部通过校验后，才原子替换最终目标。界面最后分别统计“校验通过”“新下载”“待手工获取”和“失败”，不会把需要登录或额外条款的手工条目显示成下载成功。

脚本不会调用 `git add`、`git commit` 或 `git push`。默认目录 `.cache/private_sources/arm/` 已被 Git 忽略；使用 `--cache-root` 改到其他位置时，调用者仍需自行确认该目录的版本控制和备份规则。

`pdfinfo` 是 Poppler 工具集提供的命令行程序专名，用于读取 PDF 的页数和基本结构；它不是本仓库定义的接口。

校验分为四层：

1. 目标相对路径必须保持在缓存根目录内；
2. 文件必须具有 PDF 头和结束标记；
3. 字节数与 SHA-256 必须匹配清单；
4. 系统存在 `pdfinfo` 时，页数也必须匹配。

SHA-256 只能证明当前文件与登记样本的字节内容一致，不能证明资料仍是最新版，也不能代替版权许可判断。版本更新时必须重新核对官方页面、文档封面、修订记录和法律声明，再更新清单。

## 1.6\_为什么不使用\_Git\_LFS

Git Large File Storage（Git LFS）把大文件内容存放到独立对象存储，并在 Git 提交中保存指针。它可以缓解普通 Git 对大二进制文件的性能限制，但不会改变文件的版权、许可证或再分发条件。

本批资料不使用 Git LFS，原因有三层：

1. **权利边界：** 当前没有逐份取得把 Arm、NXP 和 Rockchip 原版 PDF 重新发布到本仓库的明确授权；这是决定性原因。
2. **维护边界：** 厂商会修订或撤回文档，使用官方入口和校验清单能够保留版本身份，又不会把旧二进制永久堆入仓库历史。
3. **分发成本：** Git LFS 仍会消耗仓库所有者的存储和下载带宽，读者也需要额外的 LFS 客户端和可用额度。

只有文件采用明确允许再分发的许可证、仓库确实需要离线携带原件，并且维护者接受 LFS 的存储与带宽成本时，才应另行评估 Git LFS。本结论不自动追溯迁移仓库中已有的历史外部文件；它们需要单独完成来源和授权审计。

## 1.7\_维护规则

- 新增条目前优先使用权利人的官方文档页和官方直链。
- 版本化资料必须登记文档编号、修订号、文件长度、页数和 SHA-256。
- 需要账号或随板资料包获取的文件使用 `official_portal`，不得把登录后的临时 URL、Cookie、令牌或本机路径写进仓库。
- 不把第三方镜像伪装成官方来源，不绕过访问控制，不把 `Non-confidential` 解释成开放再分发许可。
- 规范结论进入知识正文时应链接本索引或官方页面；平台专有结论同时标明 SoC、文档版本和适用边界。
- 每次更新清单后运行下载工具的 `--verify-only`、仓库元数据/链接检查和 `git diff --check`。
- 清单当前使用 `schema_version: 2`，文档 ID 稳定，`relative_path` 表达缓存位置；`profiles` 只能引用已存在的文档 ID。组名采用英文 `snake_case`，组内不得重复或引用未知 ID。
- 一份文档只登记一个目标路径；工具拒绝 Windows 大小写语义下的路径冲突。新脚本与新清单配套使用，不保留旧路径别名或另一套活动下载入口。
- 变更资料组和目录时，同步本索引、GIC 建设路线、真实导航调用方及评审地图；“已下载”不表示“已完成讲解”。
