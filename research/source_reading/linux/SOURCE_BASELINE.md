---
id: research.source_reading.linux.source_baseline
title: "Linux 源码阅读基线"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
---

# 第1章\_Linux\_源码阅读基线

## 1.1\_当前来源

本目录保存知识正文实际引用的 Linux 源码证据，不是完整内核镜像。

| 项目 | 当前值 |
| --- | --- |
| 源码来源 | NXP 官方 i.MX 厂商内核仓库 |
| 官方远端 | `https://github.com/nxp-imx/linux-imx.git` |
| 来源分支 | `lf-6.12.y` |
| 发布标签 | `lf-6.12.20-2.0.0` |
| 版本 | Linux 6.12.20 |
| Git 提交 | `dfaf2136deb2af2e60b994421281ba42f1c087e0` |
| 配置边界 | 发布标签不携带 `.config`；已核对的不同工作树配置快照在下表分别记录 |
| 平台背景 | NXP i.MX 厂商内核树，以 i.MX6ULL/ARM 为当前平台背景；通用机制优先引用架构无关目录 |
| 本地工作树 | 由每次会话在当前环境中发现并验证，不在仓库记录绝对路径 |
| 许可证 | 以各源码文件 SPDX、版权头及原源码树 `COPYING`/`LICENSES` 为准 |

发布标签已通过官方远端核对，其解引用提交的顶层 `Makefile` 为 `VERSION=6`、`PATCHLEVEL=12`、`SUBLEVEL=20`；仓库保存的 RCU、Lockdep 与内存序源码副本也与该提交逐文件核对一致。`lf-6.12.y` 是可能继续前进的来源分支，不能替代发布标签及其不可变提交作为长期证据定位。

同一源码提交可以配合不同 `.config`，因此配置证据按核对任务分别保存，不再压成一个“当前配置”：

| 配置快照 | 已核对状态 | 使用边界 |
| --- | --- | --- |
| 既有 RCU 研究快照 | `CONFIG_TREE_RCU=y`、`CONFIG_PREEMPT_RCU=y` | 支撑现有抢占式 Tree RCU 主分支导读；不外推到本次工作树 |
| 2026-08-24 开发工作树 | `CONFIG_TREE_RCU=y`、`CONFIG_SMP=y`、`CONFIG_MUTEX_SPIN_ON_OWNER=y`、`CONFIG_RWSEM_SPIN_ON_OWNER=y`；未启用普通 `CONFIG_PREEMPT`，未启用 `CONFIG_WQ_WATCHDOG` | 支撑本次锁、等待、序列计数器和工作队列的可运行分支判断 |

2026-08-24 重新发现的候选工作树已核对官方远端与 `lf-6.12.y` 分支；其 `HEAD=7b60e547d2783f8fee61ff7d7be3e066825b9c3a`，以固定发布提交为 merge-base 并前进 3 个提交。工作树 `Makefile` 仍为 Linux 6.12.20。本文新增源码导读全部通过该工作树中的标签对象读取固定提交内容，不把分支头的后续变化混入实现讲解。

本基线标识的是 NXP `linux-imx` 仓库中的一份确定源码快照，不是某个用户名、目录名、共享地址或挂载点。以后补充或复核源码时，应先验证候选工作树的官方远端、分支、`HEAD`、`Makefile` 和相关 Kconfig，再引用上游相对路径；本地绝对路径不得写入已跟踪文档。

## 1.2\_保存规则

- 保持 Linux 上游相对路径，例如 `fs/char_dev.c` 保存为本目录的 `fs/char_dev.c`。
- C/H/RST 文件保持原文，不在源码文件内混入笔记；解释写入 Markdown 正文或独立源码导读。
- 只复制当前专题用于验证数据结构、调用链和状态机的文件，不无选择复制整棵源码树。
- 新增源码时同步更新本清单；若工作树的分支、提交或版本变化，必须记录新基线，不能让不同时期文件无标记混合。
- 稳定知识正文说明机制，版本源码负责提供具体函数、字段和目录位置证据。

## 1.3\_字符设备与\_VFS\_证据

