# 第1章_仓库信息架构设计

## 1.1_设计目标

本仓库采用面向长期维护、跨专题复用和多出版物编排的信息架构。结构应允许持续加入网络、存储、实时系统、安全、虚拟化、RISC-V、Android 内核及其他系统软件主题，而不需要改变顶层分类模型。

- 一项知识只保留一份权威正文。
- 基础、机制、子系统、工程、平台、实验和源码证据相互分层。
- 专题与学习路线只负责编排，不复制知识正文。
- 目录表达主要归属，跨领域关系由元数据、索引和链接表达。
- 文档稳定身份不依赖路径、章节编号或出版顺序。
- 同一份知识可以进入不同学习路线、专题和书籍。
- 工具检查路径、链接、元数据、依赖关系和孤立文档。

## 1.2_总体模型

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

依赖关系应由上向下。下层文档可以引用上层知识，上层知识不依赖具体实验、开发板或出版物才能成立。

## 1.3_目标目录结构

```text
linux-note/
├── atlas/
│   ├── maps/
│   ├── tracks/
│   ├── indexes/
│   └── roadmaps/
├── knowledge/
│   ├── foundations/
│   │   ├── computer_architecture/
│   │   ├── operating_systems/
│   │   ├── c_language/
│   │   ├── algorithms/
│   │   ├── data_structures/
│   │   └── concurrency_theory/
│   ├── linux/
│   │   ├── architecture/
│   │   ├── execution_context/
│   │   ├── synchronization/
│   │   ├── memory_ordering/
│   │   ├── waiting_notification/
│   │   ├── time_management/
│   │   ├── memory_management/
│   │   ├── object_lifetime/
│   │   ├── error_handling/
│   │   ├── io_model/
│   │   ├── device_model/
│   │   └── data_structures/
│   ├── kernel_subsystems/
│   ├── driver_model/
│   └── system_software/
├── engineering/
│   ├── driver_development/
│   ├── kernel_development/
│   ├── system_build/
│   ├── bsp_porting/
│   ├── performance/
│   ├── debugging/
│   ├── testing/
│   └── release/
├── platforms/
│   ├── arm/
│   ├── arm64/
│   ├── riscv/
│   └── x86/
├── labs/
│   ├── foundations/
│   ├── kernel/
│   ├── drivers/
│   ├── system/
│   ├── platforms/
│   └── debugging/
├── projects/
├── research/
│   ├── source_reading/
│   ├── call_traces/
│   ├── benchmarks/
│   ├── investigations/
│   └── reading_notes/
├── reference/
├── publications/
│   ├── books/
│   ├── articles/
│   ├── manifests/
│   ├── templates/
│   └── output/
├── tools/
├── governance/
│   ├── architecture/
│   ├── conventions/
│   ├── templates/
│   ├── schemas/
│   └── migration/
├── assets/
├── scripts/
└── tests/
```

空目录不使用占位文件。只有出现正式内容时才进入 Git。

## 1.4_顶层目录职责

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
| `tools` | 编辑器、AI 和仓库工具用法 | 技术知识正文 |
| `governance` | 架构、规范、模板、模式和迁移记录 | 学习笔记 |
| `assets` | 图片、图表、附件、数据集和归档文件 | Markdown 正文 |

无法立即形成稳定结论的内容进入 `research/investigations`，查询型材料进入 `reference`。仓库不设置通用杂物目录。

## 1.5_知识分层

### 1.5.1_基础知识

`knowledge/foundations` 保存不依赖具体 Linux 实现仍然成立的内容，例如操作系统抽象、体系结构、C 语言、算法和理论数据结构。

### 1.5.2_Linux通用机制

`knowledge/linux` 保存被多个内核子系统、驱动类型或系统工程主题复用的机制，例如锁、RCU、等待队列、工作队列、定时器、引用计数、设备模型、错误指针和 Linux 数据结构。

### 1.5.3_完整领域模型

`knowledge/kernel_subsystems`、`knowledge/driver_model` 和 `knowledge/system_software` 解释多个机制怎样组成完整领域。领域文档说明机制在本领域中的组合、约束和边界，不复制机制原理。

## 1.6_文档类型

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

专题不是正文类型。专题实现为 `atlas/tracks` 中的路线文档，通过链接组合基础、机制、子系统、工程、实验和源码阅读。

## 1.7_稳定身份与元数据

正式文档使用 YAML Front Matter 保存稳定身份：

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

第一阶段强制字段为 `id`、`title`、`kind` 和 `status`。`domains`、`topics`、`depends_on`、`related` 和版本信息按文档性质补充。

`id` 发布后不因移动文件、改变标题或调整书籍顺序而变化。脚本检查重复 ID、无效类型和失效依赖。

## 1.8_命名与编号

知识文件使用稳定语义名称，章节编号属于学习路线和出版物，不属于知识本体。

```text
knowledge/linux/waiting_notification/wait_queue.md
knowledge/linux/object_lifetime/kref.md
knowledge/driver_model/input/event_delivery.md
```

实验步骤、学习路线阶段、连续教程和出版产物可以使用顺序编号。出版构建根据 manifest 顺序生成连续章、节编号；同一篇知识文档可以在不同书籍中获得不同编号。

路径禁止空格、中文标点和全角符号。目录使用稳定英文领域名，文档文件名优先使用英文 `snake_case`，中文标题保存在 Front Matter 和 H1 中。

## 1.9_实验与项目

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

实验文档说明目标、前置知识、环境、步骤、预期结果、实际结果、失败现象和清理方法。平台实验引用 `platforms` 中的差异说明。

## 1.10_源码阅读与版本边界

源码阅读进入 `research/source_reading` 并记录项目、版本、提交或标签。稳定知识文档总结跨版本成立的模型，源码阅读文档提供具体实现证据。两者通过链接关联。

## 1.11_学习路线与出版

`atlas/tracks` 通过稳定 ID 或链接编排学习顺序。`publications/manifests` 描述书籍章节清单。出版构建负责解析 ID、合并正文、生成连续序号、重写锚点并输出 HTML、Word 或 PDF。

## 1.12_质量门禁

仓库工具至少检查：

- 非法路径和文件名。
- Markdown、Obsidian、Canvas 和 Base 断链。
- 重复或缺失的文档 ID。
- 非法 `kind` 和 `status`。
- 不存在的依赖 ID。
- 孤立知识文档。
- 学习路线和出版清单中的失效引用。
- 源码阅读缺少版本信息。
- 实验缺少入口文档。
- Git 差异中的空白错误。

## 1.13_归属决策

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
