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
| 2026-09-02 建立、2026-09-05 复核的 Tiny RCU 工作树 | `CONFIG_TINY_RCU=y`、`CONFIG_TINY_SRCU=y`、`CONFIG_PREEMPT_NONE=y`、`CONFIG_PROVE_LOCKING=y`、`CONFIG_PROVE_RCU=y`；`CONFIG_SMP=n` | 支撑普通 Tiny RCU 当前编译路径、Lockdep 接入和 early-test 条件判断；配置种子含尚未提交到 `lf-6.12.y` 的工作树差量，不外推到固定标签、Tree RCU 或 Tasks flavor |

2026-08-24 重新发现的候选工作树已核对官方远端与 `lf-6.12.y` 分支；其 `HEAD=7b60e547d2783f8fee61ff7d7be3e066825b9c3a`，以固定发布提交为 merge-base 并前进 3 个提交。工作树 `Makefile` 仍为 Linux 6.12.20。本文新增源码导读全部通过该工作树中的标签对象读取固定提交内容，不把分支头的后续变化混入实现讲解。

2026-09-06 对上述 Tiny 工作树的 `.config`、`include/config/auto.conf` 和 `include/generated/autoconf.h` 进一步核对：`CONFIG_PREEMPT_COUNT=y`、`CONFIG_DEBUG_LOCK_ALLOC=y`，但 `CONFIG_DEBUG_OBJECTS=n`、`CONFIG_DEBUG_OBJECTS_RCU_HEAD=n`、`CONFIG_RCU_TRACE=n`、`CONFIG_TASKS_RCU_GENERIC=n`。因此当前 `debug_rcu_head_queue()` 恒返 0，配套 `debug_rcu_head_unqueue()` 与 Tasks 初始化帮助器为空操作；Lockdep 接入与节点生命周期检查必须分别判断。涉及的 `kernel/rcu/rcu.h`、`kernel/rcu/tiny.c`、`include/linux/rcupdate.h`、`include/linux/rcutiny.h`、`include/linux/preempt.h`、`include/trace/events/rcu.h` 和 `lib/Kconfig.debug` 已确认与固定提交无差异；这些配置结论仍只描述本次开发构建，不是所有 Tiny 构建的固定属性。

同日核对 Tiny 队列的中断保护路径：`CONFIG_CPU_V7=y`、`CONFIG_CPU_32v7=y`、`CONFIG_TRACE_IRQFLAGS=y`，未启用 `CONFIG_CPU_V7M` 和 `CONFIG_DEBUG_IRQFLAGS`。`arch/arm/Makefile` 设置 `__LINUX_ARM_ARCH__=7`，对应保存 CPSR、屏蔽普通 IRQ 并按旧值恢复控制字段的 ARM 路径；该结论不外推到 Cortex-M 或其他架构。下表新增的三个头文件按固定提交保存并核对 Git blob，原工作树中的对应文件以及 `arch/arm/Makefile`、`arch/arm/include/uapi/asm/ptrace.h` 与固定提交无差异。

本基线标识的是 NXP `linux-imx` 仓库中的一份确定源码快照，不是某个用户名、目录名、共享地址或挂载点。以后补充或复核源码时，应先验证候选工作树的官方远端、分支、`HEAD`、`Makefile` 和相关 Kconfig，再引用上游相对路径；本地绝对路径不得写入已跟踪文档。

## 1.2\_保存规则

- 保持 Linux 上游相对路径，例如 `fs/char_dev.c` 保存为本目录的 `fs/char_dev.c`。
- C/H/RST 文件保持原文，不在源码文件内混入笔记；解释写入 Markdown 正文或独立源码导读。
- 只复制当前专题用于验证数据结构、调用链和状态机的文件，不无选择复制整棵源码树。
- 知识正文和源码导读中的文件级阅读入口优先链接本目录保存的相对路径；官方远端只承担来源身份和版本追溯，不再作为已保存文件的唯一阅读入口。
- 新增源码时同步更新本清单；若工作树的分支、提交或版本变化，必须记录新基线，不能让不同时期文件无标记混合。
- 稳定知识正文说明机制，版本源码负责提供具体函数、字段和目录位置证据。

## 1.3\_字符设备与\_VFS\_证据

有限缓冲区 I/O 的模块概念与具体函数分别从[字符设备源码阅读索引](../character_device/navigation/P01_Linux_6.12_字符设备源码阅读索引.md#1.2_由问题进入模块导读)进入。2026-09-22 重构读写契约时，已按上表官方固定提交重新核对 `fs/libfs.c`、`drivers/base/core.c`、`drivers/base/devtmpfs.c`，并补入同提交的 `include/linux/uaccess.h`；未混入工作树后续实验提交。

同日重构有限窗口和环形流模板，重新核对 `fs/char_dev.c`、`fs/open.c`、`fs/file.c`、`fs/read_write.c`、`include/linux/wait.h`，补入同提交的 `include/linux/device/class.h` 与 `include/linux/poll.h`。这些证据用于确认注册发布与引用、`stream_open`、共享位置串行化、类创建签名和等待登记接口。ARM 语法检查使用本地已生成配置头，不把该配置冒充官方发布配置；没有以语法检查替代完整模块构建或目标执行。

模块入口与设备号正文续接时，继续直接读取该固定 Git 对象中的 `kernel/module/main.c`，核对初始化失败不调用正常退出函数；读取 `drivers/char/mem.c` 与 `include/uapi/linux/major.h`，核对空设备示例的主号 1、次号 3 及打开分派。这三份文件本轮未复制全文，定位以固定提交加上游相对路径为准，不由本地分支头或目录名推断。devtmpfs 的请求链、完成通知和设备事件顺序复用并重新核对下表已有证据。

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
| `include/linux/device/class.h` | `class_create` 当前签名与 class 接口 |
| `include/linux/poll.h` | `poll_wait` 登记和就绪事件接口 |
| `include/linux/wait.h` | 条件等待宏、等待队列登记与唤醒接口 |
| `include/linux/uaccess.h` | 普通用户复制的未复制量、短复制尾部处理与包装边界 |
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

2026-08-30 对全仓 Linux 源码 URL 做离线入口审计后，已确认下列原有或新增副本的 Git blob 与固定提交一致。rbtree 与 `security/Kconfig.hardening` 的这些 blob 还与上游 Linux 6.12 发布提交 `adc218676eef25575469234709c2d87185ca223a` 逐文件一致。它们可以直接承担本仓库中的源码阅读链接，不再依赖 GitHub `blob` 或 raw 页面：

| 相对路径 | 主要用途 |
| --- | --- |
| `Documentation/core-api/rbtree.rst` | Linux rbtree 使用契约、复杂度和 cached/augmented 接口说明 |
| `include/linux/rbtree_types.h`、`include/linux/rbtree.h`、`include/linux/rbtree_augmented.h`、`lib/rbtree.c` | rbtree 结构、公共接口、增强回调和核心实现 |
| `include/linux/kref.h` | `kref` 公共接口与引用计数对象释放契约 |
| `security/Kconfig.hardening` | `STRUCTLEAK` 等编译期安全加固配置及其选择边界 |

### 1.5.1\_RCU家族证据

下列 RCU 核心文件已在 2026-08-07 与发布标签 `lf-6.12.20-2.0.0` 解引用到的 Git 提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 逐文件核对，其中 `tree.c`、`tree.h`、`tree_plugin.h`、`update.c` 和 `rcupdate.h` 的仓库副本 SHA-256 与原文件一致。2026-08-20 又通过 NXP 官方 GitHub contents API 重新比较 `tree.c`、`tree.h` 和 `rcu.h` 的 Git blob hash，三者均与该固定提交一致；2026-08-24 为补充 GP kthread 启动链，再次比较 `tree.c` 与 `include/linux/init.h` 的 Git blob hash，两份仓库副本也都与固定提交一致。2026-08-30 为追踪 `init/main.c::rcu_init()`，再次通过官方远端核对发布标签解引用提交，并比较仓库保存的 `init/main.c`、`tree.c`、`tree.h`、`tree_plugin.h`、`tree_nocb.h`、`tree_exp.h`、`update.c`、`rcu.h` 与 `rcupdate.h`，均与固定提交一致。2026-09-02 为闭合 Tiny RCU 当前实现，新增保存 `kernel/rcu/tiny.c` 与 `include/linux/rcutiny.h`；2026-09-05 再次确认两份副本的 SHA-256 同时与固定提交及当前分支头一致。未保存的 `kernel/rcu/tasks.h`、`kernel/softirq.c` 和 `include/linux/suspend.h` 只按同一固定提交只读核对：

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
| `kernel/rcu/tiny.c` | 普通 Tiny RCU 控制块、callback 入队、QS、softirq、同步、poll、barrier 与初始化 |
| `include/linux/rcupdate.h` | 公共读侧接口、发布/取得、`rcu_check_sparse()`、`RCU_LOCKDEP_WARN()`、`kfree_rcu()` |
| `include/linux/rcutiny.h` | Tiny 条件下的调度 QS、poll/expedited 包装与无须维护的 Tree 专用接口边界 |
| `include/linux/init.h` | `early_initcall()` 与 initcall 链接段登记规则，用于定位 GP kthread 创建时机 |
| `init/main.c` | `start_kernel()`、`rest_init()`、`kernel_init()` 与 initcall/SMP 启动顺序 |
| `kernel/rcu/Kconfig.debug` | `PROVE_RCU`、RCU 列表 Lockdep 和其他 RCU 调试配置 |
| [`include/linux/irqflags.h`](include/linux/irqflags.h) | `local_irq_save/restore` 与 raw 宏的完整定义、中断状态检查包装 |
| [`include/linux/typecheck.h`](include/linux/typecheck.h) | 核对 `flags` 类型而不读取其未初始化数值的编译期类型检查 |
| [`arch/arm/include/asm/irqflags.h`](arch/arm/include/asm/irqflags.h) | ARM 中断状态保存/恢复的原始架构分支、CPSR 与屏蔽指令；[Tiny 队列中的使用解释](../rcu/source_explanations/P13_Linux_6.12_Tiny_RCU源码实现.md#13.6.3_flags怎样保存和恢复中断状态) |
| `include/linux/rculist.h` | list/hlist 的 RCU 访问封装 |
| `include/linux/rcu_segcblist.h` | callback 分段列表结构和接口 |
| `include/linux/srcu.h`、`srcutree.h`、`kernel/rcu/srcutree.c` | Tree SRCU 公共接口、状态和实现 |

调度入口 `kernel/sched/core.c`、`kernel/rcu/tasks.h`、BPF/ftrace 调用方以及 6.12 context tracking 文件当前仍直接从只读原始源码树核对，未为单个调用点复制整个大文件。任务字段使用已经保存的 `include/linux/sched.h`；GP kthread 启动链使用已经保存并与同一不可变提交核对一致的 [`init/main.c`](init/main.c) 追踪 `start_kernel()`、`rest_init()`、`kernel_init()`、`do_pre_smp_initcalls()` 与 `smp_init()` 的顺序。版本化阅读记录见：

- [RCU 总阅读索引](../rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.2_先建立源码分类坐标)
- [RCU 公共接口与读侧模型模块源码概念导读](../rcu/navigation/P02_Linux_6.12_RCU公共接口与读侧模型模块源码概念导读.md#2.1_模块问题与配置边界)
- [Tree RCU GP 全局生命周期模块源码概念导读](../rcu/navigation/P03_Linux_6.12_Tree_RCU_GP全局生命周期模块源码概念导读.md#3.1_模块问题与版本边界)
- [Tree RCU 拓扑与 CPU 热插拔模块源码概念导读](../rcu/navigation/P04_Linux_6.12_Tree_RCU_拓扑与CPU热插拔模块源码概念导读.md#4.1_本模块究竟解决什么问题)
- [Tree RCU force-QS 与 Stall 模块源码概念导读](../rcu/navigation/P05_Linux_6.12_Tree_RCU_force_QS与Stall模块源码概念导读.md#5.1_为什么GP已经在等还要有force_QS)
- [Tree RCU Expedited GP 模块源码概念导读](../rcu/navigation/P06_Linux_6.12_Tree_RCU_Expedited_GP模块源码概念导读.md#6.1_Expedited不是普通GP的加速档)
- [Tree RCU 回调与 NOCB 模块源码概念导读](../rcu/navigation/P07_Linux_6.12_Tree_RCU_回调与NOCB模块源码概念导读.md#7.1_GP完成为什么还不等于callback执行)
- [Tree RCU 同步等待与 rcu_barrier 模块源码概念导读](../rcu/navigation/P08_Linux_6.12_Tree_RCU_同步等待与rcu_barrier模块源码概念导读.md#8.1_等RCU至少有三种不同对象)
- [Tree SRCU 模块源码概念导读](../rcu/navigation/P09_Linux_6.12_Tree_SRCU模块源码概念导读.md#9.1_先分清Tree_RCU与Tree_SRCU)
- [Tasks RCU 模块源码概念导读](../rcu/navigation/P10_Linux_6.12_Tasks_RCU模块源码概念导读.md#10.1_模块问题与三个flavor)
- [Tiny RCU 模块源码概念导读](../rcu/navigation/P11_Linux_6.12_Tiny_RCU模块源码概念导读.md#11.1_模块问题与当前配置前提)
- [RCU Lockdep适配模块源码概念导读](../rcu/navigation/P12_Linux_6.12_RCU_Lockdep适配模块源码概念导读.md#12.1_模块问题与实现所有权)
- [RCU 公共接口与检查机制源码详解](../rcu/source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.1_源码详解边界与引用入口)
- [Tree RCU 等待桥、QS 与节点汇聚关键函数源码实现](../rcu/source_explanations/P02_Linux_6.12_Tree_RCU_等待桥_QS与节点汇聚关键函数源码实现.md#2.1_实现讲解边界与入口)
- [Tree RCU 抢占读者债务关键函数源码实现](../rcu/source_explanations/P03_Linux_6.12_Tree_RCU_抢占读者债务关键函数源码实现.md#3.1_实现讲解边界与入口)
- [RCU Lockdep适配层源码实现](../rcu/source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.1_实现所有权与读者目标)
- [Tree RCU GP 全局生命周期源码实现](../rcu/source_explanations/P05_Linux_6.12_Tree_RCU_GP全局生命周期源码实现.md#5.5.1_先从内核启动链定位early_initcall)
- [Tree RCU 拓扑与 CPU 热插拔源码实现](../rcu/source_explanations/P06_Linux_6.12_Tree_RCU_拓扑与CPU热插拔源码实现.md#6.2_源码符号覆盖账本)
- [Tree RCU force-QS 与 Stall 源码实现](../rcu/source_explanations/P07_Linux_6.12_Tree_RCU_force_QS与Stall源码实现.md#7.2_源码符号覆盖账本)
- [Tree RCU Expedited GP 源码实现](../rcu/source_explanations/P08_Linux_6.12_Tree_RCU_Expedited_GP源码实现.md#8.2_源码符号覆盖账本)
- [Tree RCU 回调与 NOCB 源码实现](../rcu/source_explanations/P09_Linux_6.12_Tree_RCU_回调与NOCB源码实现.md#9.2_源码符号覆盖账本)
- [Tree RCU 同步等待与 rcu_barrier 源码实现](../rcu/source_explanations/P10_Linux_6.12_Tree_RCU_同步等待与rcu_barrier源码实现.md#10.2_源码符号覆盖账本)
- [Tree SRCU 源码实现](../rcu/source_explanations/P11_Linux_6.12_Tree_SRCU源码实现.md#11.2_源码符号覆盖账本)
- [Tree RCU `rcu_init()` 启动初始化源码实现](../rcu/source_explanations/P12_Linux_6.12_Tree_RCU_rcu_init启动初始化源码实现.md#12.19_直接符号覆盖账本)
- [Tiny RCU 源码实现](../rcu/source_explanations/P13_Linux_6.12_Tiny_RCU源码实现.md#13.2_源码符号覆盖账本)

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
- [Lockdep 身份与锁类源码实现](../lockdep/source_explanations/kernel/locking/lockdep.c.md#1.1_关联入口)
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

2026-08-30 又将上述索引里原先直接指向 GitHub 的文件级入口保存到本目录，并逐文件核对 Git blob。新增副本包括 `include/linux/spinlock_types.h`、`include/linux/mutex_types.h`、`kernel/locking/mutex.c`、`include/linux/rwsem.h`、`kernel/locking/rwsem.c`、`include/linux/seqlock.h`、`include/linux/wait.h`、`kernel/sched/wait.c`、`include/linux/swait.h`、`include/linux/completion.h`、`kernel/sched/completion.c`、`include/linux/workqueue.h`、`kernel/workqueue_internal.h`、`kernel/workqueue.c` 与 `Documentation/core-api/workqueue.rst`；原有 `include/linux/spinlock.h` 也重新核对一致。没有保存到本目录的其他路径仍只是版本定位，不得伪装成离线链接。

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

版本化导读见 [`../memory_ordering/navigation/P01_Linux_6.12_LKMM_源码与模型导读.md`](../memory_ordering/navigation/P01_Linux_6.12_LKMM_源码与模型导读.md)。

## 1.8\_编译器与\_Sparse\_注解证据

本专题复用 1.7 节已经完成哈希核对的 `include/linux/compiler_types.h`、`include/linux/compiler.h` 与 `include/linux/rcupdate.h`，并使用已经与固定提交核对一致的 `security/Kconfig.hardening` 说明 `STRUCTLEAK` 配置边界，不复制第二份源码。职责入口为：

- [Linux 6.12 编译器与 Sparse 注解源码导读](../compiler_annotations/navigation/P01_Linux_6.12_编译器与Sparse注解源码导读.md#1.1_基线与阅读任务)：组织 `BTF_TYPE_TAG()`、`__CHECKER__`、地址空间、context、逃生口和普通编译退化的阅读顺序；
- [Linux 6.12 compiler types 注解模块概念导读](../compiler_annotations/navigation/P02_Linux_6.12_compiler_types注解模块概念导读.md#2.1_模块问题与实现所有权)：解释参与者、两组正交状态、处理周期和代表性调用链；
- [Linux 6.12 compiler types 注解宏源码实现](../compiler_annotations/source_explanations/P01_Linux_6.12_compiler_types注解宏源码实现.md#1.1_关联入口与实现边界)：唯一展开 `compiler_types.h` 开头的具体宏体、配置分支、调用点与修改边界；
- [Sparse 地址空间与上下文记账研究型实验](../../../labs/foundations/c_language/P01_Sparse地址空间与上下文记账/README.md#1.1_实验目标)：先在独立文件中完成单变量地址域与 context 诊断，再用只构建不加载的外部模块核对 `C=1/C=2`、`M=` 和 `CF` 接入。

本源码基线确认的是 Linux 6.12.20 宏组织和仓库保存文件身份，不确认当前构建主机已经安装 Sparse，也不确认目标内核生成了带 type tag 的 BTF。后两项必须用实际工具版本、构建配置与产物转储单独验证。

## 1.9\_上游旧版本对照证据

当前目录根部仍只表示 NXP Linux 6.12.20 固定提交。确实需要比较旧实现时，旧版文件进入 `upstream_versions/<version>/`，版本目录之后继续保持 Linux 上游相对路径，不能拿根部 6.12.20 文件冒充旧版源码。

2026-08-30 从同一只读网盘 Git 对象库提取上游 Linux 6.1 发布提交 `830b3c68c1fb1e9176028d02ef86f3cf76aa2476` 的 rbtree 对照文件，并逐文件核对 Git blob：

| 保存路径 | 主要用途 |
| --- | --- |
| `upstream_versions/v6.1/Documentation/core-api/rbtree.rst` | Linux 6.1 rbtree 使用文档 |
| `upstream_versions/v6.1/include/linux/rbtree.h` | Linux 6.1 公共接口与内联辅助 |
| `upstream_versions/v6.1/include/linux/rbtree_augmented.h` | Linux 6.1 增强树接口与内部修复逻辑 |
| `upstream_versions/v6.1/lib/rbtree.c` | Linux 6.1 rbtree 核心实现 |

这些文件只服务于明确标注为 Linux 6.1 的历史比较。稳定正文讨论当前仓库基线时仍链接本目录根部的 6.12.20 文件；其他旧版本若没有保存精确副本，就不得把网络地址机械替换为当前版本。

## 1.10\_错误指针与资源失败证据

2026-09-22 重新核对官方来源、固定标签、版本和相关配置后，按同一固定提交补入下列源码。总入口为[错误指针源码阅读索引](../error_pointer/navigation/P01_Linux_6.12_错误指针源码阅读索引.md#1.2_按问题进入源码)，模块导读区分返回值与设备资源记录，具体编码函数只在 `source_explanations` 展开。

| 保存的上游相对路径 | 证据用途 |
| --- | --- |
| [include/linux/err.h](include/linux/err.h) | 编码、检测、空值组合、跨类型传递与每 CPU 注解适配 |
| [drivers/base/dd.c](drivers/base/dd.c) | probe 失败内部变号、延迟状态识别和明确清理调用 |
| [drivers/base/devres.c](drivers/base/devres.c) | NULL 分配契约、成功登记、锁内摘取与锁外逆序回调 |
| [drivers/gpio/gpiolib-devres.c](drivers/gpio/gpiolib-devres.c) | GPIO 申请、包装登记失败回退及可选获取 |
| [rust/kernel/error.rs](rust/kernel/error.rs) | 负错误范围不变量与 C 错误指针到类型化结果的桥接 |

五份新增副本，以及既有 `include/linux/device.h`、`drivers/base/core.c`，均与固定 Git 对象在换行规范化后核对一致。时钟、稳压器、pinctrl、I2C、errno 和日志格式的辅助文件只读核对，具体路径与检查点见[模块导读](../error_pointer/navigation/P02_返回值与清理路径导读.md#2.4_其他表示与观测入口)，不把未保存文件写成已建立全文副本。

本次工作树启用 ARM、MODULES、GPIOLIB、COMMON_CLK、REGULATOR、PINCTRL、I2C，未启用 KASAN；这是本地配置而非官方发布配置。实验模块仅用平台设备和 devres 观察软件失败，不访问真实外设。ARM 静态语法检查不能替代 Kbuild、MODPOST、装载与真实释放观察，也不验证其他架构页表布局。

## 1.11\_对象属性与misc入口证据

2026-09-22 按同一官方固定提交核对驱动框架与两个最小入口。版本入口为[驱动入口源码阅读索引](../driver_entries/navigation/P01_Linux_6.12_驱动入口源码阅读索引.md#1.2_从现象进入文件)，模块导读追踪属性活动与字符分派，函数体唯一展开在该专题的 source_explanations 中。

| 保存的上游相对路径 | 本批用途 |
| --- | --- |
| [lib/kobject.c](lib/kobject.c)、[include/linux/kobject.h](include/linux/kobject.h) | 既有原文核对；动态创建引用、类型回收、属性适配与延迟释放配置 |
| [fs/sysfs/file.c](fs/sysfs/file.c) | 新增原文；属性读回调、移除包装与文本输出 |
| [fs/kernfs/dir.c](fs/kernfs/dir.c) | 新增原文；节点去激活、活动计数和等待排空 |
| [drivers/char/misc.c](drivers/char/misc.c)、[include/linux/miscdevice.h](include/linux/miscdevice.h) | 新增原文；共享号码、登记失败、操作表交接与注销边界 |
| [Documentation/filesystems/sysfs.rst](Documentation/filesystems/sysfs.rst) | 新增原文；属性接口契约及 show 的格式化要求 |

上述七份原文与固定 Git 对象逐一核对一致。另核对既有 fs/libfs.c 中的有限读取辅助；drivers/base/platform.c、bus.c、base.h，include/linux/fs.h、sysfs.h 及 include/uapi/linux/major.h 仅只读核对，不称为新增全文副本。drivers/base/dd.c 的绑定集合与探测关联沿已有证据使用。

本地配置启用 ARM、MODULES、SYSFS、DEVTMPFS，仍不等于官方发布配置。两个教学模块分别通过 ARMv7 静态语法检查，依赖的已跟踪头文件与固定提交无差异；生成头和配置来自本地构建。没有执行 Kbuild、MODPOST、模块链接、实际装卸或 sysfs/misc 目标观察，没有写入外部内核树。

## 1.12\_文件操作与映射引用证据

2026-09-22 继续采用官方固定提交，不采用当前工作树的三笔实验提交。阅读从[字符设备总索引](../character_device/navigation/P01_Linux_6.12_字符设备源码阅读索引.md#1.1_版本和阅读边界)进入，再按[文件操作导读](../character_device/navigation/P03_文件操作与打开寿命导读.md)追踪文件引用、同步清理、请求位置和映射保活。

| 上游相对路径 | 本批处理和用途 |
| --- | --- |
| [include/linux/fs.h](include/linux/fs.h)、[fs/open.c](fs/open.c)、[fs/read_write.c](fs/read_write.c)、[fs/file_table.c](fs/file_table.c) | 既有副本与固定对象一致；成员、close/filp_close 分支、读分派、最终引用处理 |
| [include/linux/uio.h](include/linux/uio.h)、[fs/proc/fd.c](fs/proc/fd.c)、[fs/readdir.c](fs/readdir.c) | 新增原文；迭代器后端、fdinfo 临时引用与目录位置交接 |
| [mm/mmap.c](mm/mmap.c) | 修正既有副本与固定提交的差异；映射保存文件引用 |
| [mm/vmalloc.c](mm/vmalloc.c)、[mm/vma.c](mm/vma.c) | 新增原文；用户映射专用页及 VMA 撤销后的引用归还 |
| [Documentation/driver-api/ioctl.rst](Documentation/driver-api/ioctl.rst) | 新增原文；命令编码、用户结构与兼容 ABI |

上述 11 份副本逐一重新提取并按规范化换行核对。旧 mmap.c 缺少 memfd seals 检查、无地址提示时的 THP 条件及映射合并的预分配分支；本批恢复为指定官方对象，未混入其他版本，也未修改已有 VFS 正文或外部源码。它的旧副本不能继续作为该提交的逐行证据。

锁、splice、范围重映射、io_uring、无 MMU、procfs/debugfs 的辅助位置只读核对，完整文件清单和定位项见模块导读，不称为本批新增副本。三个模块的 ARM 语法检查采用本地生成配置，相关已跟踪头未被本地差量改变；不等于官方发布构建或设备运行。目标 Kbuild、MODPOST、用户程序 Linux 编译、模块装卸、真实 readv/pread 和映射阻止卸载实验尚未执行。

## 1.13\_分类对象与属性事务证据

2026-09-22 按同一官方固定提交重构 class 长文。新建[分类对象与属性事务导读](../driver_entries/navigation/P03_分类对象与属性事务导读.md)，实现仅在[core.c 设备创建与属性分派](../driver_entries/source_explanations/drivers/base/core.c.md)逐句展开。当前 HEAD 的三笔本地实验提交未参与证据。

| 保存的上游相对路径 | 处理与用途 |
| --- | --- |
| [drivers/base/class.c](drivers/base/class.c)、[drivers/base/base.h](drivers/base/base.h) | 新增原文，公开 class 策略与内部集合分离 |
| [drivers/base/core.c](drivers/base/core.c)、[drivers/base/devtmpfs.c](drivers/base/devtmpfs.c) | 既有副本复核一致，设备便利创建、属性分派、节点请求与事件先后 |
| [include/linux/device/class.h](include/linux/device/class.h)、[fs/sysfs/file.c](fs/sysfs/file.c) | 既有副本复核一致，当前成员及属性适配 |
| [fs/kernfs/file.c](fs/kernfs/file.c)、[lib/kstrtox.c](lib/kstrtox.c) | 新增原文，每次打开的锁、活动引用、输入复制与整数转换 |

八份源码逐一与固定对象核对一致；三段完整函数体剥离仓库注释及空白后与原实现一致。设备树、PM、cgroup、configfs、bpffs、网络 class 和配置等辅助证据的只读范围写在导读，不称为本批保存的全文。

note_class 与 note_control 采用本地 ARMv7 生成头和配置完成语法检查，实际使用的已跟踪头无相对固定提交的差量。宿主替身检查初始化回滚、属性输入及字符读取分支，不证明内核真实锁、活动计数或引用实现。未执行目标 Kbuild、MODPOST、模块装卸、节点策略、并发撤销或性能测量，未修改外部源码树。

## 1.14\_链表拓扑与一次性初始化证据

2026-09-22 继续使用 NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0（Linux 6.12.20），不采用本地三个实验提交。阅读从[链表源码索引](../linked_list/navigation/P01_Linux_6.12_链表源码阅读索引.md#1.1_版本和阅读任务)进入，模块导读与 [list.h 唯一实现讲解](../linked_list/source_explanations/include/linux/list.h.md)各自承担定位和逐句说明。

| 保存的上游相对路径 | 用途与处理 |
| --- | --- |
| [include/linux/list.h](include/linux/list.h)、[include/linux/types.h](include/linux/types.h) | 既有副本复核一致，节点、初始化、增删、游标与拼接 |
| [include/asm-generic/rwonce.h](include/asm-generic/rwonce.h) | 既有副本复核一致，单次访问与尺寸限制 |
| [include/linux/poison.h](include/linux/poison.h)、[lib/list_debug.c](lib/list_debug.c) | 新增原文，毒化值及局部拓扑检查 |
| [include/linux/once.h](include/linux/once.h)、[lib/once.c](lib/once.c)、[include/linux/once_lite.h](include/linux/once_lite.h) | 新增原文，公共锁、每展开点完成状态、静态分支优化和不同 once 契约 |
| [include/linux/llist.h](include/linux/llist.h) | 新增原文，特定生产消费组合与架构限制，不与普通 list_head 混用 |

九份副本按规范化换行与固定对象核对；十一段完整函数和一个安全遍历宏与原语句一致。辅助只读位置包括 include/linux/wait.h、include/linux/skbuff.h、mm/slab.h、drivers/base/base.h、include/linux/klist.h、lib/Kconfig.debug、arch/arm/include/asm/barrier.h 和 Documentation/core-api/wrappers/memory-barriers.rst；这些位置用于核对封装、配置和访问边界，不都属于本批新增全文。

当前 ARM、PREEMPT_NONE、TINY_RCU、非 SMP 配置启用 PROVE_LOCKING，未观察到 DEBUG_LIST/LIST_HARDENED/KASAN 启用；该工作配置不是官方发布配置。note_list 的 ARMv7 语法检查使用本地生成头，实际依赖的 357 份头中，已跟踪文件没有使用相对固定提交的差量。宿主替身只验证业务分支、内存配对和普通串行拓扑，不证明真实锁、内存序或动态检查器。未执行目标 Kbuild、MODPOST、模块装卸、真实并发或性能测试。


## 1.15\_哈希计算与编号索引证据

2026-09-22 按相同 NXP 官方固定提交核对 Linux 6.12.20 的哈希计算，不采用本地三笔实验提交。[哈希源码索引](../hash_table/navigation/P01_Linux_6.12_哈希计算源码阅读索引.md#1.1_版本和任务边界)区分计算规则、桶内连接及对象寿命；[位宽导读](../hash_table/navigation/P02_键位宽与落桶导读.md)负责调用关系，五个完整函数仅在 [hash.h 实现讲解](../hash_table/source_explanations/include/linux/hash.h.md#1.1_32位乘法与取高位)展开。

| 保存的上游相对路径 | 用途与处理 |
| --- | --- |
| [include/linux/hash.h](include/linux/hash.h)、[include/linux/hashtable.h](include/linux/hashtable.h) | 既有原文复核；乘法、截断、接口类型选择和固定桶数组宏 |
| [kernel/pid.c](kernel/pid.c)、[include/linux/pid.h](include/linux/pid.h)、[include/linux/pid_namespace.h](include/linux/pid_namespace.h) | 新增原文；当前 find_pid_ns 使用 ns->idr，upid 不含旧稿的 pid_chain |

五份原文与固定对象核对。辅助只读 arch/arm/Kconfig 与顶层 Makefile；当前 ARM、非 64 位配置未启用 HAVE_ARCH_HASH，这是工作树配置边界，不是所有平台的统一实现。

C11 宿主程序覆盖冲突、重复键、摘除、重新分桶和固定宽度计算；将固定通用函数的 BITS_PER_LONG 分别设为 32 与 64 进行算法分支比较，每种配置核对 10,000 个输入和六种输出宽度。不把宿主算法分支模拟称为 ARM 执行、32 位 ABI 编译、内核并发或性能测试。本批未执行目标 Kbuild、模块装卸及体系结构指令检查，未修改外部内核树。

## 1.16\_hlist入口槽与RCU旧路径证据

2026-09-22 继续按官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0（Linux 6.12.20）核对节点连接与对象寿命。[节点导读](../hash_table/navigation/P03_节点连接与并发边界导读.md#3.1_节点与桶数组分别负责什么)组织单桶、固定数组和 RCU 旧路径；三个唯一实现文档分别对应 list.h、hashtable.h 和 rculist.h，通用 raw 取得仍由[RCU 公共接口](../rcu/source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.3.4_rcu_dereference_raw的无检查取得)展开。

list.h、hashtable.h、types.h、rculist.h、rcupdate.h 五份既有副本与固定对象一致。十四个完整函数和二十一个宏与固定摘录逐项核对。没有用本地实验提交或目录名代替版本证据，也没有修改外部内核树。

当前 ARM、TINY_RCU、PREEMPT_NONE、非 SMP 配置启用 PROVE_LOCKING，未启用 PROVE_RCU_LIST；这不代表其他配置或历史 Tree 快照的状态。note_hlist_rcu 通过 ARMv7 语法检查，实际依赖的 358 份头中已跟踪文件没有相对固定对象的差量。C 宿主程序和固定函数替身检查入口槽、后置状态、旧 next、回调登记、失败回滚和清理，不能证明真实 RCU 宽限期或内存序。未执行目标 Kbuild、MODPOST、装卸、SMP 或性能实验。

## 1.17\_动态表迁移与接口证据

2026-09-22 按同一 NXP 官方固定提交核对 rhashtable 的布局、查找和迁移，新增 [include/linux/rhashtable.h](include/linux/rhashtable.h)与 [lib/rhashtable.c](lib/rhashtable.c)原文；[include/linux/rhashtable-types.h](include/linux/rhashtable-types.h)既有副本复核一致。辅助只读 include/linux/list_nulls.h 的通用链尾编码，不使用本地实验提交。

[动态表模块导读](../hash_table/navigation/P04_动态表迁移与接口边界导读.md#4.2_沿R0到R5追踪一次迁移)按 R0～R5 串起后继发布、尾节点迁移、链尾身份重扫、入口切换和旧桶回收；31 个函数、7 个结构体和 3 个宏与固定摘录一致。源码中的桶锁位、nulls 结束标记和 future_tbl 分别解释，未沿用旧稿的 redirect tag 伪机制。

当前 ARM、TINY_RCU、PREEMPT_NONE、非 SMP 配置下，note_rhashtable 通过 ARMv7 语法检查；实际使用 363 份头，已跟踪头没有相对固定对象的差量。C 宿主模型只重放确定跨链，公开接口替身只验证业务资源配对和失败处理。未执行目标 Kbuild、装卸、实际伸缩、SMP、walker/嵌套分配全路径或性能测量；未修改外部内核树。

## 1.18\_子系统身份与引用证据

2026-09-22 按同一 NXP 官方固定提交核对名称缓存、连接跟踪和邻居索引。[子系统导读](../hash_table/navigation/P05_子系统索引身份与寿命导读.md#5.1_先区分索引任务与业务结论)负责源码入口，具体实现按上游位置分别展开，不使用本地三笔实验提交。

本批复核已有 [fs/dcache.c](fs/dcache.c)，新增 [include/linux/list_bl.h](include/linux/list_bl.h)、[net/netfilter/nf_conntrack_core.c](net/netfilter/nf_conntrack_core.c)、[include/net/netfilter/nf_conntrack_tuple.h](include/net/netfilter/nf_conntrack_tuple.h)、[include/net/netfilter/nf_conntrack.h](include/net/netfilter/nf_conntrack.h)、[include/net/neighbour.h](include/net/neighbour.h)、[net/core/neighbour.c](net/core/neighbour.c)。七份原文按固定对象核对；辅助读取 net/ipv4/arp.c，仅用于确认 ARP 表回调和阈值归属。十四个函数的裁剪体保持上游代码，中文说明另行标识。

完整身份 C 模型与固定桶模块业务替身只验证匹配条件、串行拓扑、失败分支和内存配对。note_hash_table 通过 ARMv7 语法检查，实际使用 359 份头，已跟踪头相对固定提交的差量交集为空。当前 ARM、TINY_RCU、PREEMPT_NONE、非 SMP 配置不是 VFS 或网络并发实测；未执行目标 Kbuild、MODPOST、装卸、网络流量、SMP 或性能验证。没有修改外部内核树。

## 1.19\_rbtree查找与旋转路径证据

按同一官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0 核对 Linux 6.12.20 的查询路径。[总索引](../rbtree/navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界)组织版本边界，[查找模块导读](../rbtree/navigation/P02_查找路径与返回边界导读.md#2.2_按一次查找定位源码)串起共享字段、局部游标和返回责任，四个查找函数与遍历宏在[rbtree.h 实现](../rbtree/source_explanations/include/linux/rbtree.h.md#1.1_rb_find的任意匹配)唯一展开。

既有 include/linux/rbtree_types.h、rbtree.h、rbtree_augmented.h、lib/rbtree.c 和 Documentation/core-api/rbtree.rst 五份 raw 文件与固定 Git blob 一致。本地 HEAD 仍含三笔实验差量，未用它替代发布对象。查找首次取根、孩子 RCU 取得、WRITE_ONCE 与不成环写序、父指针论证排除项均按固定文件核对，不修改外部源码。

C11 程序串行重放相等键旋转、旧入口漏查与错误写序成环，全部对象在观察期内存活。宿主检查不证明真实内核、RCU 宽限期、SMP 可见顺序或目标 ARM 执行；未执行目标 Kbuild、模块装卸、内存模型工具或性能实验。插入/删除实现仍按后续批次整理，当前入口不代表整套 rbtree 重构完成。

2026-09-23 重构 P08 入口时，再次只读核对工作树身份并通过固定对象读取 rbtree.h 与 rbtree.rst；rb_add、rb_find、rb_find_add 的比较函数参数不能被文档中传统手写模型的说明掩盖。业务比较由调用者提供，不等于头文件没有比较辅助接口；树根不保存比较策略。2007 年使用文档的场景列表按历史背景引用，不冒充当前所有子系统的数据结构。宿主 ordered_jobs.cpp 仅观察业务排序契约，不作为 Linux 实现或性能证据。

同日补核 P08 根值和游离边界：类型头的两个初始化宏只构造空根值，rbtree.h 的 RB_EMPTY_ROOT 使用 READ_ONCE 读根，RB_EMPTY_NODE/RB_CLEAR_NODE 读写约定标记。三宏在[rbtree.h 对应标题](../rbtree/source_explanations/include/linux/rbtree.h.md#1.8_游离标记不等于成员搜索)展开。root_initializers.c 直接包含保存的类型头，只运行宿主赋值与地址观察；另以 Clang armv7a-none-eabi 前端检查类型大小、对齐和块内初始化，不等于内核配置构建或 ARM 执行。rb_erase_augmented 的内联入口位于 rbtree_augmented.h，正文已纠正文件归属。

P08 字段单元继续按同一固定提交核对 rbtree_types.h、rbtree.h、rbtree_augmented.h。三个结构、两个空根初始化器、取父/业务还原与颜色宏分别进入[类型实现](../rbtree/source_explanations/include/linux/rbtree_types.h.md#1.1_rb_node的三个字段与对齐)及既有实现页的新增标题；[布局导读](../rbtree/navigation/P07_节点布局与编码状态导读.md#7.2_沿一个节点的成员周期读写字段)以 T0～T4 组织读写者。第二低位不被自行解释为通用成员状态，保色换父只保留最低颜色位。整数模型检查 32/64 位编码，不转换真实宿主指针；当前宿主编译器报告 pointer 八字节、long 四字节，不能冒充 ARM ABI。

P09 嵌入成员单元再次核对固定 `include/linux/container_of.h`，已有原文与固定对象一致。两个宏的唯一语句讲解进入[成员地址实现](../rbtree/source_explanations/include/linux/container_of.h.md#1.1_一次还原中的求值与类型检查)，由[所有者导读](../rbtree/navigation/P07_节点布局与编码状态导读.md#7.4_从嵌入成员回到所有者)连接。GNU C 宿主实验直接包含该宏，宿主依赖适配不属于上游证据；普通宏丢失 const、同类型错误成员不被静态检查发现，均与固定正文及实际编译结果对应。

P09 比较单元继续核对固定 rbtree.h、rbtree_types.h 与 rbtree.rst：根不保存比较器，但 rb_add 接收布尔 less，rb_find_add/rb_find 接收各自签名的三态 cmp。传统说明的手写建议不覆盖掉固定头文件已有的辅助接口；比较模型不宣称内核调用次数、缓存命中或目标性能已验证。

## 1.20\_rbtree插入与父槽证据

沿同一 NXP 固定提交继续核对 include/linux/rbtree.h、rbtree_augmented.h 与 lib/rbtree.c。[插入模块](../rbtree/navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)用 I0～I5 对应空槽、红叶、上推与旋转；[修复实现](../rbtree/source_explanations/lib/rbtree.c.md#1.3_插入修复的两侧分支)恢复实际分支、变量与 WRITE_ONCE，保留教材原中文推导。新增十五个函数与已有四个查询函数逐语句核对，共十九个函数；父槽助手只在一个实现标题展开，知识正文仍保留机制任务。

note_rbtree_insert 使用实际接口观察四种三键方向和叔红输入，当前 ARMv7 语法检查通过；实际使用 351 份头，其中已跟踪文件与固定提交的差量交集为空。生成头和配置来自当前 ARM 工作树，不称为官方发布配置。未执行目标 Kbuild、MODPOST、装卸或真实日志，未修改外部树。

C 算法检查仅在忽略缓存中把宿主打包字段及两处 unsigned long 转换适配为 uintptr_t，避免 Windows LLP64 截断地址；其他核心功能语句来自固定对象，WRITE_ONCE 在该模型中只是普通赋值。八对象全排列分别按互异键和成对相等键构建，检查 645120 次插入后的排序、父链、根色、红红禁令、等黑高、可达对象及至多两次旋转，并观察到 Case 2 回调中间态。这是有限串行算法验证，不是 Linux ABI、内存序、RCU 或性能验证。

## 1.21\_查询与插入教材入口分工

固定源码与上节验证范围不变。查询模型由[P10](../../../knowledge/linux/data_structures/红黑树_rb-tree/P10_Linux_6.12_内核_rbtree_查找与返回边界.md#10.2_rbtree_查找逻辑_手写_search_与内核辅助接口)负责，红叶接入与完整观察模块由[P26](../../../knowledge/linux/data_structures/红黑树_rb-tree/P26_Linux红叶接入与插入修复.md#26.3.15_在内核模块中观察五组插入)负责；源码模块和唯一实现入口相应直达各自章节。正文和程序完整迁移，本批没有增加目标运行、体系结构或并发证据。


## 1.22\_rbtree删除与缺黑状态证据

同一 NXP 固定提交的五份原文继续作为证据，不使用本地三笔实验提交。[删除导读](../rbtree/navigation/P04_对象摘除与缺黑修复导读.md#4.2_从对象到缺黑父槽)将 D0～D4 对应到结构摘除、原色判断、缺口父槽、四类修复和业务退出。唯一实现补齐 __rb_erase_augmented、____rb_erase_color、rb_erase、rb_set_black、__rb_erase_color，以及两个游离标记宏；旧中文注释和 ASCII 图保留，纠正重复键、根退出及中间颜色的说明。

C 检查使用固定核心语句，仅在忽略缓存中适配 Windows uintptr_t 打包并加分支计数，WRITE_ONCE 为普通赋值。六对象的全部 720 种插入次序与全部 720 种删除次序，分别按唯一键和成对重复键运行，共 1036800 棵树、6220800 次删除。逐步检查对象全集、身份/键不变、排序、父链、根色、红红禁令、等黑高、子树计数及至多三旋；四类两侧均命中，328320 次 Case 3 回调观察到未收尾父色。这是有限串行算法检查，不是 Linux ABI、内存模型或性能证据。

实际 note_rbtree_erase 材料通过 ARMv7 语法检查，使用 351 份头，已跟踪头与固定提交差量交集为空，生成头来自当前配置。另以明确的宿主接口适配直接包含材料，执行两组取消及空输入、重复键、错误下标和重复删除保护；这不是实际内核执行。目标 Kbuild、MODPOST、装卸、真实日志、RCU/SMP 和弱内存序均未验证，没有改动外部树。


P09 寿命单元再次核对固定 lib/rbtree.c 与 rbtree.h：摘除与游离标记不是引用取得、等待旧读者或释放操作。教材用[S0～S5 串行持有权模型](../../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_两个入口关闭之后谁还在使用对象)区分状态；RCU 片段不在摘除后立即改写旧字段，call_rcu 只登记延迟回收。没有将该模型或片段作为并发可运行内核验证。

## 1.23\_rbtree遍历与整树销毁证据

同一固定对象中，lib/rbtree.c 的七个中序/后序函数及 rbtree.h 的 rb_entry_safe、后序 safe 宏补入[唯一遍历实现](../rbtree/source_explanations/lib/rbtree.c.md#1.7_中序端点与父链推进)与[模块导读](../rbtree/navigation/P05_有序推进与整树销毁导读.md#5.1_拓扑与游标分别保存在哪)。孩子下行和父链上行、NULL 输入差异、左优先叶子及循环体前保存 n 均按实际语句核对，不将向下 RCU 查询的边界推广到父链。

六对象全部 720 种插入次序分别按唯一键/重复键构建 1440 棵树，以独立递归次序核对中序、逆序、后序和保存后继删除，共 8640 次删除后推进；另查清标记节点、空树及 postorder 的 NULL。四键 10/20/30/40 复现 postorder 混用 rb_erase 漏访 20。实际材料的宿主适配运行两种正确回收和自动对象反例，并逐个注入八处申请失败，确认零存活分配，另查重复键、空输入和单节点。宿主采用 uintptr_t 父色适配和普通 WRITE_ONCE，不是 Linux ABI、并发或内存序验证。

note_rbtree_walk 通过 ARMv7 语法检查，实际使用 357 份头，已跟踪头对固定对象的差量交集为空，生成头仍来自当前配置。没有执行目标 Kbuild、MODPOST、装卸或真实日志；外部源码树未改。


## 1.24\_rbtree同键替换与旧对象退出证据

同一固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0 的 rb_replace_node、rb_replace_node_rcu、rb_set_parent、__rb_change_child_rcu、rb_replace_node_cached 五函数已核对到[唯一替换实现](../rbtree/source_explanations/lib/rbtree.c.md#1.9_同键替换的普通与RCU入口)及[模块导读](../rbtree/navigation/P06_同键替换与旧对象退出导读.md#6.2_一轮替换怎样交接入口)。只复制嵌入节点，孩子父地址先改，RCU 外部入口最后发布；cached 包装先写 rb_leftmost，不能据此推导无保护读取安全。

额外只读核对 include/linux/rcupdate.h 与 kernel/rcu/tiny.c，固定 blob 分别为 48e5c03df1dd83c246a61d0fcc8aa638adcd7654、b3b3ce34df6310f7bddba40b2be1bdf6c9f00232；读取、发布及等待复用既有 RCU 唯一解释，不复制函数体。访问配置仍为 ARM/TINY_RCU/PREEMPT_NONE/非 SMP，本地三笔实验提交不作为证据。

C 宿主以六对象的 720 种插入次序，分别唯一键/重复键、六个替换位置、普通/cached/RCU 三个入口，共 25920 次替换，独立递归核对中序对象集合、父链、颜色、黑高与缓存，另检查旧字段保留和新业务字段未覆盖。实际材料经显式适配运行，两个申请失败出口无泄漏，检查读侧退出后才能等待；RCU 访问替身只是普通赋值，Windows 父色用 uintptr_t，不构成内核 ABI、并发、宽限期或屏障证明。

note_rbtree_replace 通过 ARMv7 语法检查，实际使用 357 份头，已跟踪头对固定对象的差量交集为空，生成头来自当前配置。目标 Kbuild、MODPOST、装卸和真实日志未执行；外部树未改。宿主删除夹具的 rb_set_parent 从对齐前提下等价的按位或改为上游实际加法后，重新运行此前 6220800 次删除及 8640 次遍历删除，结果一致。


## 1.25\_rbtree删除遍历与替换阅读分工

B03p/q/r 的固定源码事实和检查结果不变，知识正文按任务完整组织为[P11 删除](../../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.1.3_一轮取消经过哪些状态)、[P27 遍历](../../../knowledge/linux/data_structures/红黑树_rb-tree/P27_Linux有序遍历与整树销毁.md#27.1_从一次取消走到整轮处理)、[P28 替换](../../../knowledge/linux/data_structures/红黑树_rb-tree/P28_Linux同键替换与旧对象退出.md#28.1_从保存地址走到交接地址)。三个完整模块及全部图/代码围栏原样保留；只调整开头、结尾、标题与引用。此次未重新执行算法或目标运行，沿用相应批次已明确范围的证据，不能把章节拆分当作新增并发或目标验证。


## 1.26\_普通旋转与内核修复版本对照

P05 的内核单元独立为[P29 完成边界](../../../knowledge/linux/data_structures/红黑树_rb-tree/P29_普通旋转与Linux修复的完成边界.md#29.1_为什么没有一一对应的旋转调用)。当前分析仍采用 NXP 固定 dfaf2136deb2af2e60b994421281ba42f1c087e0；为核对旧声明，只读比较 1.9 节保存的上游 6.1 固定对象 830b3c68c1fb1e9176028d02ef86f3cf76aa2476。四份旧副本与 Git blob 一致，原文未修改：

| 上游相对位置 | 上游 6.1 blob | 当前固定 blob |
| --- | --- | --- |
| lib/rbtree.c | c4ac5c2421f255c4c60954c45ee567f77403cffc | 989c2d615f927a1e7415f1c8888db4f69f6dd0c9 |
| include/linux/rbtree.h | f7edca369edaddb6adf4b7ba8a9ad745fea52356 | 7c173aa64e1e3f26a38f94ffa5c5031d9aa99c53 |
| include/linux/rbtree_augmented.h | d1c53e9d8c7532173f5aa24725f553561f805b6d | 6dbc5a1bf6a8ce04042df4c07167cc226d53a466 |
| Documentation/core-api/rbtree.rst | ed1a9fbc779e1b9de88e878035abc8e6c9a02078 | ed1a9fbc779e1b9de88e878035abc8e6c9a02078 |

剥离注释后，__rb_insert、__rb_rotate_set_parents、____rb_erase_color 的函数语句在这两个对象间一致；这不表示所调用的辅助或所有接口都相同。实际差量包括 rb_set_black/父色打包的或到加法、Case 3 注释图标签，以及 rb_find_add_rcu/rb_find_rcu/rb_add_augmented_cached 新入口。P29 五段短裁剪均为当前固定 lib/rbtree.c 的连续语句，完整实现仍在[rbtree 唯一讲解](../rbtree/source_explanations/lib/rbtree.c.md#1.3_插入修复的两侧分支)。

本次未核对 5.15，不再保留原正文对该版本的一致性主张。没有修改算法和实际材料，不重复声称新的目标运行、ABI、并发或性能验证；源码树只读。


## 1.27\_页级索引与ext4范围格式证据

[P34 页级索引](../../../knowledge/linux/data_structures/红黑树_rb-tree/P34_从多路节点到页级索引.md#34.15_文件系统中的_B/B+_树应用)只读核对同一 NXP 固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0 的 Documentation/filesystems/ext4/ifork.rst，blob 为 dc31f505e6c835bf590998a88676e9fa48d4c0f9。文档区分内部 ext4_extent_idx 与叶层 ext4_extent，根放在 inode.i_block；本次仅引用格式职责和根不一定独占外部块的边界，不展开函数体，不声称已验证挂载模式、分配算法或 I/O 性能。

数据库/存储引擎对照独立依据 SQLite 文件格式、MySQL 8.4、PostgreSQL 18 和 RocksDB 官方概览，不作为 NXP 内核实现证据。完整 C++ 四页模型实际只读宿主数组，逻辑页请求、模型未命中与设备 I/O 分开；未执行目标数据库、文件系统挂载或硬件性能实验。外部工作树未改，本地三笔实验提交仍不作证据。

## 1.28\_Maple范围与VMA查询证据

[P14 VMA 范围教材](../../../knowledge/linux/data_structures/红黑树_rb-tree/P14_从地址查询到VMA范围修改.md#14.1_一个地址为什么需要三种查询)沿同一固定提交核对区间、节点容量、查询封装和返回对象保护；[Maple 总索引](../maple_tree/navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)区分模块导读与唯一函数体。P15 的四个 VMA 查询函数体移入对应上游路径的实现文档，其余 P15 内容仍待独立审查。

| 上游相对路径 | 固定 blob | 本批职责 |
| --- | --- | --- |
| Documentation/core-api/maple_tree.rst | ccdd1615cf974f40ad2f655ca734b7bfcdb5ba94 | 闭区间、空洞树、普通/高级接口与锁契约 |
| include/linux/maple_tree.h | c2c11004085e5a98702a2aa8b1671d7a0f5bfe25 | pivot 包含同号槽上界，32/64 位容量与 gap 布局 |
| include/linux/mm.h | 8617adc6becd1f9325e7217b885c4cc4124c5cc3 | vma_lookup 和 VMA 游标入口 |
| include/linux/mm_types.h | 6894de506b364fa7f3396146f53216d0d40b80d2 | VMA、mm_mt 与 MM_MT_FLAGS |
| mm/mmap.c | 6183805f6f9e6ef1a6d3204834ff1c367d0376b1 | find_vma、相交查询、前驱组合 |
| mm/memory.c | 525f96ad65b8d77fe9d1feb5c7db1dc70d39647f | CONFIG_PER_VMA_LOCK 分支的候选稳定与边界检查 |
| lib/maple_tree.c | 8d73ccf66f3aa0588d5ee00a6e7dad3258110d83 | 封装所调用的范围查找核心位置，不据此宣称本批全核心已审查 |
| Documentation/mm/page_tables.rst | be47b192a596e5fc45d517ec75698897d92d8bef | TLB 未命中、页表遍历与访问异常的区别 |
| kernel/fork.c | e192bdbc9adebbd6472ed4bb3e3f77260e98c673 | mm_mt 初始化及外部 mmap 锁关联 |
| kernel/sched/fair.c | 58ba14ed8fbcb98ef1d2bb6779aae1a51c71e595 | pick_eevdf 仍访问红黑树，调度策略不等同于 VMA 索引 |

前七份已有副本中，mm.h、mm_types.h、memory.c、maple_tree.c 与固定对象存在差量，已只按官方固定对象同步：涉及页表共享计数、写封印辅助、缺页回退/大页地址对齐，以及 Maple 分裂、根空值存入与循环分配游标重置。未把这些差量猜作某个本地实验提交的成果，也不根据函数名宣称其全部机制已审完；仓库正文没有这些变化函数的逐句展开调用方，本批四个查询封装语句不变。七份现有副本均按 LF 规范化后的 Git blob 核对一致。

页表说明、fork.c 与 fair.c 只读固定 Git 内容，没有为简短职责比较复制整份文件。当前工作树身份仍为官方 NXP 来源、lf-6.12.y、三笔实验提交之后的 HEAD；证据只取 dfaf2136deb2af2e60b994421281ba42f1c087e0。配置为 ARM32、MMU、Tiny RCU、PREEMPT_NONE、非 SMP，未启用 CONFIG_PER_VMA_LOCK；不把该配置外推到发布标签或别的架构。

旧组合另核对 torvalds/linux 的 v5.19 标签中 mm_types.h、sched.h 和 mm/vmacache.c：缓存属于任务，mm 保存失效代号；这是历史来源，不写成 NXP 当前结构。2020-12-10 RFC 及 2022-09 v14 说明仅承担演进与历史数据，早期未支持 32 位/非 RCU 的性能结果不外推到本机。C++ 范围模型不执行真实系统调用、Maple 核心或硬件页表遍历，本次未运行目标内核性能实验。

## 1.29\_rbtree调用者接口与持有权示例

[P37 完整框架](../../../knowledge/linux/data_structures/红黑树_rb-tree/P37_构建rbtree调用者接口.md#37.16_运行完整的私有调用者框架)复用上表固定树操作，主例只在初始化中私有执行，按 U0～U5 管理根、计数、复制输出和对象交还。note_rbtree_owner 的 ARMv7 前端实际读取 354 份头，所读已跟踪头与固定提交的差量交集为空；生成头仍来自当前 ARM/TINY_RCU 配置。首次检查发现 current 与内核宏冲突，改为 entry 后通过。

宿主复用固定树语句的位宽适配夹具，普通整数锁替身只检查串行临界区。36 组插入/删除顺序检查计数、复制值、重复键、已挂入状态和移除输出，四个申请失败点验证无剩余分配。目标 Kbuild、MODPOST、装卸、并发及真实日志未执行，未改外部树。

## 1.30\_rbtree最左缓存与返回边界

固定 rbtree.h 的 rb_first_cached、rb_insert_color_cached、rb_erase_cached、rb_add_cached 进入[唯一缓存标题](../rbtree/source_explanations/include/linux/rbtree.h.md#1.12_缓存取首只读取入口)，由[缓存模块 C0～C6](../rbtree/navigation/P08_最左缓存与结构更新导读.md#8.2_沿接入与摘除跟踪C0到C6)组织状态与读写顺序。四个片段逐字核对既有固定原文；返回 NULL 不能统一解释为插入失败或删后树空。

完整 note_rbtree_cached 使用私有自动数组，故障注入普通删除后对象仍活着。宿主明确位宽适配的固定语句夹具覆盖五对象 120 种插入乘 120 种删除次序、144000 次操作检查，另运行正文反例。ARMv7 前端读取 348 份头，消费的已跟踪头对固定提交差量为空；生成配置来自本地已核对环境。没有目标 Kbuild、MODPOST、装卸、真实日志、并发或性能证据，外部树未改。

## 1.31\_rbtree增强摘要与回调边界

固定 include/linux/rbtree_augmented.h 的回调结构、两个生成宏、增强插入/删除和 cached 组合包装进入[唯一增强标题](../rbtree/source_explanations/include/linux/rbtree_augmented.h.md#1.7_三个回调的结构契约)，由[增强 A0～A5](../rbtree/navigation/P09_子树摘要与增强回调导读.md#9.2_沿A0到A5维护同一份摘要)组织。插入包装只传 rotate；propagate 的 stop 排除在外；copy 的删除临时移交与 rotate 的集合不变移交不可混同。相关片段逐字核对既有固定原文。固定 Documentation/core-api/rbtree.rst 的 Compiled code 段仅说明可能内联及每个使用者集中删除调用点，不是每编译单元只能一棵树。

完整 note_rbtree_augmented 使用私有闭区间对象；固定回调宏与语句的宿主显式位宽适配夹具覆盖 8640 组次序、120960 个稳定状态、11007360 次逐点独立线性对照，以及失败不改摘要、错误上下界和过早停止。ARMv7 前端读取 349 份头，消费的已跟踪头与固定提交差量为空；生成配置来自已核对环境。没有目标 Kbuild、MODPOST、装卸、真实并发或性能证据，外部源码未修改。

## 1.32\_rbtree并发观察与寿命边界

再次核对固定 lib/rbtree.c 的 lockless lookups 注释，以及 include/linux/rbtree.h、include/linux/rbtree_augmented.h 相关入口，已跟踪三文件与官方固定提交差量为空。孩子单次写入约束、不成环写序、有限向下路径与可能漏查相互区分；父链不在同一循环论证内。既有[查找实现](../rbtree/source_explanations/include/linux/rbtree.h.md#1.4_rb_find_rcu的孩子读取与缺失边界)和[替换实现](../rbtree/source_explanations/lib/rbtree.c.md#1.9_同键替换的普通与RCU入口)继续唯一展开，无新增重复函数体。

[P45 并发单元](../../../knowledge/linux/data_structures/红黑树_rb-tree/P45_完整操作与返回对象的并发保护.md#45.1_rbtree_与并发控制)补 M0～M4，宿主 copy_under_lock 使用真实 pthread、互斥与条件变量重放两种观察次序；重复 512 次并检查两处分配失败，既有 lookup_paths 四行结果重新验证。用户态模型只含一个入口槽，不实现树或内核锁；没有本批 ARM 并发、Linux RCU、SMP 压力或目标模块执行证据，外部树未改。

## 1.33\_rbtree调用者示例的比较收敛

[P45 示例回访](../../../knowledge/linux/data_structures/红黑树_rb-tree/P45_完整操作与返回对象的并发保护.md#45.8_回到调用者接口核对持有权)统一使用 P37 的完整 owner 模块和 P27 的完整遍历材料，不新增第二份内核接口实现。owner 的查找与插入共享 compare_key，关系判断结果相减避免任意 int 键相减溢出。宿主重新执行 36 组操作次序、四处分配失败以及 INT_MIN/INT_MAX 等极值接入、查找、拒绝重复和移除。ARMv7 前端读取 354 份头，消费的已跟踪头与官方固定提交无差量。没有目标 Kbuild、MODPOST、装卸或并发运行证据，未修改外部树。

## 1.34\_rbtree联合不变量的有限检查

[P46 有界快照](../../../knowledge/linux/data_structures/红黑树_rb-tree/P46_独立验证树形与派生状态.md#46.10.1_运行有界快照检查器)使用 C11 固定容量数组与整数槽号，在访问前识别已知对象、拒绝重复进入，再检查键界、父链、颜色、黑计数、原始载荷重算摘要、计数与缓存身份。十二类样例通过；固定增强算法宿主夹具的 120960 个稳定状态转换为模型后通过联合检查，各状态再注入计数错误与非空摘要错误验证拒绝。固定算法来源与显式位宽适配仍见 1.31，不新增内核原文或另一套实现讲解。

这是已知对象模型和有限输入证据，不验证任意损坏指针、并发快照采集、目标 ABI、RCU/SMP 或内核模块运行。结构与计数同时漏掉对象仍需外部业务台账才能发现，未报错不能超出执行路径与观察边界。

## 1.35\_rbtree实际调用场景证据

本批只读 git show 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，核对以下完整文件，三笔本地实验提交不用于结论。选择性的完整函数讲解按上游路径放入 rbtree/source_explanations，未把临时缓存当作另一份正式源码镜像。

| 上游位置 | 固定文件 Git blob |
| --- | --- |
| `kernel/sched/fair.c` | `58ba14ed8fbcb98ef1d2bb6779aae1a51c71e595` |
| `lib/timerqueue.c` | `cdb9c7658478f0505e2d1bdc8e6ede6a812fa958` |
| `include/linux/timerqueue.h` | `d306d9dd22073f04bb0106fcf3ba598e87ba9b07` |
| `include/linux/timerqueue_types.h` | `dc298d0923e3b2f3baeca682c422367635f8d0ad` |
| `kernel/time/hrtimer.c` | `db9c06bb23116a0d76d972477b40afd43b9b6d8f` |
| `block/elevator.c` | `43ba4ab1ada7fd2462a44d3582de9e115973e4be` |
| `block/mq-deadline.c` | `acdc28756d9d778ef4ac5e32cb5a1c363a08827f` |
| `fs/eventpoll.c` | `1a06e462b6efba8824456cffebad040720c4226a` |
| `include/linux/interval_tree_generic.h` | `aaa8a0767aa3a512c978d047556af3cee07af66f` |
| `lib/interval_tree.c` | `3412737ff365ec9c91ac6ffa3fab15cc82896249` |

[场景导读](../rbtree/navigation/P10_内核调用场景与选择边界导读.md#10.2_按排序键和业务问题逐项阅读)串联十八个唯一函数：timerqueue 依 expires 排序，add 的布尔值表示新成最早、del 则表示余队列非空；fair 的 entity_before 按 deadline，资格另由 vruntime 与加权状态判断，pick_eevdf 还处理当前实体及特性分支；elv 按逻辑扇区建索引，mq-deadline 另有 FIFO/批次/方向条件；epoll 注册树用 file/fd 复合键，就绪列表另有状态。interval_tree 使用闭区间相交与最大终点，不能套用 VMA 半开非重叠映射。

十八个函数逐字比较固定文件，定义无重复；只是静态源码证据，没有这些完整子系统的 Kbuild、运行、并发压力或性能测量，外部树未修改。VMA 当前 mm_mt 证据继续沿已有 Maple 基线和 P14，不复制完整范围教程。

## 1.36\_Maple共享树与模式证据

沿 1.28 的官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，再读 Documentation/core-api/maple_tree.rst、include/linux/maple_tree.h、include/linux/mm_types.h、lib/maple_tree.c、mm/mmap.c 与 kernel/fork.c，六份 blob 仍与 1.28 表一致，前五份已保存副本规范化后逐字相同。外部 HEAD 仍含三笔实验提交，本批只用固定对象，未修改外部树。

[树模式模块](../maple_tree/navigation/P03_树对象与模式选择.md#3.2_从未发布到受保护使用)核对共享根、index 0 直存条件、锁选择、外部 lockdep 描述和 MM_MT_FLAGS。七个唯一函数体为 mt_external_lock、mt_init_flags、mt_init、mt_in_rcu、mt_clear_in_rcu、mt_set_in_rcu 与 mas_free，连同字段、标志及 CONFIG_LOCKDEP 分支按上游位置展开。mas_free 在 RCU 模式交给 ma_free_rcu，在非 RCU 模式推回操作池；模式切换函数没有 synchronize_rcu，不把 exit_mmap 的特定退出背景推广为活跃树的通用许可。

当前配置仍为 ARM32、Tiny RCU、PREEMPT_NONE、非 SMP；未启用 CONFIG_PER_VMA_LOCK，头文件中的 CONFIG_MAPLE_RCU_DISABLED 是独立条件分支。只做静态源码与文档核对，未运行 Maple 核心、真实 VMA 系统调用、目标并发或性能测试，完整节点回收尚未据此宣称完成。

## 1.37\_Maple节点布局与范围分区证据

沿 1.28 和 1.36 的固定 include/linux/maple_tree.h（c2c11004085e5a98702a2aa8b1671d7a0f5bfe25）与 lib/maple_tree.c（8d73ccf66f3aa0588d5ee00a6e7dad3258110d83）核对容量分支、五个布局结构、节点类型及 maple_tree_init 完整函数。唯一展开位于[节点导读](../maple_tree/navigation/P04_节点布局与范围分区.md#4.2_按问题读取布局)关联的原有实现文件，未修改原始副本或外部树。

range/leaf 与 arange 的数组容量分别为 64 位条件分支 16/10、普通 32 位分支 32/21；名字 _64 不替代 unsigned long 与配置，容量不等于有效孩子数。叶包含 entry/NULL，非叶指向孩子；range 的 slot 末端与 metadata 共用 union，arange 另有 gap 数组。容量旁 240 字节注释不能代替完整结构 sizeof。

使用提取的固定结构、明确 __rcu/rcu_head 适配，Clang ARM32 与 x86_64 freestanding 前端断言确认 node/range 均 256 字节、arange 分别 256/248；没有完整 Kbuild 或分配器运行。另以原创 C++ 分区程序验证 NULL 槽、包含式上界和裁剪窗口，未执行内核 Maple 核心、RCU、重平衡或性能测量。

## 1.38\_Maple字段编码证据

固定提交仍为 dfaf2136deb2af2e60b994421281ba42f1c087e0，原 maple_tree.h/c 的 blob 不变；另只读取得 include/linux/xarray.h，blob 0b618ec04115fc3993bf33a7c358632bef170fc9。新增[字段编码导读](../maple_tree/navigation/P05_字段编码与状态分工.md#5.2_同一数值先按存储位置解读)与 xarray 值标记实现页，十六函数和五组常量/宏按上游相对路径唯一展开。

实际父槽掩码为 0xF8，根 parent 的 bit 0 与 ma_root 节点入口的 bit 1 分属不同字段。MA_ERROR 先转 unsigned long 再左移，mas_set_err 同时写 node/status，mas_is_err 检查独立 status；保留原始注释而在正文说明与执行表达式的差异。xa_mk_value 的有效整数宽度少一位，WARN_ON 不代替输入拒绝。

原创 C11 定宽整数模型通过 18432 次节点/父槽往返及边界反例；没有真实指针解码、Maple 核心运行、RCU 或对象有效性验证。原始 xarray 只保存在忽略缓存，正式实现页记录固定 blob；外部树未修改。

## 1.39\_Maple游标周期与暂停继续证据

使用同一官方固定 maple_tree.h/c 与 mm.h，blob 沿 1.28，不采用实验 HEAD。新增[游标模块](../maple_tree/navigation/P06_操作游标与暂停继续.md#6.2_沿一次遍历追踪状态)，十三函数及状态/写入类型枚举、ma_state 和 MA_STATE 宏分别按上游路径唯一展开。mas_pause 保留 index/last 并清 node，find/setup 的暂停分支检查 max 后推进 last；reset 则保留索引。mas_walk 的实际或条件会先置 start，mas_find 的部分 NULL 分支保持/恢复 active，不能靠泛化状态图替代代码。

另核对 mtree_lookup_walk 的快速点查契约，不承诺维护完整游标字段；P15 八段 VMA 查询结果对照保留，其临时状态说明按此修正。note_maple_state.c 通过 ARM 前端，消费 348 份头文件的非生成部分与固定对象无差量；生成头仍是当前配置证据。宿主夹具只用真实控制函数并明确替代树行走，核对私有顺序及四个 store 失败退出。目标 Kbuild、MODPOST、装卸、RCU/并发和性能未执行，外部树未改。

## 1.40\_Maple普通接口与范围契约证据

同一官方固定 maple_tree.h/c 与 xarray.h，blob 沿 1.28 与 1.38；不采用实验 HEAD。新增[普通接口模块](../maple_tree/navigation/P07_普通接口与范围契约.md#7.2_沿一次调用划分责任)，八个普通函数、三个编码 helper、锁/迭代/内部常量三组宏按固定语句唯一展开。额外只读 mas_insert 冲突出口确认 -EEXIST，保留对上游 EEXISTS 拼写的纠正；不将尚未展开的动态写入算法宣称已验证。

普通锁宏直接操作 ma_lock；load/find 返回前退出 RCU 读侧，业务对象不自动取得引用。max 不裁剪命中范围，last+1 的最大值回绕由 mt_find_after 处理；erase 擦除整段，而 NULL store 可局部清空。模块 ARM 前端通过，消费头文件按固定提交比较；宿主执行固定外围封装、范围后端为模型，internal 值转指针按 LLP64 宿主显式适配。核对五处分配失败、特殊 entry 与迭代终止，未执行目标 Kbuild、MODPOST、装卸、真实节点算法或并发。

## 1.41\_Maple写入准备与资源清理证据

固定 maple_tree.c 与 mm.h 的 blob 沿 1.28。十二函数及一组资源标志按上游路径唯一展开，新增[资源模块](../maple_tree/navigation/P08_写入准备与资源清理.md#8.2_沿S0到S5追踪资源)。mas_preallocate 零需求可不设置 PREALLOC，失败保存 ret 后清理/reset；mas_nomem 只在允许睡眠且内部锁模式下放锁补分配，成功要求重试，不代表写入完成。mas_store_gfp 的 NULL 请求在重试时恢复原 index/last。mas_destroy 可涉及批量重平衡，与销毁整树分工不同；外部锁示例用 __mt_destroy。

ARM 前端通过，348 个消费头文件的非生成部分与固定提交无差量。宿主三个固定控制函数配合显式资源与写入替身检查零需求、成功/失败清理、16 个补分配组合和清除请求范围恢复；错误载荷按宿主 intptr_t 适配，未模拟真实节点分配、回收和批量重平衡。目标 Kbuild/MODPOST/装卸与并发未运行。P15 两处 vma_find 重复函数体统一指向 mm.h 实现标题，半开边界与生命周期责任保留。

## 1.42\_VMA游标初始化与边界适配证据

固定 mm_types.h/mm.h 的 blob 沿 1.28；八个包装/初始化函数、一结构、一宏按上游位置唯一展开，既有 vma_find、vma_lookup 与 invalidate 沿已有标题。VMA_ITERATOR 省略 last，聚合初始化为零，vma_iter_init 经 mas_init 设置 last=addr；不把查询前字段当命中结果。clear/bulk 依据状态映射为 -ENOMEM，局部资源释放不等于销毁 VMA。

[VMA 模块](../maple_tree/navigation/P09_VMA游标与边界适配.md#9.2_从地址空间到局部游标)按 S0～S4 区分外围保护、局部游标、窗口转发、对象使用和退出。C 整数实验实际比较 528 个合法区间、561 个拒绝输入与 17424 次成员关系；固定包装宿主检查只核对转发，Maple 后端为替身。未执行真实 VMA、锁竞争或页表更新。额外只读固定 mm/mmap.c 与 mm/vma.h 调用名定位，说明公共头包装不是完整 MM 写入主线，未展开未验证函数体。

## 1.43\_撤销范围与临时序号树证据

新增只读固定对象 mm/vma.c（blob c9ddc06b672a5235eb7365d2197856537ce6d143）与 mm/vma.h（blob d58068c0ff2eaa38161c5bac2f27ee145ec1a2f6）；mm/memory.c 沿 1.28。官方仓库和固定提交不变。八函数按[撤销模块](../maple_tree/navigation/P10_撤销范围与临时索引.md#10.2_两棵树沿S0到S5分工)在三份实现文档完整展开，init_vma_munmap 的 CONFIG_MMU 条件说明保留。

对齐撤销以地址主树与序号临时树分工；gather 标记与清主树不是同一步，reattach 不逆转全部 split。clear_ptes 两次从序号一继续，首项由参数交付；free_pgtables 明确允许 ceiling=0 哨兵，不能推广到普通空 VMA 范围。对照 mm/mmap.c 的 exit_mmap 使用地址树状态，避免反向泛化。C++ 分区实际检验 E/F/G 与小域归属，固定 clear_ptes 的下层替身夹具验证控制顺序；未运行目标 munmap、通知、页表、TLB、失败回滚或并发。

## 1.44\_引用责任的契约核对

B04a 重新逐字核对固定提交的 include/linux/kref.h（blob d32e21a2538c292452db99b915b1bb6c3ab15e53）和 Documentation/core-api/kref.rst（blob c61eea6f1bf2bd76490718430130fe3501e2f7e8），本仓库副本按 LF 归一后相同。证据支持初始一份、已有有效引用下增加、交付前建立接收方责任、最后归还调用 release，以及 lookup 需要另行保护的契约；未使用本地实验提交。

本批[P01 的责任模型](../../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.6.1_运行完整的责任交接模型)不展开源码函数体，具体结构及唯一实现讲解仍由后续源码批次独立审查。完整 C 模型只验证串行责任闭合，不是 kref/refcount_t 的并发、内存序或饱和实现测试；不改变既有源码配置结论。

## 1.45\_一次引用交付与工作队列边界

B04b 按相同官方固定提交读取 include/linux/workqueue.h（blob 59c2695e12e7674d5bc4cdd1fb416074374f981c）与 kernel/workqueue.c（blob a9d64e08dffc7c7aef2caae2f170268d83e99e23）；与仓库既有副本 LF 归一后相同。核对 queue_work 的返回/发布契约、destroy_workqueue 的 drain 路径及 work 执行开始后允许释放工作项的实现边界，复用[工作队列生命周期导读](../workqueue/navigation/P04_Linux_6.12_flush取消与生命周期模块源码概念导读.md#4.5_destroy与对象生命期)，不复制另一套实现讲解。

[P01 完整模块](../../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.16.1_运行一次真实工作交付)使用一次私有工作交付与两份引用，无取消、重排或外部生产者；ARM 前端纳入 354 份头文件，非生成源码相对固定提交差异为空。生成配置不作为官方标签的一部分。宿主夹具仅以固定 kref 包装函数配合明确的下层替身验证五条清理顺序，不声称验证真实原子操作、调度、目标模块装卸或内存序。

## 1.46\_引用原语与饱和契约

B04c 只读核对固定提交 include/linux/refcount.h（blob 35f039ecb2725618ca098e3515c6e19e2aece3ee）、include/linux/refcount_types.h（blob 162004f06edf7c3049bac7c960e2e50a190595d6）、lib/refcount.c（blob a207a8f22b3ca35890671e51c480266d89e4d8d6），与仓库副本 LF 归一后相同。证据用于 P02 2.1～2.6 的层次和契约解释：异常原子操作后收敛到饱和，正常增加无额外发布排序，减少及归零路径有规定顺序。

[八位教学模型](../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.5.1_用八位模型观察回绕的代价)只解释回绕造成假零和拒绝释放的代价，显式布尔状态与预先饱和不同于内核算法。1001 组串行模型检查不构成实际原子、竞态或告警覆盖证明。现有 P02 2.13/P05 的源码函数体仍待后续独立审查和唯一实现入口整理，本批不新增重复函数展开。

## 1.47\_引用成员与回调地址

B04d 重新只读核对固定 include/linux/container_of.h（blob 713890c867bea78804defe1a015e3c362f40f85d），与仓库副本 LF 归一后相同。实现继续使用[既有唯一标题](../rbtree/source_explanations/include/linux/container_of.h.md#1.1_一次还原中的求值与类型检查)，新增的是 kref/work 两种回调的调用上下文，不是另一份宏展开。

一次工作模块将 ref 移到非首成员，重新通过 ARM 前端与五路径宿主控制检查；固定宏的双成员 GNU C 宿主实验也重新运行通过。未把宿主偏移数字当作 ARM 布局，未将类型检查等同于对象身份或寿命验证；目标装卸仍未执行。

## 1.48\_普通引用链与编译属性证据

B04e 建立[kref 总索引](../kref/navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)、状态模块与按上游相对位置组织的唯一实现。kref/refcount 文件沿 1.44/1.46 同一固定提交；另核对 compiler_types.h（blob 639be0f30b455d7b42adc26701fb47093012a1b8）、compiler_attributes.h（blob c16d4199bf9231b8aa8e08d6c8174247b11da82c）和顶层 Makefile（blob ca000bd227be66540185c450b749a5d5258f87eb）。后两份只裁剪本任务相关固定语句，不建立整份源码镜像。

十三个函数体、两项类型、告警宏、signed_wrap 条件、must_check 和构建选项明确分层。__signed_wrap 仅按 CONFIG_UBSAN_SIGNED_WRAP 抑制特定插桩，不能替代 -fno-strict-overflow 等构建前提；GCC 宿主选项查询与属性预处理分别验证。固定函数体配合顺序原子/屏障/告警替身运行正常周期及 21 个边界，另以实际编译目标文件验证丢弃/消费底层结果与直接 put 的诊断差异。未执行真实原子竞争、内核 WARN、目标编译链接或装卸；条件引用及锁组合仍未在本研究目录展开。

## 1.49\_定义时初始化与静态存储证据

B04f 沿同一 NXP 固定提交核对 KREF_INIT、REFCOUNT_INIT、ATOMIC_INIT 及 atomic_t，types.h blob 为 2bc8766ba20cab014a380f02e5644bd0d772ec67；前三层初始化器分别归入[kref 初始化路线](../kref/navigation/P02_普通引用与归零回调导读.md#2.6_初始化形式与存储寿命)的唯一实现。原始宏与裁剪定义逐字匹配，初值规则与 C 存储期分开解释。

完整静态模块 ARM 前端通过，348 份头文件中 336 份非生成文件与固定提交无差异；生成头仍属于当前配置。宿主固定宏、类型及普通引用函数配合顺序替身通过一次归零周期；严格 C11 接受自动运行时初值和静态常量，按预期诊断省略花括号、裸宏赋值及非恒定静态初值。未执行目标构建链接、装卸和真实并发，不以宿主结果代替目标日志。

## 1.50\_普通操作与计数快照的证明边界

B04g 重读同一固定提交 Documentation/core-api/kref.rst 的引用规则，以及普通 get/put/read 与 refcount 的既有唯一实现；未引入新版本、配置或架构结论。原子操作结果与本次旧值绑定，地址可读与正引用保证分开，返回 0 和 read 快照都不产生新的访问权限。正文[责任对照](../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.17.1_运行快照与持有的对照程序)使用宿主 C 的顺序对象模型，两条持有路径、两条分配失败及六种错误分离交错/两种绑定结果顺序通过；不执行悬空访问，不声称验证真实并发。

## 1.51\_对象模板与部分初始化清理证据

B04h 沿固定 NXP 提交核对 mm/slub.c 的 kfree(NULL) 路径（blob b9447a955f61128806d980734a02b7762aacebfa）与 mm/util.c 的 kstrdup 申请/复制/失败返回（blob 4f1275023eb7317973c061da950ef582f1660c28），只读固定对象，未修改外部树或复制完整分配器教程。kref 普通函数和 container_of 沿既有唯一实现。

[对象模板](../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.19_标准自定义引用对象模板)使用外壳与字符串两块存储，验证初始引用之后的失败也能走类型清理出口。ARM 前端通过，354 份头文件中 342 份非生成源码与固定提交无差异；生成头属于当前配置。实际模块代码配顺序计数及分配替身验证成功、两个申请失败点、NULL put、释放顺序和一次回调；未验证目标链接、装卸、真实分配器故障或并发。

## 1.52\_回调契约与C++对照边界

B04i 使用同一固定 kref.h 普通 put 与已核对 kfree(const void *) 参数，顺序 C 夹具的两种结束顺序均只选择最后一次调用的回调；直接传 kfree 的负例按预期得到 incompatible-pointer-types，未执行错误调用。唯一实现保持不重复，静态宏已经落地的范围说明一并校正。

[C++ 对照程序](../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.23.1_用完整程序观察自动归还)属于宿主教材实验，不是 Linux 源码证据：GCC/libstdc++ 14.2.0 以 C++17 严格编译，拷贝、移动、reset、异常退出检查通过。实现布局只读核对 libstdc++ bits/shared_ptr_base.h 的 _Sp_counted_ptr_inplace 内置存储，SHA-256 为 17895f579b5f9e5f4837ce2a23a20b00ab6cfe27ffc24bab024853ba0a900c88；不复制上游库实现，不外推所有标准库布局。接口语义参考 [C++ 工作草案 shared_ptr](https://eel.is/c++draft/util.smartptr.shared)（2026-09-23 查阅），只使用 C++17 已有功能。未作性能基准、真实并发、目标模块或控制块分配次数测量。

## 1.53\_容器责任与清理上下文证据

B04j 的[单槽模块](../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.30.1_设计_A_容器持有引用)沿固定 kref 普通实现，用 mutex 配合容器持有责任建立锁内 get 的正引用保证。重新只读核对同一 kernel/workqueue.c 的 __cancel_work_sync、__flush_work 及 cancel_work_sync 注释（blob 沿 1.45），确认等待执行结束与无竞态重新投递前提；正文通过调用依赖说明 worker 在自己的最后回调里等待自己不能闭合。RCU 清理按既有三种对象拓扑入口分流，不追加通用无条件等待模板。

ARM 前端通过，354 份头文件中 342 份非生成源码与固定提交无差异；生成配置另界定。实际模块配显式顺序锁/分配/原子替身通过六组控制路径，包括分配失败、正常入口、满槽拒绝、先撤下后查找及两种双读者退出。验证责任、锁外清理和无剩余分配，不证明真实竞争、内存序、目标构建链接或装卸。

## 1.54\_生命周期观察模型与固定实现分层

B04k 的[P03 C 模型](../../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.3.1_用C模型观察仍持有却被拒绝)用于观察入口、业务许可、存储和角色责任，没有新增 Linux 字段或版本事实。模型的持有者位图位于观察者账本，不能当成 kref.h 布局；固定 init 仍沿前述官方提交与唯一实现，只写入初始计数，初始份额归属由创建接口约定。两种操作/关闭顺序及两个状态分离反例通过严格 C11 编译运行，不包含真实分配、原子并发、锁或硬件可用性验证。

## 1.55\_清理诊断与链表状态表示

B04l 为 P03 的 release 诊断核对固定 NXP 提交 include/linux/list.h（blob 5f4b0a39cf46a3784a22e0319aa213551d7f4b2c）：list_empty 比较 next 与本节点，list_del 摘除后写入毒化，list_del_init 摘除后恢复自环。这里是类型回调的前置状态解释，不增加一套链表实现讲解，也不改变既有 kref 函数。

C 夹具使用六个固定函数体：INIT_LIST_HEAD、__list_del、__list_del_entry、list_del、list_del_init、list_empty。合法单节点拓扑手动建立，READ/WRITE、有效性检查和毒化地址为显式顺序替身；观察初始化、挂入、普通摘除毒化、摘除后初始化四态符合预期。未验证真实并发、内核毒化陷阱、CONFIG_LIST_HARDENED 或完整链表算法。

## 1.56\_工作实例与归还责任的观察边界

B04m 将已有固定 workqueue 的取消/等待契约与普通 kref 归还分层，实际源码仍沿前述官方提交。正文[工作票据](../../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#%287%29_所有权表要补充失败路径和取消路径)不是取消实现镜像：仅模拟单次实例、无重新投递、管理者取消期间仍持份额的协议。严格 C11 六条轨迹均归还完整；pending 取消由调用者接管那一实例的责任，已完成或执行中等待完成不盲目增加一次 put。未执行真实取消、重排竞态、目标模块、原子或分配器验证。

## 1.57\_单槽业务关闭的验证边界

B04n 的[完整关闭模块](../../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.16_一个完整的生命周期模板)继续使用本基线普通 kref 与互斥锁接口，非新的上游实现。ARM 前端纳入 354 份头文件，342 份非生成源码相对固定提交无差异；生成头仍来自当前配置。宿主八组顺序控制检查复用固定普通引用函数，显式替换原子、锁和分配器；errno 常量 ESHUTDOWN 只读取固定 include/uapi/asm-generic/errno.h。目标构建链接、装卸、真实并发和硬件资源退出未执行。

## 1.58\_三条规则的固定文档上下文

B04o 只读核对固定 Documentation/core-api/kref.rst 全文（blob c61eea6f1bf2bd76490718430130fe3501e2f7e8），将三条规则、直接转交优化、同锁查找/归零及条件取得示例放回各自协议。入口见[调用者模块](../kref/navigation/P02_普通引用与归零回调导读.md#2.10_三条规则与两类查找协议)。未新增上游函数体展开，条件接口的精确实现仍待后续单元。重编运行现有责任槽 C 程序和六安排/两分配失败夹具，重跑容器及关闭控制夹具；没有新增目标装卸或并发证据。

## 1.59\_条件取得链的固定分支

B04p 核对本基线 include/linux/kref.h 与 include/linux/refcount.h 中 kref_get_unless_zero、refcount_inc_not_zero、__refcount_inc_not_zero、__refcount_add_not_zero 四函数，blob 沿前文固定值，首次唯一展开集中在[条件模块](../kref/navigation/P03_条件取得与查找窗口导读.md#3.2_从观察到自己持有)关联的实现标题。宿主夹具复用既有普通函数及饱和处理，六类 helper 状态、oldp 和两类 kref 结果通过；异常饱和也可返回非零。条件模型仅验证 C11 确定性比较过程，不替代目标并发、发布读取或 ARM 内存序证据。

## 1.60\_最后归还锁的固定顺序

B04q 核对固定 lib/refcount.c 的 refcount_dec_not_one、refcount_dec_and_mutex_lock、refcount_dec_and_lock，以及 kref.h 两个包装，沿既有固定 blob；[模块入口](../kref/navigation/P04_最后归还与锁交接导读.md#4.2_把最后减少留在锁内)连接唯一实现。不是锁外归零后补锁，而是保留最后候选份额、取锁再减少判断。宿主六模块组及十分支通过，ARM 前端及 342 份非生成头差异核验通过；未执行目标装卸、IRQ/RT 或实际等待。

辅助构建核对 scripts/Makefile.extrawarn，blob dc081cf46d211c86c1eb725368e04129befd7a9c：W=3 分支启用 sign-compare，其他分支关闭由 Wextra 引入的该告警。本夹具明确使用 Wno-sign-compare 和既有 fno-strict-overflow，不以修改固定函数主体规避诊断；这不是完整 Kbuild 运行结果。

## 1.61\_管理者等待借用退出的组合边界

B04s 再核对固定 kernel/workqueue.c 中 cancel_work_sync 的注释及既有唯一实现，blob a9d64e08dffc7c7aef2caae2f170268d83e99e23；无竞争投递是返回后不再 pending/执行的前提。[P06 组合模块](../kref/navigation/P02_普通引用与归零回调导读.md#2.11_借用退出与最后归还)不新增上游函数展开。新完整模块 ARM 前端通过，354 份头中 342 份非生成源码相对本基线无差异，生成头仍来自工作配置。宿主九组顺序替身检查验证分配退出、管理者保留和拒绝路径，未验证目标装卸、真实阻塞与硬件内存序。

## 1.62\_定时器改期与最终退出证据

B04u 核对固定 kernel/time/timer.c（blob 7835f9b376e76a010926c3c2036c9a458b1f553c）与 include/linux/timer.h（blob e67ecd1cbc97d6b92994c15b688cdde5ec3c998f）。[模块入口](../kref/navigation/P05_定时器重启与退出导读.md#5.2_从排队到最终关闭)关联八个唯一函数展开，函数体去注释规范化一致。mod_timer 对既有 pending 的改期不追加一次接收票据；shutdown 清空 function，与普通同步删除的再启动保证不同。

宿主实际六函数配合显式 base/锁/运行退出替身，八种删除/关闭 × pending × running 组合及旧名包装通过；无真实计时、中断、SMP/RT、LOCKDEP、调度或内存序验证。独立 C 模型四条轨迹通过，只展示外层责任，不充当定时轮实现。未编造目标运行或修改外部工作树。

## 1.63\_完成通知与引用退出的组合边界

B04x 核对固定 kernel/sched/completion.c（blob 3561ab533dd4e33ddb5284bcab51736f9b9ab6bf）的 complete 与等待主干、以及既有 workqueue 排队/取消契约。[组合导读](../kref/navigation/P02_普通引用与归零回调导读.md#2.13_完成事件不消费引用)连接已有唯一实现，不新展开同一函数。新模块 ARM 前端通过，354 份头中 342 份非生成源码与固定提交一致；生成头仍沿工作配置。宿主七组仅验证实际模块控制和普通引用结算，完成量/队列为显式顺序替身，未执行真实计时等待、目标运行或内存序。

## 1.64\_队列转交与共享引用的应用边界

B04y 的[双协议模块](../kref/navigation/P02_普通引用与归零回调导读.md#2.14_队列责任沿同一份移动)复用固定普通 kref、mutex 和 workqueue 契约，没有新增上游实现。ARM 前端通过，354 份头中的 342 份非生成源码相对本基线无差异；宿主十组检查覆盖实际模块控制及拒绝/提前执行，底层仍为显式顺序替身。目标运行、真实锁竞争、多队列和内存序未执行。

## 1.65\_整数索引的发布与取得窗口

B04ab 按固定提交核对 include/linux/xarray.h（blob 0b618ec04115fc3993bf33a7c358632bef170fc9）、lib/xarray.c（32d4bac8c94ca13e11f350c6bcfcacc2040d0359）、lib/idr.c（da36054c3ca02058dcfa3338c712ba664b79b13a）。[模块入口](../kref/navigation/P06_整数索引与拥有型查找导读.md#6.2_把容器动作接到引用周期)关联八个唯一函数展开；XArray 插入/删除自行加锁，查询返回前结束内部 RCU；IDR 编号初始化需区分局部结果与对象字段。

XArray 应用模块及哈希/IDR 应用片段 ARM 前端通过，356 份头中的 344 份非生成源码与本基线一致。宿主六组执行实际应用及三个固定 XArray 外层函数，底层存储、同步、原子和分配为顺序替身；IDR/哈希不宣称运行测试。目标链接、装卸、真实容器节点分配、并发和内存序未执行。

## 1.66\_双锁链表服务的应用验证

B04ai 的[完整服务模块](../kref/navigation/P02_普通引用与归零回调导读.md#2.9_停止业务的外层状态)复用固定普通引用链与 list.h 六个辅助函数，没有新增唯一上游实现。ARM 前端通过，354 份头中的 342 份非生成源码与本基线无差异；生成头仍沿工作配置。宿主六组顺序检查验证实际模块的所有权、锁顺序和拒绝路径，不证明真实并发、目标运行或内存序。

## 1.67\_RCU旧链路径与回调退出

B04ak 核对固定 include/linux/rculist.h（blob 14dfa6008467e803d57f98cfa0275569f1c6a181）的 list_del_rcu，唯一函数体去注释规范化后与固定对象一致。[组合导读](../kref/navigation/P03_条件取得与查找窗口导读.md#3.7_从旧节点继续到最终回调)关联它与既有 Tiny rcu_barrier 实现；函数摘链保留 next，本身不归还引用或等待 GP。

完整模块与可选查找过滤 ARM 前端通过，355 份头中 343 份非生成源码无固定提交差异；生成配置仍属工作环境。宿主八组验证应用控制、固定引用链及摘链函数；RCU/锁/原子等为显式顺序替身，未执行目标运行、真实 GP 或内存序。

## 1.68\_kobject身份与类型清理证据

B04an 核对固定 include/linux/kobject.h（blob c8219505a79f98bc370e52997efc8af51833cfda）、lib/kobject.c（72fa20f405f1520a63dd50d9aa37f6609306eb3e）及 Documentation/core-api/kobject.rst。[模块入口](../kref/navigation/P07_kobject身份与类型清理导读.md#7.2_从K0到K5连接状态与回调)关联两种结构定义与九个唯一函数，去注释规范化与固定Git对象一致。del撤下层次并归还父责任，不消费本对象初始份额；init_and_add失败仍须put；DEBUG_KOBJECT_RELEASE可能延迟类型清理。

完整模块ARM前端通过，354份头中342份非生成源码无固定提交差异。宿主七组执行固定普通清理链，sysfs/命名添加等为替身；模块明确拒绝无SYSFS或启用延迟调试释放配置。未执行目标装卸、真实sysfs/事件、并发和延迟清理分支。

## 1.69\_设备引用与解绑资源的不同边界

B04ao核对固定drivers/base/core.c（blob ec0ef6a0de942742215862206ea2aee8a65199b7）和drivers/base/dd.c（bcc1f28b71f4f554ec8cf279934d13bdc6acce9c）。[模块入口](../kref/navigation/P08_device引用与资源退出导读.md#8.2_从D0到D5区分登记与存储)关联五个core函数与device_unbind_cleanup，主体规范化相同。unregister先删除再put初始份额；最终release按dev/type/class选择一个；解绑受管资源不以设备引用归零为前提。

ARM前端通过，372份头中360份非生成源码无固定提交差异。宿主八组使用固定引用/分派包装，初始化/添加删除/devres等为显式替身；device_unbind_cleanup仅源码核对。未执行目标装卸、实际解绑、完整资源框架、事件或并发。

## 1.70\_分类与总线的公共描述及内部引用

B04ap核对固定drivers/base/class.c（blob ce460e1ab1376d785d5386477ae3c91e47df4686）六个函数及drivers/base/bus.c（657c93c38b0dc2a2247e5f482fadd3a9376a58e8）两个函数，并对照base.h/class.h/bus.h声明。[模块入口](../kref/navigation/P08_device引用与资源退出导读.md#8.5_分类与总线的公共描述及内部份额)区分公共class/bus_type与内部subsys_private，配对临时查找与登记份额。源码主体规范化核对，不是class/bus运行验证；未执行注册、sysfs、事件、实际退出或并发。

## 1.71\_会话桥接使用既有设备引用实现

B04aq的[会话模块](../kref/navigation/P08_device引用与资源退出导读.md#8.6_私有会话连接设备份额)复用固定device/kobject/普通引用函数，不新增上游实现副本。十组宿主检查采用明确的锁、原子操作、分配和设备环境替身；ARM编译前端通过，372份头中360份非生成源码相对固定提交无差异。生成配置不作为不可变源码证据，未执行目标链接、模块装卸、实际设备解绑或并发。

## 1.72\_责任日志与动态诊断的证据边界

B04as使用官方固定提交dfaf2136deb2af2e60b994421281ba42f1c087e0的四份文档，关联[诊断证据模块](../kref/navigation/P02_普通引用与归零回调导读.md#2.16_异常报告与检查覆盖)：

| 上游相对位置 | blob |
| --- | --- |
| Documentation/dev-tools/kasan.rst | d7de44f5339d43aee128091930ead29511060925 |
| Documentation/dev-tools/kcsan.rst | d81c42d1063eab5db0cba1786de287406ca3ebe7 |
| Documentation/dev-tools/kmemleak.rst | 2cb00b53339fe9830a41867becc9beb4650f216a |
| Documentation/locking/lockdep-design.rst | 56b90eea27312e0a438260eb10425e811f154c9a |

本次只读工作树.config启用PROVE_LOCKING/LOCKDEP，KASAN/DEBUG_KMEMLEAK未启用，未见KCSAN启用；该快照不证明运行镜像及运行时检查器状态。离线C账本六条轨迹和两个练习变体通过，未运行KASAN、KCSAN、kmemleak或Lockdep故障场景，未改外部内核配置。

## 1.73\_工作执行与文件最终清理的责任边界

B04at只读核对固定提交的投递pending/禁用分支、执行前清pending、文件描述符复制及最终release调用位置；[模块入口](../kref/navigation/P02_普通引用与归零回调导读.md#2.17_工作与文件份额的诊断落点)区分应用对象引用和工作/文件框架状态。

| 上游相对位置 | blob |
| --- | --- |
| include/linux/workqueue.h | 59c2695e12e7674d5bc4cdd1fb416074374f981c |
| kernel/workqueue.c | a9d64e08dffc7c7aef2caae2f170268d83e99e23 |
| fs/file_table.c | 18735dc8269a10d6e7085c9bb882a0788dd3e0ca |
| fs/file.c | 4cb952541dd036a57177b3ce29587cdffa186aaa |

正文文件回调片段的三组宿主夹具使用固定普通引用函数通过，file/inode和最终分配清理为替身；未执行VFS/dup/close、真实work调度、目标装卸或并发。既有完整工作模块保持原样。

## 1.74\_分层创建模板的编译与回滚边界

B04ba复核NXP官方来源、lf-6.12.y、发布标签解引用dfaf2136及Linux6.12.20；访问工作树HEAD仍为7b60e547，三个本地实验提交不作证据。当前ARM/Tiny RCU/PREEMPT_NONE非SMP配置用于前端检查，不外推到运行镜像。

[note_kref_create创建模块](../kref/navigation/P02_普通引用与归零回调导读.md#2.21_创建阶段与单一清理入口)ARM前端通过，354份使用头文件中的342份非生成源码相对固定提交无差异。宿主保留既有普通引用函数，八例核对阶段失败、分配失败和成功清理；分配器、模块注册、错误指针及原子操作为宿主替身，未执行目标链接装卸、实际并发或内存耗尽。本批不新增或搬移上游函数体。

## 1.75\_拥有型哈希模板的上下文边界

B04bc的[note_kref_hash模板](../kref/navigation/P03_条件取得与查找窗口导读.md#3.11_哈希索引与IRQ上下文的独立边界)使用本次ARM、CPU_V7、非SMP、非PREEMPT_RT配置；DEBUG_SPINLOCK、DEBUG_LOCK_ALLOC和TRACE_IRQFLAGS启用。只读核对固定dfaf2136的include/linux/spinlock.h包装、arch/arm/include/asm/irqflags.h中ARMv6及以上保存/屏蔽/恢复普通IRQ路径，不外推到Cortex-M、FIQ/NMI或锁专题另行研究的SMP运行配置。

ARM前端通过，354份头中342份非生成源码与固定提交无差异。七组宿主应用检查保留固定普通引用函数，哈希、链表、IRQ状态、锁与分配环境为顺序替身；没有目标链接装卸、真实中断、并发、实时性测量或硬件验证。上游函数体仍沿既有锁实现页和源码副本，不新增重复展开。

## 1.76\_期待对象删除的锁窗口验证

B04bd沿固定dfaf2136既有lib/xarray.c的xa_load、xa_erase、__xa_erase实现，复核[同锁身份比较与移除](../kref/navigation/P06_整数索引与拥有型查找导读.md#6.4_删除当前条目与删除期待对象)。正式note_kref_xarray未改，可选应用包装不引入新上游源码体。

包含包装的模块变体ARM前端通过，354头中342非生成源码相对固定提交无差异；三项新增及六项既有宿主组通过。宿主普通引用和公开包装固定，下层节点、删除、锁及RCU为替身，不证明真实算法、编号复用的并发压力、目标链接装卸或多次发布代际。

## 1.77\_工作交付模式的编译与顺序验证

B04bf的[note_kref_work_modes](../kref/navigation/P02_普通引用与归零回调导读.md#2.22_工作交付的份额与关闭窗口)继续采用官方固定dfaf2136普通引用及工作队列证据。ARM前端通过，354头中342非生成源码与固定提交无差异。

两种交付模式各五例宿主检查覆盖队列/对象分配失败、拒绝、延后执行和投递返回前完成；分配器与工作调度为顺序替身。既有管理者借用模块、work票据和固定投递/执行/取消源码用于协议复核，未新增上游函数体。未验证真实工作线程并发、代码卸载竞争、目标链接装卸或硬件。

## 1.78\_单次定时器模块的验证边界

B04bg的[单次定时器模板](../kref/navigation/P05_定时器重启与退出导读.md#5.4_一次请求的独立份额与最终关闭)沿用官方固定dfaf2136普通引用、timer改期/删除/shutdown及旧名包装证据；没有新增或复制上游函数体。新增完整模块ARM前端通过，354头中342非生成源码与固定提交无差异。

七组宿主协议检查覆盖分配失败、取消、执行、关闭观察前显式推进回调、重复启动/关闭、IRQ状态恢复和关闭后拒绝。定时器与锁是顺序替身，未验证真实定时中断、等待实现、目标模块链接装卸、竞争退出或硬件。当前生成头配置只用于前端，不替代固定源码身份。

## 1.79\_完成事件模板的既有证据复核

B04bh的[P22工程契约](../kref/navigation/P02_普通引用与归零回调导读.md#2.13_完成事件不消费引用)复用官方固定dfaf2136完成量与工作取消证据，以及既有完整note_kref_completion程序。普通等待、广播与reinit的唯一函数标题保持不变；未增加上游实现副本。

完整模块与七组宿主夹具程序部分逐字一致，既有检查涵盖分配/投递失败、早完成、等待中完成、取消和迟到完成；既有ARM前端354头中342非生成源码无固定提交差异。本批是正文契约与导航修正，未重新执行程序或ARM，不将历史检查写成新运行；目标装卸、真实等待/并发和硬件内存序仍未验证。

## 1.80\_回滚模型与固定源码的分界

B04bi的[资源回滚单元](../kref/navigation/P02_普通引用与归零回调导读.md#2.23_分阶段失败的清理责任)使用独立C11程序验证十二条顺序资源责任路径。该模型refs不是Linux kref，channel不是硬件句柄；不产生新的内核源码或配置结论。

P16完整内核创建模块、官方固定dfaf2136普通初始化/最后归还实现和既有验证保留。本批未重跑ARM或目标程序，也未验证真实注册API、硬件停止、异步排空或并发读者；新增C模型只验证所列分配、清理依赖与份额配对。

## 1.81\_删除排空模板的组合边界

B04bj的[删除排空组合](../kref/navigation/P02_普通引用与归零回调导读.md#2.11_借用退出与最后归还)复用固定dfaf2136工作取消与普通归还证据，以及完整note_kref_owned_work材料。程序与九组既有宿主夹具逐字核对，既有ARM前端354头/342非生成源码差异记录保留。

本批未修改运行程序，未重跑宿主/ARM/目标验证；未验证真实阻塞、硬件停止、DMA、中断或并发关闭者。新增业务在途登记描述是扩展时须满足的协议条件，不作为已经运行的硬件排空实现。原上游唯一实现标题不变。

## 1.82\_RCU工程模板的既有验证复核

B04bk的[RCU工程组合](../kref/navigation/P03_条件取得与查找窗口导读.md#3.7_从旧节点继续到最终回调)复用官方固定dfaf2136条件取得、list_del_rcu与Tiny回调退出证据，完整note_kref_rcu不变。程序与既有八组夹具逐字核对，原ARM记录为355头、343非生成源码差异为空，不混用其他模块的354/342计数。

本批仅重构应用契约与入口，未重跑宿主/ARM/目标验证，未验证真实RCU后端调度、硬件并发或内存序。归零、宽限期、模块回调退出和业务关闭依然是不同义务，不新增上游实现副本。

## 1.83\_单槽弱缓存模块的验证边界

B04bl新增[note_kref_weak_cache](../kref/navigation/P03_条件取得与查找窗口导读.md#3.12_单个弱缓存槽的撤销与回收)，普通/条件引用证据仍为官方固定dfaf2136。再次只读核对远端身份、lf-6.12.y和发布标签固定提交；工作HEAD的三笔实验提交未作为证据。当前ARM/CPU_V7、TINY_RCU、PREEMPT_NONE配置用于前端，354头中342非生成源码相对固定提交diff为空。

九组宿主检查覆盖分配失败、完整模块、空槽、两种取得/归零顺序、替换、清空再发布、IRQ状态保持及旧回调不影响新槽。RCU、锁和原子是顺序替身，未验证真实GP调度、并发发布、目标链接装卸、动态holder/多槽协议或硬件内存序。模块静态单槽及不可变value的边界不能外推为任意弱引用实现。

## 1.84\_父子桥接模块的验证边界

B04bm新增[父子桥接模块](../kref/navigation/P02_普通引用与归零回调导读.md#2.24_父子桥接与非拥有节点)，普通引用与链表删除辅助仍采用官方固定dfaf2136证据。ARM前端通过，354头中342非生成源码相对固定提交diff为空；未新增上游函数体。

八组宿主检查覆盖两处分配失败、完整模块、关闭前后创建、分配时关门、附着节点最终释放、多child及多用户、重复摘链。锁、分配、插入为顺序替身；未执行真实并发分配、mutex阻塞、目标装卸、硬中断或硬件内存序。parent_close只对本模块同步业务建立关门保证，不验证驱动硬件排空。

## 1.85\_文件持有模板与打开交付边界

B04bn新增[文件实例持有模块](../kref/navigation/P02_普通引用与归零回调导读.md#2.25_文件实例的候选与交付)，并以官方固定dfaf2136的fs/open.c核对do_dentry_open回调、FMODE_OPENED写入和后续O_DIRECT拒绝，fs/file_table.c确认未打开与已打开清理分支。实现单一展开见[打开交付](../character_device/source_explanations/fs/open.c.md#1.3_打开回调失败与交付边界)。ARM前端450头中437非生成源码相对固定提交diff为空。

八组宿主C协议覆盖分配/注册失败、空入口、候选后失败、完整用户流程、别名、未知命令、重复关门、兼容回调及独立打开；VFS、系统调用、锁与注册为顺序替身。未执行目标模块链接装卸、真实Linux用户程序、并发或硬件；生成头仍来自工作树配置，本地实验提交不作为发布源码证据。

## 1.86\_引用封装模块的验证边界

B04bo新增[引用封装模块](../kref/navigation/P02_普通引用与归零回调导读.md#2.26_封装不改变原语前提)，仍使用官方dfaf2136普通引用链，不增加重复实现证据。ARM前端354头中342非生成源码与固定提交diff为空；六组宿主C协议验证日志开关不改变责任、宏单次求值、创建失败和拥有槽归还。

分配和原子操作为顺序替身；未执行目标链接装卸、并发槽位交换、真实跟踪器及硬件验证。示例只用显式函数名/行号标记宏展开位置，不把编译器返回地址或输出顺序视为稳定拥有身份。

## 1.87\_状态接纳模型的证据边界

B04bp新增[P30状态与活动接纳](../kref/navigation/P02_普通引用与归零回调导读.md#2.27_业务状态与引用原语的分工)，仅增加应用层独立C11顺序模型。十条宿主路径覆盖状态快照、活动登记、关闭汇聚和最后回收；refs与通知记录不冒充Linux原语。

上游仍链接官方固定dfaf2136的既有kref与完成事件实现，无新增函数体或版本结论。P24工作关闭与P28文件短操作模块保持不变，未新执行ARM前端、目标装卸、真实锁/唤醒或硬件；模型不证明多线程可直接运行。

## 1.88\_工程契约收束的证据范围

B04bq完成[P31接口与退出查询](../kref/navigation/P02_普通引用与归零回调导读.md#2.28_契约与退出不能由断言代替)与P13工程总表收束。只对照已有普通引用、工作、定时器、完成与RCU证据；未新增上游函数体或改变官方固定dfaf2136边界。

已有P20/P23/P24/P25/P27完整材料未改，未重复运行历史实验。新检查是契约与真实实现对照、正文冷读及导航校验；没有新的宿主协议、ARM编译、目标装卸、真实并发或硬件结果，不能把查询页的声明示例当成已经实现的新接口。

## 1.89\_基础引用实验的身份与执行边界

B04bs重新只读核对官方远端身份、lf-6.12.y分支、固定标签和Makefile版本：标签仍为dfaf2136、Linux6.12.20，本地HEAD含实验提交而不作为证据。工作树配置仍为TINY_RCU/PREEMPT_NONE、无SMP，不代表实际运行镜像。

新增[P32基础实验](../kref/navigation/P02_普通引用与归零回调导读.md#2.29_基础实验的返回值与回调位置)，完整模块六条宿主协议覆盖一份/两份、各自分配失败及两种非法参数，分配和原子为顺序替身。ARM前端354头中342非生成源码对固定提交无差异。未目标链接装卸、真实dmesg、两线程并发或硬件内存序；未增加上游函数体或修改外部树。

## 1.90\_缺失引用实验的证据分层

B04bt新增[P33责任错误实验](../kref/navigation/P02_普通引用与归零回调导读.md#2.30_错误轨迹与实际引用的证据层次)，复用已有ownership_audit原六条输入并对同一实现增加五条检查轨迹，十一条宿主C顺序结果通过。

没有实际分配释放被审查对象、执行Linux原语或注入内核UAF/泄漏。既有工作模块与固定dfaf2136证据未变，本批未新增ARM、目标装卸、真实竞争、KASAN或硬件结果；complete/closed只是外部证据前提，检查器不验证它们是否成立。

## 1.91\_查找实验与条件失败前提

B04bu沿固定dfaf2136的Documentation/core-api/kref.rst及既有条件唯一实现核对[查找实验](../kref/navigation/P03_条件取得与查找窗口导读.md#3.13_查找实验必须先区分两种表责任)。已有note_kref_table六组宿主夹具、conditional_take四路径、固定条件函数六分支与包装两结果重新通过；模块、C模型和源码证据未变。

顺序替身与C11显式插入干扰只验证声明的路径，不建立真实mutex、调度、内存序或地址回收结论。本批没有新ARM、目标装卸、动态检查与硬件结果；本地实验提交继续不作源码基线。

## 1.92\_工作交付实验的顺序证据

B04bv新增[P35工作实验导读](../kref/navigation/P02_普通引用与归零回调导读.md#2.31_工作实验的责任和实际投递)，既有work_ticket六路径、note_kref_work_modes十例、note_kref_owned_work九组宿主重跑通过。程序、固定dfaf2136引用与工作队列证据未变；拒绝全新单次work由夹具注入，队列/原子/取消为顺序替身。

25条或组检查不能证明真实调度、内存序、模块退出竞争或硬件。本批没有新增ARM、目标装卸与动态检查结果，目标命令明确标为待运行步骤。

## 1.93\_退休顺序实验与固定证据

B04bw新增[P36退休实验导读](../kref/navigation/P03_条件取得与查找窗口导读.md#3.14_退休实验中的归零和回收完成)，四条C模型和八组实际模块宿主夹具重新通过，源码材料未改。重新只读核对官方远端身份、lf-6.12.y及实验HEAD 7b60e547，发布标签仍解引用dfaf2136，版本6.12.20；kref.rst工作树、仓库副本与固定对象同为blob c61eea6f1bf2bd76490718430130fe3501e2f7e8。

配置仍ARM、TINY_RCU、PREEMPT_NONE，未启用SMP与KASAN，PROVE_LOCKING开启；这些是构建工作树，不是运行内核证据。夹具显式安排交错与回调成熟，不证明真实RCU后端、内存序或模块卸载。本批未新ARM、目标装卸和动态检查，不能将模型finish_gp合并回收的简化推广为真实回调完成契约。

## 1.94\_GenericKASAN实验配置证据

B04bx新增[P37诊断导读](../kref/navigation/P07_引用错误的动态诊断导读.md#7.1_固定来源与阅读任务)，保存固定dfaf2136的[lib/Kconfig.kasan](lib/Kconfig.kasan)（blob 98016e137b7f09f82b168f840565e8121b28ce87）与[Documentation/dev-tools/kasan.rst](Documentation/dev-tools/kasan.rst)（blob d7de44f5339d43aee128091930ead29511060925），原文不改。三种模式为choice，标签模式属于ARM64，当前ARM实验选择Generic。

工作树HAVE_ARCH_KASAN、CC_HAS_KASAN_GENERIC、CC_HAS_WORKING_NOSANITIZE_ADDRESS、SYSFS、STACKTRACE为y，SLUB_TINY与KASAN未启用。新note_kref_kasan默认正确路径，故障参数要求构建Generic；宿主两个配置组合合计五条正确/失败/拒绝路径通过，没有执行故意UAF。当前未启用KASAN配置的ARM前端354头/342非生成源码与固定提交无差异，不证明启用插桩后的编译链接或运行。

未修改外部树、配置或运行镜像；目标故障装卸、实际报告、检查覆盖与硬件未执行。默认首错和panic策略按固定文档解释，不把预期报告当作验证记录。

## 1.95\_字段竞争实验与KCSAN配置

B04by保存固定dfaf2136的[lib/Kconfig.kcsan](lib/Kconfig.kcsan)（blob 609ddfc73de5d47a31060420ecb5a810610664b3）和[Documentation/dev-tools/kcsan.rst](Documentation/dev-tools/kcsan.rst)（blob d81c42d1063eab5db0cba1786de287406ca3ebe7）。[诊断导读](../kref/navigation/P07_引用错误的动态诊断导读.md#7.5_KCSAN配置互斥与字段证据)记录HAVE_ARCH/编译器前提及!KASAN互斥，固定arch/arm未选择HAVE_ARCH_KCSAN，当前ARM配置不是检测环境。

新增counter_updates.cpp三个真实宿主双线程对照通过；note_kref_counter十二组顺序夹具覆盖两正确模式及清理，故意竞争分支未执行。当前ARM前端354头/342非生成源码固定差异为空，不证明KCSAN启用构建、目标链接装卸、实际报告或内存序结论。线程创建失败清理已冷读，未注入C++线程创建失败。

## 1.96\_最后引用与锁依赖实验

B04bz只读核对固定dfaf2136的kernel/locking/mutex.c、kernel/locking/lockdep.c与lib/Kconfig.debug，当前候选源码无差异。[诊断导读](../kref/navigation/P07_引用错误的动态诊断导读.md#7.6_最后归还与锁依赖证据)沿已有Lockdep事件、图与配置实现，未新增重复引擎讲解。新模块十四组宿主引用/锁顺序协议通过；ARM前端354头/342非生成源码固定差异为空。两锁历史替身不代表真实检查器，目标装卸、报告和故障恢复未执行。

## 1.97\_误用窗口与引用异常实验

B04ca沿固定dfaf2136普通引用与异常收敛实现，三条宿主对照区分正值归零、有效栈存储的零后减少和读取快照；原子为顺序替身，未访问已释放地址。[诊断导读](../kref/navigation/P07_引用错误的动态诊断导读.md#7.7_额外归还与快照的检测界限)关联唯一实现。另五条C责任/窗口模型与六组当前note_kref_table宿主协议通过；没有新增ARM构建或实际内核故障结果。

## 1.98\_实验路线的状态与父子收束

B04cb沿既有固定NXP引用原语及P27/P30证据，重编重跑十条state_ownership路径与八组父子宿主协议。未新增上游原文或版本结论，未新做ARM构建或目标运行。[收束导读](../kref/navigation/P02_普通引用与归零回调导读.md#2.32_实验路线的状态与父子回收)记录普通旧份额、活动登记与父桥接的不同责任；P14后半记录模板不再把预期KASAN输出填成观察结果。

## 1.99\_基础验收的接口前提

B04cc沿固定dfaf2136既有kref普通函数核对基础六题，复核当前note_kref_basics与夹具一致后重编六组宿主检查。没有新增源码版本结论、ARM构建或目标运行；[验收导读](../kref/navigation/P02_普通引用与归零回调导读.md#2.33_基础验收的前提与反例)区分地址保护、正计数、初始份额交付与归还后剩余资格。

## 1.100\_组合场景验收

B04cd沿固定dfaf2136既有引用/链表/RCU证据，对照当前三个完整模块并重编工作十组、表六组、RCU八组宿主检查。[窗口导读](../kref/navigation/P03_条件取得与查找窗口导读.md#3.15_组合场景验收的地址与归还窗口)记录更新者独立份额、旧读者和回调回收边界。无新上游摘录、ARM构建、目标装卸或真实并发结论。

## 1.101\_框架与工程验收边界

B04ce沿固定dfaf2136既有kobject/device与清理证据，复核当前设备八组、kobject七组宿主夹具并重编通过，另责任账本六轨迹通过。[框架验收导读](../kref/navigation/P08_device引用与资源退出导读.md#8.8_最终验收中的框架责任)保留公共描述与内部对象区别。未新增上游摘录、ARM构建、真实绑定、sysfs或目标运行结论。

## 1.102\_最终模板验收收束

B04cf复用固定dfaf2136已有引用与表协议证据，六组当前note_kref_table宿主检查重编通过。[收束导读](../kref/navigation/P02_普通引用与归零回调导读.md#2.34_最终验收回到实际表模块)记录初始/表/读者责任，不新增上游摘录、ARM构建或目标运行结论。

## 1.103\_devres核心分组与回滚

B04ch重新只读核对官方来源、标签dfaf2136、6.12.20与ARM配置；本地HEAD实验提交不作证据。既有drivers/base/devres.c、dd.c和include/linux/device.h与固定对象规范换行后相同，Documentation/driver-api/driver-model/devres.rst只读核对。新增[devres固定索引](../devres/navigation/P01_Linux_6.12_devres源码阅读索引.md#1.2_按问题进入实现)连接分组、登记及reset包装的唯一实现。六条宿主C平面模型不替代真实锁、嵌套或目标解绑；未新增硬件运行结论。

## 1.104\_devres内存映射与IRQ资源契约

B04ci按固定dfaf2136核对内存、映射、platform、GPIO和IRQ声明/包装/配置桩；新增[lib/devres.c原文](lib/devres.c)及[资源族导读](../devres/navigation/P03_内存映射与中断资源导读.md#3.1_内存失败与零大小)。kernel/irq/devres.c、manage.c，drivers/base/platform.c，drivers/gpio/gpiolib.h及相关声明头仅只读核对，已有GPIO与device头副本复用。两个映射包装11条宿主路径通过，14项公开接口ARM类型断言通过；397份包含头中384份非生成头与固定对象一致，13份生成头体现工作树配置。无Kbuild/MODPOST、真实映射、GPIO、中断或目标解绑验证。

## 1.105\_句柄启停与注册接口核对

B04cj新增[drivers/clk/clk-devres.c固定原文](drivers/clk/clk-devres.c)与[句柄启停导读](../devres/navigation/P04_句柄启停与注册契约导读.md#4.1_时钟把退出动作放进同一记录)。其余clk/regulator/reset/DMAengine/PHY/pinctrl/platform/LED/thermal相关头和实现按固定dfaf2136只读核对；thermal采用实际定义名称与指针返回，DMA不虚构devm公共入口。时钟六条宿主包装路径及21项ARM接口类型断言通过，558份头中544份非生成头与固定对象一致，14份生成配置；无硬件启停、PM、DMA、复位或实际注册/注销运行。

## 1.106\_devres退出依赖与设备发布顺序

B04ck重新只读核对官方远端、lf-6.12.y、固定标签dfaf2136和6.12.20；当前HEAD为三个本地实验提交后的7b60e547，不作证据。固定drivers/base/core.c、devtmpfs.c、dd.c与include/linux/platform_device.h用于核对add/探测、节点请求、清理入口及void remove类型，既有原文保留。[模块导读](../devres/navigation/P02_记录与分组清理导读.md#2.4_退出依赖与设备发布的不同证据)记录范围。当前ARM配置启用COMMON_CLK与DEVTMPFS，只说明配置存在，不证明目标时钟或/dev挂载运行；七条宿主C模型不作硬件结论。

## 1.107\_组合退出与私有桥接

B04cn沿固定device/kobject公开引用、device_unbind_cleanup与devres逆序记录证据组合T0～T6，具体入口见[设备导读](../kref/navigation/P08_device引用与资源退出导读.md#8.9_组合退出中的设备桥接与私有份额)。未新增函数展开或原文副本，私有ctx的门、活动与桥接属于应用设计。六条C宿主轨迹不等于Linux实现、真实同步、映射撤销或目标设备通过；继续保持固定dfaf2136而非本地实验HEAD为源码身份。

## 1.108\_并发先修与锁配置重核

B05a以固定dfaf2136的Documentation/locking/locktypes.rst核对锁类别、可睡任务上下文、本地约束与实时映射；入口见[锁规则导读](../locking/navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.8_执行路径先修与锁类别规则)。本轮只读.config确认为PREEMPT_NONE/TINY_RCU、非SMP，未启用owner spinning；纠正锁总索引、两篇模块导读和正文P04此前的当前SMP配置表述，不因此把其余正文标为整篇验收。交错程序是单线程抽象枚举，不是Linux锁、调度、中断或RT运行证据。

## 1.109\_消息发布入门证据

B05b只读固定dfaf2136的Documentation/memory-barriers.txt，并核对已保存rwonce、通用屏障与ARM屏障原文；[内存顺序导读](../memory_ordering/navigation/P01_Linux_6.12_LKMM_源码与模型导读.md#1.14_配套入口)关联M0～M3阅读任务。宿主C++17实验完成100轮独立发布，依据标准原子契约，不代表Linux内核宏、LKMM、SMP、目标ARM或实时分支运行；当前UP配置不变。

## 1.110\_编译器轮询反例校正

B05c只读固定dfaf2136的arch/arm/include/asm/vdso/processor.h，确认cpu_relax通常含barrier，特定架构/勘误分支更强；修复正文错误的无约束循环前提。已有rwonce与compiler证据继续使用固定源码；[七函数编译记录](../../../labs/kernel/memory_ordering/P01_READ_ONCE_编译器访问实验/expected/2026-09-24_compiler_access.md)仅覆盖两种x86-64目标的GCC/Clang O0/O2，不覆盖ARM执行、真实内核宏或KCSAN。

## 1.111\_屏障方向与UP配置边界

B05d核对固定include/asm-generic/barrier.h的CONFIG_SMP分支和ARM屏障映射，公共UP回退与底层__smp宏存在分开说明；[原语导读](../memory_ordering/navigation/P01_Linux_6.12_LKMM_源码与模型导读.md#1.3.2_通用屏障)继续承担入口。C单槽待写模型及单边约束变体严格编译执行，只说明教学规则下的结果，未执行herd7、Linux屏障、ARM SMP或设备协议。

## 1.112\_发布取得与反向归还

B05e沿固定通用release/acquire回退的类型和先后关系核对正文边界，删去说明性冒号伪C而保留职责表及[源码导读](../memory_ordering/navigation/P01_Linux_6.12_LKMM_源码与模型导读.md#1.3.2_通用屏障)。单槽一万轮宿主C++17发布/归还通过，标准原子一致性和严格单生产/单消费协议不冒充Linux实现；未运行herd7、目标ARM、取消、故障恢复或多参与者队列。

## 1.113\_依赖数据流与RCU取得边界

B05f只读固定Documentation/RCU/rcu_dereference.rst和include/linux/rcupdate.h，核对地址/数据依赖、普通判空与已知地址比较、READ_ONCE特例及check/protected前提；[RCU模块导读](../rcu/navigation/P02_Linux_6.12_RCU公共接口与读侧模型模块源码概念导读.md#2.9_唯一实现讲解入口)保留唯一实现入口。五函数C在GCC14.2和Clang18.1 x86-64 O2严格编译并观察：Clang替换固定地址比较后的访问来源，GCC本次仍保留指针寄存器；不以特定汇编替代Linux或ARM语义。未执行RCU、herd7或回收路径。

## 1.114\_原子更新与条件失败契约

B05g只读固定Documentation/atomic_t.txt全篇，核对非RMW、返回值、显式顺序、条件失败、atomic_set与RMW不可分性、期望值回写、辅助屏障范围及前进性；[阅读入口](../memory_ordering/navigation/P09_LKMM公理_锁关系与验证边界.md#9.5_官方文档证据)关联原始副本，其Git对象bee3b1bca9a7b46bcf9911f036c3280e77b4405a与固定提交一致。工作树HEAD仍是本地实验提交，不作为证据；当前配置仍UP、TINY_RCU和PREEMPT_NONE。宿主C++17严格编译后四工作者完成40,000次更新、错误0，同时单线程反例显示期望值重用会使OWNED→OWNED也成功。不声称执行Linux/ARM原子或herd7，不以宿主原子替代目标实现。

## 1.115\_隐式顺序与登记空窗

B05h只读固定Documentation/memory-barriers.txt的隐式屏障部分及include/linux/wait.h、include/linux/sched.h、include/linux/rcupdate.h、kernel/sched/wait.c相关路径；原始屏障文档对象93d58d9a428b879194ff03ef1aa1ca7c9e01fa14与固定提交一致。核对锁顺序、schedule完整屏障、wake_up条件保证与wake_up_process无条件保证、wait_event快查及登记后重查、task.__state和finish清理。固定wait.c普通扫描没有bookmark分段，已校正[模块边界](../waiting_notification/navigation/P02_Linux_6.12_普通等待队列模块源码概念导读.md#2.5_bookmark与长队列)及知识侧对应引用；保留分段方案为独立概念推演。C模型10条排列中先检查登记丢1条、先登记重检丢0条，只证立即可见、单次持久事件模型，不代表ARM/Linux或herd7执行。

## 1.116\_MP候选关系与模型回边

B05i按固定tools/memory-model/linux-kernel.cat核对po-rel/acq-po、ppo、prop、hb与pb及RCU/plain检查，原始副本对象adf3c4f412296269bb9f8127cd7e04f276479a57与固定提交一致。MP坏候选的fr不是直接hb边；经发布侧累积关系和flag读取来源合成同线程prop回边后进入hb。更新[版本导读的MP阅读](../memory_ordering/navigation/P09_LKMM公理_锁关系与验证边界.md#9.3_沿_MP_测试追踪一次判定)并区分Never预期与实际Observation。完整C只求这份已推导子图的可达闭包，四种后缀组合仅双侧出现环，不解析cat/不调用herd7；宿主PATH未发现herd7，本批未运行完整模型或目标硬件。

## 1.117\_模型工具与硬件生成器边界

B05j只读固定tools/memory-model/README，保存副本Git对象dab38904206a0ba0fea7ccd10469443fec1f396c与固定提交一致。首段herd7/klitmus7最低7.52不能替代后续klitmus目标兼容表；固定表对Linux5.17及之后列出7.56.1及以上，并保留未来版本兼容性不保证说明。[实验入口](../../../labs/kernel/memory_ordering/P02_LKMM_Litmus_消息传递与屏障/README.md#1.5_运行全部测试)已由Python改为Bash，八项预期移入TSV且语义不变，输入和五模型逐项摘要可定位每次使用内容。宿主仅静态检查与16项隔离驱动协议检查，未执行真实herd7、klitmus生成/构建/加载或外部源码树修改。

## 1.118\_跨轮复用与设备访问域

B05k冷读固定Documentation/memory-barriers.txt的dma屏障、一致性描述符、默认I/O映射readX/writeX及relaxed规则，并只读Documentation/driver-api/device-io.rst核对posted write与同设备读回边界。正文区分字段发布、下一轮覆盖资格、映射/缓存维护、门铃到达和设备使用完成；[诊断参考](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/memory_ordering/P10_子系统边界_误用诊断与选型.md#10.2_普通内存_MMIO_DMA_三种域)不规定未核对的具体设备寄存器或ARM指令。未执行MMIO/DMA、内核模块或herd7；共享槽位引用既有完整C++正例及其原验证，不运行有数据竞争的反例。

## 1.119\_单次访问与检测边界

B05l按固定dfaf2136的include/asm-generic/rwonce.h全文件与compiler_types.h类型/检测属性部分核对，rwonce原始副本对象8d0a6280e98247dc96450ed6a9e78fcb2e4fa555与官方固定提交一致。总索引迁入navigation并保持ID及标题锚点，新增[单次访问模块](../memory_ordering/navigation/P02_单次访问与类型边界导读.md#2.3_一条访问怎样闭合)和按上游路径保存的[唯一实现](../memory_ordering/source_explanations/include/asm-generic/rwonce.h.md#1.1_沿读取现场核对实现)。只读核对ARM无专属rwonce头及通用必选清单，不修改外部树；类型门槛、机器访问原子性、插桩配置与寿命分别说明。宿主检查采用固定宏/函数与受控依赖替身，仅验证尺寸、求值和访问值，不冒充真实KASAN/KCSAN、ARM或SMP验证；LKMM模型部分仍待审。

## 1.120\_公共SMP屏障与配置选择

B05m按固定dfaf2136核对include/asm-generic/barrier.h三种公共SMP屏障及内部回退、compiler.h的barrier定义；原始副本对象分别为d4f581c1e21da54f340bdfb8a4e846e59989e3aa和b15911e201bf95647dc89df8a61cfaa3144f68ca，均与固定提交一致。另只读include/linux/kcsan-checks.h核对弱内存与两类插桩接入，区分检测事件、功能顺序和空分支。[模块](../memory_ordering/navigation/P03_SMP屏障的配置与调用层次.md#3.3_一次公共屏障的路径)与[唯一实现](../memory_ordering/source_explanations/include/asm-generic/barrier.h.md#1.3_三种公共SMP屏障的配置分支)使用F0～F3追踪。八组配置/覆盖组合仅在宿主受控日志替身中验证，真实barrier汇编空函数只剩返回，均不证明ARM硬件顺序、真实KCSAN或LKMM运行。当前目标UP边界保持，release/acquire、atomic辅助及设备屏障不据本批标记完成。

## 1.121\_发布取得的类型与求值边界

B05n重新只读核对官方远端、lf-6.12.y分支、固定标签dfaf2136、6.12.20及UP/TINY_RCU/PREEMPT_NONE配置；HEAD仍是本地实验提交7b60e547，不纳入证据。按固定generic barrier与compiler_types核对七项发布取得定义，[模块](../memory_ordering/navigation/P04_发布取得的访问与配置导读.md#4.3_让实现回到同一组S阶段)保持知识正文S0～S3，唯一实现区分内部与公共路径。节选和固定完整头在同一依赖替身下，八组合均得到相同事件；另核对保存读值、读0返回、值参数在内部屏障后求值，以及long为4字节宿主的UP接纳/SMP拒绝8字节分支。结果只证明宏展开与受控调用语义，不是ARM、真实检测器、并发或LKMM执行。

## 1.122\_存储后屏障与RMW配对范围

B05o继续按固定dfaf2136的generic barrier核对九项存储后屏障和atomic辅助定义；atomic_t.txt的辅助契约明确限定RMW及夹入访问的范围。新增[模块](../memory_ordering/navigation/P05_存储后屏障与原子强化导读.md#5.3_按一条原子操作周期阅读)和[唯一实现](../memory_ordering/source_explanations/include/asm-generic/barrier.h.md#1.9_存储后屏障与atomic辅助的内部回退)。节选与固定完整头十组UP/SMP/公共与内部覆盖/空内部辅助得到相同调用记录，含一次左值和值副作用检查；RMW替身只是单线程加一，不具备也不声称原子性。未执行真实内核、ARM、并发、检测器或LKMM，当前UP配置和固定来源边界保持。

## 1.123\_条件加载的循环与退出样本

B05p按固定dfaf2136的generic barrier核对条件加载及控制依赖补强三项完整条件定义，并只读ARM vdso/processor.h核对cpu_relax通常为barrier、ARMv6/勘误分支另有屏障及nop。新增[模块](../memory_ordering/navigation/P06_条件加载与控制依赖导读.md#6.3_一轮轮询与一次成功退出)和[唯一实现](../memory_ordering/source_explanations/include/asm-generic/barrier.h.md#1.12_条件加载与控制依赖补强)，分开__PTR、VAL、_val及L0～L4。节选/固定头的即时满足、延后满足、一直不满足各配relaxed/acquire和默认/覆盖辅助，共二十四组受控执行；八组由夹具看门狗停止，不是宏超时。未执行真实并发、ARM、内核检测或LKMM，记录只证明展开与样本保存。

## 1.124\_ARM屏障映射与平台补充路径

B05q按固定dfaf2136核对arch/arm/include/asm/barrier.h（Git blob 83ae97c049d9bd48b474f0127164c71628bf05c0）及arch/arm/mm/flush.c（0749cf8a66371bb608f8bbd793c2e8302d56d989），新增[模块](../memory_ordering/navigation/P07_ARM屏障与配置边界导读.md#7.3_沿一条调用走完配置与运行路径)、[ARM唯一映射](../memory_ordering/source_explanations/arch/arm/include/asm/barrier.h.md#1.2_基础与DMA接口的配置选择)和[heavy函数](../memory_ordering/source_explanations/arch/arm/mm/flush.c.md#1.1_DSB之后还有什么)。当前工作树配置快照为CPU_V7/CPU_32v7、ARM_DMA_MEM_BUFFERABLE、ARM_HEAVY_MB开启，SMP和THUMB2_KERNEL关闭；不把配置快照当成固定提交唯一配置。三轴八组合分别用节选与固定头做十六次Clang ARMv7交叉编译，逐个检查八函数的DMB/DSB范围及heavy调用顺序；heavy函数十六组宿主C回调执行通过。依赖为受控替身，未执行完整内核构建、ARM板上访问或设备协议验证。

## 1.125\_模型输入与工具配置边界

B05r重新核对官方远端、来源分支、固定标签与Linux版本，外部HEAD仍为实验提交7b60e547，不用于证据。已保存linux-kernel.def和linux-kernel.cfg的Git对象分别为88a39601f52563b670876e3ef4950324dc66224a、3c8098e99f41dfe72cb75f4c276801ab2c855f83，与固定dfaf2136相同。冷读两文件及模型README工具要求，沿[完整MP输入](../memory_ordering/navigation/P08_LKMM输入_分类与工具配置.md#8.1.1_用一次发布示例核对地址和标签)区分指针/对象实参、四事件和结果谓词；[配置入口](../memory_ordering/navigation/P08_LKMM输入_分类与工具配置.md#8.3_linux_kernel_cfg_为什么要求正确工作目录)补足显示选项、工具库依赖及进程/Observation/清单三层结果。材料一致性为静态核对，未执行herd7或klitmus7；公理与锁模型继续单独审查。

## 1.126\_bell事件分类与读侧配对

B05s冷读固定linux-kernel.bell全文及cat消费分类的相关定义。已保存bell的Git对象ce068700939c559edc8c91b9a931a0feae82e159与官方dfaf2136一致；外部实验HEAD不参与结论。[事件分类单元](../memory_ordering/navigation/P08_LKMM输入_分类与工具配置.md#8.2.1_把指令种类和顺序标签分开)区分RMW类别与Accesses标签，沿三个读侧序列手算RCU配对，说明SRCU位置/数据流/值检查、Marked与Plain及本地依赖传递。没有执行herd7，手算不能替代工具求解或真实RCU进展验证；后续cat公理、锁模型与整体导读仍待审查。

## 1.127\_公理检查与锁候选关系

B05t按官方固定dfaf2136核对linux-kernel.cat（adf3c4f412296269bb9f8127cd7e04f276479a57）与lock.cat（03c12efed66a18d5f980ea03efbdba537503ad3b），仓库原文件Git对象相同。[公理入口](../memory_ordering/navigation/P09_LKMM公理_锁关系与验证边界.md#9.1.1_先问候选执行违反了哪一种约束)分开coherence、atomic、hb、pb与诊断，[锁入口](../memory_ordering/navigation/P09_LKMM公理_锁关系与验证边界.md#9.2.1_从取得成功与观察失败分开读)追踪取得读写、解锁、失败观察和临界区配对，再解释co/rf构造。补充cos-opt.cat工具库依赖。外部实验HEAD未用作证据，未运行herd7，关系推导不替代模型工具结果。

## 1.128\_内存顺序源码阅读模块收束

B05u将已核对的def/bell/cfg单元完整组织为[模型输入模块](../memory_ordering/navigation/P08_LKMM输入_分类与工具配置.md#8.1_linux_kernel_def_把原语翻译成事件)，cat/lock/MP与证据边界组织为[关系模块](../memory_ordering/navigation/P09_LKMM公理_锁关系与验证边界.md#9.1_linux_kernel_cat_怎样组织公理)。原十单元148行非标题内容迁移前后逐行保留，随后仅修正跨篇衔接和节号；没有新增源码版本或herd7运行结论。总索引保留真实阅读入口，知识与源码模块保持不同阅读职责。

## 1.129\_MMIO默认映射与送达完成边界

B05v按固定dfaf2136的Documentation/memory-barriers.txt中KERNEL I/O BARRIER EFFECTS核对五条普通访问器保证及relaxed削弱范围，并只读Documentation/driver-api/device-io.rst（Git blob 5c7e8194bef92bcb5c3c807852a3b21b04337728）。[MMIO正文](../../../knowledge/linux/io_model/mmio/P01_MMIO_访问顺序与屏障.md#1.3_普通访问器与_relaxed_访问器)明确默认映射、同外设、同锁跨CPU及一致性DMA前提；[四阶段C模型](../../../knowledge/linux/io_model/mmio/P01_MMIO_访问顺序与屏障.md#1.4.1_用完整C模型分开送达和完成)区分桥接收、送达、设备完成和驱动观察。GCC14.2与Clang18.1.8宿主严格C11 O2运行通过，不是Linux访问器或真实设备测试。

## 1.130\_DMA方向与完成前提

B05w只读核对固定dfaf2136的Documentation/core-api/dma-api-howto.rst（0bf31b6c4383c8b36d73f7361a6cdb87d1f5dba6）。[DMA交接单元](../../../knowledge/linux/io_model/dma/P01_DMA_映射同步与门铃顺序.md#1.5_streaming_映射的所有权状态机)沿D0～D5分开映射有效、访问资格与设备活动，修正FROM_DEVICE示例在映射区写元数据的错误；同步和unmap前仍须设备停止访问证据。固定文档的只读接收复用例允许无CPU写时省去for_device同步，不据此省略设备交还协议。未运行实际DMA或缓存验证，其他DMA单元继续审查。

## 1.131\_DMA地址和SG计数与终止同步

B05x继续核对固定dfaf2136的dma-api-howto和Documentation/driver-api/dmaengine/client.rst（Git blob d491e385d61a98b8a804cd823caf254a2dc62cf4），区分CPU虚拟/物理/DMA地址、单段映射缓冲限制、SG输入项与输出段、DMA设备参数及异步终止后同步。[SG完整C模型](../../../knowledge/linux/io_model/dma/P01_DMA_映射同步与门铃顺序.md#1.7.1_用C核对两套计数)以固定三项到两段检查计数职责，GCC14.2/Clang18.1.8严格C11 O2运行通过；它不实现dma_map_sg，不构成设备或缓存验证。

## 1.132\_poll登记与触发边界

B05y按固定dfaf2136提交核对include/linux/poll.h、fs/select.c、fs/eventpoll.c；文件blob与阶段入口见[poll与epoll源码阅读索引](../io_polling/navigation/P01_poll与epoll源码阅读索引.md#1.1_固定提交与文件位置)。普通poll区分临时登记、triggered与再次查询；epoll区分持久兴趣、目标队列回调、rdllist/ovflist和交付前复查，并核对LT重排、ET不自动重排、ONESHOT禁用与MOD重启。使用官方固定对象，不使用本地实验HEAD作为证据，不修改外部树。

[就绪C模型](../../../knowledge/linux/io_model/blocking_io/P01_poll登记与就绪复查.md#1.7_用完整C模型观察就绪失效)在宿主双编译器严格C11 O2运行；[Linux管道实验](../../../knowledge/linux/io_model/blocking_io/P02_epoll持久登记与交付.md#2.6_完整Linux实验_留下一个字节)未在Linux运行，正文输出明确为预期。未验证真实驱动、多线程、ARM、信号与并发关闭；不把模型通过等同于系统调用或性能验证。

2026-10-16 复审改用显式检查函数，将消费动作从 assert 表达式中分离。GCC/Clang、O0/O2、启用/关闭标准断言的八组编译运行通过，另检查两字节条件下旧预期失败及修正预期成功，共十组。三份保存源码的 Git blob 与上表固定记录一致，据此核对扫描期间 ovflist、合并未完成 txlist 及排他登记限制；本次共享盘不可访问，当前外部工作树复核未完成。Linux 系统调用程序未改，仍无本次 Linux 运行证据。

## 1.133\_等待条件与退出分支

B05z只读固定dfaf2136的include/linux/wait.h（blob 2b322a9b88a2bd122d30e70a3d6eaa12c5cec244）和kernel/sched/wait.c（blob 51e38f5f47018c953e31c834dc6385c182359359），核对初始快查、prepare后condition优先、信号摘链错误出口、正常finish、独占额度及超时边界返回1。知识侧[单槽记录协议](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/P01_等待队列.md#1.3_标准条件等待)是仓库应用示例，不是上游摘录；未编译或加载目标驱动。模块导读同步区分循环与退出，未因相同术语删减源码层。

[四窗口证明](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/P03_条件等待的统一状态机.md#3.5_逐个关闭检查睡眠窗口)明确持久条件、同步协议、通知匹配和寿命前提。复用既有C枚举模型，GCC14.2/Clang18.1.8严格C11 O2输出均为check_first十种排列丢1条、register_first十种丢0条；不证明ARM弱内存、实际睡眠、信号或设备退出已经运行通过。

## 1.134\_完成令牌与swait广播边界

B05aa按固定dfaf2136核对include/linux/completion.h（fb291567657432083162031ddb949a7e581e2848）、kernel/sched/completion.c（3561ab533dd4e33ddb5284bcab51736f9b9ab6bf）、include/linux/swait.h（d324419482a0f5282ceb9174d091ff4204a49022）、kernel/sched/swait.c（72505cd3b60a3e16c093d07da38eab6093155b2e）。区分done普通计数与UINT_MAX、成功超时边界至少1、finish swait后消费、reinit无锁清零、complete_all的实时上下文检查，以及通用swake_up_all分段与完成量专用swake_up_all_locked持锁广播。

[完成量C模型](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/P02_completion_完成量.md#2.7_用完整C模型观察令牌与广播)在GCC14.2/Clang18.1.8严格C11 O2下验证提前保存、两个观察一个令牌、累计两次与广播不消耗；无真实睡眠或内存顺序模拟。内核请求协议只静态核对，未编译加载。模块导读纠正锁释放与finish/消费时序，实现裁剪补回实际存在的RT断言。未将这两处同步称为整个源码专题完成。

## 1.135\_等待宏出口与独占扫描范围

2026-10-16 全篇复审时，将完成量模型的消费动作移出 assert，两个模型都改为不受 NDEBUG 影响的显式结果检查。GCC 14.2 / Clang 18.1.8 的 C11 严格警告、O0/O2、启用/关闭标准断言组合共 16 组通过；另验证单次完成替代广播的预期失败，以及独占首项成功、负回调提前终止、观察者前移三种扫描答案，共 20 组编译运行。完成量模型还覆盖 UINT_MAX 附近的饱和边界。复查仓库保存的 wait.h、wait.c、completion.h、completion.c 与 swait.h，Git blob 均匹配上列固定对象；当前共享源码工作树不可访问，本次没有重新验证外部工作树，也没有运行真实内核等待或驱动停机。

B05ab继续只读固定dfaf2136的wait.h与wait.c（对象身份同1.133），逐支对齐___wait_event的init_wait_entry、condition优先、信号错误出口、正常finish与循环；补回原实现文档声称覆盖但未实际展示的[宏体](../waiting_notification/source_explanations/include/linux/wait.h.md#1.3_wait_event宏循环与出口)。未修改外部树，宏中文说明明确为仓库补充。

[独占扫描C模型](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/P05_独占等待批量唤醒与公平性.md#5.3.1_用C观察额度怎样截断扫描)在GCC14.2/Clang18.1.8严格C11 O2下确认额度1/2/不限时访问3/5/6项；成功非独占回调不扣额度，失败不扣额度，额度终止会跳过后续观察者。模型使用固定回调结果，不运行内核唤醒、调度或资源领取；bookmark仍为独立概念方案，固定代码无该分段路径。

## 1.136\_普通等待源码分层与完整分支

B05ac依据1.133的固定wait.h/wait.c对象，把[结构与宏](../waiting_notification/source_explanations/include/linux/wait.h.md#1.2_队列头与等待项)和[登记扫描清理函数](../waiting_notification/source_explanations/kernel/sched/wait.c.md#1.2_源码符号覆盖账本)按上游位置组织。所选9块结构、宏和函数去除仓库中文注释与空白后，与固定源码一致；补足扫描的锁断言、空链、safe_from及自动摘链路径。中文Doxygen为仓库阅读说明。普通模块的关系和端到端时序与这些分支相互对应。

本次静态核对不表示真实信号、调度、弱内存或目标驱动已运行；未改动外部源码。头文件宏的非恒定state保守保留prepare错误检查，实际信号资格由prepare中的signal_pending_state判断，不把辅助宏解释成业务条件判断。

## 1.137\_完成量源码与非阻塞观察

B05ad按1.134的固定completion.h/completion.c对象分别组织[对象初始化](../waiting_notification/source_explanations/include/linux/completion.h.md#1.2_completion对象与初始化)与[发布等待及观察](../waiting_notification/source_explanations/kernel/sched/completion.c.md#1.2_源码符号覆盖账本)。11块结构/函数去注释及空白后与固定对象一致，保留__sched属性及完整控制分支；中文Doxygen为仓库补充。

特别核对complete_acquire/release在此版本为空、等待包装使用lock_irq、try锁内重检消费、completion_done只经过锁区间而不锁内重检；后者不预留计数或证明旧访问者退出。模块S0～S6映射这些动作，目标内核、实时配置、信号和超时竞态仍未运行，不能把静态一致性称作运行验证。

## 1.138\_一致快照有序证明与入门边界

B05ae只读固定dfaf2136的Documentation/locking/seqlock.rst（blob ec6411d02ac8f55aee92b375071c8b2e7b840e59）及include/linux/seqlock.h（blob 5298765d6ca4827eb7bf8a9dca020f1383d3b901），核对串行写者、不可被读侧抢占的奇数窗口、指针限制及关联锁抢占属性。[入门示例](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/sequence_counters/P01_seqcount_seqlock_读重试快照机制.md#1.3_基本读写模式)限定任务上下文、原始锁短写区和返回局部副本，未在内核编译加载。

[完整C枚举](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/sequence_counters/P02_一致快照的证明模型.md#2.6.1_用完整C程序枚举有序交错)在宿主GCC14.2/Clang18.1.8严格C11 O2运行，35条尾部版本交错接受5条混合值、70条奇偶交错接受0条。只证明既定动作顺序和一次更新，不验证Linux读写屏障、ARM弱内存、真实自旋、回绕或对象回收。

## 1.139\_序列读写变体与latch验证

B05af继续只读1.138固定seqlock.h对象，核对seqprop_sequence的acquire读取、稳定偶数循环、retry屏障、raw差异及latch的依赖选址/完整计数比较。明确begin/write两次重定向，end只关闭KCSAN写区。知识正文与相关源码入口分别同步，不改动外部树。

[latch完整C模型](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/sequence_counters/P04_seqcount_latch双副本状态机.md#4.4.1_用C区分暂停写者与跨CPU交错)双编译器严格C11 O2得到paused_windows=7、schedules=210、full_mixed=0、bit_mixed=21；证明范围为单写者一次有序更新，不含目标NMI、ARM弱内存、计数回绕或动态对象回收。源码实现页只修复直接受影响的真实控制流，关联锁等剩余覆盖继续独立处理。

## 1.140\_关联字段配置与回绕边界

B05ag只读固定dfaf2136的include/linux/seqlock_types.h（blob dfdf43e3fa3de3acfd294807cc207d6464db0d11）及1.138的seqlock.h，核对关联lock字段在LOCKDEP或PREEMPT_RT配置下保留，RT读取奇数时的真实锁等待，以及非RT/RT抢占属性区别。未运行RT配置或测量调度上界。

[8位回绕C模型](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/sequence_counters/P06_生命周期误用诊断与选型.md#6.3.1_用8位模型重现回绕误收)在宿主双编译器严格C11 O2输出start=0 end=0 candidate=(0,128)。这以有限有序步骤展示无回绕前提，不说明实际unsigned字段宽度被改动，也不运行真实暂停或并发。知识侧配置/寿命修复与源码模块分别同步，完整实现审查另行推进。

## 1.141\_序列类型与真实操作实现

B05ah按1.138与1.140固定对象，分别组织[seqlock_types.h类型配置](../sequence_counters/source_explanations/include/linux/seqlock_types.h.md#1.2_plain计数与条件检查字段)与[seqlock.h操作实现](../sequence_counters/source_explanations/include/linux/seqlock.h.md#1.2_源码符号覆盖账本)。42块实际文档代码去注释空白后与固定对象一致，真实属性生成宏替换概念占位；中文Doxygen为仓库补充。

模块与总索引指向唯一实现，说明普通读写、关联锁慢路径、latch和基础seqlock封装的不同状态。未扩展成所有包装或底层锁/调度器源码覆盖；静态一致性不代表配置矩阵编译、PREEMPT_RT、NMI、弱内存或目标内核运行已经通过。

## 1.142\_锁周期中的自旋事件与任务唤醒

B05ai重新核对工作树身份后只读固定dfaf2136的arch/arm/include/asm/spinlock.h（blob f610a773f2be527dde14415a61fab4c7250d9447）和kernel/locking/mutex.c（blob cbae8c0b89ab2b8074a387ebf1cca86951362cc2）。ARM SMP票号、事件等待/发送，与mutex等待任务、wake队列和handoff/pickup属于不同推进路径；[统一周期](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P03_锁的统一状态与通信周期.md#3.5_端到端竞争时序)分别表达，不再把调度唤醒套到所有自旋分支。

本地配置未启用SMP，HEAD为实验提交，不作为上述证据。没有进行目标锁竞争、真实调度、RT或ARM SMP运行验证，抽象A/B/C交错只说明资格区别，不承诺所有实现的插队或公平策略。

## 1.143\_自旋接口的配置契约

B05aj只读固定dfaf2136的Documentation/locking/locktypes.rst（blob 80c914f6eae7ab07476491c4eba96c004508e28d），核对非RT的自旋/后缀语义与RT普通spinlock、rwlock的替换。RT普通irqsave不屏蔽硬中断，允许抢占但限制迁移；raw仍严格自旋。用于[入门实例边界](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P01_自旋锁.md#1.6_spinlock_与_raw_spinlock)，不是从当前非SMP配置推断全部实现。辅助代码未在目标编译或执行，中断重入图为机制推理。

## 1.144\_ARM票号与事件的静态边界

B05ak核对固定dfaf2136的arch/arm/include/asm/spinlock.h（blob f610a773f2be527dde14415a61fab4c7250d9447）及spinlock_types.h（blob 0c14b36ef10131b2a1fc95d91ccc76871e8ba4d4）：16位owner/next、整字领号、wfe后读取owner、释放推进owner与dsb_sev。用于[领号过程与阶段](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P04_spinlock实现与上下文边界.md#4.3.1_从反复抢夺改为领号等待)。正文不复制函数体；C模型仅验证顺序资格，工作配置非SMP且HEAD为实验提交，未证明该架构分支目标执行。

## 1.145\_可睡锁应用契约

B05al核对固定dfaf2136的include/linux/rwsem.h（blob c8b543d428b0a8d4662183f3342e88ec61d10189）与Documentation/locking/mutex-design.rst（blob 7c30b4aa5e28fc32afd29ef9d9ebc873bed19c9a），确认rwsem可中断/try/降级入口和非递归契约、mutex所有者与unlock仍在执行时的对象寿命。[应用层章节](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P02_互斥锁与读写信号量.md#2.8_生命周期与停机)不把解锁发布时点当成调用结束；配置辅助C代码未做目标编译运行，工作树实验HEAD不作证据。

## 1.146\_普通mutex的交接与取消

B05am只读固定dfaf2136的kernel/locking/mutex.c（blob cbae8c0b89ab2b8074a387ebf1cca86951362cc2），核对trylock_common、lock_common、optimistic_spin、handoff及unlock_slowpath：队首请求HANDOFF，释放者写入接收者和PICKUP，内部wait_lock下先接收再判信号。见[同场景交接比较](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P05_mutex慢路径与所有权交接.md#5.6_handoff解决什么又付出什么)。同时核对kernel/Kconfig.locks中MUTEX_SPIN_ON_OWNER依赖SMP及原子RMW支持，当前非SMP配置未启用；此前该章“已启用”表述已纠正，不以实验HEAD证明优化运行。未做目标调度与信号验证。

## 1.147\_rwsem批次与授权证据

B05an核对固定dfaf2136的kernel/locking/rwsem.c（blob 2bbb6eca51445bdf434ba579ced4beddafbc52ca）：mark_wake写分支只通知，读阶段跳过WRITE节点，最多256读者，两遍先计count后release清waiter.task；读慢路径acquire观察并在信号分支复查，写慢路径原子取得。见[批量交付](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P06_rwsem读写汇聚与唤醒.md#6.4_为什么读者可以批量唤醒)。kernel/Kconfig.locks确认RWSEM_SPIN_ON_OWNER依赖SMP与原子RMW，当前工作配置非SMP，不构成优化运行证据。没有目标竞争或调度实测。

## 1.148\_实时锁契约与等待状态

B05ao核对固定dfaf2136的Documentation/locking/locktypes.rst及include/linux/mutex.h，确认RT普通自旋锁后缀、抢占/迁移区别、saved_state保存外层等待、raw与读写锁边界以及mutex的RT基础类型分支。用于[配置对比](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P07_PREEMPT_RT生命周期与选型.md#7.3_配置分支的端到端对比)。本地配置仍非RT、非SMP；没有目标优先级继承、任务唤醒或停机实测，不将文档契约视作部署验证。

## 1.149\_自旋通用包装阅读分支

B05ap核对固定dfaf2136的include/linux/spinlock_api_smp.h（blob 89eb6f4c659c706c1b7376534a25d53a316316b7），并核对spinlock.h及spinlock_types_raw.h的SMP/UP选择。[模块导读](../locking/navigation/P02_Linux_6.12_spinlock模块源码概念导读.md#2.3_普通获取调用链)明确通用常规分支的preempt/检查/架构操作顺序与GENERIC_LOCKBREAK例外，未把当前非SMP工作配置视为SMP宏展开证据。未执行目标编译与架构运行。

## 1.150\_普通锁类型与包装单元

B05aq按固定dfaf2136的include/linux/spinlock_types.h（blob 2dfa35ffec761f933e0f38d38be756d78b4b1669）及spinlock.h整理[类型配置](../locking/source_explanations/include/linux/spinlock_types.h.md#1.2_spinlock_t的配置映射)与[初始化/操作包装](../locking/source_explanations/include/linux/spinlock.h.md#1.3_初始化与对象地址)。18个实际单元规范化比较通过，保留真实声明属性，中文Doxygen为仓库补充；不包含ARM汇编、完整通用API或调试实现，未做目标编译运行。

## 1.151\_raw上下文配对

B05ar对固定dfaf2136的include/linux/spinlock_api_smp.h五个完整函数做静态比对，见[普通获取释放](../locking/source_explanations/include/linux/spinlock_api_smp.h.md#1.2_普通获取和释放的顺序)及其后irqsave/trylock。采用非lockbreak常规获取分支，说明检查获取先于功能获取而try检查仅在成功后登记；失败撤销本层约束。当前配置非SMP，未进行目标编译或实际竞争验证。

## 1.152\_ARM阻塞获取与事件发布

B05as以固定dfaf2136的arch/arm/include/asm/spinlock.h（blob f610a773f2be527dde14415a61fab4c7250d9447）展开[三个真实函数](../locking/source_explanations/arch/arm/include/asm/spinlock.h.md#1.2_取得票号不等于已经进入)，静态内容比对通过。说明ldrex/strex领号、wfe后owner复查、smp_mb与dsb_sev顺序，限定SMP分支且未展开事件替换宏体；当前工作配置非SMP，不构成架构执行证据。

## 1.153\_mutex交接与取消的真实分支

B05at从固定dfaf2136的kernel/locking/mutex.c（blob cbae8c0b89ab2b8074a387ebf1cca86951362cc2）展开[owner取得与等待](../locking/source_explanations/kernel/locking/mutex.c.md#1.3_owner指针与三个位标志)及[释放交接](../locking/source_explanations/kernel/locking/mutex.c.md#1.5_mutex_unlock_slowpath的交接)，11个完整函数静态比对通过。替换概念骨架，明确HANDOFF直接退出释放循环、选当前队首、release发布PICKUP、目标acquire接收先于错误退出。范围为非RT普通mutex，共同函数中的WW分支保留但未展开其算法；当前非SMP工作配置未启用owner自旋，未做目标编译运行。

## 1.154\_rwsem对象布局与观察契约

B05au核对固定dfaf2136的include/linux/rwsem.h（blob c8b543d428b0a8d4662183f3342e88ec61d10189），展开[两种对象布局](../locking/source_explanations/include/linux/rwsem.h.md#1.2_非RT对象的状态落点)及六个观察/断言函数。非RT的is_locked比较整个count，降级写断言只看写位；不把共享状态解释为当前任务已取得。RT仅核对类型替换，未展开rwbase内部，未进行目标配置编译或运行。

## 1.155\_rwsem预授与退出路径

B05av依据固定dfaf2136的kernel/locking/rwsem.c（blob 2bbb6eca51445bdf434ba579ced4beddafbc52ca）展开[批量授予](../locking/source_explanations/kernel/locking/rwsem.c.md#1.4_mark_wake先记账再发布)及读写获取和取消。三种类型、八个完整函数静态比对；读者先补份额后release清task，信号路径持锁复查，写者唤醒仍需acquire设置写位。限定非RT，未执行目标运行或性能测量。

## 1.156\_可睡锁模块配置与协作

B05aw重新核对固定dfaf2136的include/linux/mutex_types.h（blob fdf7f515fde8e3e06c4dd7204bcd7a469e2a76eb）和mutex.h（blob 2bf91b57591b49e4668752e773419ae945f124da），确认RT载体与配置入口。[模块导读](../locking/navigation/P03_Linux_6.12_mutex与rwsem模块源码概念导读.md#3.3_mutex完整调用链)组织已核对mutex.c/rwsem.c的状态协作，不新增重复函数体；目标编译运行及RT仍未验证。

## 1.157\_Lockdep候选与功能回退边界

B05ay核对固定dfaf2136的kernel/locking/lockdep.c（blob 536bd471557f5b4412d6babc480d0832b4ee9d51）与保存文件一致，阅读validate_chain及__lock_acquire的候选、usage、链键和有效深度提交，用于[四轴与阶段](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P02_Lockdep_抽象模型与证明边界.md#2.4_S0到S6的端到端周期)。当前工作配置LOCKDEP/PROVE_LOCKING开启不代表运行检查有效或路径已覆盖；未做目标实验。

## 1.158\_实例匹配与身份示例边界

B05az核对同一固定lockdep.c的match_held_lock与当前查询路径，确认先匹配instance，在references与nest_lock条件下还可按类匹配，用于[身份概念](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P03_锁实例_锁类_key与subclass.md#3.2_四个身份概念分别回答什么)的限定。用户态C探针仅验证静态对象身份，不构成内核查询或subclass运行证据。

## 1.159\_当前记录回退与重建

B05ba复核固定dfaf2136的lockdep.c中__lock_release，确认引用减少、prev_chain_key恢复和reacquire_held_locks重建只作用于检查状态，支持[非栈顶回退](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P04_持锁账本_依赖图与状态闭环.md#4.7_release怎样回退当前链)。沿用blob 536bd471557f5b4412d6babc480d0832b4ee9d51，未进行目标运行验证。

## 1.160\_读写阻塞与强路径

B05bb核对固定dfaf2136的Documentation/locking/lockdep-design.rst（blob 56b90eea27312e0a438260eb10425e811f154c9a）的递归读、阻塞矩阵与strong path说明，用于[读写类型反例](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P05_递归_依赖环_IRQ与读写规则.md#5.7_为何图边必须携带读写类型)。区别抽象阻塞契约与具体read_lock API；未进行目标竞争实测。

## 1.161\_查询三态与pin计数

B05bc核对固定dfaf2136的include/linux/lockdep.h（blob 67964dc4db952ea11d4b88554383ea0ec5946ef9）三态值和pin包装，并复查lockdep.c的__lock_pin_lock/__lock_unpin_lock，支持[查询与连续性](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P06_查询_断言_pin与自定义原语接入.md#6.5_pin解决的是前后断言看不见的空洞)。cookie是计数配对值，诊断不阻止功能释放；未执行内核回调测试。

## 1.162\_RCU查询退化与告警条件

B05bd核对固定dfaf2136的include/linux/rcupdate.h，blob 48e5c03df1dd83c246a61d0fcc8aa638adcd7654。未启用DEBUG_LOCK_ALLOC时普通rcu_read_lock_held返回1；PROVE_RCU告警宏包含有效性复查和调用点__warned状态。证据支持[RCU检查边界](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P07_RCU与子系统检查适配.md#7.5_从通用Lockdep到RCU实现的证据边界)，不能作为功能保护或运行时测试结果。

## 1.163\_循环报告与实验路径标记

B05be复查固定dfaf2136的kernel/locking/lockdep.c，blob 536bd471557f5b4412d6babc480d0832b4ee9d51。print_circular_bug先调用debug_locks_off_graph_unlock，再保存与打印报告；[实验章节](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P08_配置_亲手实验与报告解读.md#8.4_把运行过程记成状态接力)据此区分检查停检与功能mutex继续执行。S0/S1/S2为教学模块的预期日志，不是已取得的目标运行证据。

## 1.164\_链缓存与容量边界

B05bf核对固定dfaf2136的include/linux/lockdep_types.h，blob 9f361d3ab9d95d98428dbc9b25361c3ffe9e4e10，确认MAX_LOCKDEP_KEYS_BITS为13。复查kernel/locking/lockdep.c中validate_chain只在非trylock且check有效的新链上执行完整依赖分支；[成本章节](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P09_成本_覆盖边界与工程选择.md#9.2_链缓存消除了哪部分成本)不再把全部IRQ使用检查归入首次新链成本。未测量性能或注入容量故障。

## 1.165\_mutex事件与功能失败配对

B05bg核对固定dfaf2136的kernel/locking/mutex.c，blob cbae8c0b89ab2b8074a387ebf1cca86951362cc2。mutex_acquire_nest位于功能尝试前，错误出口通过mutex_release配对撤销已提交检查记录，支持[身份与事件模块](../lockdep/navigation/P02_Linux_6.12_Lockdep身份与事件接入模块导读.md#2.4_取得与释放调用链)的S0至S6周期。未执行信号中断的目标运行验证。

## 1.166\_新链登记与图验证顺序

B05bh核对固定dfaf2136的kernel/locking/lockdep.c，blob 536bd471557f5b4412d6babc480d0832b4ee9d51。lookup_chain_cache_add锁内复查并登记新链后才继续validate_chain规则，支持[规则模块](../lockdep/navigation/P03_Linux_6.12_Lockdep依赖图与规则引擎模块导读.md#3.2_规则链而不是一个环检测函数)修正缓存登记晚于验证的旧图示。缓存存在不单独证明检查器有效；未执行目标多CPU实验。

## 1.167\_查询返回与pin诊断周期

B05bi复核固定dfaf2136的include/linux/lockdep.h，blob 67964dc4db952ea11d4b88554383ea0ec5946ef9，以及lockdep.c的__lock_is_held、__lock_pin_lock。[查询模块](../lockdep/navigation/P04_Linux_6.12_Lockdep查询适配与诊断模块导读.md#4.3_断言与pin怎样消费held_record)区分三态返回、pin_count配对与功能同步，未执行目标回调测试。

## 1.168\_锁类登记完整控制流

B05bk核对固定dfaf2136的kernel/locking/lockdep.c，blob 536bd471557f5b4412d6babc480d0832b4ee9d51。[register_lock_class讲解](../lockdep/source_explanations/kernel/locking/lockdep.c.md#1.4_register_lock_class锁类注册)改为完整函数，去注释空白后匹配固定源码，保留初始化、verbose重新加锁、解锁标签、force缓存选择及最终返回。未执行目标竞争或容量测试。

## 1.169\_map初始化提前返回

B05bl复查同一固定lockdep.c的lockdep_init_map_type，恢复统计CPU记录及debug_locks、lockdep_enabled提前返回。去注释空白后完整函数匹配；字段写入可能早于后续诊断失败，不能将map有值视为类登记成功。未执行目标配置矩阵或故障测试。

## 1.170\_工作队列执行模型与取消版本边界

2026-10-16 全篇复审使用仓库保存的固定版本 include/linux/workqueue.h 与 kernel/workqueue.c，LF归一后的Git blob分别匹配既有记录59c2695e12e7674d5bc4cdd1fb416074374f981c与a9d64e08dffc7c7aef2caae2f170268d83e99e23。核对WQ_BH不能睡眠、ordered分配宏、queue禁用分支、同队列非重入前提、临时disable取消协议、worker睡眠钩子、flush汇聚哨兵、mayday定时检查及冻结路径。

[unbound正文](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/workqueue/P07_unbound_rescuer与回收前进性.md#7.2_属性传播关系)记录同树文档与实现差异：保存的Documentation/core-api/workqueue.rst仍称unbound额度总是per-CPU，而固定代码wq_update_node_max_active与pwq_tryinc_nr_active已采用节点共享账本。正文按实现解释，不把该文档句子当作当前实现保证。保存文档的LF归一blob为16f861c9791e4ae2914bbf517c1a1835263711bf，kernel/workqueue_internal.h为f6275944ada776814787ac5a1cfa01f891bfe934；后二者本次只记录本地副本摘要，未重新取得外部工作树对象比较。

[异步状态C模型](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/workqueue/P02_异步执行的抽象状态机.md#2.4_用完整C模型区分pending与running)在GCC14.2/Clang18.1.8、严格C11、O0/O2及NDEBUG开关共八组通过；第三次事件提前到drain之前仍计数完整，错误清除后继pending则按预期失败，共十组编译运行。它是顺序模型，不实现线程、真实锁、调度或弱内存。第一章设备门控代码明确为协议片段，未编译或加载目标驱动；当前共享源码工作树不可访问，本次没有重新验证外部工作树身份。

## 1.171\_时间基础与定时入口边界

2026-10-17 复审[定时入口](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P01_驱动中的_时间问题_概述.md)与[时间框架](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P02_Linux_时间基础与_timekeeping_框架速览.md)，从官方 linux-imx 仓库的固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 下载以下原文，记录原始字节的 Git blob。来源入口为[该提交源码树](https://github.com/nxp-imx/linux-imx/tree/dfaf2136deb2af2e60b994421281ba42f1c087e0)，不是当前本地共享目录或移动分支。

| 上游路径 | 本次取得的 blob |
| --- | --- |
| include/linux/jiffies.h | 5d21dacd62bc7d5187af9d96ec288c6517557475 |
| include/linux/timekeeping.h | fc12a9ba2c884271a75608211a72173b7ebaa24c |
| include/linux/hrtimer.h | 6caaa62d2b1f893439129bdbe42135ef1da13f8b |
| include/linux/clocksource.h | c846436b64593e913d452d2ba07870e13729ea21 |
| include/linux/clockchips.h | b0df28ddd394be3cee7e6491e04d922471fc04d4 |
| kernel/time/timekeeping.c | 96933082431fe0c99a2f0704d9c5c64dd57de3cf |
| Documentation/timers/hrtimers.rst | f88ff8bae89c8acf857225f2797478d2e502ebaf |
| Documentation/timers/highres.rst | bde5eb7e5c9efc10494b242bd055bb602a03efff |

核对 jiffies/get_jiffies_64 的位宽与读取边界、time_after 族的差值比较、ktime_get 的序列计数读取及 timekeeping_cycles_to_ns 的周期差换算、boottime/real 偏移入口、clocksource 与 clock_event_device 的读数/事件接口。hrtimer 模式说明区分软硬中断路径；高分辨率设计说明明确要求合适时钟源、事件设备及运行时启用，不能由 CONFIG_HIGH_RES_TIMERS 单独推出实际高分辨率。普通定时器删除/关闭继续复用 [1.62 定时器证据](#1.62_定时器改期与最终退出证据)及其实现讲解。

在线 Linux 6.12 的 timekeeping、ktime accessors、NO_HZ 和 highres 文档作为机制对照；历史设计文字不代替当前实现。共享源码树仍未恢复访问，本次通过不可变官方 URL 取得相关文件，不声称重新验证了工作树或目标配置。正文代码是单位/属性接入片段，未作 Kbuild、加载、实时延迟或退出竞态实测；图示是职责与状态流程，不是某块 SoC 的物理布局。

## 1.172\_普通定时器的排队执行与最终关闭

2026-10-17 复审[普通定时器](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P04_基础定时器机制_struct_timer_list.md#4.2_为什么未排队不能证明回调已结束)，复用 1.62 已记录的 timer.c 与 timer.h，并从官方固定提交下载 `include/linux/timer_types.h`，Git blob 为 `fae5a388f91495757a5f7492b0114ce4509a0988`。核对 `entry`、`expires`、`function`、`flags` 的声明，`expire_timers()` 先登记 `running_timer`、摘队，在 base 锁外调用回调再清除运行者的过程，以及 `timer_delete_sync()` 与 `timer_shutdown_sync()` 的不同保证。`TIMER_PINNED` 依赖排队 CPU，不能解释成初始化时永久绑定。

正文保留一次性、周期和转交普通工作队列三个独立模块，并明确静态对象、唯一生产者与退出后读取计数的前提。周期算法的验证只检查计划相位、跳过过期点和无符号回绕，不模拟内核执行延迟或同步实现；共享源码树不可访问，未进行目标 Kbuild、模块加载、设备移除或竞争压力测试。永久关闭不代替外部入口注销或对象引用回收。

从正文回调提取下一次到期计算，在 32 位无符号计数模型中用 GCC 14.2 与 Clang 18.1.8 的 C11/O2 分别运行一百万组随机输入及准点、迟到、回绕三个边界；结果与宽整数参考计算一致。把算法改回只递增一个周期的反例被同一检查拒绝。该结果不等于完整内核模块编译或运行通过。

## 1.173\_时间转换与组合模式的版本核对

2026-10-17 从官方不可变提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 取得以下原文，支持[单位与范围](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P03_时间表示与转换接口详解.md#3.3_转换函数为什么救不了调用前的溢出)与[组合模板](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P12_常用模式与代码模板.md#12.8_事务被唤醒以后还必须读取结果)的修正。

| 上游路径 | 原始字节 Git blob | 本次核对点 |
| --- | --- | --- |
| include/linux/types.h | 2bc8766ba20cab014a380f02e5644bd0d772ec67 | ktime_t 是 s64 类型别名 |
| include/linux/ktime.h | 3a4e723eae0f1203d5aec7bb2d460dfbf171b644 | ktime_set、ns/ms_to_ktime、加减与比较签名和边界 |
| kernel/time/time.c | 1ad88e97b4ebcf1b51745c6b73ce6422a481bc62 | 毫秒和微秒的运行时转换分支；头文件沿用 1.171 |
| include/linux/platform_device.h | 7132623e4658576f0be2a80b493e445e0c9d4de6 | platform_driver.remove 为 void 回调 |
| kernel/time/hrtimer.c | db9c06bb23116a0d76d972477b40afd43b9b6d8f | hrtimer_forward 基于旧期限推进、分辨率下限与 cancel 等待循环 |
| kernel/sched/completion.c | 3561ab533dd4e33ddb5284bcab51736f9b9ab6bf | complete 通知不携带业务结果，timeout 返回值与重新初始化责任 |

`include/linux/hrtimer.h` 沿用 1.171 的固定文件，核对 `hrtimer_start()` 返回 void、forward_now 的当前时钟取值与模式边界。普通 timer 与 work 的取消协议沿用既有固定实现证据，不把 hrtimer_cancel 写成永久关闭。模板中的 start/quiesce 是教学设备操作契约，不是上游通用函数；GPIO 绑定和物理引脚未得到本次板级验证。无目标 Kbuild、硬件去抖、IRQ、sysfs、事务或卸载竞争实测结果。

毫秒转换提取固定头文件与运行时函数，在 HZ=100/250/1000 下用 GCC/Clang 的 C11/O2 各核对 0～5000 共 5001 个运行时输入，以及零值、15 ms、极大值常量和乘法溢出反例。另把正文去抖检查函数与事务共同终结函数放入顺序模型，核对提前检查、准点、采样期间新边沿/改参/停止、GPIO 负错误，以及三种结果的 27 种先后组合；两个编译器均通过。模型将锁、队列、GPIO 和通知替换为桩，只检验这些指定状态交错，不模拟 Linux 并发、内存序、硬件或完整模块构建。

## 1.174\_hrtimer状态位置与周期计划

2026-10-17 复审[高精度定时器](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P05_高精度定时器_hrtimer_的驱动用法.md#5.2_到期点与执行者仍然分开保存)，从同一官方固定提交下载 `include/linux/hrtimer_types.h`（Git blob `ad66a30817357e727e9005417d9edbebb7013492`）和 `include/linux/hrtimer_defs.h`（`84a5045f80f36f88cb47bf23a62a4d0afbd1a2c6`）。结合 1.173 的 hrtimer.c 核对 `__run_hrtimer()`、`hrtimer_forward()`、`hrtimer_cancel()` 与 `__hrtimer_init()`。

同版本 hrtimer.h 的旧注释仍写 `timer->base->cpu_base->running`，但当前结构定义与执行函数实际使用 `timer->base->running`，属于 clock_base；正文按声明和执行代码说明。forward 以旧到期点推进，now 决定越过多少周期；小于 hrtimer_resolution 的周期会被提高，函数本身不排队。默认非实时配置与 SOFT/HARD 模式分别解释，不一概写成软中断。

原有三份 LED 驱动合为共享初始化、统计和回收的三模式教学驱动；旧两组日志原样保留，并标记缺少配置、负载和驱动身份，不能作为本轮实测或优劣结论。板级 GPIO 和 pad 示例仍需实物与手册核对，未进行目标 Kbuild、模块运行或 GPIO 波形验证。

固定 `hrtimer_forward()` 主体与正文手工锚点计算置入宿主数值模型，在 GCC/Clang C11/O2 下分别比较十万组相同输入，并检查未来期限、恰好到期、跨多周期、分辨率下限及已排队保护分支；两者在正文约束内一致。模型替换了内核时间与对象操作辅助，不模拟队列、锁、硬件事件或完整驱动执行。验证过程中补齐宿主桩的 unlikely 宏后编译通过，未把桩缺失误记为内核问题。

## 1.175\_延迟工作与按键策略核对

2026-10-17 复审[延迟工作](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P06_基于工作队列的延迟执行_delayed_work.md)并拆出[按键策略](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P14_按键窗口合并与按下释放确认.md)，复用 1.170 固定 workqueue.c/h，核对非零延迟保存目标、零延迟直接投递、pending 与运行正交、mod 返回值、flush 提前投递和同步取消临时禁用。另从同一官方不可变提交下载以下文件。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| drivers/gpio/gpiolib.c | 209871c219d697d05ac12c9a20edeb68ba59c5d8 |
| include/linux/wait.h | 2b322a9b88a2bd122d30e70a3d6eaa12c5cec244 |
| include/linux/debugfs.h | 0928a6c8ae1ee56b9fa654aa273aa6b588cc2562 |
| fs/debugfs/file.c | 67299e8b734edfef53d99dbf368d2d34403c9f93 |
| fs/debugfs/inode.c | 66d9b3b4c5881defe68fa2527f621e0162f2c48c |
| include/linux/pm.h | 97b0e23363c8283f53c361d8129ef90d7a2b9350 |
| include/linux/interrupt.h | 457151f9f263d9042625decde2c15822d860b134 |

GPIO get/set 接口采用逻辑极性，get 负错误在 active-low 翻转以前返回；wait_event_timeout 以保存的条件为准，唤醒必须晚于条件更新。debugfs 属性宏使用受保护的读写包装，remove 等待这些活动访问；unsafe 创建接口也允许已自行保护的属性宏，不能仅按接口名判断。PM 与 platform/remove 声明按本基线核对。

旧首边沿反馈、窗口噪声合并和两种按下释放事务改为共用资源与状态锁的四策略驱动；轮询、有限退避、故障注入与在线参数改为无硬件模块。错误 GPIO 极性、负数当真、丢释放通知、无界重试和先销毁队列后注销 IRQ 的路径已修正。不声称已经构建或加载目标模块，不声称板级接线、释放波形、PM 或移除竞争经过实测。

从两章正文提取业务函数与参数控制，使用 GCC/Clang 的 C11/O2 顺序模型核对推测反馈撤回、窗口合并、释放截止与回绕、读取跨过截止、负错误、通知早于等待、停止和重试预算；两套退避函数各覆盖 12502500 组有效输入及无符号极值。清零丢通知与倍增先溢出的两个反例均被断言拒绝。模型以桩替代锁、队列、GPIO 与等待，不检验 Linux 弱内存或真实并发，也不构成完整模块构建结果。

## 1.176\_睡眠等待与超时结果边界

2026-10-17 复审[睡眠与超时](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P07_睡眠与超时调度接口.md)，沿用既有固定 timer.c、hrtimer.c、completion.c 与 wait.h，核对 schedule_timeout 通过到期点减当前 jiffies 计算剩余、msleep 反复睡剩余预算、usleep_range_state 的范围与重试、hrtimeout 的 sleeper.task 判定、wait 边界正数及 completion 令牌消费。另从官方固定提交取得 include/linux/delay.h（Git blob `ff9cda975e30d7e84ea342e2576912f0be2b1d02`）与 kernel/kthread.c（`9bb36897b6c62d46a2f7f7af51b86ff545e3eecf`）。

ssleep 的秒到毫秒乘法、usleep_range 的 unsigned long 参数及不可中断封装，kthread_stop 的停止位、唤醒与等待退出分别核对。剩余为零、hrtimeout 返回-EINTR和当前信号状态不能提供唯一物理唤醒来源；普通complete/wait配对消费后不要求每轮机械reinit，广播及迟到完成仍需轮次协议。正文用release/acquire发布单写者结果，完整模拟模块所有返回路径均排空实际执行源；未验证目标构建、装载、硬件、信号或冻结行为。

正文的稳定期封装、截止轮询和等待模块提取到 GCC/Clang C11/O2 顺序模型，覆盖提前/延后恢复、读取跨过截止、读取错误、信号与输入边界，以及两类等待对象、可中断配置、完成/超时/信号/边界结果、业务失败和晚到完成的64组组合。另提取线程循环与事件生产函数，核对设任务状态以前、以后、计数检查以后、调度期间到来的事件及停止窗口。三个反例分别因接受截止后观测、缺少源同步、检查后才设睡眠状态而被拒绝。模型不验证release/acquire的真实弱内存行为，也不代替内核调度与Kbuild；验证时修正过宿主时钟桩可能倒退的问题，未把桩错误归因于正文算法。

## 1.177\_忙等待实现与轮询超时边界

2026-10-17 复审[忙等待与短延时](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P08_忙等待与短延时_udelay()_ndelay()_的边界.md)，从官方 NXP 仓库不可变提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 取得以下原始文件；结合 1.176 的通用 delay.h 核对 ARM32 的循环/计时器分派、参数换算、ndelay 回退与 cpu_relax 分支。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| include/linux/iopoll.h | 19a7b00baff43595fe320ab45109697f9d492157 |
| arch/arm/include/asm/delay.h | 1d069e558d8debcbb261f490e78edc1726e5f3d4 |
| arch/arm/lib/delay.c | b7fe84f68bf19eca6910afbac627435cf0d6cd6b |
| arch/arm/lib/delay-loop.S | 33b08ca1c2422d40896ef5ee260dc11a2e272028 |
| arch/arm/include/asm/processor.h | 326864f79d18fe035a1e3b499b7185f8ca53caa7 |
| arch/arm/include/asm/vdso/processor.h | 45efb3ff511c9c64a7540ffe7163bdb07e316fdb |
| Documentation/timers/timers-howto.rst | ef7a4652ccc9dc2b84851d1c1166d140b3de4d8a |

普通 iopoll 宏先检查条件，再检查单调时间是否大于截止，过期还补读一次；最后条件成立仍返回成功。atomic 宏不读取时间，用请求延时和每轮递减估算预算，不能证明墙钟耗时上限。两者零超时均表示不超时；条件重复求值，读取必须允许重复和补读。ARM32 常量 udelay 的 2000 微秒边界是实现限制，不是不可抢占区的延迟许可；ndelay 参数单位不能证明实际纳秒精度。

原始 iopoll.h 去掉内核 include 后，以受控时间、睡眠、延时和读取桩编译实际宏，连同正文 MMIO 封装及无硬件模块初始化在 GCC/Clang GNU11/O2 下执行。覆盖最后补读、恰好到期、睡眠延后、200 组零超时最终就绪、原子预算低估与零间隔、条件重复求值、294 组参数边界和模块结果路径；删除补读、把大于改为大于等于、允许零预算三个反例均被拒绝。该模型不模拟总线、IRQ 或调度，不等于目标内核构建和装载。

正文取消没有芯片依据的 GPIO 稳定位与 MMIO 地址，用接口契约表和真实宏实验替代；IRQ 交接保留为有明确设备前提的协议，hrtimer 到 work 提供一次性、有类型的 devres 取消辅助。后者提取到两编译器的顺序桩模型，核对参数、分配/登记失败以及未到期、已投递、已执行和取消中投递四种清理窗口；不证明真实并发。没有进行设备时序、PREEMPT_RT、调频、移除竞争或 ftrace 运行验证。

## 1.178\_时间属性解析与运行配置生效

2026-10-17 复审[时间与设备树平台数据](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P09_时间与设备树平台数据的对接.md)。继续使用官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`；前三项重新取得原始文件，后五项复用本轮设备模型核对的同提交原始缓存并计算 Git blob。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| Documentation/devicetree/bindings/writing-bindings.rst | 1ad081de2dd04e0f3a03adce0f0c9f9b6c256f00 |
| Documentation/devicetree/bindings/input/gpio-keys.yaml | cc78c2152921308fe0cad3e29ca78a5fad08f066 |
| drivers/of/property.c | c79d3645bee3be6d30baab3469d4c2e64a8c3dcc |
| drivers/base/property.c | 837d77e3af2bd5bf655e6fa0f64c10a05b361b90 |
| drivers/base/platform.c | 6f2a33722c5203ac196a6e36e153648d0fe6c6d4 |
| drivers/base/swnode.c | eb6eb25b343bafaa9c03f881b98a528305fa6093 |
| include/linux/property.h | 61fc20e5f81f8da992aa021996ddf68ef3eff0ed |
| drivers/base/dd.c | bcc1f28b71f4f554ec8cf279934d13bdc6acce9c |

设备属性标量读取委托到数组读取，不替驱动强制单元素契约；OF 数量检查会拒绝非元素大小倍数的长度，读取长度和语义 Schema 仍是不同层。统一固件节点读取不会自动转换 platform_data；platform_device_register_full 的 properties 路径创建软件节点。驱动核心成功 probe 后创建 dev_groups，在属性创建失败或解绑时调用相应清理；remove 前移除驱动属性组。工作对象采用此前 workqueue 证据中的关闭生产者后同步取消模式。

gpio-keys 的 debounce-interval 以毫秒计、默认5，poll-interval属于gpio-keys-polled；不将教学nxp私有属性冒充官方绑定，不把软件策略普遍写入DT。原三机制拼装示例中，GPIO错误、IRQ早于初始化、无锁配置/事件标志、属性写入凭空制造任务与不关闭IRQ便取消工作等问题，不再作为可直接落地代码保留。对应真实按键与事务职责回指现有机制章节；本章改用完整无硬件platform属性实验，演示统一配置验证与下一轮排队生效。

提取正文解析、工作、sysfs访问和remove函数，在GCC/Clang C11/O2顺序桩模型中核对缺失、数量/读取错误、各项上下界、混合来源、失败不发布半份配置、写入不修改已排到期点、后续排队采用新值及停止后不再生产。取消数量检查、写入强制重排、遗漏停止位三个反例均被拒绝。桩替代内核属性后端、锁、队列和数值解析；不是目标Kbuild、模块装载、DT Schema/DTB、sysfs并发或板级测试结果。

## 1.179\_异步清理依赖与电源引用

2026-10-17 复审[定时对象与设备生命周期](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P10_与_devres_驱动生命周期和_PM_的关系.md)，复用同提交 device.h/devres.c、timer.c/h、workqueue.c/h、pm.h 与设备核心证据，新增核对官方不可变提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 以下文件。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| include/linux/pm_runtime.h | d39dc863f612fe18dc34182117f87908d63c8e6d |
| include/linux/pm_wakeup.h | 76cd1f9f1365306ab45b2832767e400f09e503bc |
| drivers/base/power/wakeup.c | 752b417e81290eb97d578de7f388b83201e52421 |
| Documentation/power/runtime_pm.rst | 12f429359a823eeb2ed34eff169dab22924e2d98 |
| Documentation/driver-api/driver-model/devres.rst | 5f2ee8d717b1deafb43a60b03dbc5d5bc6b4875a |
| include/linux/devm-helpers.h | 708ca9131402e4746f98e26f924ae60be4a0b183 |
| drivers/base/power/runtime.c | 2ee45841486bc73225b3e971164466647b3ce6d3 |

devm-helpers.h 实际提供普通/延迟工作的初始化与自动同步取消，采用 devm_add_action；不宣称异步控制流没有托管封装。or_reset 仅登记失败立即执行，登记成功后的 probe 回滚由核心处理；release_action 执行并摘下记录，remove_action 只摘记录。timer_shutdown_sync 切断最终重排，timer_delete_sync 用于有共同门控的可恢复暂停；hrtimer 不存在正文原先声称的 wheel。

runtime_resume_and_get 失败平衡使用计数，get_sync 失败仍保留增加；runtime.c 的 rpm_resume 在同步路径可等待正在执行的 suspend，所以 suspend 中同步取消一个正在等待恢复的工作可能形成环。pm_wakeup_event 的毫秒参数表示事件处理保护区间，不是未来唤醒闹钟；设备唤醒状态、计时域与实际硬件唤醒分别说明。

完整无硬件模块以 timer→work→timer 环更新托管样本内存，明确LIVE/PAUSED/DEAD与排队/运行状态正交，最终动作执行并摘除，暂停保留恢复能力。提取回调、暂停恢复、probe与remove至GCC/Clang GNU11/O2顺序桩模型，覆盖两处分配失败、动作登记失败、启动后失败回滚、四种停止窗口、暂停恢复、工作中闭门后不重排；三个反例被拒绝。模型不证明真实锁并发、弱内存、设备核心回滚或电源管理执行，未进行目标Kbuild、装载、IRQ、系统睡眠或runtime PM实测。

## 1.180\_系统睡眠的闹钟委托与唤醒交接

2026-10-17 复审[系统挂起与定时唤醒](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P11_电源管理_系统挂起与定时唤醒.md)，新增核对官方不可变提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 下列文件；复用上一节同提交唤醒源、设备PM和时间管理证据。外部工作树不可访问，本轮采用官方固定提交的原始文件，不宣称重新验证本地配置。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| kernel/time/alarmtimer.c | 8bf888641694918dbbd72de297adedae1aa0386d |
| include/linux/alarmtimer.h | 05e758b8b894541c1388b8ec97300dc8573fa317 |
| include/linux/pm_qos.h | 4a69d4af3ff8e954c9e15b5879a32ea08d0d980d |
| kernel/power/qos.c | 4244b069442e08effee8f580231e670a6a87a4a0 |
| drivers/rtc/dev.c | c4a3ab53dcd4b7280a3a2981fe842729603a1feb |
| drivers/rtc/interface.c | aaf76406cd7d7d2cfd5479fc1fc892fcb5efbb6b |
| drivers/rtc/sysfs.c | e3062c4d3f2ce1fbf2ce70c9cbaa13853fc60b92 |
| include/uapi/linux/rtc.h | 97aca4503a6a390319ca7a53e8b19748912b8b19 |
| Documentation/admin-guide/pm/sleep-states.rst | ee55a460c639bcbaee09a1fa9b4e555895ce730c |
| Documentation/ABI/testing/sysfs-class-rtc | 95984289a4ee69fd9990e3d2d05a0d59e8785ac9 |
| Documentation/admin-guide/rtc.rst | 688c95b11919e780fcbe006304433f3b9936b7cd |
| include/linux/math64.h | 6aaccc1626ab4c6d38d7b4cb6cd3820ba41f628f |
| kernel/time/Makefile | 4af2a264a16073046b5dadba4ce4f8816dfa0a5b |

alarmtimer 运行时借助 hrtimer，挂起时按时间类型求最近剩余量并委托选定RTC；选取条件包含RTC闹钟能力与父设备唤醒许可。无RTC时该挂起钩子返回，但不能据此声称硬件唤醒成立。基线对不足两秒的目标可拒绝挂起，对硬件范围受限目标可提前安排唤醒；消费者的软件目标与RTC硬件目标分层。alarm_start 返回void；alarm_cancel不永久禁止其他生产者重排。回调默认路径不得睡眠，不外推所有实时配置的硬中断上下文。

RTC字符接口的ALM_SET接收rtc_time，WKALM_SET接收rtc_wkalrm；文件关闭保留一次性闹钟。sysfs +N以RTC当前时间为基准，不能与系统REALTIME或NTP状态等同。RTC核心队列、aie_timer和底层驱动IRQ有明确所有权，不能让消费者虚构第二根IRQ。CPU延迟约束按本版cpu_latency_qos_*登记、更新和撤销；不是原稿旧PM_QOS_CPU_DMA_LATENCY调用。

正文完整alarm周期模块与RTC工具分别经GCC/Clang GNU11/O2宿主模型检查：930000组相位输入、溢出、启动失败、四种退出窗口、工作结束后重读时间、事件保护与闭门；136组跨月/闰日/跨年日历转换及错误日期、参数、ioctl失败和结构传参。三种删除门控/工作排空/事件保护的反例被拒绝。模型使用原始RTC结构布局，系统调用、锁、闹钟、工作和PM由桩替代，日历转换使用宿主库适配；不等于目标Kbuild、Linux用户态编译、真实并发、系统挂起或RTC硬件实测。

## 1.181\_定时对象的事件观测与配对边界

2026-10-17 复审[定时调试与验证](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P13_调试_验证与常见陷阱.md)，核对官方不可变提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 以下事件定义、跟踪说明及配置；复用同提交timer/workqueue/alarmtimer与已审KASAN边界。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| include/trace/events/timer.h | 1ef58a04fc5798011fd664e115cceb86d91a10c8 |
| include/trace/events/workqueue.h | b0de2bc9ed52fa25af65be6126144419b92f6287 |
| include/trace/events/irq.h | 837c1740d0d07ab23f401e1d6e2d2c2dd1886bb0 |
| include/trace/events/alarmtimer.h | 13483c7ca70b3ddb35f715de4cb2830bf5d8ae85 |
| Documentation/trace/ftrace.rst | 4073ca48af4ad2639c2289eb28c691a5c3538850 |
| Documentation/trace/events.rst | 759907c20e7591ce3f03ac191cbc14a29b242f8a |
| lib/Kconfig.debug | e48375fe5a50ce1e633f3687ec84e464256568c9 |
| kernel/trace/Kconfig | 721c3b221048a9de000f785620889d537e8795f2 |

timer与hrtimer事件同属timer组，硬IRQ与softirq事件同属irq组。timer_start的expires/bucket_expiry/now以jiffies表达；hrtimer载荷使用其时间基准，不直接与任意trace前缀相减。delayed_work的queue事件发生在非零延迟已到期并真正入队时；queue/activate/execute分属不同阶段。摘除/cancel事件不是同步取消返回；remove开始后可有合法在途工作，需与同步完成和释放边界比较。

ftrace说明核对实例、跟踪时钟、静态trace与消费式trace_pipe、函数图根和过滤器差别、每CPU缓冲区丢失计数。用户工具以trace-cmd官方record手册核对-B及事件归属；工具版本需在目标环境再次确认。正文以完整timer_lifetime_demo生命周期采集，保存事件格式、模块卸载前的符号地址与丢失统计；Python工具分别保存waiting/running证据，避免新轮入队覆盖旧轮执行，并用保存的地址识别卸载后失去符号名称的记录。地址隐藏或不唯一时拒绝配对。

提取Python配对器进行正常样本、运行中新一轮、交错对象、其他函数过滤、取消待执行、分位点和十类无效记录检查；示意数据明确非实测。所有shell块经Git Bash语法检查，事件名称与固定头文件核对；未执行Linux tracefs采集、模块装卸、function_graph、trace-cmd、KernelShark或板级性能测量。Shell语法检查不是仓库format.sh的运行，后者依然使用既定Python检查体。

## 1.182\_中断通知与GPIO级联的职责边界

2026-10-17 复审中断专题前两章，核对官方不可变提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 的以下文档；外部工作树不可访问，使用官方固定提交原始文件，不宣称重新验证本地配置。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| Documentation/core-api/genericirq.rst | 25f94dfd66fad42ee07e02c9f15f6c2e9cb4189a |
| Documentation/core-api/irq/irq-domain.rst | f88a6ee67a350f5b480e894023ca29b960ba31be |
| Documentation/driver-api/gpio/driver.rst | ae433261e11a061e680d29b6dbeaf8960e882ff0 |
| Documentation/devicetree/bindings/interrupt-controller/interrupts.txt | 4ebfa00087816f14149a944891367ab3ae375b30 |

轮询可以与设备自主运行重叠，中断改变通知与进入处理方式，不替代数据存储和搬运。GPIO共享父级输出依靠本地状态解复用；GPIO irq_chip是软件控制操作，不是额外硬件块。级联分派与hierarchical irq_domain的逐层资源分配不能等同；编号需同时保留域身份，Linux IRQ不是跨启动恒定硬件身份。

教学图明确区分硬件状态、软件队列和各层完成动作。GPIO清除顺序采用显式边沿锁存、按位确认与父级电平汇总假设，不外推为实际芯片寄存器协议；寄存器表只列功能职责，不编造地址。源码文档内的历史旧入口及静态描述符数组叙述不作为Linux 6.12当前具体实现的证明。具体函数、结构及固件解析在后续章节另行核对。

本轮为机制与纸面时间线复审，图表和算术检查不等于板级测量、Linux运行或目标编译。GIC产品硬件细节保留唯一专题，本章不重复维护另一套产品布局。

## 1.183\_中断对象映射\_流程分派与消费者登记

2026-10-17 复审[Linux中断对象](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P03_Linux_的中断抽象_irq_chip_irq_domain_与_irq_desc.md)，核对官方不可变提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 以下文件。外部工作树不可访问，使用官方原始字节并记录blob身份，未重新声明本地配置或执行结果。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| include/linux/irq.h | fa711f80957b60157f38eceffa154b7b9469d3cd |
| include/linux/irqdesc.h | fd091c35d5721eee37a2fd3d5526559671d5048d |
| include/linux/irqdomain.h | e432b6a12a32f9f16ec1ea2fa8e24a649d55caae |
| include/linux/interrupt.h | 457151f9f263d9042625decde2c15822d860b134 |
| kernel/irq/handle.c | 9489f93b3db34431aa71594a95d7a20e6ec1442c |
| kernel/irq/chip.c | 271e9139de77f5348539c95e3463c4fb7585df10 |
| kernel/irq/manage.c | f0803d6bd2969820443f25b5b508ec553f4cb9a2 |
| kernel/irq/irqdomain.c | e0bff21f30e0a997464b48cc2bdfa67196d76009 |
| kernel/irq/irqdesc.c | 1dee88ba0ae449a7c83d5cfa8fe6b69f83b3b6d6 |
| kernel/irq/devres.c | b3e98668f4dd87e725626ad55f48a34d117e1c67 |
| drivers/base/platform.c | 6f2a33722c5203ac196a6e36e153648d0fe6c6d4 |

irq_domain反向映射保存irq_data关联，由domain与hwirq解析到描述符；irq_to_desc在SPARSE_IRQ下使用Maple Tree，非稀疏配置使用数组。irq_data将本层来源、chip和私有数据连接起来；desc承载核心软件状态，不是设备硬件状态的完整镜像。action的handler与desc的handle_irq分别处理设备业务和IRQ流程。

handle.c的普通事件分派遍历全部action并按位汇总返回值，不在第一个IRQ_HANDLED时停止。handle_irq_event标记处理中并在调用设备回调时释放desc锁，再重新取得；私有数据不因此自动同步。线程唤醒和结束通过action标志、desc线程计数及等待队列交接。

manage.c检查共享等约束并按自动使能策略启动，普通路径可能在请求调用返回前进入处理。free路径按dev_id摘除action，必要时关闭最后消费者的线路，等待在途处理并停止相关线程；不替驱动排空独立work/timer。devres封装记录释放动作，不改变首次回调窗口。platform_get_irq返回负错误，不能转无符号后继续登记。

正文提供对象成员与回调表、正常电平流程和退出边界；没有将示意映射号当作本机资源，也没有把机制图当作实际内核运行验证。本轮未执行目标内核编译、真实IRQ、强制线程化或设备卸载实验。

## 1.184\_中断资源归属与GPIO线程登记

2026-10-17 复审[中断注册与接口](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P04_Linux_驱动中的中断注册与接口.md)，复用上一节固定提交对象、manage、devres与platform证据，并核对官方不可变提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 以下文件。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| drivers/of/irq.c | 1fb329c0a55b8c0559b79b77a3f8f0cbc1101e1c |
| drivers/char/misc.c | f7dd455dd0dd3c7c9956e772b5ab8bd83a67a4a6 |
| include/linux/gpio/consumer.h | db2dfbae8edbd12059826183b1c0f73c7a58ff40 |
| drivers/gpio/gpiolib.c | 209871c219d697d05ac12c9a20edeb68ba59c5d8 |
| include/linux/platform_device.h | 7132623e4658576f0be2a80b493e445e0c9d4de6 |
| include/linux/irqreturn.h | d426c7ad92bfdeb2c9df104f1c055ee684f84f81 |
| include/linux/interrupt.h | 457151f9f263d9042625decde2c15822d860b134 |
| Documentation/driver-api/gpio/consumer.rst | bb3366047fadb51bd9323f51d78cbf237a370d11 |

misc_register创建字符接口设备，不自动赋予真实硬件的of_node或资源所有权。of_irq_get以正数成功、0映射失败和负错误表达不同结果；irq_of_parse_and_map返回无符号数并以0失败，不能丢掉节点引用与映射各自的生命周期。platform_get_irq和gpiod_to_irq提供负错误，保留EPROBE_DEFER。

GPIO输入先取得描述符并设置方向，再映射IRQ；cansleep读取放在线程中，逻辑active-low与物理触发配置分开。ONESHOT保护线程阶段而不保证每边沿独立执行，不能消除共享线路长时间屏蔽代价。禁用/启用有层数，synchronize_irq不停止未来请求，控制器总线回调约束仍适用。平台remove在本版为void。

完整irq_input_demo使用专用GPIO双边沿线程，所有依赖在登记前完成；移除先devm_free_irq同步并摘除记录，再读取最终调用计数，设备核心归还GPIO和内存。无额外work、timer、用户文件或重排路径；增加这些使用者后必须重新设计退出协议。DTS是标明控制器与索引占位符的板级模板，不声称对应实际板型或已通过dtc。

提取正文完整C到GCC与Clang GNU11/O2宿主桩模型，覆盖四个probe失败、提前回调与移除窗口的四种组合、GPIO读取失败和资源逆序/不重复释放；删除同步或推迟上下文初始化两种反例均被拒绝。所有shell块仅作语法检查。桩替换了内核资源管理、IRQ线程与GPIO访问，不证明真实并发、目标Kbuild、模块装卸、设备树构建部署或板级电气行为。

## 1.185\_触发条件与GPIO分层确认

2026-10-17 复审[GPIO触发语义](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P05_GPIO_与触发语义_电平_边沿_DTS_与_只来一次一直来_为何出现.md)，复用上一节GPIO消费者、IRQ登记和释放证据，以下文件来自官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| drivers/gpio/gpio-mxc.c | d8d1ae23e0935f85548d61cdbdc8a0189f14dfb6 |
| kernel/irq/generic-chip.c | 32ffcbb87fa126cc32c2cb45434415d08e79a4a7 |
| Documentation/devicetree/bindings/gpio/fsl-imx-gpio.yaml | 6b06609c649ed805f646489de5463249a028dddd |
| include/dt-bindings/gpio/gpio.h | b5d531237448241f1870e7d8d4cf1a6411877562 |
| include/dt-bindings/interrupt-controller/irq.h | 9e3d183e138191fe5227cb5ffc5ff52d81036b15 |
| kernel/trace/trace_printk.c | 29f6e95439b67c1949826f7ea9f1137665f49800 |
| include/linux/jiffies.h | 5d21dacd62bc7d5187af9d96ec288c6517557475 |
| include/linux/kernel.h | be2e8c0a187e4b26a704ee5e628b7afe404d9211 |

GPIO逻辑有效与物理边沿/电平是不同接口语义。gpio-mxc的imx35布局在ICR中编码低、高、升、降，另由EDGE_SEL选择双边沿；无该寄存器的旧布局走切换检测方向分支。绑定允许imx6ul加imx35兼容项，但不能据此推出板级引脚和运行配置。

mx3_gpio_irq_handler读取ISR与IMR交集并分派子来源；generic-chip的irq_gc_ack_set_bit只写本irq_data的mask到ack寄存器。消费驱动不应自行清整组GPIO状态；GPIO确认不替代外设消除持续请求，也不替代父控制器结束。正文嵌入该布局寄存器偏移、用途及访问责任，区分实现证据与物理板级证据。

三个按键点灯实验合并为一个完整可选模式模块：硬回调、线程、首沿固定窗口门控；日志与线程人为延时另作变量。首次事件单独标志、get_jiffies_64与time_before64、参数上限、睡眠能力检查、硬件去抖失败返回和同步注销形成完整边界。64位时间避免32位长时间无事件后的半周期歧义；比较仍遵守半周期契约。trace_printk仅用于开发调试，缓冲写入和内存有成本，不保证不丢记录。硬件去抖参数为0只是不请求配置，不承诺清除以前配置。

本轮未执行目标Kbuild、设备树编译部署、真实中断/波形或系统挂起唤醒。板级模板有明确占位符，程序不宣称为某开发板实测驱动；宿主模型只能检查抽取代码在桩约束下的分支和顺序。

## 1.186\_中断诊断观察范围与事件采集

2026-10-17 复审[中断诊断](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P06_Linux_中断的调试与诊断.md)，从官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 核对以下原始文件。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| kernel/irq/proc.c | 9081ada81c3d023a531d65e5a35a31f4d4ebbe55 |
| kernel/irq/irqdesc.c | 1dee88ba0ae449a7c83d5cfa8fe6b69f83b3b6d6 |
| kernel/irq/manage.c | f0803d6bd2969820443f25b5b508ec553f4cb9a2 |
| kernel/irq/handle.c | 9489f93b3db34431aa71594a95d7a20e6ec1442c |
| include/trace/events/irq.h | 837c1740d0d07ab23f401e1d6e2d2c2dd1886bb0 |
| include/trace/events/sched.h | 9ea4c404bd4ef3efab26e0d1f135a329d21e69de |
| Documentation/trace/events.rst | 759907c20e7591ce3f03ac191cbc14a29b242f8a |
| Documentation/trace/ftrace.rst | 4073ca48af4ad2639c2289eb28c691a5c3538850 |
| kernel/trace/trace_irqsoff.c | fce064e205706f50e3b64f345b53c0f23bb226ee |
| kernel/trace/trace_functions_graph.c | ebb61ddca749d80ee947b501b8fda679955c1788 |
| Documentation/ABI/testing/sysfs-kernel-irq | 8910d0c4bcd8af21a644fef83de4a03e7c2d9672 |
| drivers/base/pinctrl.c | c22864458511c10194e7f334219da5603df360c1 |

show_interrupts按在线CPU显示统计，并跳过hidden、无action、chained或无kstat存储的描述符，不能由父行缺失判断GPIO级联没有工作；共享action列表不互相覆盖。IRQ sysfs的type只依据软件level标志输出level/edge，per_cpu_count按possible CPU排列，不能等同于引脚极性和寄存器测量。

handle.c在action->handler前后放置irq_handler_entry/exit，manage.c在线程路径另行调用thread_fn并完成oneshot。irq_handler_exit原始ret保留整数，但TP_printk把所有非零值显示为handled，包含IRQ_WAKE_THREAD；文本handled不证明线程完成。sched事件的目标pid/prev_pid/next_pid与记录行的当前任务身份不同。

正文的事件采集脚本创建独立tracefs实例，设置global时钟、单IRQ事件和可选线程调度过滤，保存事件格式/过滤、前后计数、trace和每CPU统计，并用退出陷阱清理自己的实例。不接管根实例的函数跟踪或trace_printk输出。源码文档的部分历史实例描述不作为所有当前内核功能限制，实际能力仍按本机接口核对。

pinctrl设备核心处理默认/初始化状态，消费驱动无须为了诊断无条件重复选择；错误和延迟探测必须按实际协议处理。gpioinfo用法另核对官方libgpiod 2.2.4文档，只观察线路信息，不将其当作电平读取或抢占线路方法。irqsoff测量本地关中断区间，不能代替线程等待/业务响应耗时。

本轮仅进行源码、正文与脚本宿主验证，未在真实Linux内核运行tracefs采集、GPIO输入、pinctrl切换或延迟测量。示意计数与时序不作为测量结果。

## 1.187\_下半部选择与有界事件交接

2026-10-17 复审[下半部选择](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P07_中断下半部机制与驱动中的选择.md)，以下文件重新从官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 取得，另复用前述IRQ生命周期、GPIO消费者和平台remove证据。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| include/linux/workqueue.h | 59c2695e12e7674d5bc4cdd1fb416074374f981c |
| kernel/workqueue.c | a9d64e08dffc7c7aef2caae2f170268d83e99e23 |
| include/linux/interrupt.h | 457151f9f263d9042625decde2c15822d860b134 |
| kernel/softirq.c | 8c4524ce65fafe508c3b4ee1ccb371f29b2dd6d5 |
| Documentation/core-api/workqueue.rst | 16f861c9791e4ae2914bbf517c1a1835263711bf |
| Documentation/driver-api/basics.rst | d78b7c328ff7b3e179e36d8c91eff9ba6483ef81 |
| include/trace/events/workqueue.h | b0de2bc9ed52fa25af65be6126144419b92f6287 |

schedule_work投递到system_wq；queue_work_on对已pending或disable状态不新排队，false不等于业务完成。cancel_work_sync的完成保证以没有竞争投递为前提。工作队列概念及源码索引保持既有权威位置，当前章仅说明消费驱动的业务状态与使用协议，不复制内部唯一实现。

softirq使用固定向量与每CPUpending标志；tasklet另持对象状态及每CPU链表，在调度时可合并且同一对象串行。头文件明确deprecated，不能外推成整个softirq废弃。WQ_BH执行于softirq而不能睡眠，普通线程工作队列的睡眠说明不适用于此分支。

完整gpio_work_demo分别支持硬回调直接投递与IRQ线程转交。raw_spinlock保护计数、停止门和所有schedule位置；待取计数最多64，工作回调一次取最多8份局部批次，满则记录overflow。64位首沿时间窗口与稳定去抖区分。remove关闭门、同步撤销IRQ、同步取消work后才统计并归还资源，未取业务明确计入discarded。

正文不宣称目标Kbuild、真实工作池调度、GPIO硬件或PM已验证；板级节点是需替换控制器与索引的模板。源码证据不支持原文无条件工作队列最安全、任意devm_work_autocancel版本可用或单pending位保存所有事件的结论。

## 1.188\_定时采样与IRQ再使能协议

2026-10-17 复审[中断与定时采样](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P08_中断_x_高精度定时器(hrtimer)_去抖_延时确认与一次性中断再使能.md)，以下文件从官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 取得；GPIO描述符、platform remove及devres接口复用前述固定证据。外部标准工作树未恢复，本次不声称重新核对其配置。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| kernel/irq/manage.c | f0803d6bd2969820443f25b5b508ec553f4cb9a2 |
| kernel/time/hrtimer.c | db9c06bb23116a0d76d972477b40afd43b9b6d8f |
| include/linux/hrtimer.h | 6caaa62d2b1f893439129bdbe42135ef1da13f8b |
| drivers/gpio/gpio-mxc.c | d8d1ae23e0935f85548d61cdbdc8a0189f14dfb6 |

disable_irq_nosync不等待已有处理完成；enable/disable嵌套，enable在IRQ上下文使用受irq_chip总线锁条件限制。IRQ线程不能使用等待自身完成的disable_irq。默认primary配合ONESHOT仅提供线程处理阶段管理，不能替代驱动取得的显式禁用配对。

hrtimer的REL/ABS规定时间参数含义，PINNED在启动时起作用；回调执行域区分普通默认、SOFT及RT变化，不授予睡眠权限。NORESTART不阻止外部再次start，cancel等待当前回调但不永久关闭生产者。固定gpio-mxc使用每引脚状态位，屏蔽与pending处理不能推出所有物理边沿被计数保存。

完整教学例子用mutex保护masked所有权与stopping，IRQ线程取得禁用后启动timer，timer只排work，worker采样后配对enable；remove在关门后归还禁用，锁外同步IRQ、timer、work。它只实现屏蔽后单次采样，不实现连续稳定证明或按压/释放协议；末沿静默窗口仍使用定时器专题中唯一完整generation/deadline实现。未宣称目标Kbuild或硬件实验通过。

## 1.189\_中断亲和性与IPI工作状态

2026-10-17 复审[多核中断与IPI](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P10_SMP_与中断亲和性_IPI_机制.md)。以下源码及文档来自官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，包含显式区分的ARM/ARM64编译分支，不代表历史单核工作树配置运行过SMP实验。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| kernel/irq/proc.c | 9081ada81c3d023a531d65e5a35a31f4d4ebbe55 |
| kernel/irq/manage.c | f0803d6bd2969820443f25b5b508ec553f4cb9a2 |
| kernel/irq/cpuhotplug.c | 15a7654eff684f06dac92b98e36f2287c017469b |
| drivers/irqchip/irq-gic-v3.c | a3417461abe15616a64972427f635ba6118c3e35 |
| kernel/smp.c | f25e20617b7eb7a507b002b18672ab197f2ee8e5 |
| arch/arm/kernel/smp.c | 3431c0553f45cdf5f0b1b8dbb59bf38003da9995 |
| arch/arm64/kernel/smp.c | 2478e8d2ec88402356342b54525fc637fd6e72f9 |
| include/linux/smp.h | f1aa0952e8c30b47b58ffcda4757d137c1790342 |
| include/linux/interrupt.h | 457151f9f263d9042625decde2c15822d860b134 |
| Documentation/core-api/irq/irq-affinity.rst | 29da5000836a973d8272a431189fc44e2a142607 |
| kernel/sched/core.c | 1f817d0c5d2d0e04a8d48836bf465eb7585d16d8 |
| kernel/sched/idle.c | 53bb9193c537a8179a61d834eee7b6140e9f9082 |

proc显示请求/待迁移掩码与effective掩码的不同来源。irq_can_set_affinity_usr排除managed，通用层还检查可平衡性及chip回调。GICv3 SPI设置从请求与在线集合选单CPU，转换硬件亲和性并更新单CPU effective；这不等于硬件1-of-N广播或轮询。IRQ线程通过IRQTF_AFFINITY在自身路径调整可运行集合。

irq_update_affinity_hint只保存提示指针。irq_set_affinity_and_hint路径会尝试设置，但__irq_apply_affinity_hint没有透传内部设置返回值；返回0不能单独证明路由成功。提示存储必须有足够寿命并在销毁前清除。热插拔迁移对普通IRQ可打破无在线目标的原集合，对managed可关闭IRQ并保留集合。

kernel/smp.c使用目标每CPU的call_single_queue与CSD记录。llist_add在原队列为空时进入通知路径；目标摘队列后，SYNC类型先执行函数再release清标志，ASYNC类型可先清标志再执行已保存func/info。发送方同步等待采用acquire观察；异步节点复用不是参数对象生命周期结束。目标为本CPU有本地直接执行分支，远端离线可失败。

ARM与ARM64函数调用IPI通过架构分派进入通用队列处理。GICv3发送器在生成SGI前执行dsb(ishst)，之后使用isb，寄存器只携带SGI与目标信息。不是每次CSD都有独立IPI，也不能从IPI计数断言GPIO链路正确。正文只展开消费者流程与状态归属，不复制上游函数体；脚本试验需目标多核Linux，未宣称实机迁移或IPI测量。

call_function_single_prep_ipi还可通过set_nr_if_polling原子设置目标idle任务的thread_info.flags，在polling承诺成立时省掉IPI。do_idle退出轮询后清polling、执行smp_mb__after_atomic，再flush_smp_call_function_queue；省掉通知的成本由共享标志与轮询协议承担，不等于没有通信。

## 1.190\_系统唤醒IRQ与设备电源交接

2026-10-17 复审[中断的系统唤醒与运行时恢复](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P11_电源管理中的中断_suspendresume_与唤醒源.md)。下列材料从官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 获取，芯片与固件的物理唤醒路径仍以目标集成及试验为准。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| Documentation/power/suspend-and-interrupts.rst | f588feeecad0fccb7d8f6a169cb93d24a273960a |
| Documentation/power/runtime_pm.rst | 12f429359a823eeb2ed34eff169dab22924e2d98 |
| Documentation/devicetree/bindings/power/wakeup-source.txt | 27f1797be96375c4aa6969b0c3d09679fed8388d |
| kernel/irq/pm.c | c556bc49d2137ea30f27d18016b2104d5c48f456 |
| kernel/irq/manage.c | f0803d6bd2969820443f25b5b508ec553f4cb9a2 |
| drivers/base/power/wakeirq.c | 5a5a9e978e85f3fc9d89cb7d43527dc1dd42a9b1 |
| drivers/base/power/wakeup.c | 752b417e81290eb97d578de7f388b83201e52421 |
| drivers/base/power/main.c | 4a67e83300e164cfcaf216004df3786de3597369 |
| drivers/base/power/sysfs.c | a1474fb67db9b2f09812b293127730efc6655243 |
| drivers/pinctrl/core.c | b3eec63c00ba044da37d8f75ac1eb94ede13fc14 |
| drivers/gpio/gpio-mxc.c | d8d1ae23e0935f85548d61cdbdc8a0189f14dfb6 |
| include/linux/pm_wakeirq.h | d9642c6cf85211af603ce39e280a5b4de6617ee5 |
| include/linux/pm_wakeup.h | 76cd1f9f1365306ab45b2832767e400f09e503bc |
| include/linux/pm_runtime.h | d39dc863f612fe18dc34182117f87908d63c8e6d |
| drivers/input/keyboard/gpio_keys.c | 380fe8dab3b06379f9744e8190a4afcc0aee20b4 |
| Documentation/devicetree/bindings/input/gpio-keys.yaml | cc78c2152921308fe0cad3e29ca78a5fad08f066 |
| drivers/input/keyboard/Kconfig | 3bb67dc9ea0facb16d22527db50b5f6181a06ea2 |
| kernel/power/main.c | 6254814d481714f769a7ae95bad1310ba1bf1d45 |
| include/dt-bindings/input/input.h | 221253480b21c0786c13fe4fda3d14078763e20f |
| include/dt-bindings/input/linux-event-codes.h | 693bbcd2670a41ce72418214217c613d8ad52037 |
| include/uapi/linux/input-event-codes.h | 5a199f3d4a26a22a94e51d4b53aeed3767c97bce |
| include/dt-bindings/input/gpio-keys.h | 8962df79e753b7b80bf793d9d180154a543ef416 |

设备能力、power/wakeup策略、IRQ wake配置与软件wakeup_source活动相互区分。suspend_device_irqs在late后/noirq前处理普通与wake IRQ，irq_pm_check_wakeup将armed来源转为pending/suspended并报告PM，正常设备action不在此时照常执行。resume在noirq设备恢复后恢复IRQ，早于常规设备resume。NO_SUSPEND与同一设备enable_irq_wake不可作为双保险叠加；COND_SUSPEND仅用于满足来源判断和显式唤醒条件的特殊共享方案。

dev_pm_set_wake_irq关联已有工作IRQ，dedicated版本另行注册专用线程。helper调用底层enable/disable wake不透传每次错误，因此登记不能证明物理唤醒成功。runtime远程唤醒与system wake策略可不同；专用wake线程可同步恢复设备，不表示普通hardirq可以任意调用可睡眠恢复。

gpio-keys解析按键子节点的wakeup-source、debounce-interval和wakeup-event-action。GPIO分支正常双边沿，低有效asserted选择下降沿唤醒；suspend检查设置错误，部分失败回滚，resume恢复配置并报告状态。它自身PM回调未调用pinctrl sleep/default选择；仅声明sleep状态不会自动证明切换。硬件去抖失败后按GPIO读取是否睡眠选择hrtimer或delayed_work，不能用软件确认撤销已发生的物理唤醒。

输入常量路径include/dt-bindings/input/linux-event-codes.h在Git中为指向../../uapi/linux/input-event-codes.h的符号链接，表中对应blob是链接内容；KEY_WAKEUP实值143来自目标UAPI头，EV_ACT_ASSERTED为1来自gpio-keys.h。模板待替换板级引脚，不声称DTB构建或schema校验。

pm_wakeup_irq暴露当前记录首槽，无记录时返回ENODATA，不是全部物理来源或最后事件历史。wakeup_count读取/写回握手可拒绝竞争活动。正文保留原有deep/IRQ46日志作为历史观察，未将其升级为本轮实测；不执行真实挂起，也不根据gpio-mxc多SoC分支推断i.MX6ULL有同样pad固件通路。

## 1.191\_PCI消息中断的分配与生命周期

2026-10-17 复审[MSI与MSI-X的软件框架](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P12_MSI_MSI-X_与_PCIe_设备的中断框架.md)。下列原始文件来自官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，Linux 6.12.20。当前文档网站仅用于交叉确认，接口和结构边界以固定源码为准；未使用不可访问的本地源码工作树冒充现场核验。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| Documentation/PCI/msi-howto.rst | 0692c9aec66fd48cb3d35af20d44b55101c2bcc8 |
| drivers/pci/msi/api.c | b956ce591f964b41a7cac5e563dc7a2747026907 |
| drivers/pci/msi/msi.c | 2f647cac4cae3484276ecdf658537d9006a7f3d7 |
| drivers/pci/msi/irqdomain.c | d7ba8795d60f81622744c58f44db6dada5e8d7e9 |
| drivers/pci/msi/legacy.c | db761adef652b63f256987ac2f2fcf8925c35a2c |
| include/linux/msi.h | b10093c4d00ea5a2ce1c7ab10bab00a20bfca327 |
| include/uapi/linux/pci_regs.h | 12323b3334a9c16beda9e13a039cd38ae2cacfad |
| drivers/pci/Kconfig | 0d94e4a967d81d50b9a3f3aebda56d37b36fc271 |
| drivers/pci/host-bridge.c | afa50b446567f77a1aacbf09ed5a0de9e8e81881 |
| drivers/of/irq.c | 1fb329c0a55b8c0559b79b77a3f8f0cbc1101e1c |
| Documentation/devicetree/bindings/pci/pci-msi.txt | b73d839657b6673827176a6a247b9111676a778d |
| Documentation/devicetree/bindings/interrupt-controller/msi-controller.yaml | 449d6067ec88f36caf50e32bc2a8cece48f7aa9e |
| drivers/pci/pcie/pme.c | a2daebd9806cd7273ee331406201402a758bd7b8 |
| arch/x86/kernel/apic/msi.c | 340769242deaec10d03c462aa95f995ea4bafa23 |
| kernel/irq/msi.c | a83fdc053c2b10519b00ab57633a6f0e3faab929 |
| Documentation/ABI/testing/sysfs-bus-pci | 5da6a14dc326bd651d8cabb795d769f221daa81e |
| include/linux/pci.h | ef8624ee459dec8b20bac3ff10db0a3dad2f6a74 |
| drivers/pci/devres.c | 643f85849ef64b180e8f0c40a60a119272f1eb22 |
| Documentation/PCI/pcieaer-howto.rst | f013f3b27c8274a65dd746e2c073061c365f408c |
| drivers/pci/pcie/portdrv.c | 9700b211c821b17f18001d11f041702ae003b851 |
| Documentation/PCI/pci.rst | f4d2662871ab1cc1554220cc074340e5e562f649 |
| Documentation/devicetree/bindings/interrupt-controller/msi.txt | c20b51df7138a213111749c066c523477516b829 |

pci_alloc_irq_vectors_affinity按许可尝试MSI-X、MSI、INTx；固定名称为PCI_IRQ_INTX，仅min=1且dev->irq有效时可走该fallback。pci_irq_vector区分MSI索引、MSI-X表索引和INTx唯一索引，越界返回错误；pdev->irq或sysfs irq不能代替MSI-X集合查询。

PCI MSI-X能力头的Table/PBA字段在配置空间，完整表位于BAR窗口，每项16字节。pci_write_msg_msix在修改地址/数据时遵守mask协议。普通MSI描述符可包含多个nvec_used，MSI-X通常一项一描述符，不能把内核对象数量当成硬件向量的统一定律。

pcim_enable_device设置受管理身份，pci_setup_msi_context会调用pcim_setup_msi_release登记向量回收动作；只有devm_request_irq并不足以证明向量也托管。无论是否托管，handler须先于其向量资源回收，硬件停产与DMA完成并非devres隐含保证。IRQ名字和affinity hint指针不得引用已失效临时对象，亲和性合同沿用1.189。

pci_msi_setup_msi_irqs保留层级域与旧架构入口分支；域的parent关系不等于硬件串联。x86 MSI可修改APIC目标与向量，不能概括为必经IO-APIC。PCI的msi-map按RID区间和提供者标识映射，msi-parent不是所有平台唯一入口；msi-controller表示接收能力，不能给没有发送能力的设备增添硬件。

端口服务按消息编号分配MSI/MSI-X，PME服务挂起可尝试enable_irq_wake；不能说所有唤醒必须另拉GPIO。PM恢复同时依赖PCI状态、平台消息接收与业务设备队列，不能把恢复后不通知写成正常结果。正文只读sysfs脚本、IRQ资源组件及映射示例不构成真实PCIe硬件、目标Kbuild或设备树schema验证。

## 1.192\_特殊中断的屏蔽边界与PMU样本交付

2026-10-17 复审[特殊中断的用途与屏蔽边界](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P13_特殊与高优先级中断_NMI_定时器_性能中断.md)。以下来自官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，Linux 6.12.20；分别保留arm64、ARM32与x86实现边界，不以历史单核配置证明所有分支已运行。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| drivers/clocksource/arm_arch_timer.c | 03733101e23174a6a327c8388862caf5d541cf73 |
| drivers/perf/arm_pmu.c | 398cce3d76fc44dcc1e1f84f9e3c62b3574a8d12 |
| drivers/perf/arm_pmuv3.c | 0afe02f879b45a8600b9cea1bfd88e6a68096a67 |
| drivers/perf/arm_v7_pmu.c | 420cadd108e79821f43537fab23fe081ca6fd9bc |
| arch/arm64/Kconfig | 57a56500c79593f3fbd46bf5f1d778ad3a742e4b |
| arch/arm64/include/asm/irqflags.h | d4d7451c2c129f47216d51200d29297610122112 |
| arch/arm64/kernel/entry-common.c | 3fcd9d080bf2a9086ebe6a596e73dee63fd2cb7e |
| drivers/irqchip/irq-gic-v3.c | a3417461abe15616a64972427f635ba6118c3e35 |
| kernel/irq/manage.c | f0803d6bd2969820443f25b5b508ec553f4cb9a2 |
| arch/x86/kernel/nmi.c | ed163c8c8604e30afed3855c856b461a51c9561a |
| arch/x86/include/asm/nmi.h | 41a0ebb699ec64284832878191404a9cfa8ffbc4 |
| Documentation/admin-guide/lockup-watchdogs.rst | 3e09284a8b9bef75c0ac1607a1809ac3b8a4c1ea |
| Documentation/locking/locktypes.rst | 80c914f6eae7ab07476491c4eba96c004508e28d |
| Documentation/devicetree/bindings/timer/arm,arch_timer.yaml | c5fc3b6c8bd0bfeaf06867ab2d58b0be07729236 |
| arch/arm/boot/dts/nxp/imx/imx6ul.dtsi | 0926ea811f61cf9c7280635b4843d8658d71b6ad |
| include/linux/perf_event.h | 347901525a46ae7569073853682c3d5371325413 |
| kernel/events/core.c | 5fff74c736063c9be4af20bb2fc8863e3c71b413 |
| kernel/events/ring_buffer.c | 4f46f688d0d4909d5a307fdc0359b4c0a0d65955 |
| arch/arm64/include/asm/perf_event.h | eb7071c9eb343c0e6affac21747cadb247906b08 |
| include/linux/perf/arm_pmuv3.h | 3372c1b56486fcfcd2068ff3a49df35a35e8eeb5 |
| arch/arm64/include/asm/arm_pmuv3.h | 468a049bc63b59f75b6aa2e828021d8bb6b9cf6a |
| include/clocksource/arm_arch_timer.h | cbbc9a6dc571587db49b5e9b965f9e3610b83c55 |
| Documentation/devicetree/bindings/arm/pmu.yaml | 528544d0a1614c9f9bddaafa8dac4782d09ac7e9 |
| tools/perf/Documentation/perf-stat.txt | 2bc06367248691dd1320d5bbd1c433f1a8a7ddfe |
| tools/perf/Documentation/perf-record.txt | 242223240a08045e0e6e5c946737cdb8896f8294 |
| tools/perf/Documentation/perf-report.txt | 7c66d81ab978c84bf46244d9e4fccea28d2162db |
| Documentation/admin-guide/perf-security.rst | 34aa334320cad3d0f632429670cc31e727709dca |
| arch/arm64/include/asm/arch_timer.h | f5794d50f51dc371177cfb838b0c024cef31e4cf |

arm64固定实现的CONFIG_ARM64_PSEUDO_NMI利用GICv3优先级与ICC_PMR_EL1屏蔽普通IRQ，入口仍使用irq_desc/domain及generic_handle_domain_nmi；DAIF屏蔽与运行时集成条件仍然存在。request_nmi要求相应每CPU用途、禁止自动使能及irqchip能力，不能共享/线程化；perCPU版本需逐CPU prepare/enable及反向清理。x86 register_nmi_handler为宏，按NMI分类登记nmiaction；其分发考虑有限锁存会汇聚事件，不作无条件必达保证。

arm_arch_timer的每核分支request_percpu_irq并按CPU上线配置clock_event_device，timer_handler读控制状态、mask并调用event_handler；内存映射分支可request_irq。imx6ul.dtsi描述了disabled的armv7-timer节点与PMU SPI94节点，最终板级选择另行核对。tick/clockevent与可读取clocksource不同，NO_HZ和CPU覆盖必须纳入计数解释。PREEMPT_RT的spinlock_t irq后缀不按非RT语义改变硬IRQ状态，raw锁亦不能消除NMI同CPU自等。

armpmu_request_irq按perCPU描述符选择NMI尝试与普通IRQ回退，保存每CPU操作集；armpmu_dispatch_irq从注册的arm_pmu指针地址取出对象并调用其handle_irq。PMUv3读清溢出、更新事件与周期，perf_event_overflow处理采样/节流。perf环先写记录再发布data_head，消费者按协议更新data_tail；pending_wakeup与pending_irq的irq_work实现延后唤醒，不等于所有样本写入都延后。计数模式也可能通过溢出折叠短硬件计数器。

正文给出有限用户负载及stat/record观察过程，未实施真实目标perf采样、NMI生成、关IRQ锁死或启动参数修改。watchdog文档描述的perf后端需结合配置与CPU覆盖，未告警不等于无限范围的正确性证明。寄存器表仅列当前路径所用角色，不复制另一架构指令或厂商绝对地址。

## 1.193\_中断过载的隔离恢复与误报检测边界

2026-10-17 复审[中断风暴与有界恢复](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P14_中断风暴_抖动与防御机制.md)。以下读取官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，Linux 6.12.20；不把generic IRQ的未处理检测当作所有设备的高频限流保证。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| kernel/irq/spurious.c | 02b2daf074414f1340a4f25cf134145150d90c1b |
| kernel/irq/handle.c | 9489f93b3db34431aa71594a95d7a20e6ec1442c |
| kernel/irq/manage.c | f0803d6bd2969820443f25b5b508ec553f4cb9a2 |
| kernel/irq/chip.c | 271e9139de77f5348539c95e3463c4fb7585df10 |
| kernel/irq/internals.h | a29df4b02a2ed9c580bbaf7db0b8331543cb1a7e |
| kernel/irq/proc.c | 9081ada81c3d023a531d65e5a35a31f4d4ebbe55 |
| include/linux/interrupt.h | 457151f9f263d9042625decde2c15822d860b134 |
| drivers/gpio/gpio-mxc.c | d8d1ae23e0935f85548d61cdbdc8a0189f14dfb6 |
| drivers/input/keyboard/gpio_keys.c | 380fe8dab3b06379f9744e8190a4afcc0aee20b4 |
| Documentation/devicetree/bindings/input/gpio-keys.yaml | cc78c2152921308fe0cad3e29ca78a5fad08f066 |
| Documentation/networking/napi.rst | dfa5d549be9c03f863669f6396bf28fb6424a9a0 |
| net/core/dev.c | 7b7b36c43c82cc01f84554f5df3eb5e19bfa334d |
| Documentation/driver-api/gpio/consumer.rst | bb3366047fadb51bd9323f51d78cbf237a370d11 |
| Documentation/core-api/genericirq.rst | 25f94dfd66fad42ee07e02c9f15f6c2e9cb4189a |
| kernel/workqueue.c | a9d64e08dffc7c7aef2caae2f170268d83e99e23 |
| kernel/softirq.c | 8c4524ce65fafe508c3b4ee1ccb371f29b2dd6d5 |
| include/linux/irqdesc.h | fd091c35d5721eee37a2fd3d5526559671d5048d |
| kernel/irq/generic-chip.c | 32ffcbb87fa126cc32c2cb45434415d08e79a4a7 |
| kernel/irq/settings.h | 7b7efb1a114bd8132e45409fcc99c7a10492b5e6 |

handle_irq_event_percpu汇总action返回值并在调试条件成立时调用note_interrupt；线程返回HANDLED递增threads_handled，由后续硬入口比较。检测在irq_count到100000时检查irqs_unhandled > 99900；未处理间隔超过last_unhandled + HZ/10则重新计相应历史。达到条件时置IRQS_SPURIOUS_DISABLED、增加depth并irq_disable，安排定时轮询。try_one_irq只对符合条件的共享action等路径尝试，不承诺禁用后绝无handler执行，也不自动证明硬件修复。noirqdebug在注册配置阶段设置相应no-debug状态。

irq_thread_fn无论thread_fn返回值如何均调用irq_finalize_oneshot，解除条件涉及threads_oneshot、disabled与masked状态，IRQ_NONE不是保持屏蔽指令。disable_irq_nosync增加嵌套深度而不等待；disable_irq会同步，enable_irq只归还一层。不得在自身处理者或持有其所需锁时同步等待。work登记不保存每次业务事件，cancel同步须结合生产入口关闭，不能靠单个last_status字段代替业务队列。

gpio-mxc的imx35_gpio_hwdata给出IMR0x14与ISR0x18，irq_gc_mask_clr_bit及irq_gc_ack_set_bit分别修改屏蔽位和写ack位，父入口读ISR & IMR。该布局不能推广imx1等分支，控制器确认也不等于外设数据消费。gpio-keys解析debounce-interval毫秒，默认5；尝试gpiod_set_debounce微秒，失败软件回退，依gpiod_cansleep选择work或hrtimer。软件采样不证明窗口内连续稳定，不能将任意厂商自造属性说成标准binding。

NAPI文档说明napi_struct消费调度所有权、预算poll及设备队列通知mask；显式mask路径先schedule_prep再mask后提交，成功complete_done后才放行。budget为0、恰好耗尽及停止并发另有约束，不能以局部调用替代网卡完整生命周期。中断计数、设备业务量、缓冲积压和CPU执行时间分开解释。

正文恢复程序是离散事件的完整可执行模型，验证重复故障、有限重试、旧探测结果与停止交错，不是内核锁或硬件驱动。只读快照脚本不修改IRQ状态；未实施真实风暴注入、硬件复位、目标Kbuild或板级滤波配置。irq_spurious_proc_show输出保存的last_unhandled时间值经jiffies_to_msecs转换，不是距当前时刻的差值。

## 1.194\_fasync的订阅身份与标准信号合并

2026-10-17 重写[异步通知入口](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P01_异步通知全景与知识地图.md)，读取官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，Linux 6.12.20。以下是首轮证据集合，不据此声明整个信号、epoll或evdev实现均已逐函数复审。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| fs/fcntl.c | 3d89de31066ae030ff3d4d3ca2cfe44e1c81539e |
| include/linux/fs.h | b98f128c9afa78f02ea273a7f98b12c7761e7ae5 |
| include/uapi/asm-generic/fcntl.h | 80f37a0d40d7d4efefe406f73598f0f150bd9973 |
| include/uapi/asm-generic/siginfo.h | b7bc545ec3b2ceaaf373224c884a8bdc27a6e834 |
| fs/pipe.c | 12b22c2723b7ead48f8ffcaecee869631c68dd05 |
| kernel/signal.c | 2ae45e6eb6bb8ed9c9293efea2ee1c1b9fda00b5 |
| fs/file_table.c | 18735dc8269a10d6e7085c9bb882a0788dd3e0ca |
| fs/signalfd.c | 736bebf9359186c528e2310df395ba766e2739df |
| fs/eventpoll.c | 1a06e462b6efba8824456cffebad040720c4226a |
| drivers/input/evdev.c | b5cbb57ee5f60073a41d16da23a8d3fa6c09f04a |
| include/linux/poll.h | fc641b50f1298eaf463b8c42251ee0df363a7eb3 |
| include/uapi/asm-generic/poll.h | f9c520ce4bf4e4cd3128e298d0d06a415514046e |

fs/fcntl.c的setfl仅在FASYNC变化且f_op->fasync存在时调用回调；回调负责该标志，不能凭F_SETFL成功证明任意文件支持通知。fasync_insert_entry按fa_file匹配同一file，已有项更新fa_fd，新项发布并设置FASYNC；fasync_remove_entry清fa_file、摘链并kfree_rcu。include/linux/fs.h声明fasync接口，固定file的f_owner为指针并经file_f_owner访问，不能直接复制其他版本内嵌布局。

kill_fasync_rcu沿订阅取得file owner，经send_sigio检查目标和权限；所选signum来自owner配置，第三参数是POLL_IN等reason，不能把POLLIN位掩码或任意信号号混进参数。标准SIGIO与配置的实时信号有不同队列语义，后者发送失败存在回退SIGIO。kernel/signal.c的legacy_queue在同一种标准信号已pending时直接汇聚，不构成设备数据存储。

fs/pipe.c的pipe_fasync登记读/写端订阅，pipe_write在写后向fasync_readers请求SIGIO/POLL_IN，字节保存在独立管道缓冲。fs/file_table.c最终__fput在FASYNC与回调条件成立时先fasync(-1,file,0)，再调用release；关闭一个共享fd不保证已发生最终fput。evdev的file_operations也包含fasync，说明标准子系统事件语义与信号通知不是互斥分类。

首章完整实验仅使用进程自身管道：先屏蔽SIGIO并清除继承忽略处置，设置接收者/非阻塞/异步标志，三次小写入后同步接收一次信号，再读尽字节，最后撤销并关闭；进程退出前保持SIGIO屏蔽。未在当前Windows环境执行Linux信号内核路径，控制流模型和语法验证不能代替目标Linux运行或板级中断验证。

## 1.195\_异步通知的文件共享与等待基础

2026-10-17 重写[异步通知P02](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P02_Linux_IO_与信号机制基础.md)，继续核对官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，Linux 6.12.20。复用1.194的fcntl、pipe、eventpoll、signal和poll头文件，并增加以下原始文件；不声明整套VFS或信号实现已逐函数覆盖。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| fs/open.c | 5da4df2f9b18a637ed83cde8f2fe52f9034f5290 |
| fs/char_dev.c | 57cc096c498a29fc3bc2278a142a494ffbe5f3fd |
| fs/select.c | 834f438296e2ba97c87636e3c5d12d65c7b94030 |
| fs/file.c | 4cb952541dd036a57177b3ce29587cdffa186aaa |
| include/linux/fdtable.h | b1c5722f2b3ce4ba82e89716d5e27f85c67a8afd |
| include/linux/sched/signal.h | c8ed09ac29ac580d15e9bad0175e0507349faee9 |
| include/linux/sched.h | 0d1d70aded38f6be700c120c3529096b6a0be1db |

fs/file.c的dup取得原file引用并把它安装到新fd；close_on_exec在fdtable中单独维护。fs/open.c的do_dentry_open先取得inode操作表，fs/char_dev.c的chrdev_open再取得cdev的ops并replace_fops后调用驱动open，不能把inode->i_fop直接等同最终驱动表。最终fput的异步撤销路径见1.194。

fs/select.c的__pollwait登记文件引用、wait_address、key和pollwake回调；__pollwake设置triggered并唤醒polling_task，do_poll重新扫描。do_pollfd可直接产生POLLNVAL，错误/挂断加入过滤条件；include/linux/poll.h的poll_wait调用_qproc并建立检查顺序，本身不睡眠。fs/pipe.c的pipe_poll先登记rd_wait/wr_wait，再检查缓冲与读写端状态，可同时返回输入和挂断条件。状态检查与消费仍需驱动同步，登记顺序不能代替缓冲锁。

task_struct含blocked及pending，signal_struct含shared_pending，sighand_struct含action数组；通常多线程共享动作但屏蔽按线程维护。fs/fcntl.c的band_table把六种POLL_原因转换为掩码，send_sigio_to_task在配置signum路径设置si_code、si_fd及si_band，默认及失败回退路径为普通SIGIO；不能传POLLIN组合掩码作为reason，也不能凭SA_SIGINFO假定普通SIGIO带有效fd/band。

正文完整管道实验观察dup共享O_NONBLOCK、关闭一个引用后继续访问、空缓冲EAGAIN、POLLIN与POLLHUP并存、短读保留剩余字节与最终EOF。当前Windows环境未执行Linux ABI，控制流程模拟和编译检查不代替真实系统运行；用户契约另外以正文所列Linux man-pages核对。

## 1.196\_fasync的文件类型与设计边界

2026-10-17 重写[异步通知P03](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P03_fasync_机制的历史与设计动机.md)，在官方固定Linux 6.12.20提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 下复用1.194的fcntl、pipe和evdev证据，并核对以下文件：

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| drivers/tty/tty_io.c | dcb1769c3625cd0f7d1555260adfe6051139a305 |
| net/socket.c | 042451f01c6520b85496213e6b3d8d2116ab2533 |

tty_fasync持有tty锁，__tty_fasync按需要分配file owner并调用helper维护tty->fasync，新增订阅还走终端自己的owner处理。sock_fasync在socket锁下维护wq->fasync_list及SOCK_FASYNC标志，sock_wake_async按WAITDATA、NOSPACE、普通I/O和紧急条件选择是否发送及SIGIO/SIGURG；本章只用于证明文件类型的连接与差异，不作为完整终端或网络教程。

evdev_fasync使用file->private_data取得evdev_client，链头为client->fasync；__pass_event先写客户端缓冲，在EV_SYN/SYN_REPORT更新packet_head后请求SIGIO/POLL_IN。这证明标准输入业务可与fasync共存，并提供批次边界实例，不证明任意设备都应复用input包边界。

历史只保留Linux man-pages明确标注的SIGIO与4.2BSD关联、管道/FIFO自Linux2.6支持O_ASYNC，不虚构Linux内部helper的首次提交或阻塞、poll、fasync、input的单线演进。400条记录、3个稳定链项与4批处理为正文纸面推演，1200/12是给定完整遍历前提的访问机会，不是性能实测或完整内核成本。

## 1.197\_fasync注册发送与撤销的固定实现

2026-10-17 重写[异步通知P04](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P04_内核视角_核心数据结构与控制路径.md)，逐段核对官方Linux 6.12.20固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 的下列路径。正文统一到本提交，不沿用旧3.x/Android字段和伪调用链。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| fs/fcntl.c | 3d89de31066ae030ff3d4d3ca2cfe44e1c81539e |
| include/linux/fs.h | b98f128c9afa78f02ea273a7f98b12c7761e7ae5 |
| kernel/signal.c | 2ae45e6eb6bb8ed9c9293efea2ee1c1b9fda00b5 |
| fs/file_table.c | 18735dc8269a10d6e7085c9bb882a0788dd3e0ca |

setfl比较请求与当前FASYNC并检查回调存在，先调用fasync，负值退出、正值归零；SETFL_MASK不含FASYNC。helper新增先GFP_KERNEL分配，insert持file->f_lock、fasync_lock，按fa_file查重，已有项在fa_lock写锁下更新fa_fd；新项初始化后rcu_assign_pointer发布并置FASYNC。返回0可伴随fd更新，不代表完全未改状态；同标志F_SETFL并不调用helper。

fasync_struct.fa_lock为rwlock_t，fa_fd传递给扩展信号信息；file.f_owner是按需分配的指针。file_f_owner_allocate用cmpxchg协调并发分配，fown含file回指、pid对象引用、pid_type、uid/euid与signum；F_SETOWN调用force=1，F_SETSIG经f_owner_sig，不存在旧稿拼造的本版本f_setown_signum调用。

kill_fasync_rcu在RCU读侧逐节点取fa_lock读锁，检查fa_file和owner，SIGURG且未选扩展信号有独立跳过分支。send_sigio读锁保护目标读取，线程/线程组与进程组使用不同查找路径；send_sigio_to_task先sigio_perm（设置者与目标凭据及LSM）再按READ_ONCE(signum)选扩展或普通路径，扩展失败回退SIGIO。kernel/signal.c按pid_type选择pending位置，legacy_queue合并已有标准信号。未将可从中断调用外推为NMI或所有PREEMPT_RT锁配置安全。

fasync_remove_entry先节点写锁清fa_file，再摘链、kfree_rcu、清FASYNC，file和全局修改锁包围过程；未找到返回0。RCU保护节点存储、fa_lock保护file交接，各有职责。__fput在FASYNC与回调成立时先fasync(-1,file,0)，再release，随后file_f_owner_release；不是所有驱动都必须在release额外重复撤销。该协议不删除已生成pending，也不保活提前释放的驱动private_data。

正文局部源码摘录通过去注释与空白后的原文匹配核对；图与纸面状态表只解释本路径，不声明完成目标Kbuild、真实信号运行或硬件验证。

## 1.198\_VFS接入fasync的打开配置与最终释放

2026-10-17 重写[异步通知P05](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P05_VFS_与文件系统中的异步通知支持.md)，继续使用官方固定Linux 6.12.20提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。复用1.194～1.197的open、file_table、char_dev、fcntl、pipe、socket、TTY、evdev与signalfd文件，并增加以下原始字节证据：

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| include/linux/fcntl.h | a332e79b3207957d0b8b614679a151e986a07544 |
| fs/namei.c | 6795600c5738a56740bdeb2b4ec33f29545d918a |
| fs/ext4/file.c | f14aed14b9cf3d944b8d6726362ee5a69a9a0597 |
| fs/eventfd.c | 22c934f3a080effc1b838293c5a7932b900fda46 |
| include/linux/net.h | b75bc534c1b351955ac2ada9da09c52bd3bdae3e |
| drivers/tty/n_tty.c | 5e9ca4376d686e34395958d0e3595aab4df89be2 |

include/linux/fcntl.h的VALID_OPEN_FLAGS包含FASYNC；fs/open.c整理open参数，fs/namei.c的path_openat以open_flag分配file，file_table初始化f_flags。通用打开不自动登记fasync，直接open带O_ASYNC不能当作订阅成立，后续同位请求还可能不触发回调。用户契约按open(2) BUGS核对。

字符设备do_dentry_open先取得inode表，chrdev_open再按cdev->ops取得驱动表并replace_fops，然后调用驱动open。普通close在file_close_fd后执行filp_flush及__fput_sync，后者只有最终引用归零才进入__fput；filp_close的普通fput路径不能冒充所有close调用链。__fput的fasync条件清理先于release，正文相邻摘录与源文匹配。

pipe_fasync按FMODE_READ/WRITE分别登记两链，写链登记失败且含读模式时撤销读链；pipe_release对相关对端变化使用POLL_IN/POLL_OUT通知，不捏造POLL_HUP原因。pipe_write通知不只以空变非空判断，read/poll重新观察可能被其他读者改变的数据。

include/linux/net.h将fasync_list放在socket_wq内，socket含wq。sock_fasync在sk为空时EINVAL，之后调用helper却未传播返回值，以链是否为空更新SOCK_FASYNC并返回0。sock_wake_async参数为socket_wq指针、how、band，WAITD与SPACE检查标志，IO和URG分别走SIGIO/SIGURG，不是无条件通用封装。

TTY新增订阅还有默认owner处理；n_tty.c规范行结束发布canon_head后通知，其他接收分支发布commit_head，输出唤醒使用POLL_OUT。ext4_file_operations、eventfd_fops与signalfd_fops均未提供fasync；evdev客户端在报告边界通知，不只是严重错误告警。正文比较只覆盖所列实现，不概括所有文件系统或网络协议。

完整用户实验比较临时普通文件与管道的F_SETFL返回及回读O_ASYNC，恢复标志并关闭/删除自身资源；未设置owner、未写入数据，不验证信号交付。当前Windows下的编译与控制流模型验证不替代真实Linux ABI运行，也未进行目标内核Kbuild。

## 1.199\_软件fasync设备的数据等待与寿命协议

2026-10-17 重写[异步通知P06](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P06_字符设备中实现异步通知的基础模式.md)，以官方固定Linux 6.12.20提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 核对教学模块接口。复用前述fs/fcntl.c、fs/char_dev.c、fs/open.c、fs/file_table.c、include/linux/fs.h及poll.h，并增加以下原始字节证据：

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| include/linux/uaccess.h | 43844510d5d0facfcb08c7d32798fe7e0fdd804c |
| include/linux/wait.h | 2b322a9b88a2bd122d30e70a3d6eaa12c5cec244 |
| include/linux/mutex.h | 2bf91b57591b49e4668752e773419ae945f124da |
| drivers/base/dd.c | bcc1f28b71f4f554ec8cf279934d13bdc6acce9c |
| drivers/base/devres.c | 2152eec0c1352cc46bdef703123e9f2d718bf111 |
| Documentation/kbuild/modules.rst | cd5a54d91e6d29b5eb62b6d5192650aad08ba8d8 |
| Documentation/driver-api/driver-model/devres.rst | 5f2ee8d717b1deafb43a60b03dbc5d5bc6b4875a |

教学模块为仓库自写代码，不作上游摘录。软件write是唯一生产源，256字节共享流队列用mutex保护；用户复制按未复制长度计算实际进度，短读前移剩余字节，短写不覆盖既有内容。空/满阻塞路径解锁后wait_event_interruptible，READ_ONCE等待条件只作提示，重新持锁检查；poll_wait先登记再取同一锁检查。成功写入解锁后唤醒读者并kill_fasync，成功消费唤醒写者；仅有读权限的file可登记读就绪信号。

include/linux/uaccess.h的用户复制路径有might_fault并返回未复制量；copy_from_user短复制会补零未复制区域，但教学length只增加实际复制字节。不能将用户复制放在自旋锁内；mutex方案只适用于本章进程上下文，不外推硬中断。stream_open去除定位/位置串行语义，不代替数据锁。

模块数据对象静态存活，独立cdev_alloc对象由核心引用释放负责；设备号申请失败、cdev分配失败和cdev_add失败分别清理已取得资源，未发布cdev用kobject_put，正常退出cdev_del后归还设备号。fops及cdev设置THIS_MODULE，正常打开引用阻止正常卸载；无硬件解绑入口、不支持强制卸载。最终fasync撤销依赖已核对__fput协议，不虚构必须重复release的要求。

fs/char_dev.c的cdev_del说明既有file操作仍可调用。drivers/base/dd.c解绑先device_remove，再device_unbind_cleanup调用devres_release_all；托管内存不等待用户fd全部关闭才释放。正文比较手动/托管责任、跨解绑软件引用和生产者停止，未提供无寿命协议的伪platform模板。固定devres及Kbuild文档与正文在线官方说明交叉核对。

本章完整内核代码在GCC和Clang模型中检查初始化失败回滚、空满、部分用户复制、阻塞重查、方向订阅及两万次独立队列序列；完整用户接收代码各25种控制流场景、容量Python程序12种正常/错误模型，Bash和Python语法检查通过。模型替换内核/系统接口，仅验证教学控制流和状态约束，不代表真实调度、信号ABI、内核头文件或模块链接验收；当前Windows环境未完成目标Kbuild与实际装载，正文给出Linux上完整构建、有限事务、容量及阻塞实验和清理步骤。

## 1.200\_GPIO异步通知的采样与解绑协议

2026-10-17 重写[异步通知P07](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P07_中断驱动设备的异步通知实践.md)，以官方固定Linux 6.12.20提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 核对GPIO消费者、IRQ、延迟工作、misc入口和平台驱动接口。复用前章fs/file_table.c、fs/fcntl.c及用户复制接口，新增原始字节证据如下。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| drivers/char/misc.c | f7dd455dd0dd3c7c9956e772b5ab8bd83a67a4a6 |
| include/linux/platform_device.h | 7132623e4658576f0be2a80b493e445e0c9d4de6 |
| include/linux/gpio/consumer.h | db2dfbae8edbd12059826183b1c0f73c7a58ff40 |
| drivers/gpio/gpiolib.c | 209871c219d697d05ac12c9a20edeb68ba59c5d8 |
| drivers/gpio/gpio-mxc.c | d8d1ae23e0935f85548d61cdbdc8a0189f14dfb6 |
| drivers/input/keyboard/gpio_keys.c | 380fe8dab3b06379f9744e8190a4afcc0aee20b4 |
| Documentation/devicetree/bindings/input/gpio-keys.yaml | cc78c2152921308fe0cad3e29ca78a5fad08f066 |
| kernel/workqueue.c | a9d64e08dffc7c7aef2caae2f170268d83e99e23 |
| Documentation/driver-api/gpio/consumer.rst | bb3366047fadb51bd9323f51d78cbf237a370d11 |
| kernel/irq/manage.c | f0803d6bd2969820443f25b5b508ec553f4cb9a2 |

GPIO consumer文档与gpiolib实现确认get_value_cansleep在可睡眠上下文使用，active-low已转换为逻辑值，负值为错误；gpiod_to_irq取得该输入GPIO的Linux IRQ映射，触发类型另行设定。gpio-mxc的mx3_gpio_irq_handler按ISR和IMR求待处理位，mxc_gpio_irq_handler调用generic_handle_domain_irq；消费者不自行推算IRQ号或清上级中断状态。

gpio_keys_gpio_isr安排工作或hrtimer，gpio_keys_gpio_report_event取得逻辑状态并拒绝负读取结果；gpio_keys_setup_key尝试硬件去抖，失败进入软件路径，根据是否可睡眠选择计时方案，并使用上升/下降双边沿。教学驱动不冒充gpio-keys或NXP绑定；板级引脚未获本次硬件证据，不保留旧稿GPIO1_18即可上板的承诺。

workqueue的mod_delayed_work_on契约允许IRQ上下文重排，重排不撤销已执行采样。教学代码增加generation/last_edge/stopping自旋锁状态，工作在窗口之后读GPIO，提交前持data_lock再edge_lock检查代次与停止标志。记录为16字节小端序号加逻辑状态及保留字节，最新单槽覆盖，序号不是原始IRQ数；用户复制全成功才消费，等待循环重查，poll先登记再查状态，采样失败保存错误而非伪造释放。

misc_open持misc_mtx查表、设置private_data并执行驱动open，misc_deregister使用同锁撤销入口；教学open在绑定引用仍存在时取得kref。固定__fput先fasync撤销再release，release归还file引用。platform_driver.remove在本版本为void。教学probe最后才misc_register，失败先free_irq与cancel_delayed_work_sync；remove先撤入口、停止提交、同步IRQ和工作，再标记offline和通知，最后归还绑定引用。旧file只访问软件状态，GPIO由devm在解绑清理，软件对象独立kref管理，不等待所有fd关闭才remove，也不让devm提前释放旧file对象。

完整模块以GCC和Clang模型检查申请失败回滚、属性错误/零窗口、逻辑值与读取错误、代次变更和停止期间采样、时间回绕、复制失败保留、阻塞重查、离线剩余数据及最终引用释放。有限Python接收器18种正常/错误场景核对关闭、订阅恢复和30秒期限。模型替代内核与系统调用，只证明受测控制流；没有目标Kbuild、实际Linux ABI、真实并发或板上电气验收。

## 1.201\_流式异步通知的队列与限频证据

2026-10-17 重写[异步通知P08](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P08_流式设备与高级场景中的异步通知.md)，继续以官方固定Linux 6.12.20提交 dfaf2136deb2af2e60b994421281ba42f1c087e0 为证据身份。复用前章n_tty.c、tty_io.c、evdev.c、fs/fcntl.c与kernel/signal.c，新增下列原始字节证据。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| Documentation/input/event-codes.rst | b4557462edd7b3fef9e9cd6c2c3cb2d05bb531ab |
| include/uapi/linux/input.h | 2557eb7b056178b2b8be98d9cea855eba1bd5aaf |
| Documentation/core-api/dma-api-howto.rst | 0bf31b6c4383c8b36d73f7361a6cdb87d1f5dba6 |
| fs/proc/array.c | 5e4f7b411fbdb9d2d8153db76340be532334fbd7 |
| net/netlink/af_netlink.c | 775d707ec708a7ad6879f81f578ca3a1bd282879 |
| Documentation/userspace-api/netlink/intro.rst | aacffade8f84c362962dd342e2e3839661f432eb |

n_tty.c的input_available_p在规范且非EXTPROC时比较canon_head/read_tail；否则比较commit_head-read_tail与amt。poll且TIME_CHAR为0、MIN_CHAR非零时amt为MIN_CHAR，否则为1。n_tty_poll先登记read_wait/write_wait，再查输入及挂断、输出状态；接收路径先发布规范或非规范head再通知，不概括为空转非空唯一策略。read的完整VMIN/VTIME行为不能被poll局部谓词替代。

evdev_client各有buffer/head/tail/packet_head与fasync。__pass_event溢出后保留SYN_DROPPED和最新事件并调整packet_head；SYN_REPORT时设置packet_head并kill_fasync。event-codes规范要求客户端忽略直到下一个SYN_REPORT（包含）再EVIOCG*查询当前状态；不承诺补回相对增量、历史或丢失数。input_event包含时间字段及type/code/value，用户布局必须按目标头文件，不能使用省略时间的近似结构。

DMA HOWTO区分一致性与流式映射，说明一致性内存仍需适当屏障，流式复用可能需要CPU/设备访问同步。教学块所有权FREE/DEVICE/READY/CONSUMER是推理模型，不是API枚举；未把中断完成或mmap误作缓冲可复用凭证，也未宣称i.MX6ULL某ADC具有特定DMA能力。

Netlink intro建立消息族、命令、属性和组语义；af_netlink.c的netlink_overrun按RECV_NO_ENOBUFS条件报告ENOBUFS并增加sk_drops，多播投递失败可达此路径，不能当作可靠无损广播。fs/proc/array.c的task_sig取task_ucounts与UCOUNT_RLIMIT_SIGPENDING计数，取task_rlimit的RLIMIT_SIGPENDING作为上限；SigPnd/ShdPnd分别来自线程pending和共享pending的signal位图，不代表标准信号累计次数。

正文替换了旧稿提前出队再复制、满队列再插两项、每槽独立序号、限频跳过且无补发回调、dup/fork复制订阅节点等错误示例。采用显式count环形队列、丢新、队列外累计丢失报告与复制成功后确认，报告和数据交替责任防止持续溢出饿死数据。真实IRQ读取协议使用read_mutex串行读者、ring_lock保护索引，复制期间不推进tail；该证明依赖生产者不覆盖占用槽，不可直接套覆盖旧数据策略。

Python模型实跑8种容量共40000次队列操作，对照独立deque检查顺序/容量/丢失/复制失败；通知模型核对固定期限、无新输入补发、读空跳过、零间隔、迟到回调、每设备隔离及10000时刻的间隔约束。模型为单线程确定性模拟，不代表Linux锁并发、信号ABI、Kbuild或DMA硬件测试；真实数据源、计数ABI与硬件所有权仍需对应实现验证。

## 1.202\_用户态异步通知的接收与退出协议

2026-10-17 重写[异步通知P09](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P09_用户态编程_从_SIGIO_到_signalfdepoll.md)。沿用官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0 的 fs/fcntl.c、kernel/signal.c、fs/signalfd.c、fs/pipe.c 及文件关闭证据；新增原始字节记录如下。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| include/uapi/linux/signalfd.h | 83429a05b698e32fcc9be74827577058a5aec974 |
| fs/timerfd.c | 137523e0bb211389888283b1a87c25c1aa4991ef |
| include/linux/signalfd.h | 9a47c380bda8b1310d7d287cdddcf61e095daf1e |

signalfd_ctx只保存选择掩码；signalfd_poll先向current->sighand->signalfd_wqh登记，再在siglock下查看current->pending与current->signal->shared_pending。signalfd_dequeue调用dequeue_signal从调用线程可见pending取走记录，不能画成独立signalfd消息队列或永久绑定创建者。内核signalfd_notify唤醒该等待队列，标准信号的合并行为没有改变。

signalfd_copyinfo按siginfo_layout选择字段，SIL_POLL填ssi_band/ssi_fd，SIL_TIMER填定时器标识与overrun；ssi_tid不是接收线程TID。UAPI固定结构为128字节，正常read_iter输出整记录整数倍，缓冲不足一记录EINVAL，非阻塞无可见信号EAGAIN。kernel/signal.c的do_sigtimedwait同样使用dequeue_signal，在等待前后修改并恢复线程阻塞状态，返回信号号、EINTR或EAGAIN。

fs/fcntl.c的send_sigio_to_task区别signum零和非零：非零构造I/O siginfo并尝试发送，失败回退SIGIO；选SIGIO非零不使标准信号变成实时信号。用户必须先准备接收、再启用O_ASYNC、再检查已有数据；掩码属于线程，owner/signum/文件状态属于file，处置在线程组共享。dup/fork引用同一file，不因另一个调用者切O_ASYNC就自动改变owner。proc信号掩码和fdinfo的判断边界复用前章证据。

timerfd_read_iter以u64 ticks返回到期计数，周期重启补算积压，不是设备事件计数。正文完整程序用pipe订阅前AB、timerfd稍后C和关闭写端，分别检查启动现状、通知后读取、EOF；三种F_SETSIG模式统一接收回退SIGIO。有限数据/信号批处理保留继续责任，LT读取与ET扩展边界分开，使用单调时钟总截止时间，撤销源后关闭接收设施并保持信号阻塞至进程退出。

用户接口交叉核对Linux man-pages的signalfd、sigaction、sigtimedwait、F_SETSIG、pselect及signal-safety说明。纠正旧稿先开启再屏蔽、counter++保证事件数、read非异步信号安全、只事件线程阻塞、ssi_tid为线程ID、关signalfd取消通知等错误；保留多线程队列/IPC分发与共享fd交接的具体责任，不声称任意节点只换路径即可复用。

完整程序替换系统调用接口后，经GCC与Clang分别检查3种选择各54场景，包含部分数据读取、各阶段失败清理、EINTR/EAGAIN、回退SIGIO、短记录拒绝、EOF、定时积压、持续数据公平预算和超时。模型不能证明Linux libc ABI、真实信号调度或目标板行为；当前Windows环境未完成Linux头文件构建与实际运行，正文保留完整目标编译和有限运行命令。

## 1.203\_异步通知的并发与对象寿命证据

2026-10-17 重写[异步通知P10](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P10_并发_竞态与内存可见性问题.md)。继续使用官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，复用 fs/fcntl.c、fs/file_table.c、fs/char_dev.c、fs/select.c、include/linux/wait.h、include/linux/poll.h 与前章设备退场证据。新增原始字节如下。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| Documentation/memory-barriers.txt | 93d58d9a428b879194ff03ef1aa1ca7c9e01fa14 |
| Documentation/core-api/wrappers/atomic_t.rst | ed109a964c778914d0b1641d201f65cb6e94ea8c |
| Documentation/atomic_t.txt | bee3b1bca9a7b46bcf9911f036c3280e77b4405a |
| Documentation/locking/locktypes.rst | 80c914f6eae7ab07476491c4eba96c004508e28d |
| kernel/sched/wait.c | 51e38f5f47018c953e31c834dc6385c182359359 |
| Documentation/dev-tools/kcsan.rst | d81c42d1063eab5db0cba1786de287406ca3ebe7 |
| Documentation/locking/lockdep-design.rst | 56b90eea27312e0a438260eb10425e811f154c9a |

atomic_t.rst仅通过include引用atomic_t.txt，语义核对来自后者。无返回值原子操作不提供一般的跨对象排序；内存屏障文档明确锁的获取和释放分别是单向约束，不能把任意acquire/release组合视为全屏障。READ_ONCE/WRITE_ONCE不替代其他负载的发布协议或对象保活。locktypes区分非RT的spinlock_t与PREEMPT_RT的可睡眠实现，raw_spinlock_t保留严格自旋语义；正文未把非RT上下文规则无条件推广。

fasync_remove_entry在file锁和全局链表锁下，以节点写锁清空fa_file，再解除链接、kfree_rcu、清FASYNC；发送的RCU区间保护节点存储，fa_lock读侧协调file交接。这些机制不延长驱动dev或链表头地址寿命。fasync_helper启用先以GFP_KERNEL分配再检查旧节点，不能以更新为由放入原子上下文。__fput先执行启用中的fasync取消，再调用release；devres退场与最终file归还不是同一时刻。

wait_event_interruptible内层先prepare_to_wait_event再检查条件并决定schedule，finish_wait清理。poll_wait先登记，fs/select.c的triggered与调度/唤醒配合处理交接；它们不预留设备数据。多个读者同时观察可读后，实际消费必须在自己的锁协议内重查；第二名读者看到空应重试或EAGAIN，不能一律判EIO。

正文用数据、订阅、信号pending、边沿采样、等待登记和对象引用的正交状态重建S0至S6，补订阅删除与设备解绑两条时序。纠正只关本地中断等同跨CPU互斥、cancel_work_sync阻止未来重新排队、模块引用阻止设备解绑、devm自动等待旧fd、所有锁都是全屏障等旧说法。

正文Python模型枚举正确等待的10种顺序一致性交错，构造先检查后登记和先通知后发布的丢唤醒反例；两读者6种交错中4种双方都先观察到就绪，实际仅一方消费。该模型验证逻辑交接，不是Linux调度、LKMM弱内存、硬件或Kbuild验证。lockdep/KCSAN等动态检查仅对已启用、未失效且实际执行的接入路径提供有限证据，未告警不构成完整正确性证明。

## 1.204\_异步通知的调试观测与时间关联证据

2026-10-17 重写[异步通知P11](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P11_调试_排错与性能分析.md)，沿用官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0。复用 fs/fcntl.c、kernel/signal.c、fs/signalfd.c、fs/pipe.c、fs/proc/array.c 及前章状态与接收协议，新增原始字节如下。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| include/trace/events/signal.h | 1db7e4b07c0148683065641799886c4845964bc4 |
| Documentation/trace/ftrace.rst | 4073ca48af4ad2639c2289eb28c691a5c3538850 |
| Documentation/trace/events.rst | 759907c20e7591ce3f03ac191cbc14a29b242f8a |
| fs/proc/fd.c | 5e391cbca7a3be0cb02f55e96d6728ad98e3ea16 |
| Documentation/core-api/timekeeping.rst | 22ec68f244210681d89ab36445ecf3f67f7a597d |
| kernel/trace/trace_functions.c | fbbc3c719d2f68c9bfa8eaf64864191a639d5594 |
| kernel/trace/trace.c | ffe1422ab03f888101f836e73d4c14b47ce3b54c |
| include/linux/time_namespace.h | 876e31b4461d0ee01fe2bd3d136acdea2611789f |

fs/proc/fd.c的seq_show读取file的flags并合入fd当前close-on-exec，以0%o显示；不能按十进制解析或从flags推出owner。fs/proc/array.c分别输出线程pending、共享pending、线程blocked和共享处置，SigQ取用户计费与任务限额，非设备队列长度。

include/trace/events/signal.h定义生成结果0到4为DELIVERED、IGNORED、ALREADY_PENDING、OVERFLOW_FAIL、LOSE_INFO；kernel/signal.c的__send_signal_locked在生成路径记录结果。DELIVERED不是用户handler完成；signal_deliver位于get_signal普通递达路径，signalfd通过dequeue_signal取走pending，不要求产生同一递达事件。事件专用pid/comm是目标，记录通用前缀是执行者，不能混作同一角色。

ftrace.rst的Instances旧段落仍声称实例只有事件、没有function tracer，但本提交kernel/trace/trace_functions.c的function_trace明确allow_instances=true，trace.c按该标志允许实例选择。正文以实现和运行时能力检查为准，不照抄旧段落。trace.c的printk_trace默认global_trace，选项可转移目的地，因此trace_printk不会天然进入新建实例；正文不修改该共享目的地。ftrace文档区分local跨CPU时间边界、mono时钟、每CPU缓冲统计、trace与trace_pipe。

timekeeping.rst将ktime_get_ns对应MONOTONIC，区分boottime与raw；time_namespace.h为用户时间增加命名空间偏移的接口进一步限定跨域相减。正文用run/device/sequence关联不可变记录与IRQ、发布、read完成、业务完成时间，不把最近一次ioctl快照配给旧事件，不把kill_fasync返回当作远端接收的必然前驱，也不提供无测量依据的通用微秒阈值。

三个完整Python工具从正文提取检查一致并实际运行模拟验证：proc快照9个场景、tracefs/Popen替代接口33个场景、CSV关联统计12个场景，覆盖八进制、共享pending、线程读取失败、PID复用、能力缺失、超时TERM/KILL、采集输出失败与清理失败、重复键/时钟域/倒序及跨设备分组。窗口内多次proc读取仍非原子，模拟不能证明Linux proc/tracefs ABI、真实函数插桩或硬件延迟；当前环境未运行目标内核采集。正文给出有限Linux实验和清理方法，并把合成CSV明确标成练习数据。
## 1.205\_通知机制比较的实现与接口边界

2026-10-17 重写[异步通知P12](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P12_fasync_与其他通知机制的对比与整合.md)，沿用官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0。复用 fs/select.c、fs/eventpoll.c、fs/signalfd.c、drivers/input/evdev.c、Documentation/input/event-codes.rst 与 net/netlink/af_netlink.c。新增原始字节如下。

| 上游路径 | 原始字节 Git blob |
| --- | --- |
| io_uring/rw.c | 6abc495602a4e941e9870f612d2dd47108e9e45a |
| io_uring/poll.c | b93e9ebdd87c8f9b1c6b1879dc9f6695703c322d |
| fs/aio.c | e8920178b50f73267306a6da8f177785267f0c15 |
| fs/kernfs/file.c | 8502ef68459b9842d090a4ac338591778d1b3b24 |
| fs/sysfs/file.c | d1995e2d6c943a644ff9f34cf2488864d57daf81 |
| lib/kobject_uevent.c | b7f2fa08d9c82c2838b703f3fd25f6e43b88b68a |
| Documentation/filesystems/sysfs.rst | c32993bc83c70436db118df8f582f59715798b20 |
| Documentation/userspace-api/netlink/intro.rst | aacffade8f84c362962dd342e2e3839661f432eb |
| include/linux/input.h | cd866b020a01d67081788e1baa2d372ce1c79767 |
| drivers/input/input.c | c51858f1cdc556939c2ee1bd21669c71c169d84e |

io_iter_do_read优先使用read_iter，缺少时可经loop_rw_iter回退普通read；回退拒绝HIPRI，NOWAIT与文件阻塞模式的组合也有约束。原生AIO读写路径则要求相应read_iter/write_iter。io_uring poll通过vfs_poll取得就绪并登记等待；这些证据不支持“有poll就支持全部读写”或“所有io_uring读都必须read_iter”。用户接口分别核对Linux man-pages的POSIX AIO、原生io_submit，以及liburing的prep_poll_add、wait_cqe_timeout和queue_exit，区分就绪完成、数据完成、多次完成和取消结果。

kernfs普通属性poll比较打开实例观察代次与当前通知代次，变化时增加PRI/ERR；默认可读写位不能用来判断属性变化。sysfs_notify按属性查找进入kernfs_notify，后者更新代次、唤醒等待并安排fsnotify工作；属性生产者没有通知时，不能推导任意值变化都可监视。kobject_uevent_env沿对象查找kset，缺失时返回错误，并有抑制和过滤条件；裸kobject不是无条件广播入口。

正文将input数据语义、每客户端evdev记录、Netlink对象消息、uevent设备模型、sysfs当前属性和fasync就绪提示放回不同职责；保留事件码、报告边界、SYN_DROPPED恢复、port ID、丢消息重查询与多通道去重。实现引用用于核对接口边界，未测量任何目标硬件吞吐或延迟。

两个完整程序分别验证select/poll/epoll不消费数据、POLL_ADD完成后仍需read。正文代码提取一致检查后，Python替代接口执行42个场景，涵盖普通与优化运行、就绪、短进度和清理错误；C程序经GCC与Clang各执行16个替代接口场景，包含初始化/提交/等待失败、错误CQE、先复制后归还、数据和EOF检查及清理。替代接口只验证应用控制流，不证明Linux ABI、真实io_uring或目标板行为；当前Windows环境未完成Linux头文件构建及内核运行。
## 1.206\_异步通知工程整合与资源迁移证据

2026-10-17 重写[异步通知P13](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P13_工程实践案例与编码核对表.md)。沿用官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，复用第六至十二章的文件订阅、信号、pipe、signalfd、timerfd、epoll、GPIO、工作与对象寿命证据。新增原始字节为 kernel/irq/devres.c（Git blob b3e98668f4dd87e725626ad55f48a34d117e1c67）与 include/linux/regmap.h（9f7b39a2342a19d1b5570b0015844c8455c74e62）。

kernel/irq/devres.c的devm_request_threaded_irq在成功申请后登记irq/dev_id；devm_free_irq先devres_destroy再free_irq，支持按配套接口提前释放。不能普通free_irq后留下托管登记，也不能删除显式停产而依赖晚到的自动归还。已有drivers/base/devres.c提供自定义托管action，devres文档列出托管clock接口，regmap头提供托管初始化；是否托管取决于此次句柄的责任与寿命，非资源类别的绝对禁止。

正文不再假设i.MX6ULL任意板卡GPIO接线、不使用旧class_create签名与不存在的class引用字段、不将release误写成单参数回调、不在自旋锁下调用可能分配的fasync_helper。GPIO完整驱动与16字节最新状态协议回链第七章，软件对象保活独立于硬件devres；六类原工程任务都保留为集成、迁移与失败现场的连续推导。

新增完整multi_async.c用两pipe与本地socketpair，比较direct/signal/both三模式，使用稳定索引、已知源扫描、每源预算、EOF撤销登记与单调截止。普通SIGIO的ssi_fd不作可信来源；两条提示都只产生检查责任，业务由同一读取逻辑消费。socketpair与accept4边界交叉核对Linux man-pages，未声称实现TCP服务器。正常数据为abc/123/N!，含信号模式还要求至少一条提示，不按通知次数恢复业务记录。

正文程序提取一致，替代系统接口经GCC与Clang各运行248个场景：三模式正常数据及逐调用失败注入、短写/分段读、EINTR、合并提示、无信号、坏信号记录、定时积压、超时、终止和清理错误；检查所有取得的fd均归还，失败不打印PASS。另修第九章成功输出必须晚于清理并重跑两编译器各162场景。模拟验证应用控制流，不证明Linux libc ABI、实际内核调度或硬件；目标Linux编译、运行和GPIO解绑仍需实际环境验证。

## 1.207\_锁应用契约与历史配置复核

2026-10-17复审[自旋锁](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P01_自旋锁.md)与[可睡锁](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P02_互斥锁与读写信号量.md)。从官方固定提交dfaf2136deb2af2e60b994421281ba42f1c087e0重新取得include/linux/rwsem.h、kernel/locking/rwsem.c、kernel/locking/mutex.c、include/linux/spinlock_types.h，与仓库保存原件逐字节相同；Git blob依次为c8b543d428b0a8d4662183f3342e88ec61d10189、2bbb6eca51445bdf434ba579ced4beddafbc52ca、cbae8c0b89ab2b8074a387ebf1cca86951362cc2、2dfa35ffec761f933e0f38d38be756d78b4b1669。另核对spinlock_types_raw.h（91cb36b65a1705717a476bf634843e6eb8f9b8b4）和kernel/Kconfig.locks（4198f0273ecdcfd61420f652cc124925be2b534b）。

rwsem公开接口和非RT实现确认可中断/killable获取返回0或负错误码，trylock返回1或0，写降读之后归还读资格；普通接口保留所有者责任，专用non-owner接口另有契约。mutex_unlock的对象存活要求持续到函数返回。locktypes.rst确认RT普通自旋锁的可抢占、迁移约束和IRQ后缀差异，不能从“内部可阻塞”推导临界区可任意睡眠。

正文辅助代码提取后经GCC与Clang编译，用顺序替代锁接口验证中断原状态恢复、无符号计数回绕、非法参数、获取中断、条件冲突和数值改回的语义；不模拟真实竞争、调度或内存顺序，不等于目标Kbuild。先修交错材料另验证原版、加二及过早释放模型。标准工作树本次不可访问，既有非SMP、非RT配置均只作历史记录，未重新确认当前配置，未完成目标运行。

## 1.208\_锁资格交接与诊断检查点复核

2026-10-17按同一官方提交dfaf2136deb2af2e60b994421281ba42f1c087e0复核[统一周期](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P03_锁的统一状态与通信周期.md)、[ARM实现](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P04_spinlock实现与上下文边界.md)、[mutex交接](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P05_mutex慢路径与所有权交接.md)、[rwsem批次](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P06_rwsem读写汇聚与唤醒.md)与[实时和寿命边界](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/P07_PREEMPT_RT生命周期与选型.md)。新增或重新下载的关键来源如下，已有mutex.c、rwsem.c和locktypes.rst身份沿用上节及先前各实现证据。

| 上游相对文件 | Git blob |
| --- | --- |
| arch/arm/include/asm/spinlock.h | f610a773f2be527dde14415a61fab4c7250d9447 |
| arch/arm/include/asm/spinlock_types.h | 0c14b36ef10131b2a1fc95d91ccc76871e8ba4d4 |
| include/linux/spinlock_api_smp.h | 89eb6f4c659c706c1b7376534a25d53a316316b7 |
| kernel/locking/osq_lock.c | 75a6f6133866d81ab4bd94f128d3f20f85c7c15f |
| include/linux/osq_lock.h | ea8fb31379e3cba3520c202e1d4b42fb08e13e04 |
| kernel/sched/core.c | 1f817d0c5d2d0e04a8d48836bf465eb7585d16d8 |
| include/linux/kernel.h | be2e8c0a187e4b26a704ee5e628b7afe404d9211 |
| Documentation/dev-tools/kasan.rst | d7de44f5339d43aee128091930ead29511060925 |
| Documentation/dev-tools/kcsan.rst | d81c42d1063eab5db0cba1786de287406ca3ebe7 |

32位ARM票号的next/owner各16位，获取成功领号后复查owner，释放推进owner后执行事件路径；不外推ARM64。普通获取与irqsave/bh是调用者选择的不同包装入口，不是spin_lock自动调用irqsave。票号顺序C模型经GCC与Clang编译运行，不能替代真实独占访问与弱内存验证。

mutex非交接释放可先清owner再处理等待队列，所以锁资格可用与旧unlock返回是不同事件。HANDOFF保持旧所有权直至指定接收者，PICKUP由接收者acquire确认；取消检查晚于锁下尝试接收。OSQ的队尾在锁对象中，optimistic_spin_node为每CPU节点；已在等待链表队首的自旋者可绕过OSQ。rwsem读批次先完整记份额再release清waiter.task，写者收到唤醒仍须实际取得独占；读批次扫描可跳过写节点，单次上限256属于版本实现。

DEBUG_ATOMIC_SLEEP分支中的might_sleep先调用__might_sleep检查，后续might_resched有自己的配置选择；__might_resched检查抢占、IRQ等状态并带有启动阶段/故障状态例外及限频。该报告不证明已经调度睡眠，安静也不能排除未检查或被抑制路径。KASAN与KCSAN文档只支持各自检测范围，不能把一种检查的未告警当成全部锁和寿命正确。此次未运行目标锁竞争、RT调度、解绑或检查器实验；历史配置与本次工作树不可访问的边界保持不变。

## 1.209\_教学模型语言补充复核

2026-10-17依据新增的C/C++教学要求，将[异步通知第十章交错模型](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P10_并发_竞态与内存可见性问题.md#10.7_用交错反例和有限测试检查证明边界)替换为完整C11程序。原有内核证据与逻辑责任不变，先前Python测试记录保留为历史；当前正文以C模型及编译命令为准。

程序递归枚举保持各参与者内部顺序的交错：正确等待10条且零丢唤醒，错误登记10条含1条反例，提前通知10条含2条反例；双读者6条中4条双方均观察到就绪，每条只消费一次。GCC与Clang编译运行得到相同结果，GCC定义NDEBUG后仍执行require检查；把通知分支的wake置为false的练习变体在两编译器下均正常编译后以失败状态报告correct wait。代码提取一致，章节精确链接和全仓检查通过。此顺序模型仍不模拟Linux调度、弱内存、真实信号或设备，不能把语言替换后的通过视为新增硬件验证。

[中断恢复协议模型](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P14_中断风暴_抖动与防御机制.md#14.5.2_用可执行模型找出重复禁用与旧结果复活)同样改为C11，保留重复故障、预算耗尽、探测中停止及工作前停止四条事件序列。GCC与Clang的严格警告构建、普通运行及NDEBUG构建均通过；去掉finish身份保护的练习变体在两编译器下均编译成功，并以退出码1报告stop during probe。正文明确禁用计数平衡不足以证明停止后的终态正确，并限定unsigned代次不回绕的短序列范围。源码与正文一致；没有新增内核调用、真实恢复I/O、多核或设备停机验证。

[流式队列与补发模型](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P08_流式设备与高级场景中的异步通知.md#8.4.1_运行一个同时检查队列与延迟补发的模型)改为C11，正文保留完整数组队列、队列外丢失确认、数据轮次与固定期限补发。GCC/Clang严格警告及NDEBUG构建运行一致；配套C检查以独立线性队列对照40000步入队、成功读取和失败复制，并检查持续丢失下的数据机会、补发期限、过时提示和零间隔。容量改为2、数据复制失败、到期已读空三个正文练习均通过；删除数据轮次保证的变体按预期失败。代码与正文一致。检查未覆盖真实信号、Linux文件操作、DMA、并发或计数回绕，通知日志16格的上限仅属于教学模型。

[字符设备容量实验](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P06_字符设备中实现异步通知的基础模式.md#6.5_用同一周期检查唤醒竞争与容量边界)改为Linux用户态C11程序，保留零长度操作、EAGAIN、容量256、短写、短读顺序与poll条件，并显式拒绝POLLERR/HUP/NVAL。宿主测试替换系统调用和Linux声明，直接编译正文函数体；GCC/Clang严格警告、NDEBUG下各通过21种情形，覆盖单字节短读、错误EOF、错误容量或就绪、尾部丢失、额外数据、调用错误和关闭失败，核对所有已打开fd的关闭责任。该结果不是Linux头文件原生编译、系统调用或模块运行；原内核模块和信号接收程序没有因语言迁移改写。

[三种就绪接口比较](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P12_fasync_与其他通知机制的对比与整合.md#12.2.1_用同样的AB与C观察三种就绪接口)改为C11，保留AB逐字节消费、在线空读EAGAIN、写入C后关闭写端及排空EOF；补select集合范围检查、epoll登记与身份检查、close失败后其他资源继续清理。GCC/Clang以显式系统调用替身编译正文函数体，逐调用位置注入EIO，另测错误电平就绪、提前EOF和无效描述符，每种编译器89次运行通过（超过实际调用数的注入位置重复正常路径，不计为独立错误覆盖）。未做Linux原生构建或真实select/poll/epoll运行；既有io_uring的C实验没有改动。

[定时工作配对工具](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P13_调试_验证与常见陷阱.md#13.5_用完整小工具计算等待时间_同时拒绝不完整证据)改为C++17，使用整数纳秒、地址数值键及分别保存的waiting/running。GCC/Clang++严格警告与NDEBUG构建运行通过；C++检查覆盖正常与运行中再次入队、待执行残留、符号地址、对象地址规范化、无关事件、重复/缺失/倒序/重叠、卸载后事件、零地址、64位上限与溢出、小数位数和最近秩分位点。两编译器各六项命令行测试覆盖构造CSV、数值符号、隐藏/歧义符号、参数与缺失文件。全部是构造文本和本地解析，不新增真实跟踪、目标模块或硬件时延证据。

[按键观察程序](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P07_中断驱动设备的异步通知实践.md#7.6_先观察稳定状态_再分别验证poll和SIGIO)改为C11，逐字节解码16字节小端协议，保留poll与同步信号两种模式、独立记录/信号计数、期限和退出屏蔽。预算用尽回到期限检查后继续消费，只有EAGAIN才等待。GCC/Clang严格警告与NDEBUG下各80次显式接口替身运行通过，覆盖逐调用EIO、持续数据、序号回绕、EINTR、信号超时、非法状态/保留字节/长度、设备错误及无效fd；超出实际调用数的故障位置重复正常路径，不计为独立故障。替身替换Linux声明和系统调用，结构体标签随替身重命名，正文控制流与字节解码保留；未执行Linux原生构建、真实信号、GPIO、解绑或目标模块测试。

调试章的[proc快照](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P11_调试_排错与性能分析.md#11.3_proc快照能看见当前状态_却不能替代事件历史)与[延迟统计](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P11_调试_排错与性能分析.md#11.6.1_先用合成记录验证统计_再接入真实采样)先改为C++17。GCC/Clang++严格警告及NDEBUG验证字段数制、64位信号位、含空格括号的stat起始时间、错误字段和参数；proc工具仅替换本机不具备的Linux信号声明，未运行真实proc采样、PID复用或线程退出。延迟工具以真实普通文件和C++测试验证两条示例、跨轮/设备分组、引号/换行CSV、重复键、负值、越界、缺列、多列、时间倒序及分位点；示例IRQ到完成仍为700000和1100000 ns。专用tracefs采集器此时尚待语言迁移，不能把两个分析工具的完成计成整章完成。

随后将[专用实例采集器](../../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/P11_调试_排错与性能分析.md#11.4.1_用专用实例完成一次有起止和清理的采集)改为C++17，保留能力检查、mono时钟、函数及信号过滤、应用日志与每CPU统计；增加父进程停止标记、独立清理尝试和显式子进程回收期限。两编译器严格警告/NDEBUG下各15项模型通过：正常与实时信号模式、子程序失败、超时、忽略TERM、用户停止、无法回收、wait/open/fork/close失败、缺少时钟/tracer/函数与删除实例失败。模型用普通目录模拟tracefs，以显式替身替换进程、时钟和Linux声明，只检查控制流、输出文件与资源归属；未执行真正fork/exec子分支、真实信号、tracefs或原生Linux构建，不能据此宣称目标采集成功。至此本章三份工具完成语言替换，旧Python测试保留为历史证据。

## 1.210\_锁源码图文与阶段对应复核

2026-10-17，独立冷读锁源码的两篇模块导读与七篇实现讲解。依据仍为固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`；对七份缓存原件重新计算 Git blob 摘要，与 1.207、1.208 所列身份一致。44 个 C 摘录块中的 55 个类型、宏或函数单元在去除注释、空白及宏续行差异后与对应原件一致；本次编辑未改变其代码语义。静态一致性检查不覆盖完整预处理配置、汇编生成或链接。

正文分开历史配置记录与本次工作树不可访问的事实，统一 M/R 局部步骤与 S0～S7 模型的对应，修正远端取得、本地恢复及读者授予/取消两种交错。模块五图与实现六图渲染目视通过；mutex 获取一节保留同一完整函数及其登记辅助解释，258 行长度提示经人工核对，不为降低计数拆散控制流。本次没有新增目标内核构建、SMP/RT 竞争、真实信号或硬件运行验证。

## 1.211\_序列计数器的快照与实时补偿复核

2026-10-17，独立冷读序列计数器七篇知识文档与四篇源码文档。固定版本仍为 NXP Linux 6.12.20 提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。重新核对 `include/linux/seqlock.h` 与 `include/linux/seqlock_types.h` 的 Git blob 分别为 `5298765d6ca4827eb7bf8a9dca020f1383d3b901`、`dfdf43e3fa3de3acfd294807cc207d6464db0d11`；42 个 C 摘录块在忽略注释、空白和宏续行差异后与原件一致。另从同一官方固定提交取得 `Documentation/locking/seqlock.rst`，Git blob 为 `ec6411d02ac8f55aee92b375071c8b2e7b840e59`。该文档关于关闭 Lockdep 后关联锁为 NOOP 的概括未覆盖此版本头文件中的 PREEMPT_RT 路径，正文以实际字段条件及获取、释放分支界定行为。

普通序列计数的 S 周期、latch 的 L 周期和关联锁的 A 周期分别命名；修正先取得外部写锁再开窗、RT 读者取得后立即释放关联锁，以及禁止把普通关中断当作阻止 NMI 的条件。源码中文说明逐项对应实际函数参数和状态作用，不再把读起点描述套给写入口。19 张图全部渲染并目视检查，11 篇的精确章节链接与严格连续性检查通过。

正文三个完整 C 模型经 GCC 14.2 与 Clang 18.1.8 严格警告构建，普通及 NDEBUG 配置共 32 次运行。快照模型枚举末尾比较 35 条交错（接受 16 条，其中 5 条混合）及奇偶协议 70 条交错（接受 2 条，无混合）；latch 模型覆盖 7 个暂停窗口及 210 条交错，完整序号无混合，只有选择位的变体出现 21 条混合。8 位回绕模型说明 128 次更新可伪装相同序号，127 次的对照未回绕；删除奇偶、版本或任一次重定向的四种变体均按预期报告失败。检查使用始终有效的 require，不因 NDEBUG 消失。这些是宿主单线程枚举与静态源码核对，不证明真实多核、弱内存、RT/NMI、目标内核构建或硬件行为；本次源码工作树仍不可访问。

## 1.212\_RCU入门边界与配置模块复核

2026-10-17，重读 RCU 前三章，用固定提交的原件核对发布、读区、GP 和异步回收的保证。以下 Git blob 来自 NXP 官方固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 的原始文件；五份代码/配置与仓库已保存副本按 LF 规范化比较一致，三份文档仅作为本次取证缓存，不替代正文解释。

| 上游相对路径 | Git blob |
| --- | --- |
| `include/linux/rcupdate.h` | `48e5c03df1dd83c246a61d0fcc8aa638adcd7654` |
| `kernel/rcu/tree.c` | `8e52c1dd06284cc8e9cfb0416c38798e5003079a` |
| `kernel/rcu/tree_plugin.h` | `1c7cbd145d5e3766c1cfd67e99ffb8a69ce8adc5` |
| `kernel/rcu/Kconfig` | `3e079de0f5b434bd394033f08a38a00228c07432` |
| `kernel/rcu/Kconfig.debug` | `9b0b52e1836fa42b60539edcd33f0c70358b74f2` |
| `Documentation/RCU/Design/Requirements/Requirements.rst` | `6125e7068d2ce9dc7fc7bb3d7758d8f5b1eef77a` |
| `Documentation/RCU/whatisRCU.rst` | `1ef5784c1b84e272a7f8eaa90137b04a1f73b2fb` |
| `Documentation/locking/percpu-rw-semaphore.rst` | `247de64108557a85a9096ec56de4855cd19bdec7` |

前三章区分顺序可见模型与真实跨 CPU 传播，不能从“写者已执行发布”推出所有并发读者立即取得新指针，也不能把不必等待后来读者解释成绝不保守地等待。详细任务/CPU 字段仍在后续读侧模型章展开，入门章保留正常与抢占分支的角色交接。配置模块把业务 `u32` 输出与错误返回分开，异步更新仍明确要求可睡眠的 mutex 上下文，逐对象选定唯一退休路径。

[完整配置实验](../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P03_RCU_通用API与最小使用闭环.md#3.7_运行一次配置发布到退出的完整闭环) 与材料逐字一致。GCC 14.2、Clang 18.1.8 的严格警告及 NDEBUG 构建共 60 次宿主替身运行通过预期：覆盖正常/立即或延迟回调、三处分配失败、空入口输出保持、最大无符号值、业务代次回绕、退出后查询及两个练习；其中 8 次为移除 GP 或 barrier 的预期失败。替身显式替换 Linux 头文件、锁、RCU 与分配接口，只检查控制流和资源归属，不模拟真实 RCU 并发、内存屏障、回调调度、Kbuild/MODPOST 或内核装卸。15 张图渲染目视通过；第一章读写锁成本链 161 行、第三章完整实验 250 行的长度提示分别经人工保留，未以拆散因果链或完整程序降低计数。

## 1.213\_RCU分类坐标与读侧交接复核

2026-10-17，冷读 RCU 第四至第六章，继续使用 1.212 的固定官方提交。补取以下原件，核对静态 SRCU 域的文件作用域要求、任务状态字段，以及调度钩子相对实际切换的位置：

| 上游相对路径 | Git blob |
| --- | --- |
| `include/linux/srcu.h` | `835bbb2d1f88ad7efb4d5db4f4bf8e71fc8e22e9` |
| `include/linux/srcutree.h` | `ed57598394de37eedeb0b97774f0f617cd3da242` |
| `include/linux/sched.h` | `0d1d70aded38f6be700c120c3529096b6a0be1db` |
| `kernel/sched/core.c` | `1f817d0c5d2d0e04a8d48836bf465eb7585d16d8` |

`rcu_note_context_switch()` 在登记读区内任务后调用 `rcu_qs()`；这个动作发生在调度钩子内，不能写成实际切换完成后才记录。普通 QS 本地锁存与清除节点位分开；`rcu_read_unlock_special()` 在不适宜的上下文可保留责任并请求后续处理，任务逻辑退出不等于共享登记已经移除。正文增加原叶节点、迁移后任务与延后清理的关系图及逐步纸面推演。EQS 快照证明当前或两次采样间经过静止状态，不要求整个观察窗口始终空闲。

第五章把默认普通同步路径限定在调度器正常运行、普通 GP、`rcutree.rcu_normal_wake_from_gp=0`，同时说明另一等待请求链的存在；区分根条件、全局完成序列、回调资格与实际执行者。九张图渲染并目视检查，三章入链/出链及全仓 9213 个有效链接检查通过。第四章的 85 个内部链接主要承担分类矩阵和深化查询，逐节已有本地解释，按人工冷读保留该密度提示。此轮没有执行目标内核构建、调度/中断跟踪或真实 RCU 实验；纸面状态推演不作为运行证据。

## 1.214\_RCU启动依赖与GP交付复核

2026-10-17，冷读第七、八章，继续使用固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 的 `tree.c`、`tree_plugin.h` 等已核对原件。另取同提交 `kernel/rcu/rcu.h`、`kernel/rcu/tree_nocb.h`、`include/trace/events/rcu.h`，Git blob 依次为 `feb3ac1dc5d59030db1131677dda08fa9caa41a7`、`2605dd234a13c8aec5b74ebf5e006005ce98ea0f`、`e81431deaa50b21725b08ad7c1a2992a0074065a`。

初始化 B0～B6 与运行期 S 周期分开，说明 possible、online、编号上界和扇出；`n_online_cpus` 在 prepare 阶段增加，不是任意瞬间的 online mask 计数。`rcutree.use_softirq` 默认随 PREEMPT_RT 构建改变，线程分支的待办标志与条件唤醒不能合并为无条件启动线程。配置、启动参数、日志和任务观察按顺序就地讲解，未声称在目标环境执行。

GP 模块以 G0～G10 对应公共周期，修正 `rcu_gp_init()` 内部建债先于成功返回的顺序，以及各消费者交付可与下一 GP 重叠的关系。请求漏斗遇到进行中 GP 可留下需求由 cleanup 接手，get-state 快照本身不发起 GP，NOCB 回调也可能先经 bypass。跟踪事件区分 `rcu_callback` 的登记与 `rcu_invoke_callback` 的调用。五图渲染目视、两章严格连续性与精确锚点检查通过，全仓 9214 个有效链接无缺失；只修改入口的其他 RCU 文档不计入全文复核完成。没有新增目标内核构建、实际启动、CPU 热插拔或跟踪运行证据。

## 1.215\_RCU执行环境快照与节点证明复核

2026-10-17，冷读第九、十章。固定 NXP 提交继续为 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，补取下列上下文跟踪原件；`tree.c`、任务插件与 trace 事件依据沿用前述已核对原件。

| 上游相对路径 | Git blob |
| --- | --- |
| `kernel/context_tracking.c` | `938c48952d26533a1daf77bb743384a833246ef3` |
| `include/linux/context_tracking.h` | `af9fe87a092251348df0a2a2e7f13bf63388eb5a` |
| `include/linux/context_tracking_state.h` | `7b8433d5a8efe5bc0d7ff84fb22ea5de9a34d464` |
| `include/linux/context_tracking_irq.h` | `c50b5670c4a52fd905da5a7b70b581d6a88959b2` |

用户态 EQS 路径以目标 CPU 的 context tracking 已激活为前提；主状态在 `context_tracking.state`，GP 扫描者将远端原子读取值保存在对应 `rcu_data.watching_snap`。正文分开当前状态、历史快照、任务登记与本轮 CPU 位，补 IRQ/NMI 嵌套和“两次都 watching 但中间经过 EQS”的反例。旧版本仅以官方上游 v5.10 的 `kernel/rcu/tree.c` 对照字段访问与入口，Git blob 为 `bd04b09b84b327cde1aa8472986dfc0a5ff39719`；该版本 `tree.h` 下载两次收到 429，未将其记作已核对原件，比较依据限于已取得的每 CPU 初始化、原子访问及函数实现。

节点章用多机器字位图承载 256 CPU 示例，按 A0～A5 说明报告者、锁交接、代际核对、零掩码分支和根通知，补完整四 CPU 时序与任务清理后恢复汇聚。五图渲染目视、两章严格连续性和精确链接检查通过；全仓 9216 有效本地链接无缺失。快照、重复报告和过期报告练习属于纸面推演，观察命令只读检查目标能力；没有实际开启 trace、执行 nohz 工作负载、目标构建或热插拔实验。

## 1.216\_RCU回调分段与批次交接复核

2026-10-17，冷读第十一、十二章，继续以固定 NXP 提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 核对。补取 `kernel/rcu/rcu_segcblist.c`、`kernel/rcu/rcu_segcblist.h`、`include/linux/rcu_segcblist.h`，Git blob 分别为 `298a2c573f02c30c5bfdd69c11315c728f9ad692`、`2599040756369a0cb4bbd7dd66d2a37d47d7223d`、`2fdc2208f1ca3a51bc2567413cdddaaf1ec03b65`。`tree.c`、`tree_nocb.h`、`rcupdate.h` 与 trace 事件原件沿用前述固定版本证据。

分段图修正为 tail 保存指针槽地址，不再把边界画成链表节点。正文区分 `seglen` 与包含在途回调的 `len`，以提取、并发登记、部分执行、回插和结算解释 barrier 的保守计数；entrain 可接到 DONE，热插拔 merge 的未成熟部分回到 NEXT，不能宣称精确保留原目标代际。`call_rcu()` 返回不构成“回调仍未执行”的保证。

批次章明确数量退出条件、建立时间目标的配置及上下文条件、每 32 项常规检查以及单项不可阻塞契约；NOCB 线程在回调之间重调度不允许回调内部睡眠。双参数 `kfree_rcu` 与 `kvfree_rcu` 在本版本展开到同一公共宏，无 head 的 mightsleep 变体另有调用前提。C、E 阶段分别回指完整周期，并同步旧锚点及第八章错误的纯文本章节号。

四张图完成渲染和目视检查，两章严格连续性及精确链接检查通过；全仓扫描 721 篇 Markdown、9222 个有效本地链接，无缺失。纸面练习对照原件推演边界和计数，未执行目标内核构建、实际回调压测或 trace 采集；只读接口检查命令不冒充运行结果。新增 skill 支持文档来自并行编辑，其存在纳入后续覆盖登记，本次不将其记为已复审。

## 1.217\_RCU同步等待与退出屏障复核

2026-10-17，冷读第十三章并核对固定 NXP 提交的默认同步等待、可选 SRS 与 barrier 路径。补取 `kernel/rcu/update.c` 与 `kernel/rcu/tree.h`，Git blob 为 `f8436969e0c89e221b6f07743fe845969d2e6fa4`、`a9a811d9d7a372abc80dc64e7cf3808e1e8f14fd`；其余依据沿用已核对的 `tree.c`、分段链表和 trace 原件。

退出骨架明确业务占位函数的契约，关闭标志不能替代在途生产者退出证据；回调转交工作队列或对象另有独立引用时仍须另行排空。同步等待实例限定默认普通 GP 分支，解释栈上 completion 的提前完成与生命周期；SRS 先说明交付成本与分隔批次，再列字段。barrier 按 R0～R6 说明全局代次、每 CPU 快照、在线跨 CPU 处理失败重试、离线责任、在途总数和计数基础引用；基础值 2 不表示两项回调。纸面交错覆盖扫描期间哨兵提前执行，以及扫描者自己归零并发布完成的分支。

两图渲染目视、严格连续性与精确链接检查通过；全仓扫描 721 篇 Markdown、9225 个有效本地链接无缺失。未执行目标内核构建、模块卸载、CPU 热插拔或实际 trace 采集；业务骨架与计数练习不冒充可直接加载的完整模块。

## 1.218\_RCU强制扫描与迟延诊断复核

2026-10-17，冷读第十四章。固定 NXP 提交补取 `kernel/rcu/tree_stall.h`，Git blob 为 `4432db6d0b99b3bfd79194abfe3be478e8929f59`；并核对已取得的 `tree.c`、`tree_plugin.h` 与 context tracking 原件。F0～F5 子循环分开目标主状态、GP 扫描快照、叶节点欠债位与局部 mask/rsmask，修正 recheck 返回 0 仍可设置 urgent，以及已视为 offline 却仍欠位时先 WARN 的异常分支。

正文区分 tick 对 urgent 的消费、解锁后直接重调度请求、诊断 irq_work 与可选 rt_mutex 优先级继承；boost 不能代替旧任务退出。诊断拆分进行中 GP、GP 线程/定时器进展及需求迟迟未启动，保留时间异常、虚拟机暂停和检查者执行机会的边界。两图渲染目视、严格连续性、精确锚点及全仓 9228 有效本地链接检查通过。错误关中断循环仅作静态反例，没有运行故障注入；配置和事件检查仅提供只读步骤，未执行目标构建、stall 复现或跟踪采集。

## 1.219\_RCU加急请求与远端报告条件复核

2026-10-17，冷读第十五章。补取固定 NXP 提交的 `kernel/rcu/tree_exp.h`，Git blob 为 `fb664d3a01c957ad3bb78a9cf4e1da36af4eefed`，并结合已核对的 `tree_plugin.h`、`tree.h` 与 `update.c` 检查。MMIO 示例明确 RCU 只保护软件读者，DMA、设备事务和关闭握手由业务协议另行完成。X0～X8 区分 leader 调用任务、推进 worker、叶选择工作与节点等待者；漏斗快速路径在内部开始代次，根等待与公开调用者的节点等待不混写。

远端 handler 补深度为零但抢占/BH 仍禁用的延期分支，显式读者分开每 CPU exp 位与任务 exp_hint，并说明被抢占任务责任在 CPU 位解除后仍然存在。选择阶段保留曾上线集合、发请求前二次 EQS 检查及 hotplug 重试；同代搭车时序明确两个调用者在代次开始前取得相同目标快照。公开 API 可受 normal 策略覆盖，end 与 endwake 也分开。两图渲染目视、严格连续性、精确链接及全仓 9229 有效本地链接检查通过；没有执行硬件窗口撤销、目标构建、加急压力测试或 trace 采集。

## 1.220\_RCU回调卸载与隔离部署边界复核

2026-10-17，冷读第十六章，继续核对固定 NXP 提交的 `tree_nocb.h`、`tree.c`、Kconfig 与回调链表原件。补取 `kernel/time/tick-sched.c` 和 `Documentation/admin-guide/kernel-parameters.txt`，Git blob 分别为 `f203f000da1ad803deef3bcc05eea022b648be91`、`8874a9da71b7cb1dbcfc24152c453044a1ca4891`。

从 CPU 时间干扰推导卸载与执行位置选择，按 N0～N7 区分生产者、旁路、分段主链、普通 GP、GP/CB 线程及通知条件。修正 `cblist.len` 已计入 bypass 的事实，并回改第十一章总数定义；flush 不得重复增加总数。补 lazy、高频入队、轮询和延期通知的实际条件，以及 CB 线程也可推进已完成分段的分支。启动配置按有效 mask 解释，动态改变卸载状态须目标 CPU 离线，重新卸载还受启动期组织限制；普通 IRQ、managed IRQ、实时优先级、线程放置与回调安全性分别处理。

四图渲染目视、严格连续性与精确链接检查通过，篇幅审计为 9506 字词；全仓实际文件扫描为 727 篇 Markdown、9260 个有效本地链接无缺失，包含其他并行编辑的新 GIC 结构。没有执行目标内核构建、启动参数修改、CPU 隔离部署或实际 trace 采集；纸面队列练习与只读检查步骤不作为运行证据。

## 1.221\_RCU热插拔参与集合与队列接管复核

2026-10-17，冷读第十七章，以固定 NXP 提交的 `tree.c`、`tree_plugin.h`、分段链表和 barrier 原件核对；补取同提交 `kernel/cpu.c`，Git blob 为 `9ee6c9145b1df9ee9b63c0c3b55bbf2f8c3d2518`。分开 CPU 当前位、未来参与集合、旧任务记录与 callback 所有权，以 H0～H9 表达可交错的子过程。普通 GP 全局代次开始先于逐叶吸收 next 集合，不能把新上线 CPU 一概排除于正在初始化的代次。

迁移保留 DONE 成熟资格，源未成熟项并入目标 NEXT；`||` 短路使推进调用不是无条件逐个执行。修正 barrier 尚未覆盖源队列时的哨兵交接解释，以及 NOCB 不迁移不等于自动放在 housekeeping CPU。源码实现页只修正本次直接发现的事实与锚点，整页仍待独立冷读，不记为全文完成。

两图渲染目视、严格连续性、精确链接及格式检查通过；全仓实际扫描 727 篇 Markdown、9277 个有效本地链接无缺失。离线/上线命令按配置、能力、原状态、写入结果与恢复逐步解释，本轮未在目标机器执行热插拔、内核构建或跟踪，三题属于纸面状态推演。

## 1.222\_SRCU私有计数与回调交付复核

2026-10-17，冷读第十八章，补取固定 NXP 提交的 `kernel/rcu/srcutree.c`，Git blob 为 `31706e3293bcee7ab75d2100899cb2f77ec207bd`；头文件沿用前述固定版本证据。先从持锁慢通知的串行代价引出私有域，补业务监听函数与回收 callback 的不同睡眠契约，以及初始化失败、唯一注销者和总线销毁的前提。

状态图把共享 `srcu_idx` 与每 CPU 计数分开，小形态的集中 callback 队列与大形态的树汇聚分别说明；possible CPU 求和、先 unlock 后 lock 的屏障扫描和晚到增量不能压成一次普通计数比较。U0～U6 分开入口撤销、两轮扫描、完成序列、callback work 和 completion 通知；加急策略、normal 覆盖及清理告警边界据原件核对。

两图渲染目视、严格连续性与精确链接检查通过，篇幅为 5691 字词；全仓实际扫描 727 篇 Markdown、9293 个有效本地链接无缺失。跨 CPU 配平与三个变体仅作纸面练习；业务骨架明确不是可加载完整模块，没有执行目标构建、通知注销并发或 SRCU 跟踪实验。

## 1.223\_Tasks家族观察集合与完成交付复核

2026-10-17，冷读第十九章，补取固定 NXP 提交的 `kernel/rcu/tasks.h`、`include/linux/rcupdate_trace.h`、`kernel/workqueue.c`，Git blob 依次为 `4d7ee95df06e6792fa06207997d57cd4bb306f7a`、`eda493200663faf15780bff26c9ddc797afe9434`、`a9d64e08dffc7c7aef2caae2f170268d83e99e23`。以 K0～K6 区分共同代码与各 flavor 独立对象、序号、线程和队列，按实际函数体核对通知、聚合队列与 callback 交付。

经典 Tasks 保留被抢占而仍可运行的任务，以 nvcsw 快照、静止点状态、退出链及前后普通 GP 形成证据；Trace 使用在线 current 快照加 possible CPU 的 blocked 链，区分两类链、need_qs 握手、负深度、迁移与在途 IPI。unlock 特殊路径不直接唤醒 GP，由周期复查取得已发布状态。Rude 的同步入口与内部 static callback 入口分开，记录 `schedule_on_each_cpu()` 可分配失败而调用处未检查返回值的固定实现边界，不把成功路径图外推成失败保证。

角色图与三条时序图完成渲染目视，严格连续性、精确链接与格式检查通过；实际全仓扫描 732 篇 Markdown、9315 个有效本地链接无缺失，其他并行新增仍待覆盖登记。没有运行目标内核、修改跳板、进行故障注入或采集 Tasks 跟踪；线程和配置命令只是只读观察步骤。

## 1.224\_Tiny单CPU交错与成熟边界复核

2026-10-17，冷读第二十章，补取固定 NXP 提交的 `kernel/rcu/tiny.c` 和 `include/linux/rcutiny.h`，Git blob 分别为 `b3b3ce34df6310f7bddba40b2be1bdf6c9f00232`、`0ee270b3f5ed2fb8cb8aa5dc4d11e7a24c9675f3`。以同CPU旧读者被IRQ更新者打断的场景说明单CPU仍需延期回收；按T0～T5区分入队、QS成熟、通知、摘链和实际调用。

两个尾字段指向链中的指针槽，softirq只摘成熟前缀；内核tick设置重调度需求不等于已经报告QS。同步等待的合法调用现场排除了旧读者，函数体只更新poll序号并检查非法嵌套，没有旧注释所称的might_sleep调用；其返回不代替barrier的回调完成证明。将2026-09-05配置明确限定为历史快照，未将不可访问的当前工作树写成已核验。

角色图与时序图完成渲染目视；严格连续性、精确链接、标题、元数据和命名检查通过，篇幅3660字词。全仓实际扫描732篇Markdown、9321个有效本地链接无缺失。三道状态题提供推演答案，未执行目标内核配置切换、构建或运行实验。

## 1.225\_RCU与引用计数组合的责任交接复核

2026-10-17，冷读第二十一章，重新阅读已保存的固定版本 `kref.h` 与 `refcount.h`，沿前述Git blob和唯一实现讲解核对接口。修正当前入口指向旧根的图示，补模型B专用put包装，禁止混用A的release协议；由整块复制与异步工作约束引出复合快照，再以C0～C6连接私有构造、发布、借用转引用、根退休与长期归还。

补目的槽、分配失败回滚、一次发布、不可变数据及读—改—发布串行化前提，区分引用安全与业务更新丢失。工作投递失败、合并和取消都需归还明确责任，RCU屏障不能代替工作退出。两个图完成渲染目视，严格连续性、精确链接、篇幅及文档检查通过。正文13个C块分别组成A/B/C宿主顺序夹具，以GCC与Clang的严格警告及优化构建运行六次，覆盖两种归还顺序、零计数窗口、构造回滚、空查找和分配失败。替身只检查片段类型与顺序所有权，不验证Linux调度、真实原子操作或内存序，未执行目标Kbuild和并发压测。

## 1.226\_驱动快照与设备关闭责任复核

2026-10-17，冷读第二十二章，核对既有固定版本 `rculist.h`、`poll.h` 与 `workqueue.h` 的摘链、等待登记和工作交付接口。设备链表示例明确外层设备引用及过程上下文前提，新增失败返回；状态示例删除未复制数据却返回len的伪read，改为只复制不可变软件快照。补poll等待/唤醒及业务许可边界，避免以ready或kref证明硬件可操作。

复合更新改用调用者已持锁的发布函数，保持取旧根、clone、修改和发布的同一事务；同块占两个槽必须取得两份责任。D0～D5区分关闭入口、硬件活动收敛、旧软件读者、callback与后续工作，修正“回调不能访问旧对象”的错误核对项。三图渲染目视，严格连续性和精确链接检查通过；全仓732篇Markdown、9325有效本地链接无缺失。九段原样C骨架经GCC和Clang的优化严格构建及宿主顺序运行，覆盖四场景和分配失败；替身不构成Linux或硬件验证，未执行目标构建、热拔出、DMA停止及模块卸载实验。

## 1.227\_RCU静态类型与运行时诊断范围复核

2026-10-17，冷读第二十三章，核对固定 `rcupdate.h`、`update.c`、`compiler_types.h`、`rculist.h` 与RCU Kconfig检查分支。更正普通check访问器先READ_ONCE取得值、再执行可选告警的顺序；普通赋值没有诊断hook，raw变体也不能统称经过同一类型检查。功能状态、Sparse分析器账本、current的held记录及检查器有效性分开，以V0～V6解释进入/退出的实际包围关系。

删除旧Tree/PREEMPT配置冒充当前配置的表述，补debug_locks、递归、启动状态、held保守返回及调用点抑制的证明边界；上游函数/宏体保持在唯一实现讲解，正文保留契约、表达式推理和精确链接。Sparse 0.6.4宿主构建实际检查固定类型桥接表达式：三个错误样例产生预期诊断，正确转换与裸指针逃逸两个类型样例无告警，后者说明类型检查的能力边界。未执行坏代码、目标Kbuild或动态Lockdep验证。两图渲染目视、严格连续性、精确链接及文档检查通过；全仓732篇、9329有效本地链接无缺失。

## 1.228\_RCU异步交付与诊断证据复核

2026-10-17，冷读第二十四章。除前述RCU回调源码外，复核已保存固定 `kernel/time/timer.c` 的 `timer_shutdown_sync()`，Git blob为 `7835f9b376e76a010926c3c2036c9a458b1f553c`，以及固定trace事件和ftrace实例资料。修正复用work时先覆盖对象指针再处理queue_work失败的责任漏洞，改为独立分配且只交付一次，由worker归还引用并释放工作项；关闭骨架先阻止timer重装，再同步其work生产者。

修正“未dereference便可忽略旧读区”的错误故障目标，区分进入前与进入后；地址复用、丢失事件和invoke只表示调用开始均加入证据限制。采集改为独立tracefs实例，保存事件format和缓冲统计后清理，不修改全局事件开关。两图渲染目视、严格连续性、精确链接和文档检查通过；全仓732篇及9329有效本地链接无缺失。工作交接原样C片段在GCC/Clang严格优化宿主夹具中验证分配/查找/投递失败、两个独立工作、立即执行及零计数窗口。没有运行故障代码、目标tracefs或内核测试；本机无Bash，采集段仅对照接口人工核读，未宣称shell或硬件运行通过。

## 1.229\_RCU发布顺序与最终选型复核

2026-10-17，冷读第二十五章，沿固定 `rcupdate.h` 与 `srcu.h` 核对release发布、常量NULL分支、读侧依赖和SRCU同域等待约束。修正把全部逃逸引用写成条件get、所有kfree_rcu形式写成不阻塞、Tiny同步必然实际睡眠的压缩结论；补Tasks Rude的独立边界。

L0～L6将私有初始化、发布、旧指针消费、撤销、GP资格与实际释放放回同一周期，给出policy修复骨架及SRCU间接锁循环。平均退休容量用具体速率和延迟计算，明确不承诺峰值。三图渲染目视、精确链接和严格连续性通过；全仓732篇、9334有效本地链接无缺失。policy骨架双编译器宿主严格运行覆盖空入口、分配失败、替换及重复撤销；替身只检验类型和顺序行为，未执行LKMM模型、目标架构汇编核对或真实内核并发，不以这些结果证明弱内存顺序。

## 1.230\_源码读侧导读的债务转移与观察路径复核

2026-10-17，冷读RCU源码导航P02，核对固定提交的 `tree_plugin.h`、`tree.c` 与 `kernel/sched/core.c`。`rcu_preempt_ctxt_queue()` 根据普通/加速等待位及已有任务边界插入，并允许保守的额外等待，不能按被抢占时刻判定reader代际。调度器在底层context_switch之前调用RCU钩子，钩子先登记仍在读区的任务再记录本CPU QS；最外层退出按保存的原节点立即或延后清债。EQS远端采样不强制经过本地rcu_qs。

补状态归属图、R0～R7源码映射、两条有编号时序和交错题答案，修正历史配置冒充当前运行配置以及把全部IPI归为超时慢路径的说法。三图完成渲染目视，18条出链、9条标题入链、严格连续性、篇幅4424字词及文档检查通过；全仓732篇、9364条有效本地链接无缺失。调用示例为契约骨架，未执行目标内核构建、调度交错或trace实验。

## 1.231\_GP源码导读的阶段与完成交付复核

2026-10-17，冷读RCU源码导航P03，沿固定 `tree.c` 核对请求漏斗、唤醒、init、FQS、cleanup和主循环。沿用正文G0～G10，把一次性线程创建移出周期编号；修正入口函数反复调用、所有字段均持根锁、需求已被另一普通GP线程满足等误导表述。保留逐叶hotplug吸收与全局先开始的边界，明确poll观察区间、节点序列、全局序列及SRS交付不是同一发布点。

旧的裸章节编号改为实际模块入口，增加请求/证明/回调执行的端到端时序和未获调度的反例。两图完成渲染目视，33条出链、14条标题入链及严格连续性通过，篇幅4942字词；全仓732篇、9375有效本地链接无缺失。未重新生成历史冻结出版物，未执行目标内核调度或trace实验。

## 1.232\_热插拔源码导读的集合边界与迁移复核

2026-10-17，冷读RCU源码导航P04，核对固定 `tree.c` 的starting/dead、prepare计数、迁移与拓扑初始化，并对照 `kernel/cpu.c` 的迁移调用现场及 `rcu_segcblist.c` 的合并。修正“全局GP开始即冻结所有叶集合”、`qsmask`不变和私有计数等同online mask等表述；保留上报函数释放叶锁后重取的真实顺序。

用H0～H9对应正文，分开CPU责任、迁移后仍归原叶的任务责任和回调所有权。明确DONE资格保留、未成熟源进入NEXT、advance短路求值和barrier跨扫描窗口哨兵，补两道边界推演。两图渲染目视，22条出链、10条标题入链、严格连续性及篇幅3814字词检查通过；全仓732篇、9377有效本地链接无缺失。没有执行目标CPU热插拔或真实队列迁移实验。

## 1.233\_FQS导读的证据分类与诊断入口复核

2026-10-17，冷读RCU源码导航P05，对照固定 `tree.c`、`tree_stall.h` 与 `context_tracking_state.h`。将主状态、watching快照和节点责任分开；正值报告、零值可能已有urgent、负值锁外resched按真实分支解释。残留offline位先告警再解除等待，不能写成正常热插拔依赖的常规路径。

修正Stall调用入口为调度时钟到rcu_pending再到check_cpu_stall，补PROVE_RCU、一次报告、抑制和观察窗口限制。F0～F5与正文对应，Stall作为独立诊断，不再伪装成必经回收阶段。两图渲染目视，17条出链、8条标题入链、严格连续性及篇幅3345字词检查通过；全仓732篇、9379有效本地链接无缺失。未运行故意制造stall的代码或真实目标诊断。

## 1.234\_加速GP源码导读的执行者与任务责任复核

2026-10-17，冷读RCU源码导航P06，核对固定 `tree_exp.h` 的漏斗快速路径、叶选择、IPI处理、worker委托和结束唤醒，以及 `tree_plugin.h` 的任务登记和特殊退出。修正exp_hint独自承载迁移债务的说法，明确切换先保存原叶任务责任再报告CPU位；leader调用任务、推进worker和叶工作分别承担开始、选择与结束交付。

对齐X0～X8，补非抢占handler、早期启动、normal策略和合法单CPU快速分支。exp_wake_mutex串行结束/唤醒，下一轮开始可与上一轮剩余叶唤醒重叠。两图渲染目视，17条出链、4条标题入链、严格连续性和篇幅3507字词检查通过；全仓732篇、9381有效本地链接无缺失。未执行目标IPI、真实调度迁移或加速延迟实验。

## 1.235\_回调源码导读的等待地址与在途计数复核

2026-10-17，冷读RCU源码导航P07，核对固定 `tree.c` 的登记、accelerate、advance和batch，及 `tree_nocb.h` 的新工作/GP等待。修正把节点nocb_gp_wq数组写到rcu_data的错误，分别说明组首新工作队列、节点代际槽和每CPU回调队列。补offload可直入主链、回调可能在生产者返回前推进、总len覆盖旁路及已提取责任、线程执行不授权单个回调阻塞等边界。

统一N0～N7并映射C0～C5，使用两回调例子解释提取、回插与结算，明确NOCB仍需affinity才能离开隔离CPU。两图渲染目视，26条出链、8条标题入链、严格连续性和篇幅3804字词检查通过；全仓732篇、9383有效本地链接无缺失。未执行目标NOCB配置切换、回调洪峰或线程迁移实验。

## 1.236\_同步等待源码导读的迁移窗口与完成条件复核

2026-10-17，冷读RCU源码导航P08，对照固定 `tree.c` 与 `rcu_segcblist.c` 的同步等待、哨兵登记和barrier扫描。纠正迁移风险为目标已扫描而源尚未扫描，说明已登记源的entrain幂等早退；补离线offload队列、释放锁后同步IPI和失败重试的真实路径。SRS由cleanup与worker分担交付，哨兵沿末非空段接入，在途总长度非零而主链为空仍可接入DONE。

统一R0～R6，明确前序业务回调须执行返回，count=2保护扫描登记，扫描者也可能成为完成者。补模块关闭生产入口与两哨兵交错练习。两图渲染目视，20条出链、5条标题入链、严格连续性和篇幅3380字词检查通过；全仓732篇、9385有效本地链接无缺失。未执行真实CPU迁移、模块卸载或barrier压力实验。

## 1.237\_SRCU源码导读的状态地址与等待桥复核

2026-10-17，冷读RCU源码导航P09，对照固定 `srcutree.c`、`srcutree.h` 的累计计数、双扫描、尺寸状态、回调和同步等待。修正图中srcu_idx归属为srcu_struct，明确GP work的真实字段为srcu_usage.work；扫描先取所有possible CPU的unlock总数、屏障、再取lock总数。小域集中boot CPU队列，大域才分散，srcu_node组织需求而非读者退出证明。

对齐U0～U6，补未配平重试、flip不是独立编码、callback唤醒栈上completion及返回屏障。强调读侧可睡眠不授权回调阻塞。三图渲染目视，17条出链、6条标题入链、严格连续性与篇幅3542字词检查通过；全仓732篇、9387有效本地链接无缺失。未运行目标SRCU实例、迁移或销毁实验。

## 1.238\_Tasks源码导读的取证集合与交付复核

2026-10-17，冷读RCU源码导航P10，核对固定 `tasks.h`、`rcupdate_trace.h` 和 `kernel/workqueue.c`。由旧代码执行背景建立三种flavor，补控制实例、per-CPU队列选择、经典holdout引用与调度快照、退出补充路径。Trace按在线current快照再possible CPU阻塞链建立集合；特殊unlock写共享状态，后续GP扫描结合IPI结束才摘除holdout，不把旧注释当作直接唤醒证据。

统一K0～K6，补GP序列与回调completion的交付、早期启动和回调不阻塞条件。保留Rude辅助函数分配失败而调用者未检查的固定版本边界。两图渲染目视，10条本地出链、3条标题入链、严格连续性和篇幅3295字词检查通过；全仓732篇、9395有效本地链接无缺失。未执行目标Tasks线程、IPI或动态代码卸载实验，也未新增逐函数实现页。

## 1.239\_Tiny源码导读的单CPU交错与配置时间边界复核

2026-10-17，冷读RCU源码导航P11并核对固定 `tiny.c`、`rcutiny.h`。将旧配置核对明确为2026-09-05历史记录，区分当前固定源码阅读与未取得的目标运行状态。由硬中断打断旧读者引入延迟回收，分开头文件读侧展开与tiny.o提供的后端符号。

补A在QS前、B在QS后入队的T0～T5时序与指针槽位推演，说明本地IRQ保护共享链、摘链后私有执行、内核tick只催促QS，以及synchronize_rcu只推进poll序列而不推进回调成熟。两图渲染目视，18条出链、6条标题入链、严格连续性和篇幅2337字词检查通过；全仓732篇、9398有效本地链接无缺失。未重新构建内核、生成Bear数据库或执行目标中断实验。

## 1.240\_RCU检查适配导读的事件时序与有效性复核

2026-10-17，冷读RCU源码导航P12，核对固定 `rcupdate.h`、`update.c`、Tree/Tiny callback包装及配置定义。区分功能动作、Sparse静态注解、当前held账本、框架历史和检查器有效性；说明RCU acquire的read=2/check=0，不把map写成实体锁或所有路径通用的callback标签。

修正访问器先加载指针再诊断的顺序，列出普通/BH/sched held的实际分支以及关闭配置时的差异；补有效性重查、一次告警和无告警证明边界。按V1～V6贯通读侧事件，修正旧小节编号。三图渲染目视，19条出链、6条标题入链、严格连续性和篇幅3088字词检查通过；全仓732篇、9400有效本地链接无缺失。未执行目标Lockdep误用注入或新构建。

## 1.241\_公共RCU宏实现的求值与诊断边界复核

2026-10-17，冷读RCU实现P01，核对固定 `rcupdate.h` 的14处宏展开（含配置分支）。新增RCU_INITIALIZER的唯一类型桥接讲解，补常量NULL与运行时零值、组合替换并非原子交换、条件不应有业务副作用、protected内部local参数未使用等边界。以替换场景建立状态图与V阶段时序，各实现簇补修改约束，区别普通取得先加载与protected先诊断。

宏体去注释/续行后的C词法序列与固定源码一致；三图渲染目视，41条出链、33条标题入链及篇幅5990字词检查通过。严格连续性报告1项LINK_DENSITY，人工复核为源码索引、唯一实现交叉引用和底层所有权链接，正文已有本地因果与源码展开，保留这些必要链接。全仓732篇、9404有效本地链接无缺失。未重新构建内核、运行目标诊断或重跑此前Sparse实验。
