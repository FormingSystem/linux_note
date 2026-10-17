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

《深入理解 Linux RCU》把仓库中按职责分层保存的 RCU 权威材料重新编排为一条连续问题链：先从并发读写与对象回收问题推演抽象机制，建立通用 API；再提炼保护域、reader 状态、GP 请求、局部证据、汇聚、结果交付和初始化七项共同实现职责，并据此解释分类；随后分别闭合普通 Tree RCU、SRCU、Tasks 与 Tiny 的背景、原理和当前已有源码证据，最后用对象生命周期、工程应用、检查与选择收束。

知识正文、源码导航、逐函数实现和实验仍保存在各自权威目录，书内则按阅读问题交错出现。读者读完一个模块的原理后即可核对对应状态地址、调用路径或实验，不再先读完全部机制、隔两卷再寻找源码。

本目录只保存发行入口和按月冻结的派生产物。技术事实仍在原始专题、源码研究和实验目录维护，不能直接修改发行版 HTML 来代替正文修订。

## 1.2\_发行入口

- [打开当前版本](latest.html)
- [2026.09 月刊](releases/2026.09/index.html)
- [2026.09 来源与产物台账](releases/2026.09/publication.json)
- [2026.08 月刊（已显式刷新）](releases/2026.08/index.html)
- [2026.08 来源与产物台账](releases/2026.08/publication.json)

后续每月版本由 `catalog.json` 记录。生成成功不代表内容已经完成人工评审；人工确认程度以[知识库专题阅读与评审地图](../../../atlas/maps/knowledge_review_map.md#1.5_知识正文)为准。

2026.09 是当前版本，也是首次按常规月度窗口生成的版本；其来源仓库当时处于 clean 状态，具体提交、逐文件哈希和产物哈希以 [2026.09 来源与产物台账](releases/2026.09/publication.json)为准。2026.08 是首次建刊月份，维护者曾于 2026-08-30 明确授权覆盖刷新；该历史版本的证据单独保存在 [2026.08 来源与产物台账](releases/2026.08/publication.json)中。已经发布的两个月份都保持不可变，普通内容修订进入下一期。

当前 [编排清单](../../manifests/rcu.json) 已采用下一期的七卷交错编排，收录 59 篇来源文档和 2 个实验附件；`latest.html` 仍指向已经冻结的 2026.09 产物，其台账记录 51 篇来源文档和 2 个附件。两者是不同时间点的内容，不能从清单新增了章节就推断当前月刊也有这些章节。在到达下一月发布窗口或再次获得明确覆盖授权前，不用新清单重写 2026.08 或 2026.09。

要阅读正在维护的最新版，请进入 [RCU 专题大纲](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/大纲.md) 与 [Linux 6.12 RCU 源码阅读索引](../../../research/source_reading/rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md)。要复现某一期的内容，则使用上面的月份入口并保留对应台账。历史台账中有未提交来源时，单凭其中的 Git `HEAD` 不能重建这些未提交字节；详细验证边界见 [MarkBook 维护说明](../../README.md#1.4_维护命令)。

2026-10-17 在当前 Windows 检出中复核旧期时，2026.09 的 CSS、JavaScript 和两个实验附件因 `core.autocrlf=true` 被检出为 CRLF，字节哈希验证失败；对应 Git blob 以及将工作副本 CRLF 还原为 LF 后的哈希均与台账相符。因此目前不能把工作树上的整期验证记为通过，也不应通过改台账接受这些字节差异。需要复验原始字节时，应使用保留 Git blob 字节的独立检出或导出，再执行验证。

2026.08 的验证还发现了两处相同的书外旧链接：Lockdep 的 `lock_class_key` / `lockdep_map` 实现讲解已经迁移。现在可从 [lockdep_types.h 的身份结构讲解](../../../research/source_reading/lockdep/source_explanations/include/linux/lockdep_types.h.md#1.2_lock_class_key与lockdep_map身份结构) 继续阅读。冻结的是成书自身，指向活跃仓库的书外链接仍可能随正文迁移而失效；此处提供现行入口，不改写历史 HTML。

## 1.3\_内容分卷

下面描述 **当前清单为下一期准备的阅读顺序**。各月已经发行的章节以该月成书目录为准。

1. **问题背景、抽象契约与应用接口**：从读写锁和对象回收缺口走到同步 / 异步回收、失败清理与模块退出。
2. **实现抽象、分类依据与源码坐标**：先建立七项共同实现职责，再区分保护域、底层实现、执行模型、GP 策略、callback 策略和检查设施。
3. **普通 Tree RCU 原理、初始化与源码闭环**：以 S0～S9 为主线，把读侧证明、配置与树形容量、`rcu_init()`、早期请求交接、GP、QS、节点汇聚、callback、同步等待与批处理、慢路径、源码和现有实验交错编排；回调分段与 NOCB 执行交接分别解释。
4. **SRCU 背景、原理与实现**：连续阅读可睡眠 reader 的需求、双 index 状态机、模块导读和逐函数实现，再追踪实例建立、扩展和销毁。
5. **Tasks RCU 家族背景、原理与源码导读**：区分 Tasks、Tasks Rude、Tasks Trace 的完成证据；当前只承诺到模块级源码导读。
6. **Tiny RCU 单 CPU 原理与源码实现**：解释跨 CPU 汇聚为何消失以及安全边界怎样保留，再沿二级指针队列、QS 和 softirq 闭合逐函数实现；由 ARM 中断状态保存解释队列互斥，最后核对配置与构建证据。
7. **对象生命周期、工程应用、诊断与选择**：收束 kref 组合、应用模板、Sparse/Lockdep、调试验证、内存序与选型边界。

书内源码结论的固定基线是 NXP `linux-imx` 的 `lf-6.12.20-2.0.0`，对应 Linux 6.12.20 和提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`；每期实际知识仓库快照另见该期 `publication.json`。
