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

阅读前可先打开[知识库专题阅读与评审地图](knowledge_review_map.md)，确认目标专题或章节处于 🔴 **未校正**、🟡 **人工评审中** 还是 🟢 **评审完成**。本页只解释知识关系，不重复维护评审状态。

## 1.1\_全局关系

```text
计算机与操作系统基础
        ↓
Linux 内核结构、模块与数据结构
        ↓
同步和异步机制、对象生命周期与 I/O
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
- [Linux 内核概貌：从读取一份文件开始](../../knowledge/linux/architecture/kernel_composition/linux内核概貌.md#1.1_先让程序读到几个字)：运行短读取程序，由结果建立应用、内核、缓存和设备的关系。
- [Linux 源码树：从问题找到文件](../../knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md#1.1_先区分源码目录与正在运行的系统)：按问题定位职责，区分源码、配置、构建产物和运行系统。初学者依照 [内核学习路线](../tracks/linux_kernel_track.md#1.2_第一阶段_内核边界与源码定位)连续读这两篇。
- [内核模块构建与部署](../../engineering/build/kernel_modules/大纲.md#1.1_四章怎样连起来)：先完成目标身份、Hello 构建、文件组织与装载排错，再进入[模块与设备节点](../../knowledge/linux/architecture/modules_and_device_nodes/Linux_内核模块与设备节点操作入门.md)。
- 《奔跑吧 Linux 内核》相关编排已归入[电子书目录](../../publications/books/running_linux_kernel/README.md)；其中的数据结构章节可作为知识正文的辅助阅读材料。

## 1.3\_通用机制

| 机制 | 解决的问题 | 当前入口 |
| --- | --- | --- |
| 数据结构 | 怎样在对象地址、成员关系、同步与查找代价之间做选择 | [Linux 双向循环链表](../../knowledge/linux/data_structures/单链表_linked_list/大纲.md#1.1_从一组任务走到容器选择)、[哈希表](../../knowledge/linux/data_structures/哈希表_Hash_Table/大纲.md#1.1_沿问题增加约束)、[树结构完整路线](../../knowledge/linux/data_structures/红黑树_rb-tree/大纲.md#1.1_沿问题进入现有章节)，其中[Maple 撤销周期](../../knowledge/linux/data_structures/红黑树_rb-tree/P42_撤销映射中的两棵Maple树.md#42.3_沿S0到S5观察职责转移)连接索引与对象退出 |
| 同步和异步机制 | 如何约束并发状态，并让事件跨上下文或时间继续推进 | [总纲](../../knowledge/linux/synchronization_and_asynchrony/大纲.md)、[同步机制](../../knowledge/linux/synchronization_and_asynchrony/synchronization/大纲.md)、[异步机制](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/大纲.md) |
| 生命周期 | 如何确保对象被安全持有和释放 | [kref 引用责任](../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.6.1_运行完整的责任交接模型)、[静态模块](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.14.2_运行一个不释放静态内存的完整模块)、[快照对照](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.17.1_运行快照与持有的对照程序)、[对象模板](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.19_标准自定义引用对象模板)、[C++ 对照](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.23.1_用完整程序观察自动归还)、[容器责任](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.30.1_设计_A_容器持有引用)、[业务状态模型](../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.3.1_用C模型观察仍持有却被拒绝)、[清理诊断](../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.7.1_release_阶段_对象销毁点)、[交付票据](../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#%287%29_所有权表要补充失败路径和取消路径)、[关闭周期](../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.16_一个完整的生命周期模板)、[引用规则](../../knowledge/linux/object_lifetime/kref/P04_kref_三条核心规则.md#4.12_mutex/list_lookup_的最小模型)、[条件取得](../../knowledge/linux/object_lifetime/kref/P05_基础_API_源码逐行讲解.md#5.7.2_kref_get_unless_zero%28%29_的使用场景)、[锁交接](../../knowledge/linux/object_lifetime/kref/P05_基础_API_源码逐行讲解.md#5.8.2_kref_put_mutex%28%29_的典型用途)、[接口契约](../../knowledge/linux/object_lifetime/kref/P05_基础_API_源码逐行讲解.md#5.10_API_封装模板)与[固定源码索引](../../research/source_reading/kref/navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)、[devres](../../knowledge/linux/object_lifetime/devres/devres_API说明.md) |
| I/O 模型 | 用户进程如何等待设备事件并完成数据传输 | [poll 与 epoll](../../knowledge/linux/io_model/blocking_io/poll与epoll的区别.md)、[VFS I/O 数据路径](../../knowledge/kernel_subsystems/vfs/P14_VFS_read_write分派.md) |
| 错误处理 | 如何在指针返回值中表达错误 | [错误指针专题](../../knowledge/linux/error_handling/error_pointer/大纲.md#1.1_四次认识变化) |

## 1.4\_子系统与驱动模型

- [中断的定位与演化](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P01_中断的定位与演化.md)解释硬件事件进入 Linux 后的处理链。
- [VFS 子系统](../../knowledge/kernel_subsystems/vfs/大纲.md)完整解释文件系统注册、挂载、路径、打开文件、I/O、缓存和对象回收；字符设备只是其特殊文件交叉分支之一。
- [设备模型抽象机制与 Driver Core 状态拓扑](../../knowledge/linux/device_model/大纲.md)解释 kobject、device、driver、bus 与 class 的关系，以及注册、匹配和生命周期状态机。
- [class 与 sysfs](../../knowledge/linux/device_model/class_sysfs/大纲.md#1.1_从分类观察到可控数据入口)通过分类实例和属性控制模块，把功能视图、字符分派、节点发布及业务同步分开观察。
- [驱动框架入门](../../knowledge/driver_model/fundamentals/framework_model/大纲.md#1.1_四个问题怎样接起来)从多实例引出对象关系、绑定周期、sysfs 属性与 misc 字符入口，配套完整模块和固定版本源码。
- [文件操作教材](../../knowledge/driver_model/file_operations/大纲.md#1.1_沿对象寿命逐步增加约束)在 misc 与字符读写之后，通过打开引用、迭代请求及只读映射连接 VFS 的文件和内存模型。
- [字符设备最小模型](../../knowledge/driver_model/character_device/P01_字符设备最小模型.md)解释设备号、`cdev`、VFS 与文件操作如何形成用户入口。
- [GPIO 专题](../../knowledge/driver_model/gpio/大纲.md)从连接抽象、状态机和源码实现连接控制器、消费者、设备树与中断。
- [旧式平台设备与资源机制](../../knowledge/driver_model/device_tree/设备树+platform开发/P01_旧式平台设备与资源机制.md)进入 Platform 与设备树匹配。
- [Input 子系统起点](../../knowledge/driver_model/input/P01_从硬件样本到统一输入事件.md)从问题出发建立完整输入事件流水线。

## 1.5\_系统启动与构建

启动和构建内容形成另一条纵向链路：

1. [U-Boot 启动流程](../../publications/books/running_linux_kernel/P04_uboot启动流程说明.md)。
2. [内核引导和初始化](../../publications/books/running_linux_kernel/P03_内核引导和初始化.md)。
3. [Buildroot 引言与基础](../../knowledge/system_software/buildroot/P01_引言与基础.md)。
4. [文件系统构建与定制](../../knowledge/system_software/buildroot/P05_文件系统构建与定制.md)。

## 1.6\_平台与证据

- 平台实现记录：[i.MX6ULL 移植](../../platforms/arm/nxp/imx6ull/porting/imx6ull-移植u-boot-2025.04_and_kernel-6.1.md)、[RK3566 Linux SDK 编译](../../platforms/arm/rockchip/rk3566/environment/linux_sdk编译说明.md)。
- 外部规范与芯片资料：[Arm、GIC、RK3588 与 i.MX6ULL 版本选择](../../reference/external_resources/arm/README.md#1.4_版本选择与阅读顺序)。该入口提供原始资料身份和取得方式，不替代通用机制正文或平台实现记录。
- GIC 硬件学习：[GICv3 物理中断专题](../../platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图)。从零建立概念并沿官方目录查证，先学 GIC-600 r1p6 / GICv3.0，不以 GICv2 或 GICv4 为前置，也不并入 Linux IRQ 管理正文。
- 最小验证实验：[i.MX6ULL 驱动实验](../../labs/platforms/nxp/imx6ull/drivers)。
- 调查材料入口：[调查目录说明](../../research/investigations/README.md)。
- 版本化并发源码入口：[锁](../../research/source_reading/locking/navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.6_建议阅读顺序)、[序列计数器](../../research/source_reading/sequence_counters/navigation/P01_Linux_6.12_序列计数器源码总阅读索引.md#1.5_建议阅读顺序)、[等待与完成量](../../research/source_reading/waiting_notification/navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.5_建议阅读顺序)、[工作队列](../../research/source_reading/workqueue/navigation/P01_Linux_6.12_工作队列源码总阅读索引.md#1.6_建议阅读顺序)、[Lockdep](../../research/source_reading/lockdep/navigation/P01_Linux_6.12_Lockdep源码导读.md#1.6_建议阅读顺序)、[RCU](../../research/source_reading/rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.6_建议的源码阅读顺序)。
- 跨职责聚焦阅读：[RCU MarkBook](../../markbook/topics/rcu/README.md#1.2_发行入口)把稳定正文、版本源码和实验编排进同一月度快照；它只改变阅读界面，不改变各层实现所有权。

学习时应先从通用文档形成模型，再用平台记录确认差异，用实验确认行为，最后以特定版本源码材料解释实现细节。
