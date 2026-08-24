---
id: research.source_reading.lockdep.linux_6_12_index
title: "Linux 6.12 Lockdep 源码导读"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
topics:
  - locking
  - lockdep
---

# 第1章\_Linux\_6.12\_Lockdep源码导读

## 1.1\_基线与阅读目标

本专题源码证据来自 NXP 官方 `linux-imx` 仓库的发布标签 `lf-6.12.20-2.0.0`，不可变提交为 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，内核版本为 Linux 6.12.20。Lockdep 核心位于架构无关路径，因此本文讨论通用实现；PREEMPT_RT、NMI 和架构 IRQ flags 支持仍属于配置边界。

目标不是从 `kernel/locking/lockdep.c` 第一行顺序翻译到最后一行，而是追踪三条状态链：

1. 锁实例怎样经 `lockdep_map`、key 和 subclass 映射到锁类；
2. acquire/release 怎样维护 current 持锁账本、链缓存与全局依赖图；
3. 查询、断言、IRQ 使用状态和 proc 输出怎样消费这些记录。

稳定机制先读 [Linux Lockdep 专题大纲](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/大纲.md#1.1_专题定位)。

## 1.2\_源码地图

| 上游相对路径 | 主要职责 | 仓库证据 |
| --- | --- | --- |
| `include/linux/lockdep_types.h` | `lock_class_key`、`lock_class`、`lockdep_map`、`held_lock` | [`lockdep_types.h`](../../linux/include/linux/lockdep_types.h) |
| `include/linux/lockdep.h` | 初始化、acquire/release、查询、断言、pin 与配置关闭分支 | [`lockdep.h`](../../linux/include/linux/lockdep.h) |
| `include/linux/sched.h` | current 的链键、深度、递归状态和 `held_locks[]` | [`sched.h`](../../linux/include/linux/sched.h) |
| `kernel/locking/lockdep.c` | 锁类登记、事件状态机、图搜索、IRQ 规则、查询和告警 | [`lockdep.c`](../../linux/kernel/locking/lockdep.c) |
| `kernel/locking/lockdep_internals.h` | 容量、链与内部辅助定义 | [`lockdep_internals.h`](../../linux/kernel/locking/lockdep_internals.h) |
| `kernel/locking/lockdep_proc.c` | `/proc/lockdep*` 与 lockstat 输出 | [`lockdep_proc.c`](../../linux/kernel/locking/lockdep_proc.c) |
| `lib/Kconfig.debug` | `PROVE_LOCKING`、`DEBUG_LOCK_ALLOC`、`LOCKDEP` 等选择关系 | [`Kconfig.debug`](../../linux/lib/Kconfig.debug) |
| `Documentation/locking/lockdep-design.rst` | 锁类、IRQ、依赖、注解、闭包与性能设计 | [`lockdep-design.rst`](../../linux/Documentation/locking/lockdep-design.rst) |

以上保存文件已与该提交对应 Git blob 逐文件核对；清单和版本说明见 [Linux 源码阅读基线](../../linux/SOURCE_BASELINE.md#1.5.2_Lockdep证据)。

## 1.3\_模块导读

| 阅读入口 | 解决的问题 | 对应实现讲解 |
| --- | --- | --- |
| [身份与事件接入模块导读](P02_Linux_6.12_Lockdep身份与事件接入模块导读.md#2.1_模块问题) | 标准锁怎样取得检查身份，事件怎样进入 current 账本 | [身份与锁类](../source_explanations/P01_Linux_6.12_Lockdep身份与锁类源码实现.md#1.1_关联入口)、[取得释放与持锁账本](../source_explanations/P02_Linux_6.12_Lockdep取得释放与持锁账本源码实现.md#2.1_关联入口) |
| [依赖图与规则引擎模块导读](P03_Linux_6.12_Lockdep依赖图与规则引擎模块导读.md#3.1_模块问题) | 局部持锁链怎样变成全局边，环与 IRQ 冲突怎样检查 | [依赖图与规则引擎](../source_explanations/P03_Linux_6.12_Lockdep依赖图与规则引擎源码实现.md#3.1_关联入口) |
| [查询适配与诊断模块导读](P04_Linux_6.12_Lockdep查询适配与诊断模块导读.md#4.1_模块问题) | current 查询怎样服务断言和 RCU，告警后怎样判断检查器状态 | [查询、注解与配置](../source_explanations/P04_Linux_6.12_Lockdep查询注解与配置源码实现.md#4.1_关联入口) |

模块导读借源码讲清职责、调用链和阅读顺序，不逐函数复制实现；具体宏、结构和函数只在 `source_explanations/` 的唯一标题展开。

## 1.4\_一次acquire的主调用链

以 mutex 为例，常见主线是：

```text
mutex_init()
  → 静态lock_class_key
  → __mutex_init()
  → debug_mutex_init()
  → lockdep_init_map_type()

mutex_lock*()
  → mutex_acquire_nest()
  → lock_acquire()
  → __lock_acquire()
  → register_lock_class()
  → mark_usage()
  → validate_chain()
      → check_deadlock()
      → check_prevs_add()
          → check_prev_add()
              → check_noncircular()
              → check_irq_usage()
              → add_lock_to_list()
  → 提交current->held_locks[]与curr_chain_key

mutex_unlock()
  → mutex_release()
  → lock_release()
  → __lock_release()
```

注意：阻塞 mutex 在真正取得功能锁以前就上报 acquire 尝试，以便建模等待关系；可中断取得失败时会走 release 注解回退。不要把 `lock_acquire()` 的函数名误读成 CPU acquire memory ordering 或功能锁已经成功。

## 1.5\_状态地址与所有权

```mermaid
flowchart LR
    INST["具体锁实例<br/>嵌入dep_map"] -->|"key与subclass"| CLASS["全局lock_class<br/>usage_mask与前后依赖"]
    INST -->|"acquire事件"| HELD["current->held_locks[]<br/>具体实例与类索引"]
    HELD -->|"当前前驱"| ENGINE["规则引擎"]
    CLASS -->|"历史可达路径"| ENGINE
    ENGINE -->|"验证通过"| CLASS
    ENGINE -->|"提交／回退"| HELD
    HELD -->|"lock_is_held_type"| QUERY["断言、RCU与子系统检查"]
```

`held_locks[]` 是任务侧当前事实，`lock_class` 图是全局历史；两者不能互相替代。

## 1.6\_建议阅读顺序

1. 先读 `lockdep_types.h` 的四个核心结构，建立实例、类和 held record 区别；对应[锁类身份结构讲解](../source_explanations/P01_Linux_6.12_Lockdep身份与锁类源码实现.md#1.2_lock_class_key与lockdep_map身份结构)。
2. 再读 `mutex_init()` 一类标准原语怎样提供静态 key，随后跟到 `lockdep_init_map_type()` 与 `register_lock_class()`；不要从地址猜类身份。
3. 读 `sched.h` 的任务字段，然后沿 `lock_acquire()` 进入 `__lock_acquire()`，把 S0～S5 的每次写入标在纸上；对应[取得状态提交](../source_explanations/P02_Linux_6.12_Lockdep取得释放与持锁账本源码实现.md#2.4___lock_acquire取得状态提交)。
4. 只在当前状态闭环清楚以后阅读 `validate_chain()`、`check_prev_add()` 与 IRQ 搜索，否则容易把链缓存、图边和当前栈混在一起。
5. 最后读 `lock_is_held_type()`、断言和 proc 输出，观察同一 held record 怎样被业务检查和诊断消费。

## 1.7\_配置边界

- `CONFIG_PROVE_LOCKING=y` 选择完整依赖验证所需的 `LOCKDEP`、`DEBUG_LOCK_ALLOC` 和 IRQ flags 跟踪支持；
- `CONFIG_DEBUG_LOCK_ALLOC=y` 也选择 `LOCKDEP`，侧重活锁对象释放、重初始化和任务持锁退出等检查；
- `CONFIG_LOCKDEP=n` 时 `lockdep_map`/key 可以成为空结构，多数事件和断言为空操作；
- `CONFIG_LOCK_STAT=y` 复用 hook 增加性能统计，但不是 Lockdep 正确性图的同义词；
- 当前仓库基线曾核对 Tree RCU 配置，但没有保存目标板 Lockdep 已启用的断言；本文只说明源码配置分支，不宣称某块板当前就在运行 Lockdep。

## 1.8\_阅读完成标准

读者应能：

1. 从一个具体锁实例指出 map、key、class 和 held record 各自位置；
2. 解释 acquire 尝试为什么在功能取得成功以前发生，以及失败怎样回退；
3. 说明链缓存命中省略了什么、仍保留什么；
4. 从候选 `A → B` 推导为什么搜索 `B → ... → A`；
5. 区分 IRQ 使用位、全局历史边和 current 状态；
6. 解释 `lockdep_is_held()` 查询指定实例而不是全局锁占用；
7. 从 `/proc/lockdep_stats` 判断容量和 `debug_locks` 是否仍有效。