| 相对路径 | 主要用途 |
| --- | --- |
| `fs/char_dev.c` | 设备号登记、`cdev_map`、`chrdev_open()`、cdev 生命周期 |
| `fs/open.c` | 打开系统调用、`do_dentry_open()` 和 VFS open 边界 |
| `fs/namei.c` | 路径查找、创建与打开状态机 |
| `fs/file.c` | fd table 扩展、安装和关闭 |
| `fs/read_write.c` | read/write 系统调用与 VFS 分派 |
| `fs/inode.c` | inode 缓存和生命周期 |
| `fs/dcache.c` | dentry cache、查找和回收 |
| `fs/super.c` | superblock 建立、激活和关闭 |
| `fs/namespace.c` | mount 与 mount namespace |
| `fs/filesystems.c` | `file_system_type` 注册 |
| `include/linux/fs.h` | superblock、inode、file、file_operations 等核心定义 |
| `include/linux/cdev.h` | `struct cdev` 和字符设备接口 |
| `include/linux/dcache.h` | dentry 定义与接口 |
| `include/linux/mount.h` | mount 的公开边界 |
| `include/linux/file.h` | file/fd 辅助接口 |
| `drivers/base/devtmpfs.c` | devtmpfs 设备节点处理 |

## 1.4\_VFS\_扩展证据

| 相对路径 | 主要用途 |
| --- | --- |
| `fs/fs_context.c`、`include/linux/fs_context.h` | 挂载上下文、参数解析和建树事务 |
| `fs/mount.h`、`fs/pnode.c` | 内部 mount 状态和传播关系 |
| `fs/file_table.c` | file 分配、`fput()` 与释放 |
| `mm/filemap.c` | 页缓存、通用 buffered I/O 和文件 fault |
| `mm/page-writeback.c`、`fs/fs-writeback.c` | dirty 节流、inode/folio 写回 |
| `fs/sync.c` | sync、fsync 和同步入口 |
| `fs/direct-io.c`、`fs/iomap/direct-io.c` | Direct I/O 请求与完成 |
| `fs/notify/`、`include/linux/fsnotify*.h` | fsnotify group、mark、event 和 VFS 通知入口 |
| `fs/ramfs/inode.c`、`fs/libfs.c` | 最小内存文件系统和通用文件系统辅助实现 |
| `fs/anon_inodes.c`、`fs/pipe.c` | 匿名 file 与 pipe 特殊接入 |

## 1.5\_已有其他机制证据

本目录还保存 RCU、kobject、引用计数、内存管理和数据结构等已有源码。后续会根据实际来源逐步核对其版本；在完成核对前，不应仅凭目录共存就断言所有旧文件都来自本章记录的 6.12.20 基线。

### 1.5.1\_RCU家族证据

下列 RCU 核心文件已在 2026-08-07 与发布标签 `lf-6.12.20-2.0.0` 解引用到的 Git 提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 逐文件核对，其中 `tree.c`、`tree.h`、`tree_plugin.h`、`update.c` 和 `rcupdate.h` 的仓库副本 SHA-256 与原文件一致。2026-08-20 又通过 NXP 官方 GitHub contents API 重新比较 `tree.c`、`tree.h` 和 `rcu.h` 的 Git blob hash，三者均与该固定提交一致；2026-08-24 为补充 GP kthread 启动链，再次比较 `tree.c` 与 `include/linux/init.h` 的 Git blob hash，两份仓库副本也都与固定提交一致：

| 相对路径 | 主要用途 |
| --- | --- |
| `kernel/rcu/tree.c` | 普通 GP 请求、初始化、QS 汇聚、FQS、cleanup、同步等待入口 |
| `kernel/rcu/tree.h` | `rcu_node`、`rcu_data`、`rcu_state` 与 Tree RCU 内部接口 |
| `kernel/rcu/rcu.h` | RCU 内部序列辅助、共享声明与跨实现公共基础 |
| `kernel/rcu/tree_plugin.h` | PREEMPT_RCU / 非 PREEMPT_RCU 读侧、调度 QS、blocked task 与 boost |
| `kernel/rcu/update.c` | 通用等待 callback、RCU 初始化、RCU lockdep maps 与读侧状态查询 |
| `kernel/rcu/tree_exp.h` | expedited GP |
| `kernel/rcu/tree_nocb.h` | NOCB callback offload |
| `kernel/rcu/tree_stall.h` | stall 检测与诊断 |
| `kernel/rcu/rcu_segcblist.c`、`rcu_segcblist.h` | callback 分段列表实现 |
| `include/linux/rcupdate.h` | 公共读侧接口、发布/取得、`rcu_check_sparse()`、`RCU_LOCKDEP_WARN()`、`kfree_rcu()` |
| `include/linux/init.h` | `early_initcall()` 与 initcall 链接段登记规则，用于定位 GP kthread 创建时机 |
| `kernel/rcu/Kconfig.debug` | `PROVE_RCU`、RCU 列表 Lockdep 和其他 RCU 调试配置 |
| `include/linux/rculist.h` | list/hlist 的 RCU 访问封装 |
| `include/linux/rcu_segcblist.h` | callback 分段列表结构和接口 |
| `include/linux/srcu.h`、`srcutree.h`、`kernel/rcu/srcutree.c` | Tree SRCU 公共接口、状态和实现 |

