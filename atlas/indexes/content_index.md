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

本索引按内容本质提供稳定入口。专题内部的章节顺序以目录中的 `PXX` 文件和大纲为准；人工确认程度统一查看[知识库专题阅读与评审地图](../maps/knowledge_review_map.md)，本索引不复制评审状态。

## 1.1\_基础知识

| 领域 | 当前内容 |
| --- | --- |
| 计算机体系结构 | [缓存一致性专题](../../knowledge/foundations/computer_architecture/cache_coherence/大纲.md)、[体系结构内存顺序专题](../../knowledge/foundations/computer_architecture/memory_ordering/大纲.md) |
| 操作系统概念 | [宏内核和微内核](../../knowledge/foundations/operating_systems/concepts/宏内核和微内核.md) |
| C 语言扩展与分析注解 | [GNU C 扩展](../../knowledge/foundations/c_language/gnu_extensions/C_language_extension.md)、[Linux 内核编译器与静态分析注解专题](../../knowledge/foundations/c_language/kernel_static_annotations/大纲.md#1.1_专题定位) |

## 1.2\_Linux通用机制

| 领域 | 当前内容入口 |
| --- | --- |
| 内核架构 | [内核概貌](../../knowledge/linux/architecture/kernel_composition/linux内核概貌.md)、[源码树](../../knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md)、[模块与设备节点](../../knowledge/linux/architecture/modules_and_device_nodes) |
| 数据结构 | [单链表](../../knowledge/linux/data_structures/单链表_linked_list/大纲.md)、[哈希表专题](../../knowledge/linux/data_structures/哈希表_Hash_Table)、[红黑树专题](../../knowledge/linux/data_structures/红黑树_rb-tree) |
| 同步和异步机制 | [总纲](../../knowledge/linux/synchronization_and_asynchrony/大纲.md)、[同步机制](../../knowledge/linux/synchronization_and_asynchrony/synchronization/大纲.md)、[异步机制](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/大纲.md)、[锁](../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/大纲.md)、[序列计数器](../../knowledge/linux/synchronization_and_asynchrony/synchronization/sequence_counters/大纲.md)、[等待与完成量](../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/大纲.md)、[RCU](../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/大纲.md)、[Lockdep](../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/大纲.md)、[工作队列](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/workqueue/大纲.md) |
| 对象生命周期 | [kref](../../knowledge/linux/object_lifetime/kref)、[devres](../../knowledge/linux/object_lifetime/devres) |
| I/O 模型 | [阻塞 I/O](../../knowledge/linux/io_model/blocking_io)、[MMIO](../../knowledge/linux/io_model/mmio/大纲.md)、[DMA](../../knowledge/linux/io_model/dma/大纲.md) |
| 设备模型 | [设备模型专题](../../knowledge/linux/device_model/大纲.md) |
| 错误处理 | [错误指针](../../knowledge/linux/error_handling/error_pointer) |

## 1.3\_内核子系统与驱动模型

| 领域 | 当前内容入口 |
| --- | --- |
| VFS | [VFS 子系统专题](../../knowledge/kernel_subsystems/vfs/大纲.md) |
| 日志与跟踪 | [Linux 内核日志](../../knowledge/kernel_subsystems/tracing/logging/Linux_内核日志.md) |
| 驱动基础 | [驱动框架模型](../../knowledge/driver_model/fundamentals/framework_model) |
| 字符设备 | [character_device](../../knowledge/driver_model/character_device) |
| 设备树与 Platform | [device_tree](../../knowledge/driver_model/device_tree)、[platform_bus](../../knowledge/driver_model/platform_bus/readme.md) |
| GPIO | [GPIO 专题](../../knowledge/driver_model/gpio/大纲.md)、[标准 GPIO Consumer](../../knowledge/driver_model/gpio_consumers/大纲.md) |
| Input | [专题大纲](../../knowledge/driver_model/input/大纲.md) |
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
| 内存顺序实验 | [访问宽度与 ARM 反汇编](../../labs/foundations/computer_architecture/memory_ordering/P01_访问宽度_对齐与ARM反汇编/README.md)、[READ_ONCE 编译器访问](../../labs/kernel/memory_ordering/P01_READ_ONCE_编译器访问实验/README.md)、[LKMM Litmus](../../labs/kernel/memory_ordering/P02_LKMM_Litmus_消息传递与屏障/README.md) |
| C 语言静态分析实验 | [Sparse 地址空间与上下文记账](../../labs/foundations/c_language/P01_Sparse地址空间与上下文记账/README.md#1.1_实验目标) |
| Lockdep 实验 | [锁顺序反转与报告解读](../../labs/kernel/lockdep/P01_锁顺序反转与报告解读/README.md) |
| i.MX6ULL 实验 | [驱动实验目录](../../labs/platforms/nxp/imx6ull/drivers) |
| 调查 | [investigations](../../research/investigations/README.md) |
| 源码阅读 | [Linux 源码阅读基线](../../research/source_reading/linux/SOURCE_BASELINE.md#1.1_当前来源)、[Linux 6.12 编译器与 Sparse 注解导读](../../research/source_reading/compiler_annotations/navigation/P01_Linux_6.12_编译器与Sparse注解源码导读.md#1.1_基线与阅读任务)、[Linux 6.12 LKMM 导读](../../research/source_reading/memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md)、[Linux 6.12 RCU 总阅读索引](../../research/source_reading/rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.6_建议的源码阅读顺序)、[Linux 6.12 Lockdep 总阅读索引](../../research/source_reading/lockdep/navigation/P01_Linux_6.12_Lockdep源码导读.md#1.1_基线与阅读目标) |
| 标准 | [GPL 协议说明](../../reference/standards/gpl/GPL协议说明.md) |

## 1.6\_电子书与出版物

| 书名 | 当前内容入口 |
| --- | --- |
| 《深入理解 Linux RCU》MarkBook | [当前版本](../../markbook/topics/rcu/latest.html)、[版本目录与说明](../../markbook/topics/rcu/README.md#1.2_发行入口)、[2026.09 来源与产物台账](../../markbook/topics/rcu/releases/2026.09/publication.json) |
| 《奔跑吧 Linux 内核（入门篇·第 2 版）》 | [电子书说明与目录](../../publications/books/running_linux_kernel/README.md) |

## 1.7\_维护入口

- [知识库导航](../home.md)
- [知识库专题阅读与评审地图](../maps/knowledge_review_map.md)
- [知识库建设路线图](../roadmaps/content_roadmap.md)
- [Linux I/O 与驱动子系统建设路线](../roadmaps/linux_io_driver_subsystems.md)
- [仓库信息架构设计](../../governance/architecture/repository_information_architecture.md)
- [Git 协作与提交规范](../../governance/conventions/git_guide.md)
- [全量目录重构记录](../../governance/migration/P01_全量目录重构记录.md)
- [并发与竞争专题迁移记录](../../governance/migration/P02_并发与竞争专题迁移地图.md)
