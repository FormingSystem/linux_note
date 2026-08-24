---
id: markbook.topic.rcu
title: "RCU MarkBook"
kind: publication
status: evolving
domains:
  - linux
  - synchronization
  - rcu
---

# 第1章\_RCU\_MarkBook

## 1.1\_阅读定位

《深入理解 Linux RCU》把仓库中按职责分层保存的 RCU 权威材料组织为一条连续路径：先从并发读写与对象回收问题推演抽象机制，再建立 Tree RCU 的状态、通信和证明模型，随后进入 Linux 6.12.20 的源码导航与唯一实现讲解，最后用可复现实验和交叉专题边界收束。

本目录只保存发行入口和按月冻结的派生产物。技术事实仍在原始专题、源码研究和实验目录维护，不能直接修改发行版 HTML 来代替正文修订。

## 1.2\_发行入口

- [打开当前版本](latest.html)
- [2026.08 首刊](releases/2026.08/index.html)
- [2026.08 来源与产物台账](releases/2026.08/publication.json)

后续每月版本由 `catalog.json` 记录。生成成功不代表内容已经完成人工评审；人工确认程度以[知识库专题阅读与评审地图](../../../atlas/maps/knowledge_review_map.md#1.5_知识正文)为准。

## 1.3\_内容分卷

1. **机制与应用**：RCU 专题大纲及 P01～P28 权威正文。
2. **Linux 6.12 源码导航**：总索引与按模块组织的概念导读。
3. **Linux 6.12 唯一实现讲解**：宏、字段、函数体和配置分支的唯一展开位置。
4. **实验与交叉边界**：晚到读者与抢占读者实验，并链接 kref、Linux 数据结构和 Lockdep/Sparse 的权威交叉专题。

书内源码结论的固定基线是 NXP `linux-imx` 的 `lf-6.12.20-2.0.0`，对应 Linux 6.12.20 和提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`；每期实际知识仓库快照另见该期 `publication.json`。
