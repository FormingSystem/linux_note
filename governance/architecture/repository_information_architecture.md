---
id: governance.architecture.repository_information_architecture
title: "仓库信息架构设计"
kind: reference
status: evolving
domains:
  - governance
---

# 第1章\_仓库信息架构设计

## 1.1\_设计目标

本仓库采用面向长期维护、跨专题复用和多出版物编排的信息架构。结构应允许持续加入网络、存储、实时系统、安全、虚拟化、RISC-V、Android 内核及其他系统软件主题，而不需要改变顶层分类模型。

- 一项知识只保留一份权威正文。
- 基础、机制、子系统、工程、平台、实验和源码证据相互分层。
- 专题与学习路线只负责编排，不复制知识正文。
- 目录表达主要归属，跨领域关系由元数据、索引和链接表达。
- 文档永久身份由稳定 `id` 维护，路径表达主要归属；移动和改名同步链接但不重算 ID。正文序号用于阅读定位和出版排版。
- 同一份知识可以进入不同学习路线、专题和书籍。
- 工具检查路径、链接、元数据、依赖关系和孤立文档。

## 1.2\_总体模型

```text
基础理论
   ↓
Linux通用机制
   ↓
内核子系统_驱动模型_系统软件
   ↓
工程方法与平台实现
   ↓
实验_项目_源码阅读_问题调查

学习路线和出版清单从侧面编排以上内容
```

这张图表示内容职责的常见关系，不是全部专题必须经过的目录流水线。平台可以从基础架构问题展开，源码研究可以为通用机制提供证据；引用可以双向跳转，但知识正文不能靠某个未说明的实验环境或派生出版物才能理解自己的结论。学习依赖由专题大纲明确，不能仅凭目录层级猜测。

## 1.3\_目标目录结构

以下是归属规划，包含尚未建立的领域；它不是当前文件清单。真实入口以 [内容索引](../../atlas/indexes/content_index.md) 为准，不为补齐这棵树创建空目录或占位页。

```text
linux-note/
├── atlas/                         # 知识地图与学习编排
│   ├── maps/                      # 知识地图
│   ├── tracks/                    # 学习路线
│   ├── indexes/                   # 内容索引
│   └── roadmaps/                  # 建设与学习路线图
├── knowledge/                     # 稳定、可复用的权威知识
│   ├── foundations/               # 计算机基础理论
│   │   ├── computer_architecture/ # 计算机体系结构
│   │   ├── operating_systems/     # 操作系统原理
│   │   ├── c_language/            # C 语言
│   │   ├── algorithms/            # 算法
│   │   ├── data_structures/       # 通用数据结构
│   │   └── concurrency_theory/    # 并发理论
│   ├── linux/                     # Linux 通用机制
│   │   ├── architecture/          # Linux 架构与源码组织
│   │   ├── execution_context/     # 执行上下文
│   │   ├── synchronization_and_asynchrony/ # 同步和异步机制
│   │   │   ├── synchronization/   # 同步机制
│   │   │   └── asynchrony/        # 异步机制
│   │   ├── memory_management/     # 内存管理
│   │   ├── object_lifetime/       # 对象生命周期
│   │   ├── error_handling/        # 错误处理
│   │   ├── io_model/              # I/O 模型
│   │   ├── device_model/          # Linux 设备模型
│   │   └── data_structures/       # Linux 专用数据结构
│   ├── kernel_subsystems/          # 内核子系统
│   ├── driver_model/               # 驱动模型与设备类别
│   └── system_software/            # 系统软件
├── engineering/                    # 工程方法与流程
│   ├── driver_development/         # 驱动开发方法
│   ├── kernel_development/         # 内核开发方法
│   ├── system_build/               # 系统构建
│   ├── bsp_porting/                # BSP 移植
│   ├── performance/                # 性能分析与优化
│   ├── debugging/                  # 调试方法
│   ├── testing/                    # 测试方法
│   └── release/                    # 发布流程
├── platforms/                      # 平台与架构差异
│   ├── arm/                        # Arm 架构、SoC 与开发板（含 AArch32/AArch64）
│   │   ├── architecture/          # 架构与控制器专题
│   │   ├── nxp/                   # NXP 平台差异
│   │   └── rockchip/              # Rockchip 平台差异
│   ├── riscv/                      # RISC-V 平台
│   └── x86/                        # x86 平台
├── labs/                           # 最小可复现实验
│   ├── foundations/                # 基础理论实验
│   ├── kernel/                     # 内核机制实验
│   ├── drivers/                    # 驱动实验
│   ├── system/                     # 系统软件实验
│   ├── platforms/                  # 平台实验
│   └── debugging/                  # 调试实验
├── projects/                       # 综合项目
├── research/                       # 研究证据与过程记录
│   ├── source_reading/             # 源码阅读
│   ├── call_traces/                # 调用链追踪
│   ├── benchmarks/                 # 基准测试
│   ├── investigations/             # 问题调查
│   └── reading_notes/              # 阅读记录
├── reference/                      # 查询型参考资料
├── publications/                   # 出版物编排与产物
│   ├── books/                      # 书籍工程
│   ├── articles/                   # 文章工程
│   ├── manifests/                  # 内容编排清单
│   ├── templates/                  # 出版模板
│   └── output/                     # 构建产物
├── markbook/                       # 月度专题电子书与可复现快照
│   ├── manifests/                  # 专题跨层编排清单
│   ├── templates/                  # 专题阅读与打印模板
│   ├── runtime/                    # 版本化离线运行时
│   ├── scripts/                    # 生成、验证与防重复工具
│   └── topics/                     # 按专题和月份冻结的版本
├── tools/                          # 工具使用说明
├── governance/                     # 仓库治理规范
│   ├── architecture/               # 信息架构
│   ├── conventions/                # 协作与写作约定
│   ├── templates/                  # 文档模板
│   ├── schemas/                    # 元数据模式
│   └── migration/                  # 迁移记录
├── assets/                         # 图片、附件与数据集
├── scripts/                        # 自动化脚本
└── tests/                          # 仓库工具测试
```

