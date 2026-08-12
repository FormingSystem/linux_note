---
id: research.source_reading.rcu.linux_6_12_source_guide
title: "Linux 6.12 Tree RCU 与 SRCU 源码导读"
kind: source
status: evolving
domains:
  - linux
  - kernel
topics:
  - synchronization
  - rcu
  - source_reading
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_Tree\_RCU\_与\_SRCU\_源码导读

## 1.1\_版本与阅读边界

本章对应 NXP Linux 6.12.20 源码，只解释仓库中已保存的 Tree RCU 和 Tree SRCU 核心文件。本章是版本化实现证据，前面章节仍负责总结跨版本成立的机制模型。

## 1.2\_源码文件地图

| 文件 | 主要内容 |
| --- | --- |
| [`include/linux/rcupdate.h`](../../linux/include/linux/rcupdate.h) | 公共 RCU API、指针发布/取得、读侧标记、`kfree_rcu()` |
| [`include/linux/rculist.h`](../../linux/include/linux/rculist.h) | list/hlist 的 RCU 发布、删除和遍历宏 |
| [`include/linux/rcu_segcblist.h`](../../linux/include/linux/rcu_segcblist.h) | 分段回调列表的公共结构与接口 |
| [`kernel/rcu/tree.h`](../../linux/kernel/rcu/tree.h) | `rcu_node`、`rcu_data`、`rcu_state` 等 Tree RCU 核心结构 |
| [`kernel/rcu/tree.c`](../../linux/kernel/rcu/tree.c) | GP 线程、静止状态上报、回调推进与执行 |
| [`kernel/rcu/tree_plugin.h`](../../linux/kernel/rcu/tree_plugin.h) | PREEMPT_RCU 等配置相关的读者跟踪实现 |
| [`kernel/rcu/update.c`](../../linux/kernel/rcu/update.c) | 通用等待 callback、RCU lockdep maps、读侧状态查询与检查器启用条件 |
| [`kernel/rcu/tree_exp.h`](../../linux/kernel/rcu/tree_exp.h) | expedited grace period |
| [`kernel/rcu/tree_nocb.h`](../../linux/kernel/rcu/tree_nocb.h) | `rcu_nocbs` 回调 offload |
| [`kernel/rcu/tree_stall.h`](../../linux/kernel/rcu/tree_stall.h) | RCU CPU stall 检测与诊断 |
| [`include/linux/srcu.h`](../../linux/include/linux/srcu.h) | SRCU 公共 API 和读侧封装 |
| [`include/linux/srcutree.h`](../../linux/include/linux/srcutree.h) | `srcu_data`、`srcu_node`、`srcu_usage`、`srcu_struct` |
| [`kernel/rcu/srcutree.c`](../../linux/kernel/rcu/srcutree.c) | Tree SRCU 初始化、GP、回调和同步等待 |

阅读公共 API 时应把同一宏中的两类路径分开：`READ_ONCE()`、release 发布、GP 和 callback 属于功能机制；`rcu_check_sparse()`、`rcu_lock_acquire()` 与 `RCU_LOCKDEP_WARN()` 属于静态或动态检查机制。检查路径可以发现已覆盖的误用，但不替代功能路径的正确性证明。

## 1.3\_Tree\_RCU\_的三层状态

### 1.3.1\_rcu\_data

`struct rcu_data` 是每 CPU 状态，它把当前 CPU 与所属 `rcu_node` 叶节点关联起来，并保存：

- 当前 CPU 观察到的 GP 序列与静止状态。
- 该 CPU 的回调分段列表 `cblist`。
- `mynode`、`grpmask`、NOCB、延迟 QS 与 watching 快照等每 CPU 执行状态。

**6.12.20 版本边界：** user/idle 的 RCU watching 主状态位于通用 `context_tracking.state`，不再是 Linux 5.10 中 `rcu_data.dynticks_nesting`、`dynticks_nmi_nesting` 和 `atomic_t dynticks` 那组字段。`rcu_data.watching_snap` 等字段保存 GP 扫描使用的观察快照，不等于拥有 watching 状态。

### 1.3.2\_rcu\_node

`struct rcu_node` 构成一棵分层树。叶节点聚合一组 CPU 的静止状态，中间节点继续向上聚合。`qsmask` 类位图表示当前 GP 仍在等待哪些子节点或 CPU。