调度入口 `kernel/sched/core.c`、任务字段 `include/linux/sched.h`、`kernel/rcu/tasks.h`、`kernel/rcu/tiny.c`、BPF/ftrace 调用方以及 6.12 context tracking 文件当前直接从只读原始源码树核对，未为单个调用点复制整个大文件。GP kthread 启动链使用同一不可变提交的官方 [`init/main.c`](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/init/main.c) 核对 `start_kernel()`、`rest_init()`、`kernel_init()`、`do_pre_smp_initcalls()` 与 `smp_init()` 的顺序；该大文件也不为这一个调用点复制。版本化阅读记录见：

- [RCU 总阅读索引](../rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.2_第一步必须先判断正在读哪一种RCU)
- [Tree RCU GP 全局生命周期模块源码概念导读](../rcu/navigation/P06_Linux_6.12_Tree_RCU_GP全局生命周期模块源码概念导读.md#6.1_模块问题与版本边界)
- [非抢占式 Tree RCU 模块源码概念导读](../rcu/navigation/P02_Linux_6.12_非抢占式_Tree_RCU_模块源码概念导读.md#2.1_证据目标和配置边界)
- [抢占式 Tree RCU 模块源码概念导读](../rcu/navigation/P03_Linux_6.12_抢占式_Tree_RCU_模块源码概念导读.md#3.1_取证问题)
- [Tasks RCU 与 Tiny RCU 模块源码概念导读](../rcu/navigation/P04_Linux_6.12_Tasks_RCU与Tiny_RCU模块源码概念导读.md#4.1_Linux_6.12_Tasks_RCU与_Tiny_RCU模块源码概念导读)
- [RCU Lockdep适配模块源码概念导读](../rcu/navigation/P05_Linux_6.12_RCU_Lockdep适配模块源码概念导读.md#5.1_模块问题与实现所有权)
- [Tree SRCU 模块源码概念导读](../rcu/navigation/P07_Linux_6.12_Tree_SRCU模块源码概念导读.md#7.1_先分清Tree_RCU与Tree_SRCU)
- [Tree RCU 拓扑与 CPU 热插拔模块源码概念导读](../rcu/navigation/P08_Linux_6.12_Tree_RCU_拓扑与CPU热插拔模块源码概念导读.md#8.1_本模块究竟解决什么问题)
- [Tree RCU force-QS 与 Stall 模块源码概念导读](../rcu/navigation/P09_Linux_6.12_Tree_RCU_force_QS与Stall模块源码概念导读.md#9.1_为什么GP已经在等还要有force_QS)
- [Tree RCU Expedited GP 模块源码概念导读](../rcu/navigation/P10_Linux_6.12_Tree_RCU_Expedited_GP模块源码概念导读.md#10.1_Expedited不是普通GP的加速档)
- [Tree RCU 回调与 NOCB 模块源码概念导读](../rcu/navigation/P11_Linux_6.12_Tree_RCU_回调与NOCB模块源码概念导读.md#11.1_GP完成为什么还不等于callback执行)
- [Tree RCU 同步等待与 rcu_barrier 模块源码概念导读](../rcu/navigation/P12_Linux_6.12_Tree_RCU_同步等待与rcu_barrier模块源码概念导读.md#12.1_等RCU至少有三种不同对象)
- [RCU 公共接口与检查机制源码详解](../rcu/source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.1_源码详解边界与引用入口)
- [非抢占式 Tree RCU 关键函数源码实现](../rcu/source_explanations/P02_Linux_6.12_非抢占式_Tree_RCU_关键函数源码实现.md#2.1_实现讲解边界与入口)
- [抢占式 Tree RCU 关键函数源码实现](../rcu/source_explanations/P03_Linux_6.12_抢占式_Tree_RCU_关键函数源码实现.md#3.1_实现讲解边界与入口)
- [RCU Lockdep适配层源码实现](../rcu/source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.1_实现所有权与读者目标)
- [Tree RCU GP 全局生命周期源码实现](../rcu/source_explanations/P05_Linux_6.12_Tree_RCU_GP全局生命周期源码实现.md#5.5.1_先从内核启动链定位early_initcall)
- [Tree RCU 拓扑与 CPU 热插拔源码实现](../rcu/source_explanations/P06_Linux_6.12_Tree_RCU_拓扑与CPU热插拔源码实现.md#6.2_源码符号覆盖账本)
- [Tree RCU force-QS 与 Stall 源码实现](../rcu/source_explanations/P07_Linux_6.12_Tree_RCU_force_QS与Stall源码实现.md#7.2_源码符号覆盖账本)
- [Tree RCU Expedited GP 源码实现](../rcu/source_explanations/P08_Linux_6.12_Tree_RCU_Expedited_GP源码实现.md#8.2_源码符号覆盖账本)
- [Tree RCU 回调与 NOCB 源码实现](../rcu/source_explanations/P09_Linux_6.12_Tree_RCU_回调与NOCB源码实现.md#9.2_源码符号覆盖账本)
- [Tree RCU 同步等待与 rcu_barrier 源码实现](../rcu/source_explanations/P10_Linux_6.12_Tree_RCU_同步等待与rcu_barrier源码实现.md#10.2_源码符号覆盖账本)
- [Tree SRCU 源码实现](../rcu/source_explanations/P11_Linux_6.12_Tree_SRCU源码实现.md#11.2_源码符号覆盖账本)

### 1.5.2\_Lockdep证据

下列 Lockdep 核心文件已在 2026-08-12 与发布标签 `lf-6.12.20-2.0.0` 解引用到的 Git 提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 逐文件核对，仓库副本 Git blob hash 与原文件一致：

| 相对路径 | 主要用途 |
| --- | --- |
| `include/linux/lockdep_types.h` | `lock_class_key`、`lock_class`、`lockdep_map` 与 `held_lock` |
| `include/linux/lockdep.h` | map 初始化、acquire/release、查询、断言、pin 与关闭配置分支 |
| `include/linux/sched.h` | current 的链键、持锁深度、递归状态和 `held_locks[]` |
| `kernel/locking/lockdep.c` | 锁类登记、取得释放状态机、链缓存、依赖图、IRQ 规则与查询 |
| `kernel/locking/lockdep_internals.h` | 图、链、容量和内部辅助定义 |
| `kernel/locking/lockdep_proc.c` | `/proc/lockdep`、`/proc/lockdep_chains`、`/proc/lockdep_stats` 与 lockstat |
| `lib/Kconfig.debug` | `PROVE_LOCKING`、`DEBUG_LOCK_ALLOC`、`LOCKDEP` 与容量配置 |
| `kernel/rcu/Kconfig.debug` | `PROVE_RCU` 与 `PROVE_LOCKING` 的选择关系 |
| `Documentation/locking/lockdep-design.rst` | 锁类、IRQ 状态、依赖规则、注解、闭包、成本与故障排查设计 |

当前基线没有记录目标板 `.config` 启用 `CONFIG_PROVE_LOCKING` 或 `CONFIG_LOCKDEP` 的证据，因此 Lockdep 专题只核对源码可选分支，不宣称当前板级内核已经运行该检查器。版本化阅读记录见：

- [Lockdep 总阅读索引](../lockdep/navigation/P01_Linux_6.12_Lockdep源码导读.md#1.1_基线与阅读目标)
- [Lockdep 身份与事件接入模块导读](../lockdep/navigation/P02_Linux_6.12_Lockdep身份与事件接入模块导读.md#2.1_模块问题)
- [Lockdep 依赖图与规则引擎模块导读](../lockdep/navigation/P03_Linux_6.12_Lockdep依赖图与规则引擎模块导读.md#3.1_模块问题)
- [Lockdep 查询适配与诊断模块导读](../lockdep/navigation/P04_Linux_6.12_Lockdep查询适配与诊断模块导读.md#4.1_模块问题)
- [Lockdep 身份与锁类源码实现](../lockdep/source_explanations/P01_Linux_6.12_Lockdep身份与锁类源码实现.md#1.1_关联入口)
- [Lockdep 取得释放与持锁账本源码实现](../lockdep/source_explanations/P02_Linux_6.12_Lockdep取得释放与持锁账本源码实现.md#2.1_关联入口)
- [Lockdep 依赖图与规则引擎源码实现](../lockdep/source_explanations/P03_Linux_6.12_Lockdep依赖图与规则引擎源码实现.md#3.1_关联入口)
- [Lockdep 查询注解与配置源码实现](../lockdep/source_explanations/P04_Linux_6.12_Lockdep查询注解与配置源码实现.md#4.1_关联入口)

### 1.5.3\_锁\_序列计数器\_等待与工作队列证据

2026-08-24 使用同一官方工作树中的发布标签对象，直接读取固定提交并核对下列架构无关文件。它们没有为了单个调用点全部复制进本目录；版本身份由不可变提交和上游相对路径共同定位，具体裁剪实现保存在各专题的 `source_explanations/`：

| 机制 | 已核对上游相对路径 | 主要证据 |
| --- | --- | --- |
| spinlock | `include/linux/spinlock_types.h`、`include/linux/spinlock.h`、`kernel/locking/spinlock.c`、`kernel/locking/spinlock_rt.c` | 普通/RT 类型映射、raw 包装、IRQ/抢占与架构边界 |
| mutex/rwsem | `include/linux/mutex.h`、`include/linux/mutex_types.h`、`include/linux/rwsem.h`、`kernel/locking/mutex.c`、`kernel/locking/rwsem.c` | owner/count、waiter、乐观自旋、handoff 与读者批量唤醒 |
| seqcount/seqlock | `include/linux/seqlock.h`、`Documentation/locking/seqlock.rst` | 普通读写、关联锁、PREEMPT_RT 补偿、latch 与 seqlock 包装 |
| waitqueue/completion | `include/linux/wait.h`、`include/linux/swait.h`、`include/linux/completion.h`、`kernel/sched/wait.c`、`kernel/sched/swait.c`、`kernel/sched/completion.c` | 入队/设态、wake 扫描、exclusive waiter、done 令牌与 swait |
| workqueue | `include/linux/workqueue.h`、`kernel/workqueue.c`、`kernel/workqueue_internal.h`、`Documentation/core-api/workqueue.rst` | work/pwq/pool/wq/worker、queue、active、worker、flush、cancel、rescuer 与 hotplug |

版本化阅读记录见：

- [Linux 6.12 锁源码总阅读索引](../locking/navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.1_版本边界与阅读任务)
- [Linux 6.12 序列计数器源码总阅读索引](../sequence_counters/navigation/P01_Linux_6.12_序列计数器源码总阅读索引.md#1.1_版本边界与阅读任务)
- [Linux 6.12 等待与完成量源码总阅读索引](../waiting_notification/navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.1_版本边界与阅读任务)
- [Linux 6.12 工作队列源码总阅读索引](../workqueue/navigation/P01_Linux_6.12_工作队列源码总阅读索引.md#1.1_版本边界与阅读任务)

这些专题的 PREEMPT_RT、KCSAN、Lockdep、watchdog 与 WQ 属性分支不都由当前配置启用。源码存在只证明该固定提交提供相应实现；部署结论仍需匹配目标配置和实际路径执行。

## 1.6\_Input\_子系统证据

| 相对路径 | 主要用途 |
| --- | --- |
| `drivers/input/input.c` | 设备与 handler 注册、事件过滤和分发、能力及 devres 生命周期 |
| `drivers/input/evdev.c` | evdev 客户端缓冲、read/poll/ioctl 与 `SYN_DROPPED` |
| `drivers/input/input-mt.c` | MT slot、tracking ID 辅助和帧同步 |
| `include/linux/input.h`、`include/linux/input/mt.h` | Input 内核对象与 MT 接口 |
| `include/uapi/linux/input.h`、`include/uapi/linux/input-event-codes.h` | evdev ABI 与标准事件编号 |
| `Documentation/input/input-programming.rst` | Input 驱动编程说明 |
| `Documentation/input/multi-touch-protocol.rst` | 多点触控 Protocol A/B 契约 |

专题导读见 [`drivers/input/README.md`](drivers/input/README.md)。

## 1.7\_内存顺序证据

下列文件已在 2026-08-07 与发布标签 `lf-6.12.20-2.0.0` 解引用到的 Git 提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 逐文件核对，仓库副本 SHA-256 与原文件一致：

| 相对路径 | 主要用途 |
| --- | --- |
| `include/asm-generic/rwonce.h` | `READ_ONCE()` / `WRITE_ONCE()` 的访问大小约束与公共实现 |
| `include/linux/compiler.h` | `barrier()` 等编译器约束 |
| `include/linux/compiler_types.h` | 编译器属性、类型与编译期检查基础 |
| `include/asm-generic/barrier.h` | SMP 屏障、release/acquire 公共封装与通用回退 |
| `include/linux/rcupdate.h` | RCU 指针发布、取得及读侧公开契约 |
| `arch/arm/include/asm/barrier.h` | ARMv7 屏障、shareability 域与 DMA/普通内存具体映射 |
| `Documentation/memory-barriers.txt` | Linux 屏障、依赖、锁、等待和 I/O 边界说明 |
| `Documentation/atomic_t.txt` | atomic RMW、顺序后缀与条件失败语义 |
| `tools/memory-model/README` | LKMM 工具需求、herd7/klitmus7 使用入口 |
| `tools/memory-model/linux-kernel.def` | C-like 原语到 herd 事件的语法映射 |
| `tools/memory-model/linux-kernel.bell` | 访问、屏障、锁和 RCU 事件分类 |
| `tools/memory-model/linux-kernel.cat` | LKMM 关系、公理和一致性判定 |
| `tools/memory-model/linux-kernel.cfg` | herd7 公共配置与模型文件装配 |
| `tools/memory-model/lock.cat` | 锁 acquisition/release 前端关系 |
| `tools/memory-model/Documentation/simple.txt` | 优先使用封装同步原语的工程路线 |
| `tools/memory-model/Documentation/litmus-tests.txt` | Litmus 语法、运行方法和模型限制 |

版本化导读见 [`../memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md`](../memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md)。

## 1.8\_编译器与\_Sparse\_注解证据

本专题复用 1.7 节已经完成哈希核对的 `include/linux/compiler_types.h`、`include/linux/compiler.h` 与 `include/linux/rcupdate.h`，不复制第二份源码。职责入口为：

- [Linux 6.12 编译器与 Sparse 注解源码导读](../compiler_annotations/navigation/P01_Linux_6.12_编译器与Sparse注解源码导读.md#1.1_基线与阅读任务)：组织 `BTF_TYPE_TAG()`、`__CHECKER__`、地址空间、context、逃生口和普通编译退化的阅读顺序；
- [Linux 6.12 compiler types 注解模块概念导读](../compiler_annotations/navigation/P02_Linux_6.12_compiler_types注解模块概念导读.md#2.1_模块问题与实现所有权)：解释参与者、两组正交状态、处理周期和代表性调用链；
- [Linux 6.12 compiler types 注解宏源码实现](../compiler_annotations/source_explanations/P01_Linux_6.12_compiler_types注解宏源码实现.md#1.1_关联入口与实现边界)：唯一展开 `compiler_types.h` 开头的具体宏体、配置分支、调用点与修改边界；
- [Sparse 地址空间与上下文记账研究型实验](../../../labs/foundations/c_language/P01_Sparse地址空间与上下文记账/README.md#1.1_实验目标)：先在独立文件中完成单变量地址域与 context 诊断，再用只构建不加载的外部模块核对 `C=1/C=2`、`M=` 和 `CF` 接入。

本源码基线确认的是 Linux 6.12.20 宏组织和仓库保存文件身份，不确认当前构建主机已经安装 Sparse，也不确认目标内核生成了带 type tag 的 BTF。后两项必须用实际工具版本、构建配置与产物转储单独验证。
