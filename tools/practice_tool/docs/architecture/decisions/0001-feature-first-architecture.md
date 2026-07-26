---
id: tools.practice_tool.architecture.decisions.0001_feature_first
title: "ADR 0001 采用 Feature-first 工程组织"
kind: reference
status: maintained
domains:
  - tools
---

# 第1章\_ADR\_0001\_采用\_Feature-first\_工程组织

## 1.1\_状态

`accepted`

## 1.2\_背景

回路包含知识源、训练分类、训练模块、训练计划、会话、复习和导入导出。若按全局 `components`、`services`、`utils` 平铺，业务所有权和修改范围会随功能增长而模糊。

## 1.3\_决策

浏览器端按业务功能纵向组织，每个 feature 拥有界面、模型、业务操作和测试；外部存储与系统访问进入 infrastructure；应用入口只负责装配。模块之间只通过公开入口协作。

## 1.4\_替代方案

- 全局按技术类型分层：初期简单，但长期查找和所有权差。
- 完整 DDD 多层结构：边界严谨，但对当前规模过重。
- Monorepo：当前只有一个产品应用，没有足够收益。

## 1.5\_后果

- 新功能必须先确定业务归属。
- `shared` 不能成为杂物目录。
- 跨 feature 操作需要明确协调模块。
- 目录层次保持适度，不能把每个用例拆成过深路径。