空目录不使用占位文件。只有出现正式内容时才进入 Git。

## 1.4\_顶层目录职责

| 目录 | 职责 | 不存放 |
| --- | --- | --- |
| `atlas` | 知识地图、学习路线、索引和路线图 | 知识正文 |
| `knowledge` | 稳定、可复用的权威知识 | 实验日志和板级差异 |
| `engineering` | 完成工程任务的方法与流程 | 通用机制的重复讲解 |
| `platforms` | 架构、SoC、开发板和 BSP 差异 | 跨平台通用原理 |
| `labs` | 验证单一结论的最小实验 | 多目标综合项目 |
| `projects` | 组合多个机制的完整成果 | 零散知识摘录 |
| `research` | 源码阅读、调用链、调查和基准证据 | 未注明版本的稳定结论 |
| `reference` | API、命令、术语、标准和外部资料 | 连续学习教程 |
| `publications` | 出版物编排、模板和构建产物 | 知识正文副本 |
| `markbook` | 按月生成的专题聚焦电子书、来源台账与生成工具 | 可独立修改的第二份知识正文 |
| `tools` | 编辑器、AI 和仓库工具用法 | 技术知识正文 |
| `governance` | 架构、规范、模板、模式和迁移记录 | 学习笔记 |
| `assets` | 图片、图表、附件、数据集和归档文件 | Markdown 正文 |

无法立即形成稳定结论的内容进入 `research/investigations`，查询型材料进入 `reference`。仓库不设置通用杂物目录。

`reference/external_resources/` 默认保存外部资料的书目信息、权威来源、版本边界、版权声明和完整性校验值，不直接保存厂商原版文件。没有明确再分发授权的 PDF、压缩包和其他二进制资料只能下载到 Git 忽略的 `.cache/`；Git LFS 只改变存储方式，不能替代版权和许可证授权，因此也不得用于绕过这项边界。

