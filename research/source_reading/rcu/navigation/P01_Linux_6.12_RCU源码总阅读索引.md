---
id: research.source_reading.rcu.index
title: "Linux 6.12 RCU 源码总阅读索引"
kind: source
status: evolving
domains:
  - linux
  - kernel
topics:
  - rcu
  - source_reading
---

# 第1章\_Linux\_6.12\_RCU源码总阅读索引

## 1.1\_版本边界与总索引职责

已经理解普通RCU的发布、借用和回收后，追源码常遇到第二个困难：同名接口为何有多个实现，某个GP为何同时涉及任务、CPU、节点和callback队列？本索引先定位当前问题所属的保护域与功能模块，再引向该模块的状态协作和具体实现。它是源码阅读导航，不要求读者先把所有优化分支通读一遍。

本目录固定于NXP `linux-imx` 官方标签 `lf-6.12.20-2.0.0`、提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`、Linux 6.12.20。历史快照包括Tree/PREEMPT_RCU，以及2026-09-05的Tiny、非SMP、PREEMPT_NONE生成配置；后者来自当时未提交的配置种子差量，不是固定标签默认配置。本轮以固定源码核对条件分支，未取得当前目标生成配置，不能把这些历史记录写成今天的板上运行结果。

总索引承担三项职责：

1. 先建立分类坐标；
2. 按功能模块选择概念导读；
3. 再从模块导读进入具体宏、字段和函数体的唯一实现标题。

它不复制知识正文，也不把文件列表冒充机制解释。

完整源码身份、保存规则和哈希证据见 [Linux 源码阅读基线](../../linux/SOURCE_BASELINE.md#1.5.1_RCU家族证据)。跨版本机制和应用主线从 [RCU 专题大纲](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/大纲.md#1.1_专题定位) 进入。

## 1.2\_先建立源码分类坐标

| 轴 | 先问什么 | Linux 6.12 主要位置 |
| --- | --- | --- |
| 保护域 | 普通对象 reader、SRCU 私有域还是任务 / trace 轨迹 | `tree*`、`srcutree.c`、`tasks.h` |
| 底层实现 | 普通 RCU 在 [SMP 构建](../../../../knowledge/foundations/computer_architecture/cache_coherence/P01_缓存一致性问题与缓存行.md#1.1.3_Linux中的CONFIG_SMP表示构建能力) 上走 Tree，还是在 `CONFIG_SMP=n`、`!PREEMPT_RCU` 构建中走 Tiny | `tree.c` / `tiny.c` 与 Kconfig |
| Tree 读侧模型 | 非抢占读区怎样以CPU边界证明退出；PREEMPT_RCU怎样保存被换出的旧任务责任 | `tree_plugin.h`、`sched.h` |
| GP 策略 | normal 还是 expedited | `tree.c` / `tree_exp.h` |
| callback 策略 | 普通 per-CPU 还是 NOCB | `tree.c`、`tree_nocb.h`、`rcu_segcblist.c` |
| 检查层 | 功能状态还是 Lockdep/Sparse 影子状态 | `update.c`、`rcupdate.h`、`Kconfig.debug` |

不要从某个函数名反推整套类别。`rcu_read_lock_sched()` 是普通 RCU 的上下文包装；NOCB 改 callback 执行位置；PREEMPT_RCU 改被抢占 reader 的债务保存；它们不会各自创建一套完整 GP 系统。

## 1.3\_模块主轴与配置比较轴

```mermaid
flowchart LR
    A["公共接口与读侧模型"] -->|"选择Tree后跟踪请求和完成"| B["普通Tree GP生命周期"]
    B -->|"参与集合从何建立和变更"| C["拓扑与hotplug"]
    B -->|"证据迟迟不来时如何观察"| D["FQS/Stall"]
    B -->|"另一完成策略如何组织"| E["Expedited"]
    B -->|"完成证据怎样交给回调"| F["Callback/NOCB"]
    F -->|"等待者到底等哪段责任"| G["同步等待/barrier"]
    A -->|"改为可睡眠私有域"| H["SRCU"]
    A -->|"改为任务执行轨迹"| I["Tasks"]
    A -->|"普通RCU在单CPU构建如何简化"| J["Tiny"]
    A -->|"区分功能与诊断状态"| K["Lockdep适配"]