这种层次聚合避免了大型机器上所有 CPU 频繁争用同一个全局锁。

### 1.3.3\_rcu\_state

`struct rcu_state` 表示 Tree RCU 全局状态，包括 `gp_seq`、GP 线程、`rcu_node` 树和宽限期控制信息。源码中的全局 `rcu_state` 实例是普通 Tree RCU 宽限期协调的中心。

## 1.4\_一次宽限期的主线

```mermaid
flowchart TD
    A["call_rcu() 或同步等待提出 GP 需求"] --> B["rcu_gp_kthread() 被唤醒"]
    B --> C["rcu_gp_init() 启动新 GP"]
    C --> D["初始化 rcu_node 树的等待状态"]
    D --> E["CPU / 任务经过静止状态"]
    E --> F["rcu_check_quiescent_state()"]
    F --> G["rcu_report_qs_rnp() 逐层上报"]
    G --> H{"根节点是否仍有等待位？"}
    H -- 是 --> I["rcu_gp_fqs_loop() 等待或强制扫描"]
    I --> E
    H -- 否 --> J["GP 结束，回调进入可执行阶段"]
```

### 1.4.1\_启动

`rcu_gp_kthread()` 是普通 GP 管理线程。它在存在新 GP 需求时调用 `rcu_gp_init()`，后者通过 `rcu_seq_start()` 推进 `rcu_state.gp_seq`，并对 CPU hotplug 与节点等待状态进行初始化。

### 1.4.2\_等待与强制扫描

`rcu_gp_fqs_loop()` 在 GP 期间等待静止状态上报。必要时它会触发 force-quiescent-state 扫描，处理 idle/EQS、dynticks、CPU hotplug 以及长时间未报告的 CPU。

### 1.4.3\_每\_CPU\_核心处理

`rcu_core()` 运行每 CPU RCU 核心工作：

1. 处理延后的静止状态。
2. 调用 `rcu_check_quiescent_state()` 更新 GP 状态。
3. 将新回调加速到对应的 GP 分段。
4. 对已经可执行的回调调用 `rcu_do_batch()`。

## 1.5\_回调为什么需要分段列表

`struct rcu_segcblist` 不是一个单纯 FIFO。它把回调按 GP 进度划分成不同段，使新注册、已分配 GP、已等待完成和可执行回调不会混为一谈。

可以用下列抽象理解：

```text
新回调
   ↓ 分配目标宽限期
等待对应 GP
   ↓ GP 完成后推进
可执行回调
   ↓ rcu_do_batch()
调用 func(struct rcu_head *)
```

`call_rcu()` 本身只负责将回调交给这套系统；它不是立即启动一个专属 GP，多个回调可以共享后续宽限期进度。

## 1.6\_synchronize\_rcu()与\_call\_rcu()

`synchronize_rcu()` 是阻塞等待接口，但“同步”不等于默认选择 expedited GP。Linux 6.12.20 的普通入口进入 `synchronize_rcu_normal()`；默认 `rcu_normal_wake_from_gp=0` 时，通过 `wait_rcu_gp(call_rcu_hurry)` 排入一个 GP 后唤醒 callback 并等待 completion。只有显式调用 `synchronize_rcu_expedited()`，或专门配置/调用路径，才进入 expedited 机制。它的 lockdep 检查会报告在 RCU 读侧临界区内调用的非法情况。

`call_rcu()` 通过 `__call_rcu_common()` 排队回调，调用者不等待 GP。两者都依赖 Tree RCU 的 GP 推进，但交付方式不同。

## 1.7\_Tree\_SRCU\_数据结构

| 结构 | 作用 |
| --- | --- |
| `srcu_struct` | 使用者持有的 SRCU 域入口 |
| `srcu_usage` | 该域的 GP 序列、锁、工作和生命期状态 |
| `srcu_data` | 每 CPU 的读计数、锁和回调列表 |
| `srcu_node` | 层次聚合读计数与回调进度 |

Tree SRCU 通过两个 index 分期统计读者。`synchronize_srcu()` 的源码注释明确描述了“先等待一个 index 的计数排空，翻转 index，再等待另一个”的基本模型。

## 1.8\_Tree\_SRCU\_回调与宽限期主线

