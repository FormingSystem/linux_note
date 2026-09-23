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

[P34 页级索引](../../../knowledge/linux/data_structures/红黑树_rb-tree/P34_从多路节点到页级索引.md#34.2.13_文件系统中的_B/B+_树应用)只读核对同一 NXP 固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0 的 Documentation/filesystems/ext4/ifork.rst，blob 为 dc31f505e6c835bf590998a88676e9fa48d4c0f9。文档区分内部 ext4_extent_idx 与叶层 ext4_extent，根放在 inode.i_block；本次仅引用格式职责和根不一定独占外部块的边界，不展开函数体，不声称已验证挂载模式、分配算法或 I/O 性能。

数据库/存储引擎对照独立依据 SQLite 文件格式、MySQL 8.4、PostgreSQL 18 和 RocksDB 官方概览，不作为 NXP 内核实现证据。完整 C++ 四页模型实际只读宿主数组，逻辑页请求、模型未命中与设备 I/O 分开；未执行目标数据库、文件系统挂载或硬件性能实验。外部工作树未改，本地三笔实验提交仍不作证据。

## 1.28\_Maple范围与VMA查询证据

[P14 VMA 范围教材](../../../knowledge/linux/data_structures/红黑树_rb-tree/P14_Maple_Tree_与_VMA_管理.md#14.1_一个地址为什么需要三种查询)沿同一固定提交核对区间、节点容量、查询封装和返回对象保护；[Maple 总索引](../maple_tree/navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)区分模块导读与唯一函数体。P15 的四个 VMA 查询函数体移入对应上游路径的实现文档，其余 P15 内容仍待独立审查。

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

[P12 并发单元](../../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#12.4_rbtree_与并发控制)补 M0～M4，宿主 copy_under_lock 使用真实 pthread、互斥与条件变量重放两种观察次序；重复 512 次并检查两处分配失败，既有 lookup_paths 四行结果重新验证。用户态模型只含一个入口槽，不实现树或内核锁；没有本批 ARM 并发、Linux RCU、SMP 压力或目标模块执行证据，外部树未改。

## 1.33\_rbtree调用者示例的比较收敛

[P12 示例回访](../../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#12.5_Linux_内核_rbtree_示例代码)统一使用 P37 的完整 owner 模块和 P27 的完整遍历材料，不新增第二份内核接口实现。owner 的查找与插入共享 compare_key，关系判断结果相减避免任意 int 键相减溢出。宿主重新执行 36 组操作次序、四处分配失败以及 INT_MIN/INT_MAX 等极值接入、查找、拒绝重复和移除。ARMv7 前端读取 354 份头，消费的已跟踪头与官方固定提交无差量。没有目标 Kbuild、MODPOST、装卸或并发运行证据，未修改外部树。

## 1.34\_rbtree联合不变量的有限检查

[P12 有界快照](../../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行有界快照检查器)使用 C11 固定容量数组与整数槽号，在访问前识别已知对象、拒绝重复进入，再检查键界、父链、颜色、黑计数、原始载荷重算摘要、计数与缓存身份。十二类样例通过；固定增强算法宿主夹具的 120960 个稳定状态转换为模型后通过联合检查，各状态再注入计数错误与非空摘要错误验证拒绝。固定算法来源与显式位宽适配仍见 1.31，不新增内核原文或另一套实现讲解。

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

B05b只读固定dfaf2136的Documentation/memory-barriers.txt，并核对已保存rwonce、通用屏障与ARM屏障原文；[内存顺序导读](../memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md#1.14_配套入口)关联M0～M3阅读任务。宿主C++17实验完成100轮独立发布，依据标准原子契约，不代表Linux内核宏、LKMM、SMP、目标ARM或实时分支运行；当前UP配置不变。

## 1.110\_编译器轮询反例校正

B05c只读固定dfaf2136的arch/arm/include/asm/vdso/processor.h，确认cpu_relax通常含barrier，特定架构/勘误分支更强；修复正文错误的无约束循环前提。已有rwonce与compiler证据继续使用固定源码；[七函数编译记录](../../../labs/kernel/memory_ordering/P01_READ_ONCE_编译器访问实验/expected/2026-09-24_compiler_access.md)仅覆盖两种x86-64目标的GCC/Clang O0/O2，不覆盖ARM执行、真实内核宏或KCSAN。

## 1.111\_屏障方向与UP配置边界

B05d核对固定include/asm-generic/barrier.h的CONFIG_SMP分支和ARM屏障映射，公共UP回退与底层__smp宏存在分开说明；[原语导读](../memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md#1.3.2_通用屏障)继续承担入口。C单槽待写模型及单边约束变体严格编译执行，只说明教学规则下的结果，未执行herd7、Linux屏障、ARM SMP或设备协议。

## 1.112\_发布取得与反向归还

B05e沿固定通用release/acquire回退的类型和先后关系核对正文边界，删去说明性冒号伪C而保留职责表及[源码导读](../memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md#1.3.2_通用屏障)。单槽一万轮宿主C++17发布/归还通过，标准原子一致性和严格单生产/单消费协议不冒充Linux实现；未运行herd7、目标ARM、取消、故障恢复或多参与者队列。

## 1.113\_依赖数据流与RCU取得边界

B05f只读固定Documentation/RCU/rcu_dereference.rst和include/linux/rcupdate.h，核对地址/数据依赖、普通判空与已知地址比较、READ_ONCE特例及check/protected前提；[RCU模块导读](../rcu/navigation/P02_Linux_6.12_RCU公共接口与读侧模型模块源码概念导读.md#2.9_唯一实现讲解入口)保留唯一实现入口。五函数C在GCC14.2和Clang18.1 x86-64 O2严格编译并观察：Clang替换固定地址比较后的访问来源，GCC本次仍保留指针寄存器；不以特定汇编替代Linux或ARM语义。未执行RCU、herd7或回收路径。

## 1.114\_原子更新与条件失败契约

B05g只读固定Documentation/atomic_t.txt全篇，核对非RMW、返回值、显式顺序、条件失败、atomic_set与RMW不可分性、期望值回写、辅助屏障范围及前进性；[阅读入口](../memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md#1.11_官方文档证据)关联原始副本，其Git对象bee3b1bca9a7b46bcf9911f036c3280e77b4405a与固定提交一致。工作树HEAD仍是本地实验提交，不作为证据；当前配置仍UP、TINY_RCU和PREEMPT_NONE。宿主C++17严格编译后四工作者完成40,000次更新、错误0，同时单线程反例显示期望值重用会使OWNED→OWNED也成功。不声称执行Linux/ARM原子或herd7，不以宿主原子替代目标实现。