```

这张图表示阅读问题的分流，不是函数调用图。普通Tree的两种读侧模型先在A建立，再在节点完成、加急及hotplug等涉及旧任务责任的位置回看差异；公共GP与callback骨架不必按配置重复学习。

## 1.4\_模块概念导读入口

| 顺序 | 模块导读 | 读者任务 | 对应知识正文 |
| --- | --- | --- | --- |
| [P02](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P02_RCU_抽象机制推演.md) | [公共接口与读侧模型](P02_Linux_6.12_RCU公共接口与读侧模型模块源码概念导读.md#2.1_模块问题与配置边界) | 公共 API 在哪里分流；CPU 债务与任务债务怎样比较并合流 | [P03 公共接口闭环](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P03_RCU_通用API与最小使用闭环.md#3.3_完整同步实现)、[P06 R0～R7 读侧阶段](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P06_Tree_RCU_读侧执行模型与配置差异.md#6.6_一组统一阶段怎样覆盖两种配置) |
| [P03](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P03_RCU_通用API与最小使用闭环.md) | [Tree RCU GP 全局生命周期](P03_Linux_6.12_Tree_RCU_GP全局生命周期模块源码概念导读.md#3.1_模块问题与版本边界) | 请求漏斗、长期 GP kthread、S0～S9 和完成发布 | [P05 公共 S0～S9](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P05_Tree_RCU_公共骨架与完整周期.md#5.5_S0到S9的一次完整周期)、[P08 物理 GP 生命周期](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P08_Tree_RCU_GP请求与全局生命周期.md#8.9_G0到G10_一轮物理GP的统一生命周期) |
| [P04](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P04_RCU_分类坐标与内核配置.md) | [拓扑与 CPU 热插拔](P04_Linux_6.12_Tree_RCU_拓扑与CPU热插拔模块源码概念导读.md#4.1_本模块究竟解决什么问题) | 静态树、CPU 参与集合和 callback 所有权怎样交接 | [P07](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P07_Tree_RCU_初始化_拓扑与执行上下文.md)、[P10](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P10_Tree_RCU_rcu_node树与分层汇聚.md)、[P17](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P17_Tree_RCU_CPU热插拔与回调迁移.md) |
| [P05](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P05_Tree_RCU_公共骨架与完整周期.md) | [force-QS 与 Stall](P05_Linux_6.12_Tree_RCU_force_QS与Stall模块源码概念导读.md#5.1_为什么GP已经在等还要有force_QS) | 被动观察、催促和三类 stall 时间线 | [P14](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P14_Tree_RCU_force_QS迟延与Stall.md) |
| [P06](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P06_Tree_RCU_读侧执行模型与配置差异.md) | [Expedited GP](P06_Linux_6.12_Tree_RCU_Expedited_GP模块源码概念导读.md#6.1_Expedited不是普通GP的加速档) | 独立序列、漏斗、CPU 选择、IPI 与共享安全条件 | [P15](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P15_Tree_RCU_Expedited_GP.md) |
| [P07](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P07_Tree_RCU_初始化_拓扑与执行上下文.md) | [Callback 与 NOCB](P07_Linux_6.12_Tree_RCU_回调与NOCB模块源码概念导读.md#7.1_GP完成为什么还不等于callback执行) | 分段、执行、bypass 和 GP/CB 线程交接 | [P11](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P11_Tree_RCU_rcu_segcblist回调状态机.md)、[P12](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P12_Tree_RCU_回调执行_批处理与限流.md)、[P16](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P16_Tree_RCU_NOCB回调卸载.md) |
| [P08](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P08_Tree_RCU_GP请求与全局生命周期.md) | [同步等待与 rcu_barrier](P08_Linux_6.12_Tree_RCU_同步等待与rcu_barrier模块源码概念导读.md#8.1_等RCU至少有三种不同对象) | 等 GP、等一个 callback 与等历史 callback 的区别 | [P13](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P13_Tree_RCU_同步等待与rcu_barrier.md) |
| [P09](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P09_Tree_RCU_QS_EQS与Context_Tracking.md) | [Tree SRCU](P09_Linux_6.12_Tree_SRCU模块源码概念导读.md#9.1_先分清Tree_RCU与Tree_SRCU) | 私有域、双 index reader 证明和 callback 需求树 | [P18](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P18_SRCU_私有域与双_index_状态机.md) |
| [P10](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P10_Tree_RCU_rcu_node树与分层汇聚.md) | [Tasks RCU](P10_Linux_6.12_Tasks_RCU模块源码概念导读.md#10.1_模块问题与三个flavor) | 共享控制骨架与 Tasks/Rude/Trace 的 flavor 证据 | [P19](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P19_Tasks_RCU_任务轨迹宽限期.md) |
| [P11](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P11_Tree_RCU_rcu_segcblist回调状态机.md) | [Tiny RCU](P11_Linux_6.12_Tiny_RCU模块源码概念导读.md#11.1_模块问题与当前配置前提) | 单 CPU 怎样保留普通 RCU 的 QS 与 callback 边界 | [P20](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P20_Tiny_RCU_单CPU实现.md) |
| [P12](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P12_Tree_RCU_回调执行_批处理与限流.md) | [RCU Lockdep 适配](P12_Linux_6.12_RCU_Lockdep适配模块源码概念导读.md#12.1_模块问题与实现所有权) | 功能状态、检查器影子状态和配置退化 | [P23](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P23_RCU_类型语义_Sparse与Lockdep.md)、[P24](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P24_RCU_调试验证与集成误用.md) |

表中“对应知识正文”只是读者任务映射，不要求两层文档逐句同步。知识正文建立稳定因果模型；模块导读固定版本化文件、状态地址和函数协作。

## 1.5\_唯一实现讲解入口

| 实现文档 | 唯一展开范围 |
| --- | --- |
| [P01 公共接口与检查机制](../source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.2_接口与源码索引) | 发布、取得、同步入口、Sparse 与 RCU Lockdep 宏 |
| [P02 CPU QS 与节点汇聚](../source_explanations/P02_Linux_6.12_Tree_RCU_等待桥_QS与节点汇聚关键函数源码实现.md#2.1_实现讲解边界与入口) | GP 变化感知、CPU QS 和节点上报 |
| [P03 抢占读者债务](../source_explanations/P03_Linux_6.12_Tree_RCU_抢占读者债务关键函数源码实现.md#3.1_实现讲解边界与入口) | 任务嵌套、blocked-task 登记和清债 |
| [P04 RCU Lockdep 适配层](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.1_实现所有权与读者目标) | map 定义、acquire/release、查询与 callback 上下文 |
| [P05 GP 全局生命周期](../source_explanations/P05_Linux_6.12_Tree_RCU_GP全局生命周期源码实现.md#5.2_源码符号覆盖账本) | 请求、线程、init、FQS、cleanup 与序列 |
| [P06 拓扑与 CPU 热插拔](../source_explanations/P06_Linux_6.12_Tree_RCU_拓扑与CPU热插拔源码实现.md#6.2_源码符号覆盖账本) | 节点构建、参与集合和 callback 迁移 |
| [P07 force-QS 与 Stall](../source_explanations/P07_Linux_6.12_Tree_RCU_force_QS与Stall源码实现.md#7.2_源码符号覆盖账本) | watching 快照、FQS 扫描和 stall 检测 |
| [P08 Expedited GP](../source_explanations/P08_Linux_6.12_Tree_RCU_Expedited_GP源码实现.md#8.2_源码符号覆盖账本) | expedited 序列、漏斗、CPU 选择、handler 与等待 |
| [P09 Callback 与 NOCB](../source_explanations/P09_Linux_6.12_Tree_RCU_回调与NOCB源码实现.md#9.2_源码符号覆盖账本) | 分段队列、batch、bypass 和双线程 |
| [P10 同步等待与 barrier](../source_explanations/P10_Linux_6.12_Tree_RCU_同步等待与rcu_barrier源码实现.md#10.2_源码符号覆盖账本) | `synchronize_rcu()`、哨兵、entrain 与全队列扫描 |
| [P11 Tree SRCU](../source_explanations/P11_Linux_6.12_Tree_SRCU源码实现.md#11.2_源码符号覆盖账本) | reader 累计账本、双扫描、callback 与 barrier |
| [P12 `rcu_init()` 启动初始化](../source_explanations/P12_Linux_6.12_Tree_RCU_rcu_init启动初始化源码实现.md#12.19_直接符号覆盖账本) | `start_kernel()` 调用现场、Tree/Tiny 分派、I0～I13、boot CPU 发布、workqueue 与后继 initcall 边界 |
| [P13 Tiny RCU](../source_explanations/P13_Linux_6.12_Tiny_RCU源码实现.md#13.2_源码符号覆盖账本) | UP 非抢占 reader 边界、二级指针 callback 分界、QS、softirq、同步、poll、barrier 与初始化 |

P02 与 P03 是同一读侧模块中的公共 CPU 路径和 PREEMPT_RCU 增量，不是非抢占 / 抢占两套完整系统。

表内13篇是当前实际存在的实现页；Tasks家族目前只有模块概念导读，没有逐函数实现页。遇到Tasks的精确实现问题，应承认这一展开范围，而不是把其他flavor的函数体当作它的替代证据。

## 1.6\_建议的源码阅读顺序

首次阅读普通 Tree RCU：

```text
知识正文P01-P07，先建立普通契约、读侧与初始化问题
    → navigation/P01本索引
    → navigation/P02公共接口与读侧模型
    → source_explanations/P12 rcu_init启动事务
    → navigation/P03 GP生命周期
    → navigation/P04拓扑与hotplug
    → navigation/P07 callback/NOCB
    → navigation/P08同步等待/barrier
    → 按问题选navigation/P05和P06慢路径
