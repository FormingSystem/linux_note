---
id: atlas.indexes.content
title: "仓库内容索引"
kind: reference
status: maintained
domains:
  - navigation
  - repository
---

# 第1章\_仓库内容索引

本索引按内容本质提供稳定入口。专题内部的章节顺序以目录中的 `PXX` 文件和大纲为准。

## 1.1\_基础知识

| 领域 | 当前内容 |
| --- | --- |
| 计算机体系结构 | [缓存一致性专题](../../knowledge/foundations/computer_architecture/cache_coherence/大纲.md) |
| 操作系统概念 | [宏内核和微内核](../../knowledge/foundations/operating_systems/concepts/宏内核和微内核.md) |
| C 语言扩展 | [GNU C 扩展](../../knowledge/foundations/c_language/gnu_extensions/C_language_extension.md) |

## 1.2\_Linux通用机制

| 领域 | 当前内容入口 |
| --- | --- |
| 内核架构 | [内核概貌](../../knowledge/linux/architecture/kernel_composition/linux内核概貌.md)、[源码树](../../knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md)、[模块与设备节点](../../knowledge/linux/architecture/modules_and_device_nodes) |
| 数据结构 | [单链表](../../knowledge/linux/data_structures/单链表_linked_list/大纲.md)、[哈希表专题](../../knowledge/linux/data_structures/哈希表_Hash_Table)、[红黑树专题](../../knowledge/linux/data_structures/红黑树_rb-tree) |
| 并发同步 | [Linux 同步机制总纲](../../knowledge/linux/synchronization/大纲.md)、[RCU 专题](../../knowledge/linux/synchronization/rcu/大纲.md) |
| 对象生命周期 | [kref](../../knowledge/linux/object_lifetime/kref)、[devres](../../knowledge/linux/object_lifetime/devres) |
| 时间管理 | [定时器专题](../../knowledge/linux/time_management/定时器简介) |
| I/O 模型 | [阻塞 I/O](../../knowledge/linux/io_model/blocking_io)、[异步通知](../../knowledge/linux/io_model/async_notification/异步通知简介) |
| 设备模型 | [设备模型专题](../../knowledge/linux/device_model/设备模型简介) |
| 错误处理 | [错误指针](../../knowledge/linux/error_handling/error_pointer) |

## 1.3\_内核子系统与驱动模型

| 领域 | 当前内容入口 |
| --- | --- |
| 中断 | [中断机制专题](../../knowledge/kernel_subsystems/irq/中断机制简介) |
| 日志与跟踪 | [Linux 内核日志](../../knowledge/kernel_subsystems/tracing/logging/Linux_内核日志.md) |
| 驱动基础 | [驱动框架模型](../../knowledge/driver_model/fundamentals/framework_model) |
| 字符设备 | [character_device](../../knowledge/driver_model/character_device) |
| 设备树与 Platform | [device_tree](../../knowledge/driver_model/device_tree)、[platform_bus](../../knowledge/driver_model/platform_bus/readme.md) |
| GPIO | [gpio](../../knowledge/driver_model/gpio) |
| Input | [input](../../knowledge/driver_model/input/readme.md) |
| misc 设备 | [misc](../../knowledge/driver_model/misc/readme.md) |

## 1.4\_系统软件

| 领域 | 当前内容入口 |
| --- | --- |
| Buildroot | [学习地图](../../knowledge/system_software/buildroot/P00_全书学习地图.md) |
| Kconfig | [基础语法](../../knowledge/system_software/kconfig/基础语法.md) |
| 链接脚本 | [LDS 基础语法](../../knowledge/system_software/linker/lds_基础语法.md) |
| U-Boot | [Makefile](../../knowledge/system_software/uboot/uboot-makefile.md)、[问题记录](../../knowledge/system_software/uboot/uboot提问.md) |

## 1.5\_平台\_实验\_研究与参考

| 类型 | 当前内容入口 |
| --- | --- |
| i.MX6ULL 平台 | [U-Boot 与内核移植](../../platforms/arm/nxp/imx6ull/porting/imx6ull-移植u-boot-2025.04_and_kernel-6.1.md)、[内核配置编译](../../platforms/arm/nxp/imx6ull/porting/imx_v8_config_kernel编译说明.md) |
| RK3566 平台 | [Linux SDK 编译](../../platforms/arm/rockchip/rk3566/environment/linux_sdk编译说明.md) |
| i.MX6ULL 实验 | [驱动实验目录](../../labs/platforms/nxp/imx6ull/drivers) |
| 调查 | [investigations](../../research/investigations/README.md) |
| 源码阅读 | [Linux 源码阅读目录](../../research/source_reading/linux) |
| 标准 | [GPL 协议说明](../../reference/standards/gpl/GPL协议说明.md) |

## 1.6\_电子书与出版物

| 书名 | 当前内容入口 |
| --- | --- |
| 《奔跑吧 Linux 内核（入门篇·第 2 版）》 | [电子书说明与目录](../../publications/books/running_linux_kernel/README.md) |

## 1.7\_维护入口

- [知识库导航](../home.md)
- [知识库建设路线图](../roadmaps/content_roadmap.md)
- [仓库信息架构设计](../../governance/architecture/repository_information_architecture.md)
- [Git 协作与提交规范](../../governance/conventions/git_guide.md)
- [全量目录重构记录](../../governance/migration/P01_全量目录重构记录.md)
