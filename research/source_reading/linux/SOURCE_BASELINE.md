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
| 版本 | Linux 6.12.20 |
| 原始位置 | `\\192.168.31.142\work\linux\nxp\kernel\linux-imx-6.12` |
| Git 提交 | 原始目录未提供可读取的 Git 提交标识 |
| 平台背景 | NXP i.MX 内核源码树；通用机制优先引用架构无关目录 |
| 许可证 | 以各源码文件 SPDX、版权头及原源码树 `COPYING`/`LICENSES` 为准 |

版本号读取自原源码树根 `Makefile`：`VERSION=6`、`PATCHLEVEL=12`、`SUBLEVEL=20`。

## 1.2\_保存规则

- 保持 Linux 上游相对路径，例如 `fs/char_dev.c` 保存为本目录的 `fs/char_dev.c`。
- C/H/RST 文件保持原文，不在源码文件内混入笔记；解释写入 Markdown 正文或独立源码导读。
- 只复制当前专题用于验证数据结构、调用链和状态机的文件，不无选择复制整棵源码树。
- 新增源码时同步更新本清单；若切换版本，必须记录新基线，不能让不同时期文件无标记混合。
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

## 1.4\_已有其他机制证据

本目录还保存 RCU、kobject、引用计数、内存管理和数据结构等已有源码。后续会根据实际来源逐步核对其版本；在完成核对前，不应仅凭目录共存就断言所有旧文件都来自本章记录的 6.12.20 基线。
