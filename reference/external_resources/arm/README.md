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

# 第1章\_Arm、RK3588 与 i.MX6ULL 外部资料索引

## 1.1\_索引定位与存储边界

本索引登记学习 Arm A-profile 架构、通用中断控制器（Generic Interrupt Controller，GIC）、RK3588 和 i.MX6ULL 时使用的外部资料。仓库只保存书目信息、上游入口、预期文件名、字节数、页数、SHA-256 和下载工具，不保存这些厂商发布的 PDF 原件。

下载工具把文件写入仓库根目录的 `.cache/private_sources/arm/`。整个 `.cache/` 已被 Git 忽略，因此本地资料不会随提交进入 GitHub。规范正文、芯片手册和本仓库的学习笔记属于不同产物：规范负责定义硬件或架构契约，学习笔记负责解释和引用，二者不能用同一份许可证处理。

机器可读清单见 [manifest.json](manifest.json)，下载与校验工具见 [download_external_resources.py](../../../scripts/download_external_resources.py)。

## 1.2\_版权、许可与商标声明

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

`DDI`、`IHI` 和 `DAI` 在本索引中都是 Arm 分配的 **文档编号前缀**，后面的数字和修订号共同定位一份文档；它们不是本章需要展开的硬件机制缩写。GIC 的标识（Identification，ID）寄存器则是硬件提供的软件可读寄存器，用于确认控制器实现的版本和能力。

本清单中的具体编号不能脱离标题单独理解：`DDI0406` 对应《ARMv7-A 与 ARMv7-R 架构参考手册》（ARM Architecture Reference Manual ARMv7-A and ARMv7-R edition）；`IHI0069` 对应《Arm 通用中断控制器 v3/v4 架构规范》（Arm Generic Interrupt Controller Architecture Specification, GIC architecture version 3 and version 4），`IHI0048` 对应它的 v2 架构规范（ARM Generic Interrupt Controller Architecture Specification Version 2.0）；`DAI0492` 对应《GICv3 与 GICv4 软件概览》（GICv3 and GICv4 Software Overview）。这些标题和版本仍以 `manifest.json` 中的具体条目为机器可读依据。

| 阅读阶段 | 主资料 | 作用与边界 |
| --- | --- | --- |
| Armv8/Armv9 当前权威基线 | DDI0487 M.c | 当前 A-profile 总规范，同时覆盖 Armv8 与 Armv9 演进内容；发生冲突时以它为准 |
| RK3588 代际对照 | DDI0487 G.b、Cortex-A76 TRM、Cortex-A55 TRM | G.b 已被取代，只用于观察更接近 RK3588 处理器核代际的 Armv8-A 规范形态 |
| Armv9 初始增量 | DDI0608 B.a | 已退役，只用于快速识别早期 v8→v9 差量，不能替代 DDI0487 M.c |
| ARMv7-A 对照 | DDI0406 C.d、Cortex-A7 TRM | 用于理解 i.MX6ULL 的 ARMv7-A 执行环境，并与 AArch64 对照 |
| 中断控制器 | IHI0069 H.b、DAI0492、IHI0048 B.b | RK3588 以 GICv3 路径为主，i.MX6ULL 以 GICv2 路径为主；SoC 实际能力仍要回到芯片手册、设备树和 ID 寄存器确认 |
| SoC 落地 | RK3588 与 i.MX6ULL 芯片资料 | 解释地址映射、外设、中断号和厂商集成差异，不能把 SoC 实现写成通用 Arm 架构保证 |

`irqchip` 是 Linux 源码为中断控制器驱动使用的目录和实现标识符，不是可展开的英文缩写。学习顺序应先建立当前 Armv8-A/AArch64 主线，再用 Armv9 查看演进方向，最后以 ARMv7-A 作为对照。阅读 Linux IRQ 子系统时，再把架构异常模型、GIC 硬件状态和 Linux `irqchip` 软件实现分层对齐。

## 1.5\_下载与校验

在仓库根目录运行：

```bash
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

Windows 没有 `python3` 命令时使用 `python`。默认缓存根目录来自 `manifest.json`，也可以用 `--cache-root` 指向另一个本地目录。

官方页面和下载直链统一在 [manifest.json](manifest.json) 的 `documents[]` 条目中维护：`official_page` 是读者可以查看条款和版本信息的官方页面，`download_url` 是允许自动获取时使用的官方直链。工具启动界面会显示该清单的绝对路径，并明确提示这两个字段的修改位置。文档换版时不能只改链接；必须同时核对 `version`、`size_bytes`、`pages` 和 `sha256`，避免新链接与旧校验值混用。

工具在联网前会完整展示本次模式、清单位置、绝对缓存目录，以及每个条目的文档编号、版本、发布者、文件名、大小、页数、官方页面和最终保存位置。它先检查目标文件是否存在，再执行 PDF 结构、大小、页数和 SHA-256 校验：已有文件校验通过就跳过；文件缺失才下载；已有文件校验失败时，先把新内容写入 `.download` 临时文件，等新文件全部校验通过后才原子覆盖旧文件。校验失败的新文件不会破坏原文件。

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

## 1.6\_为什么不使用 Git LFS

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