外部资料缓存按对象身份组织，学习资料组通过机器清单引用稳定文档 ID；同一原件不因同时用于多个专题而复制多份。以 [Arm 资料缓存与学习资料组](../../reference/external_resources/arm/README.md#1.1.1_缓存目录与学习资料组)为例，架构规范、处理器核、控制器规范与实现、芯片集成各自归类，RK3588 的学习顺序由资料组和 Atlas 路线表达。缓存路径用于取得文件，不能替代文档编号、修订号和完整性校验等来源身份。

`atlas/maps/knowledge_review_map.md` 同时承担全仓阅读跳转和人工评审状态台账。它使用 🔴 **未校正**、🟡 **人工评审中**、🟢 **评审完成** 表示读者可见的人工确认程度；红色内容可能包含部分 AI 生成或辅助整理，但不表示全文由 AI 生成。新内容默认 🔴。这套状态不属于 Front Matter 维护阶段，不得在根 README、专题元数据、路线或索引中复制维护。

## 1.5\_知识分层

### 1.5.1\_基础知识

`knowledge/foundations` 保存不依赖具体 Linux 实现仍然成立的内容，例如操作系统抽象、体系结构、C 语言、算法和理论数据结构。

### 1.5.2\_Linux通用机制

`knowledge/linux` 保存被多个内核子系统、驱动类型或系统工程主题复用的机制。同步和异步机制统一进入 `synchronization_and_asynchrony`：`synchronization` 保存 Linux 内存顺序、锁、序列计数器、等待/完成、RCU 与同步协议验证，`asynchrony` 保存中断、工作队列、定时/延迟执行和异步通知。引用计数、设备模型、错误指针和 Linux 数据结构仍按各自知识本质独立保存；子系统中的异步完成或驱动中的机制组合不从领域正文拆走，只链接通用入口。

### 1.5.3\_完整领域模型

`knowledge/kernel_subsystems`、`knowledge/driver_model` 和 `knowledge/system_software` 解释多个机制怎样组成完整领域。领域文档说明机制在本领域中的组合、约束和边界，不复制机制原理。

## 1.6\_文档类型

正式文档的 `kind` 从以下集合中选择：

| 类型 | 含义 |
| --- | --- |
| `concept` | 基础概念与理论 |
| `mechanism` | 可跨领域复用的机制 |
| `subsystem` | 完整子系统或领域模型 |
| `interface` | API、协议或模块边界 |
| `engineering` | 工程方法和操作流程 |
| `platform` | 架构、SoC 或开发板差异 |
| `lab` | 验证单一结论的实验 |
| `project` | 多模块综合项目 |
| `source` | 特定版本源码阅读 |
| `investigation` | 问题调查、性能分析和故障复盘 |
| `reference` | 查询型资料 |
| `track` | 学习路线和专题编排 |
| `publication` | 出版物编排 |

专题不是另一份知识正文。跨目录学习路线放在 `atlas/tracks`，权威专题目录内的 `大纲.md` 也使用 `kind: track`，说明本专题的因果阅读链、每章前提和未决问题；二者都链接真实正文，不复制教程。

## 1.7\_文档元数据

正式 Markdown 必须使用 YAML Front Matter 保存机器可读属性。下面是字段形状示意，ID 与依赖名是示例，正式采用时应替换为仓库真实且唯一的 ID，不能照抄后制造不存在的依赖：

```yaml
---
id: linux.wait_queue
title: 等待队列
kind: mechanism
domains:
  - linux
  - kernel
  - driver
topics:
  - concurrency
  - blocking_io
depends_on:
  - os.process_state
  - linux.scheduler_wakeup
status: maintained
---
```

强制字段为 `id`、`title`、`kind`、`status` 和 `domains`：

- `id` 是文档永久身份，创建后不因移动、改名或出版顺序变化而重算。
- `title` 是不含阅读序号的可读标题。
- `kind` 必须来自本规范的文档类型集合。
- `status` 使用 `draft`、`evolving`、`maintained` 或 `archived`。
- `domains` 记录能够可靠确认的知识领域。

元数据工具维护 Front Matter，标题工具另行负责标题；不能把修复元数据当作已完成正文结构重构。Codex skill 的运行时 Markdown 与 `.github` 宿主配置按各自格式保存，不套用普通知识文档的字段。使用以下命令检查或补齐：

```bash
# 当前目录：linux-note 仓库根目录；Linux Bash 或 MSYS2 Bash。
./format.sh check metadata --summary
./format.sh fix metadata --summary
```

## 1.8\_命名与编号

目录使用稳定英文领域名。章节型文件采用 `PXX_NAME`，没有章节属性的文件保留语义名称。路径禁止空格、中文标点和全角符号。

Markdown 标题采用统一阅读序号：

```markdown
# 第5章\_文件系统构建与定制
## 5.1\_引言
### 5.1.1\_为什么需要文件系统
#### (1)\_根文件系统的职责
##### 1)\_用户空间支持
###### a)\_基础目录
```

- H1 使用 `第N章\_NAME`。
- H2 使用 `N.N\_NAME`。
- H3 使用 `N.N.N\_NAME`。
- H4 使用 `(N)\_NAME`。
- H5 使用 `N)\_NAME`。
- H6 使用 `a)\_NAME`，超过 26 项后使用 `aa)`、`ab)`。
- 标题源码中的下划线统一转义为 `\_`，以兼容 GitHub、VS Code 和 Typora 的 Markdown 渲染。
- 每级序号在父标题内重新计数，保持有限长度和清晰辨识度。
- 序号是知识正文的阅读结构，目录重构、元数据和出版工具不得擅自删除。
- 多文档出版时由 manifest 确定文档顺序，构建工具校正跨文档章号连续性，但保留上述排版体系。

## 1.9\_实验与项目

`labs` 验证一个主要结论，`projects` 组合多个机制形成完整成果。标准实验目录可以包含：

```text
实验目录/
├── README.md
├── src/
├── config/
├── scripts/
├── expected/
└── troubleshooting.md
```

这是可选材料布局，不要求每次实验生成所有目录。实验文档说明目标、前置知识、环境、步骤、预期结果、实际结果、失败现象和清理方法。连续教材的本单元实验放在相应正文中就地解释，材料目录保存与正文一致的代码和配置；独立实验才建立自己的入口和先修契约，不能让读者跳到后续综合实验才能完成当前章节。平台实验引用 `platforms` 中的差异说明。

## 1.10\_源码阅读与版本边界

源码阅读进入 `research/source_reading` 并记录项目、版本、提交或标签。稳定知识文档总结跨版本成立的模型，源码阅读文档提供具体实现证据。两者通过链接关联。知识正文建立可复用的因果模型，模块源码导读组织特定版本的状态落点、协作路径和阅读次序，二者不是互相同步的副本；只有同一版本的具体宏体、函数体及逐句实现说明实行唯一展开。

同一源码专题同时需要概念阅读和逐函数实现时，在专题目录下分离 `navigation/` 与 `source_explanations/`：前者保存一个总阅读索引以及按子功能组织的源码概念导读，后者保存宏、字段和函数体的唯一实现讲解。知识正文先链接总索引和相关模块导读，遇到具体接口时再使用“相对文件路径 + 完整标题锚点”直达实现讲解；Atlas、源码基线和相邻专题只组织这些权威入口，不复制实现正文。

## 1.11\_学习路线与出版

`atlas/tracks` 通过稳定 ID 或链接编排学习顺序。`publications/manifests` 是长期书籍和文章清单的规划位置；只有真实出版工程需要时才建立。相应构建器应明确支持哪些输入、序号和锚点转换及输出格式，不能把本架构规划当成已经存在通用 HTML、Word、PDF 导出器。

`markbook/manifests` 面向单一专题跨 `knowledge`、`research`、`labs` 等职责层收集真实材料，并在每月 1 日生成一个不可变的 `YYYY.MM` 聚焦阅读快照。MarkBook 与普通出版物共享“权威正文只保存一次”的原则，但额外要求同月防重复、显式覆盖授权、逐源文件哈希、工作树状态、全文检索和离线阅读运行时。生成后的书只用于阅读和追溯，不作为正文回写入口。

## 1.12\_质量门禁

质量门禁包含工具能够检查的结构问题，也包含必须阅读和核对的问题。现有统一入口与边界如下：

| 检查对象 | 当前入口 | 通过不代表什么 |
| --- | --- | --- |
| 路径、标题、元数据、链接 | `./format.sh check all --summary` | 不等于术语、因果讲解或技术结论正确 |
| 重复 ID、必需字段、kind/status | `./format.sh check metadata --summary` | 不证明依赖图已经构成合适的学习路线 |
| Markdown 与已支持的 Obsidian/Canvas/Base 引用 | `./format.sh check links --summary` | 目标文件存在不能证明完整标题片段和跳转内容符合阅读任务 |
| 特定 Mermaid 伪层级子图 | `./format.sh check mermaid --summary` | 不是所有图的语法解析、渲染和视觉检查 |
| Git 空白与暂存范围 | `git diff --check`、`git diff --cached --check` 及差异审查 | 不证明内容属于本任务或示例运行成功 |
| MarkBook 清单、来源与产物 | 真实出版工程的构建和验证入口 | 不能替代作者对正文和来源版本的审查 |

孤立文档、真实依赖关系、版本边界、实验入口、完整标题锚点和学习链仍要结合入链检索、相应专题工具与人工阅读核对；不能笼统宣称一个 `check all` 已自动覆盖全部项目。移动或拆分后，检查旧路径、稳定 ID、专题大纲、Atlas、真实出版调用方和人工评审地图。没有对应出版或 Obsidian 调用方时记录无须修改，不创建占位工程。

## 1.13\_归属决策

遇到归属冲突时依次判断：

1. 不依赖 Linux 是否仍然成立，成立则属于 `foundations`。
2. 是否会被多个 Linux 领域复用，是则属于 `knowledge/linux`。
3. 是否描述完整子系统、驱动类型或系统软件，是则属于对应领域模型。
4. 是否描述完成任务的方法，是则属于 `engineering`。
5. 是否只对特定架构、SoC 或开发板成立，是则属于 `platforms`。
6. 是否通过运行验证结论，是则属于 `labs` 或 `projects`。
7. 是否依赖具体源码版本或调查过程，是则属于 `research`。
8. 是否主要用于查询，是则属于 `reference`。

目录归属只选择一个主要位置，其他关系通过元数据、索引和链接表达。
