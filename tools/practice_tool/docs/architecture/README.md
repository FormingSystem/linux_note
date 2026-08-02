---
id: tools.practice_tool.architecture.index
title: "回路桌面 Markdown 工作台架构索引"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_回路桌面\_Markdown\_工作台架构索引

## 1.1\_当前目标

目标产品是独立 Electron 桌面 Markdown 工作台：新建文件、打开单个文件或打开单个文件夹，直接编辑磁盘 Markdown 并实时预览。它不使用专题电子书、训练内容包、正文导入副本、浏览器正式运行或 IndexedDB 主存储。

本文索引中的目标设计处于评审和实施前状态。当前 `0.1.0` 浏览器训练代码只在 [实现状态与版本边界](implementation_status.md) 中作为旧代码事实记录，不能作为新模块设计依据。

## 1.2\_建议阅读顺序

### 1.2.1\_产品语义

1. [文件与文件夹工作区设计](product/file_and_folder_workspace.md)：打开文件、打开文件夹、文件树、标签和文件操作边界。
2. [Markdown 编辑与实时预览设计](product/markdown_editing_and_live_preview.md)：Dirty、保存、恢复备份、Hot Exit、冲突和预览。
3. [产品导航与交互设计](product/navigation_and_interaction.md)：编辑器式窗口布局、命令、快捷键和状态反馈。

### 1.2.2\_工程与安全

1. [桌面运行时与文档服务设计](engineering/desktop_runtime_and_document_services.md)：Main/Preload/Renderer、IPC、文件服务、备份、Markdown Engine 和资源协议。
2. [桌面运行时安全与威胁模型](engineering/desktop_runtime_security_and_threat_model.md)：能力、路径、IPC、内容、网络、写入和供应链安全。
3. [工程结构与模块边界](engineering/project_structure_and_module_boundaries.md)：包、依赖、所有权和旧代码清理边界。
4. [工作区文件操作与数据安全](engineering/workspace_file_operations_and_data_safety.md)：新建、移动、回收站、资源复制、历史和卸载。
5. [跨平台与仓库独立性设计](../cross_platform_and_repository_independence.md)：Windows/Linux 安装包与独立仓库边界。
6. [无障碍、性能与产品验收标准](engineering/accessibility_performance_and_acceptance.md)：发布门槛与测试矩阵。

### 1.2.3\_决策与状态

1. [架构决策记录](decisions/README.md)：已接受、待评审与已取代决策。
2. [当前实现状态与版本边界](implementation_status.md)：现有代码、目标差距和实施顺序。

## 1.3\_权威归属

| 问题 | 权威文档 |
| --- | --- |
| 单文件与单文件夹语义 | `product/file_and_folder_workspace.md` |
| 输入、Dirty、保存、备份、Hot Exit、预览 | `product/markdown_editing_and_live_preview.md` |
| 窗口布局与交互 | `product/navigation_and_interaction.md` |
| 进程、IPC、文件服务和 Markdown Engine | `engineering/desktop_runtime_and_document_services.md` |
| 路径、内容、网络、写入与更新安全 | `engineering/desktop_runtime_security_and_threat_model.md` |
| 新建、移动、删除、历史与清理 | `engineering/workspace_file_operations_and_data_safety.md` |
| package、依赖与状态所有权 | `engineering/project_structure_and_module_boundaries.md` |
| 验收目标 | `engineering/accessibility_performance_and_acceptance.md` |
| 当前完成度 | `implementation_status.md` |

## 1.4\_维护规则

- 目标文档不为旧浏览器、电子书或 IndexedDB 增加兼容条款。
- “打开”与“导入”不可混用；文件/文件夹打开不复制正文。
- “已备份”与“已保存”不可混用；每次按键不得写源文件或持久历史。
- 修改保存语义时同步检查产品文档、文件服务、安全、文件操作与验收。
- 修改路径或 IPC 时同步检查能力撤销、符号链接、资源协议和恶意输入测试。
- ADR 记录历史，过时的专题设计正文直接删除，不另建长期 legacy 文档树。
