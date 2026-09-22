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

版本化导读见 [`../memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md`](../memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md)。

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
