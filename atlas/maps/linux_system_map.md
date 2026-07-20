---
id: atlas.maps.linux_system
title: "Linux系统与驱动知识地图"
kind: track
status: maintained
domains:
  - navigation
  - linux
  - driver
---

# 第1章\_Linux系统与驱动知识地图

## 1.1\_全局关系

```text
计算机与操作系统基础
        ↓
Linux 内核结构、模块与数据结构
        ↓
并发同步、对象生命周期、时间与 I/O
        ↓
中断、设备模型和驱动框架
        ↓
字符设备、GPIO、设备树、Platform、Input
        ↓
具体 SoC 平台、实验、调试与源码证据

系统构建链：U-Boot → Linux kernel → Buildroot 根文件系统
```

上层知识解释下层机制成立的原因，下层材料展示机制如何组合。平台和实验用于验证，不反向替代通用原理。

## 1.2\_基础与内核骨架

- [宏内核和微内核](../../knowledge/foundations/operating_systems/concepts/宏内核和微内核.md)：理解内核组织方式。
- [Linux 内核概貌](../../knowledge/linux/architecture/kernel_composition/linux内核概貌.md)：建立 Linux 内核组成视图。
- [Linux kernel 目录结构说明](../../knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md)：定位源码功能域。
- [Linux 内核模块与设备节点操作基础](../../knowledge/linux/architecture/modules_and_device_nodes/Linux_内核模块与设备节点操作基础.md)：连接模块、设备号和用户空间入口。
- 《奔跑吧 Linux 内核》相关编排已归入[电子书目录](../../publications/books/running_linux_kernel/README.md)；其中的数据结构章节可作为知识正文的辅助阅读材料。

## 1.3\_通用机制

| 机制 | 解决的问题 | 当前入口 |
| --- | --- | --- |
| 数据结构 | 如何组织和检索内核对象 | [单链表](../../knowledge/linux/data_structures/单链表_linked_list/大纲.md)、[哈希表](../../knowledge/linux/data_structures/哈希表_Hash_Table/P01_数据结构理论基础/P01_哈希表核心原理_空间与时间的终极博弈.md)、[红黑树](../../knowledge/linux/data_structures/红黑树_rb-tree/P01_树的基本概念.md) |
| 并发与同步 | 如何处理竞争、可见性和执行上下文约束 | [Linux 同步机制总纲](../../knowledge/linux/synchronization/大纲.md) |
| 生命周期 | 如何确保对象被安全持有和释放 | [kref](../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md)、[devres](../../knowledge/linux/object_lifetime/devres/devres_API说明.md) |
| 时间管理 | 如何完成延时、超时和定时回调 | [驱动中的时间问题](../../knowledge/linux/time_management/定时器简介/P01_驱动中的_时间问题_概述.md) |
| I/O 模型 | 用户进程如何等待或接收设备事件 | [poll 与 epoll](../../knowledge/linux/io_model/blocking_io/poll与epoll的区别.md)、[异步通知](../../knowledge/linux/io_model/async_notification/P01_异步通知全景与知识地图.md) |
| 错误处理 | 如何在指针返回值中表达错误 | [错误指针机制](../../knowledge/linux/error_handling/error_pointer/错误指针机制简介.md) |

## 1.4\_子系统与驱动模型

- [中断的定位与演化](../../knowledge/kernel_subsystems/irq/中断机制简介/P01_中断的定位与演化.md)解释硬件事件进入 Linux 后的处理链。
- [设备模型基础与对象模型](../../knowledge/linux/device_model/设备模型简介/P01_基础与对象模型.md)解释 kobject、device、driver、bus 与 class 的关系。
- [驱动框架模型](../../knowledge/driver_model/fundamentals/framework_model/P01_驱动框架模型.md)把公共机制映射到驱动结构。
- [字符设备最小模型](../../knowledge/driver_model/character_device/P01_字符设备最小模型.md)解释设备号、`cdev`、VFS 与文件操作如何形成用户入口。
- [GPIO 总体框架](../../knowledge/driver_model/gpio/gpio子模块/P01_基础与总体框架.md)连接控制器、消费者、设备树与中断。
- [旧式平台设备与资源机制](../../knowledge/driver_model/device_tree/设备树+platform开发/P01_旧式平台设备与资源机制.md)进入 Platform 与设备树匹配。
- [Input 子系统起点](../../knowledge/driver_model/input/input_子系统简介/P01_为什么需要_Linux_Input_从问题到最小可跑.md)展示完整输入事件流水线。

## 1.5\_系统启动与构建

启动和构建内容形成另一条纵向链路：

1. [U-Boot 启动流程](../../publications/books/running_linux_kernel/P04_uboot启动流程说明.md)。
2. [内核引导和初始化](../../publications/books/running_linux_kernel/P03_内核引导和初始化.md)。
3. [Buildroot 引言与基础](../../knowledge/system_software/buildroot/P01_引言与基础.md)。
4. [文件系统构建与定制](../../knowledge/system_software/buildroot/P05_文件系统构建与定制.md)。

## 1.6\_平台与证据

- 平台实现记录：[i.MX6ULL 移植](../../platforms/arm/nxp/imx6ull/porting/imx6ull-移植u-boot-2025.04_and_kernel-6.1.md)、[RK3566 Linux SDK 编译](../../platforms/arm/rockchip/rk3566/environment/linux_sdk编译说明.md)。
- 最小验证实验：[i.MX6ULL 驱动实验](../../labs/platforms/nxp/imx6ull/drivers)。
- 调查材料入口：[调查目录说明](../../research/investigations/README.md)。

学习时应先从通用文档形成模型，再用平台记录确认差异，用实验确认行为，最后以特定版本源码材料解释实现细节。
