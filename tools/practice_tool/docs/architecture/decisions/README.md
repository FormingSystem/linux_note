---
id: tools.practice_tool.architecture.decisions.index
title: "回路架构决策记录"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_回路架构决策记录

## 1.1\_记录规则

ADR 一旦接受后保留历史正文。目标改变时更新状态并新增取代决策，不原地把旧背景伪装成新结论。产品设计正文只保留当前目标，不为旧架构维护平行说明。

## 1.2\_当前有效决策

1. [0009：采用文件与文件夹工作区](0009-file-and-folder-workspace.md)，`accepted`。

## 1.3\_待评审决策

1. [0006：采用桌面优先的多进程与包级架构](0006-desktop-first-multiprocess-architecture.md)，拟取代 0001 的浏览器结构结论。
2. [0007：采用磁盘正文与分层保存](0007-disk-markdown-and-desktop-persistence.md)，拟取代 0002 的 IndexedDB 主存储结论。
3. [0008：分离 Markdown 编辑与实时渲染管线](0008-separate-markdown-editing-and-rendering.md)。

评审应分别确认运行时、保存/恢复和编辑/渲染管线。0009 已明确文件与文件夹产品模型，不代表 0006～0008 的具体实现自动通过。

## 1.4\_已取代的历史决策

1. [0003：冻结不可变训练计划快照](0003-immutable-training-plan-snapshot.md)，由 0009 取代。
2. [0004：AI 草稿必须经过人工审核](0004-ai-draft-requires-review.md)，由 0009 从当前产品范围移除。
3. [0005：专题电子书取代单篇学习导引](0005-topic-ebook-content-model.md)，由 0009 取代。

## 1.5\_仍属于当前旧实现的决策

1. [0001：采用 Feature-first 前端结构](0001-feature-first-architecture.md)。
2. [0002：本地优先持久化](0002-local-first-persistence.md)。

0001 与 0002 只解释 `0.1.0` 浏览器代码。0006、0007 接受后将它们改为 `superseded`；不得通过兼容层让两组决策长期同时生效。
