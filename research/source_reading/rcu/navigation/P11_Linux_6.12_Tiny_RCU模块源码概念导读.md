---
id: research.source_reading.rcu.tiny_rcu_navigation
title: "Linux 6.12 Tiny RCU 模块源码概念导读"
kind: source
status: evolving
domains:
  - linux
  - kernel
topics:
  - rcu
  - tiny_rcu
  - source_reading
---

# 第11章\_Linux\_6.12\_Tiny\_RCU模块源码概念导读

## 11.1\_模块问题与单CPU前提

本篇阅读 `kernel/rcu/tiny.c` 怎样在单 CPU、非 PREEMPT_RCU 构建中兑现普通 RCU 契约。它不重新定义 reader，不属于 Tasks flavor；它删除的是跨 CPU 汇聚复杂度。

进入前必须核对目标构建的 `CONFIG_SMP`、`CONFIG_TINY_RCU` 和 `CONFIG_PREEMPT_RCU`。仓库已核对的既有 RCU 快照走 Tree + PREEMPT_RCU，本篇是固定 Linux 6.12.20 源码分支阅读，不声称在该快照上运行 Tiny。

## 11.2\_公共接口怎样落到Tiny

应用仍从 `include/linux/rcupdate.h` 调用普通 `rcu_read_lock()`、`call_rcu()` 和 `synchronize_rcu()`。Kconfig 与构建选择 `tiny.c`，不是某个对象在运行时选择 Tiny。

阅读时先从公共 API 确认语义，再进入 Tiny 的本地回调控制状态，避免把实现短小误读为契约为空。

## 11.3\_核心状态与所有权

| 状态 | 所有者 | 写入事件 | 消费者 |
| --- | --- | --- | --- |
| 待等待 callback | 唯一 CPU 的 Tiny 控制块 | `call_rcu()` 入队 | QS 推进路径 |
| 已成熟 callback | 同一控制块的可调用部分 | 合法 QS 到达 | `RCU_SOFTIRQ` 执行路径 |
| QS 观察 | 唯一 CPU | 调度或对应边界 | 回调成熟逻辑 |

没有 `rcu_node` 树不等于没有状态机。至少仍要区分 callback 提交、QS 证明、成熟和实际执行。

## 11.4\_一次回调完整周期

```text
call_rcu()把callback放入等待区
    → 唯一CPU仍可能运行旧非抢占reader
    → 后续合法QS排除旧reader
    → 等待区callback转入可调用区
    → RCU_SOFTIRQ执行回调
```

同步等待若在该构建前提下显得很轻，原因是调用路径和单 CPU 调度边界已经提供强前提，不能外推到 Tree RCU。

## 11.5\_源码阅读顺序

1. `kernel/rcu/Kconfig`：确认 Tiny 的选择条件；
2. `include/linux/rcupdate.h`：确认普通公共契约；
3. `kernel/rcu/tiny.c` 的控制块和 `call_rcu()` 接入；
4. `rcu_qs()`：观察唯一 CPU 怎样推进回调边界；
5. softirq 路径：确认成熟 callback 何时真正执行；
6. 同步等待入口：解释为何能在强前提下退化。

## 11.6\_Tree与Tiny对照验收

| 问题 | Tree RCU | Tiny RCU |
| --- | --- | --- |
| CPU 证据 | 多 CPU 分层汇聚 | 唯一 CPU 本地推进 |
| 节点拓扑 | `rcu_node` 树 | 无跨 CPU 树 |
| callback 边界 | 分段队列绑定 GP 代际 | 紧凑等待 / 可调用边界 |
| 公共调用者语义 | 普通 RCU | 同一普通 RCU |

完成后应能指出 Tiny 删除的状态、保留的时间边界，以及为什么 Tasks/SRCU 的结论不能因单 CPU 而自动折叠进 Tiny。

上一篇：[Tasks RCU 模块源码概念导读](P10_Linux_6.12_Tasks_RCU模块源码概念导读.md)。

下一篇：[RCU Lockdep 适配模块源码概念导读](P12_Linux_6.12_RCU_Lockdep适配模块源码概念导读.md)。