```

比较其他家族时，从分类坐标直接进入 P09 SRCU、P10 Tasks 或 P11 Tiny，不必先读完普通 Tree 的所有优化模块。

当前只研究 Tiny 时，采用更短路径：

```text
本索引P01
    → navigation/P11 Tiny模块概念导读
    → source_explanations/P13 Tiny源码实现
    → 用目标生成配置核对是否选择tiny.c
```

两个目录各自编号，因此单写P12可能指Lockdep模块导读，也可能指启动实现；以上路径前缀用于消除这个歧义。目标构建树存在时先核验源码身份与生成配置，再检查实际编译入口。Bear生成的编译数据库可以帮助定位命令，但必须来自同一次有效构建；没有数据库不影响阅读固定源码，有旧数据库也不能替当前配置作证。

## 1.7\_实验与观察入口

[晚到读者与抢占读者实验](../../../../labs/kernel/rcu/P01_晚到读者与抢占读者/README.md#1.1_实验要回答的两个问题)对应 P02 的两个结论，并可选观察抢占任务登记 / 清债和 GP event。实验没有覆盖 callback、NOCB、hotplug 和 stall 的完整运行闭环；对这些模块，本目录只提供版本化源码和 trace 入口，不夸大为已完成实验。

## 1.8\_总索引验收

开始追源码前，应能回答：

1. 当前调用点属于普通 RCU、SRCU 还是 Tasks 域；
2. Tree/Tiny 与 PREEMPT_RCU 分别在哪条轴；
3. 当前问题属于 reader、GP、汇聚、callback、等待还是慢路径模块；
4. 需要的是模块协作导读，还是某个函数体的唯一实现讲解；
5. 目标配置与固定源码基线能支持哪些结论。

无法回答时先回到 1.2 节分类，不要从 `kernel/rcu/` 的任意搜索结果开始拼调用链。

## 1.9\_职责边界

总索引只负责分流读者任务，不承担某个函数体的第二份解释。模块导读必须先就地讲清参与者、状态地址和调用协作；实现文档再提供裁剪源码、中文 Doxygen、上下文和副作用。若某个函数尚无唯一实现标题，应明确记录证据缺口，不能用一串原始源码链接替代讲解。
