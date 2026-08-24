---
id: atlas.home
title: "知识库导航"
kind: track
status: maintained
domains:
  - navigation
---

# 第1章\_知识库导航

本页是知识库的统一入口。`atlas` 只组织链接和阅读顺序，不保存知识正文；具体结论仍以各领域文档为准。

## 1.1\_按目标进入

| 目标 | 入口 | 适合场景 |
| --- | --- | --- |
| 选择可读内容并查看人工确认程度 | [知识库专题阅读与评审地图](maps/knowledge_review_map.md) | 使用 MarkMind 按领域、专题和章节跳转，并识别未校正、人工评审中、评审完成三种状态 |
| 建立全局认识 | [Linux系统与驱动知识地图](maps/linux_system_map.md) | 了解各领域边界及依赖关系 |
| 系统学习内核机制 | [Linux内核机制学习路线](tracks/linux_kernel_track.md) | 从内核结构学到并发、中断和设备模型 |
| 聚焦连续阅读一个专题 | [RCU MarkBook](../markbook/topics/rcu/README.md#1.2_发行入口) | 在一册月度快照中连续阅读稳定机制、Linux 6.12 源码导航、唯一实现讲解与实验，同时保留来源追溯 |
| 推演同步与异步机制 | [同步和异步机制总纲](../knowledge/linux/synchronization_and_asynchrony/大纲.md)、[Linux 6.12 源码基线](../research/source_reading/linux/SOURCE_BASELINE.md#1.5.3_锁_序列计数器_等待与工作队列证据) | 从锁、快照、等待和异步执行的稳定模型进入版本化状态与调用链 |
| 读懂内核编译期注解 | [编译器与 Sparse 注解专题](../knowledge/foundations/c_language/kernel_static_annotations/大纲.md#1.1_专题定位)、[研究型实验](../labs/foundations/c_language/P01_Sparse地址空间与上下文记账/README.md#1.1_实验目标)、[Linux 6.12 源码导读](../research/source_reading/compiler_annotations/navigation/P01_Linux_6.12_编译器与Sparse注解源码导读.md#1.1_基线与阅读任务) | 从预处理展开进入地址域、context、BTF 与工具验证 |
| 理解并验证锁协议 | [Lockdep 专题](../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/大纲.md#1.1_专题定位)、[锁顺序反转实验](../labs/kernel/lockdep/P01_锁顺序反转与报告解读/README.md)、[Linux 6.12 源码总阅读索引](../research/source_reading/lockdep/navigation/P01_Linux_6.12_Lockdep源码导读.md#1.6_建议阅读顺序) | 从锁序与 IRQ 反例建立模型，亲手读取报告，再核对版本化状态和调用链 |
| 学习驱动开发 | [Linux驱动开发学习路线](tracks/linux_driver_track.md) | 从模块、字符设备走到平台驱动和 Input 子系统 |
| 查找现有内容 | [仓库内容索引](indexes/content_index.md) | 按领域定位文档、实验和研究材料 |
| 规划后续建设 | [知识库建设路线图](roadmaps/content_roadmap.md) | 查看内容覆盖、缺口和维护优先级 |

## 1.2\_按内容类型进入

- 知识本体：[基础知识](../knowledge/foundations)、[Linux 通用机制](../knowledge/linux)、[内核子系统](../knowledge/kernel_subsystems)、[驱动模型](../knowledge/driver_model)、[系统软件](../knowledge/system_software)。
- 平台与验证：[平台实现](../platforms)、[实验](../labs)。
- 研究与查询：[源码阅读与调查](../research)、[参考资料](../reference)。
- 电子书与出版物：[RCU MarkBook](../markbook/topics/rcu/README.md#1.2_发行入口)、[《奔跑吧 Linux 内核》](../publications/books/running_linux_kernel/README.md)。MarkBook 是按月冻结的专题派生快照，事实仍回到权威正文、源码研究和实验维护。
- 仓库维护：[信息架构设计](../governance/architecture/repository_information_architecture.md)、[Git 协作与提交规范](../governance/conventions/git_guide.md)。

## 1.3\_导航约定

- 地图说明知识之间的关系，路线规定建议阅读顺序，索引回答“内容在哪里”，路线图记录建设计划。
- [知识库专题阅读与评审地图](maps/knowledge_review_map.md)是面向读者的唯一评审状态台账；状态不写回专题 Front Matter，也不在其他入口重复维护。
- 路线中的“选读”表示可按任务跳过，不表示内容不重要。
- 尚未形成正式文档的领域只在路线图中记录，不创建空目录或无效链接。
- 新增长篇专题后，应同步更新相应地图、路线和索引；移动文件后使用仓库链接工具维护引用。
