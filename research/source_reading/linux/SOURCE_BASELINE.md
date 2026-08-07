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
| 配置边界 | 核对时 `.config` 启用 `CONFIG_TREE_RCU=y` 与 `CONFIG_PREEMPT_RCU=y` |
| 平台背景 | NXP i.MX 厂商内核树，以 i.MX6ULL/ARM 为当前平台背景；通用机制优先引用架构无关目录 |
| 本地工作树 | 由每次会话在当前环境中发现并验证，不在仓库记录绝对路径 |
| 许可证 | 以各源码文件 SPDX、版权头及原源码树 `COPYING`/`LICENSES` 为准 |

发布标签已通过官方远端核对，其解引用提交的顶层 `Makefile` 为 `VERSION=6`、`PATCHLEVEL=12`、`SUBLEVEL=20`；仓库保存的 RCU 与内存序源码副本也与该提交逐文件 SHA-256 一致。配置边界来自此前核对的本地工作树，只说明阅读时采用的 Kconfig 分支，不代表发布标签自带 `.config`。`lf-6.12.y` 是可能继续前进的来源分支，不能替代发布标签及其不可变提交作为长期证据定位。

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

### 1.5.1\_Tree\_RCU与SRCU证据

下列 RCU 核心文件已在 2026-08-07 与发布标签 `lf-6.12.20-2.0.0` 解引用到的 Git 提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 逐文件核对，其中 `tree.c`、`tree.h`、`tree_plugin.h`、`update.c` 和 `rcupdate.h` 的仓库副本 SHA-256 与原文件一致：

| 相对路径 | 主要用途 |
| --- | --- |
| `kernel/rcu/tree.c` | 普通 GP 请求、初始化、QS 汇聚、FQS、cleanup、同步等待入口 |
| `kernel/rcu/tree.h` | `rcu_node`、`rcu_data`、`rcu_state` 与 Tree RCU 内部接口 |
| `kernel/rcu/tree_plugin.h` | PREEMPT_RCU / 非 PREEMPT_RCU 读侧、调度 QS、blocked task 与 boost |
| `kernel/rcu/update.c` | 通用等待 callback、RCU 初始化和部分公共实现 |
| `kernel/rcu/tree_exp.h` | expedited GP |
| `kernel/rcu/tree_nocb.h` | NOCB callback offload |
| `kernel/rcu/tree_stall.h` | stall 检测与诊断 |
| `kernel/rcu/rcu_segcblist.c`、`rcu_segcblist.h` | callback 分段列表实现 |
| `include/linux/rcupdate.h` | 公共读侧接口、发布/取得、`kfree_rcu()` |
| `include/linux/rculist.h` | list/hlist 的 RCU 访问封装 |
| `include/linux/rcu_segcblist.h` | callback 分段列表结构和接口 |
| `include/linux/srcu.h`、`srcutree.h`、`kernel/rcu/srcutree.c` | Tree SRCU 公共接口、状态和实现 |

调度入口 `kernel/sched/core.c`、任务字段 `include/linux/sched.h`、`kernel/rcu/tasks.h`、`kernel/rcu/tiny.c`、BPF/ftrace 调用方以及 6.12 context tracking 文件当前直接从只读原始源码树核对，未为单个调用点复制整个大文件。版本化阅读记录见：

- [`../rcu/P01_Linux_6.12_Tree_RCU_与_SRCU_源码导读.md`](../rcu/P01_Linux_6.12_Tree_RCU_与_SRCU_源码导读.md)
- [`../rcu/P02_Linux_6.12_非抢占式_Tree_RCU_源码调用链.md`](../rcu/P02_Linux_6.12_非抢占式_Tree_RCU_源码调用链.md)
- [`../rcu/P03_Linux_6.12_抢占式_Tree_RCU_源码调用链.md`](../rcu/P03_Linux_6.12_抢占式_Tree_RCU_源码调用链.md)
- [`../rcu/P04_Linux_6.12_Tasks_RCU与Tiny_RCU源码调用链.md`](../rcu/P04_Linux_6.12_Tasks_RCU与Tiny_RCU源码调用链.md)

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