```mermaid
flowchart TD
    A["call_srcu()"] --> B["__call_srcu()"]
    B --> C["srcu_gp_start_if_needed()"]
    C --> D["srcu_funnel_gp_start()"]
    D --> E["SRCU GP 状态机两阶段扫描读者"]
    E --> F["srcu_gp_end()"]
    F --> G["srcu_invoke_callbacks()"]
```

`call_srcu()` 的回调在 process context 执行，但 Linux 6.12.20 的注释仍要求回调必须快速且不得阻塞。“SRCU 读者可阻塞”不能推导出“SRCU 回调也可以任意阻塞”。

`synchronize_srcu()` 内部通过一个栈上 `rcu_synchronize` 对象注册唤醒回调，然后等待 completion。这正是它必须在 process context 调用、且不得从同域读侧内调用的直接实现证据。

## 1.9\_建议的源码阅读顺序

1. 先从 `rcupdate.h` 阅读 `rcu_assign_pointer()`、`rcu_dereference()` 和读侧封装，并标出功能语句与 Sparse/Lockdep 检查语句；要查看宏体、中文 Doxygen 说明和实现原理时，进入[RCU 公共接口与检查机制源码详解](../source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.2_接口与源码索引)。
2. 再读 `update.c` 的 RCU lockdep maps、`debug_lockdep_rcu_enabled()` 和 `rcu_read_lock_held()`，理解检查状态从哪里来、关闭配置后哪些分支为空操作；具体宏实现见[RCU Lockdep 状态来源](../source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.6_RCU_Lockdep状态来源)和[`RCU_LOCKDEP_WARN()` 检查适配层](../source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.7_RCU_LOCKDEP_WARN检查适配层)。
3. 阅读 `tree.h` 中的 `rcu_data`、`rcu_node`、`rcu_state`，先分清每任务、每 CPU、每节点和全局状态。
4. 沿[非抢占式 Tree RCU 模块源码概念导读](P02_Linux_6.12_非抢占式_Tree_RCU_模块源码概念导读.md)建立 CPU QS、树形汇聚和同步等待闭环；遇到具体函数再进入[非抢占式 Tree RCU 关键函数源码实现](../source_explanations/P02_Linux_6.12_非抢占式_Tree_RCU_关键函数源码实现.md#2.2_函数实现索引)。
5. 再沿[抢占式 Tree RCU 模块源码概念导读](P03_Linux_6.12_抢占式_Tree_RCU_模块源码概念导读.md)增加 `task_struct`、`blkd_tasks` 和 `gp_tasks` 这条任务债务轴；具体字段、链表入队和退出实现见[抢占式 Tree RCU 关键函数源码实现](../source_explanations/P03_Linux_6.12_抢占式_Tree_RCU_关键函数源码实现.md#3.2_任务与节点的共享状态实现)。
6. 沿[Tasks RCU 与 Tiny RCU 模块源码概念导读](P04_Linux_6.12_Tasks_RCU与Tiny_RCU模块源码概念导读.md)区分任务扫描、Trace reader 与单 CPU 回调边界。当前仓库未保存 `tasks.h` 和 `tiny.c` 快照，因此本轮不伪造对应的函数实现文档。
7. 沿 `call_rcu()` 进入回调排队，再阅读 `rcu_segcblist` 的分段模型与 `rcu_do_batch()`。
8. 最后对照 `srcu.h`、`srcutree.h` 和 `srcutree.c`，比较 Tree SRCU 的私有域和两 index 模型。

## 1.10\_源码阅读验收

1. 能画出 `rcu_data` → `rcu_node` → `rcu_state` 的层次关系。
2. 能从 `call_rcu()` 追踪到回调入队、GP 推进和 `rcu_do_batch()`。
3. 能解释 `qsmask` 类状态为什么需要在 `rcu_node` 树中逐层聚合。
4. 能解释 `rcu_segcblist` 为什么不能只是一个普通 FIFO。
5. 能说明 Tree SRCU 两 index 读计数与私有域的关系。
6. 能把公共 API 中的功能机制、Sparse 静态检查与 Lockdep 动态检查分别追到源码落点，并说明关闭检查不等于取消调用契约。

专题入口：[RCU 专题大纲](../../../../knowledge/linux/synchronization/rcu/大纲.md)。

首篇模块概念导读：[Linux 6.12 非抢占式 Tree RCU 模块源码概念导读](P02_Linux_6.12_非抢占式_Tree_RCU_模块源码概念导读.md)。

