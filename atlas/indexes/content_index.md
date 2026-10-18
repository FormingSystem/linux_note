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

本索引按内容本质提供稳定入口。专题内部按大纲的依赖顺序阅读；`PXX` 是稳定定位编号，新增章节插入学习链后不一定按文件名排序；人工确认程度统一查看[知识库专题阅读与评审地图](../maps/knowledge_review_map.md)，本索引不复制评审状态。

## 1.1\_基础知识

| 领域 | 当前内容 |
| --- | --- |
| 计算机体系结构 | [存着程序的芯片为什么不一定能直接取指](../../knowledge/foundations/computer_architecture/boot_storage_paths.md)、[地址空间有限为什么仍能处理更大的数据集](../../knowledge/foundations/computer_architecture/address_space_and_capacity.md)、[缓存一致性专题](../../knowledge/foundations/computer_architecture/cache_coherence/大纲.md)、[体系结构内存顺序专题](../../knowledge/foundations/computer_architecture/memory_ordering/大纲.md) |
| 操作系统概念 | [宏内核和微内核](../../knowledge/foundations/operating_systems/concepts/宏内核和微内核.md) |
| C 语言扩展与分析注解 | [GNU C 扩展](../../knowledge/foundations/c_language/gnu_extensions/C_language_extension.md)、[Linux 内核编译器与静态分析注解专题](../../knowledge/foundations/c_language/kernel_static_annotations/大纲.md#1.1_专题定位) |

## 1.2\_Linux通用机制

| 领域 | 当前内容入口 |
| --- | --- |
| 内核架构 | [从读取文件认识内核](../../knowledge/linux/architecture/kernel_composition/linux内核概貌.md#1.1_先让程序读到几个字)、[从问题定位源码](../../knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md#1.1_先区分源码目录与正在运行的系统)、[模块与设备入口](../../knowledge/linux/architecture/modules_and_device_nodes/大纲.md#1.1_沿三个问题进入正文) |
| 数据结构入门 | [链表](../../knowledge/linux/data_structures/单链表_linked_list/大纲.md#1.1_从一组任务走到容器选择)、[哈希表](../../knowledge/linux/data_structures/哈希表_Hash_Table/大纲.md#1.1_沿问题增加约束)：从成员关系和键查找进入共享修改、扩容与返回对象寿命 |
| 树与范围结构 | [树结构学习路线](../../knowledge/linux/data_structures/红黑树_rb-tree/大纲.md#1.1_沿问题进入现有章节)：从树表示、BST 和旋转进入平衡树，再比较页级索引与 Maple 范围结构；各章的完整实验与反例由大纲逐项连接 |
| 同步和异步机制 | [消息发布入门](../../knowledge/linux/synchronization_and_asynchrony/synchronization/memory_ordering/P01_READ_WRITE_ONCE_与_SMP_内存顺序原语.md#1.2.1_从准备到使用的四个阶段)、[执行路径先修](../../knowledge/linux/synchronization_and_asynchrony/P01_同一对象的多条执行路径.md#1.1_先把一次加一拆开)、[总纲](../../knowledge/linux/synchronization_and_asynchrony/大纲.md)、[同步机制](../../knowledge/linux/synchronization_and_asynchrony/synchronization/大纲.md)、[异步机制](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/大纲.md)、[锁](../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/大纲.md)、[序列计数器](../../knowledge/linux/synchronization_and_asynchrony/synchronization/sequence_counters/大纲.md)、[等待与完成量](../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/大纲.md)、[RCU](../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/大纲.md)、[Lockdep](../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/大纲.md)、[工作队列](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/workqueue/大纲.md)、[Tiny 构建证据](../../engineering/build/P01_Tiny_RCU配置与构建证据核对.md#1.1_源码能跳转不代表镜像用了这条路径) |
| 对象生命周期 | [kref连续阅读地图](../../knowledge/linux/object_lifetime/kref/大纲.md#第1章_kref_引用计数机制章节大纲)、[工程模板选择](../../knowledge/linux/object_lifetime/kref/P13_工程模板.md#13.9_本章模板总表)、[源码实验路线](../../knowledge/linux/object_lifetime/kref/大纲.md#1.15_源码阅读实验)、[诊断与练习](../../knowledge/linux/object_lifetime/kref/P12_典型错误模式与调试线索.md#12.10.2_最小审查流程)、[固定版本源码入口](../../research/source_reading/kref/navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)、[devres资源账本](../../knowledge/linux/object_lifetime/devres/大纲.md#1.1_从退出责任进入资源接口) |
| I/O 模型 | [阻塞 I/O](../../knowledge/linux/io_model/blocking_io)、[MMIO](../../knowledge/linux/io_model/mmio/大纲.md)、[DMA](../../knowledge/linux/io_model/dma/大纲.md) |
| 设备模型 | [设备模型专题](../../knowledge/linux/device_model/大纲.md) |
| class 与 sysfs | [分类、属性与节点教材](../../knowledge/linux/device_model/class_sysfs/大纲.md#1.1_从分类观察到可控数据入口)、[成员参考](../../knowledge/driver_model/fundamentals/kernel_driver_mechanisms/data_strcuture_说明/struct_class.md#1.2_按职责查询当前成员)、[实验材料](../../labs/kernel/class_sysfs/materials/README.md)、[源码导读](../../research/source_reading/driver_entries/navigation/P03_分类对象与属性事务导读.md) |
| 错误处理 | [错误指针](../../knowledge/linux/error_handling/error_pointer/大纲.md#1.1_四次认识变化) · [固定版本源码](../../research/source_reading/error_pointer/navigation/P01_Linux_6.12_错误指针源码阅读索引.md#1.2_按问题进入源码) |

## 1.3\_内核子系统与驱动模型

| 领域 | 当前内容入口 |
| --- | --- |
| VFS | [VFS 子系统专题](../../knowledge/kernel_subsystems/vfs/大纲.md) · [源码阅读索引](../../research/source_reading/vfs/navigation/P01_Linux_6.12_VFS源码阅读索引.md#1.2_从状态变化选择阅读方向) |
| 日志与跟踪 | [Linux 内核日志](../../knowledge/kernel_subsystems/tracing/logging/Linux_内核日志.md) · [固定源码阅读索引](../../research/source_reading/printk/navigation/P01_Linux_6.12_日志路径源码阅读索引.md#1.2_L0到L3分别由谁推进) |
| 驱动基础 | [框架学习地图](../../knowledge/driver_model/fundamentals/framework_model/大纲.md#1.1_四个问题怎样接起来)、[只读属性实验](../../knowledge/driver_model/fundamentals/framework_model/kobject讲解.md#1.3_完整的只读属性模块)、[固定版本入口源码](../../research/source_reading/driver_entries/navigation/P01_Linux_6.12_驱动入口源码阅读索引.md#1.2_从现象进入文件) |
| pinctrl配置与状态 | [引脚配置](../../knowledge/driver_model/device_tree/P02_引脚配置怎样连接外设与焊盘.md#2.1_内部信号接对以后还需要电气条件)、[状态交接](../../knowledge/driver_model/device_tree/P03_引脚状态切换与GPIO占用交接.md#3.3_沿一次切换规定谁先停止谁后继续)、[诊断](../../knowledge/driver_model/device_tree/P04_沿状态与寄存器定位引脚问题.md#4.1_先定位配置在哪一步停止推进)、[固定版本源码索引](../../research/source_reading/pinctrl/navigation/P01_Linux_6.12_pinctrl源码阅读索引.md#1.1_版本边界与阅读路线) |
| 设备树与平台资源 | [连续学习路线](../../knowledge/driver_model/device_tree/大纲.md#1.1_沿描述怎样成为可用资源逐步阅读)、[离线 DTS 与 overlay 实验](../../knowledge/driver_model/device_tree/设备树+platform开发/P03_设备树语法与标准示例.md#3.6_编译后亲手检查修改是否进入二进制)、[平台总线入口](../../knowledge/driver_model/platform_bus/readme.md#1.1_从配对实验进入真实资源)、[组合外设应用](../../knowledge/driver_model/device_tree/P06_从供电复位到SDIO无线设备枚举.md#6.1_模块还没出现时谁来准备它的电源与复位)、[组合交接源码](../../research/source_reading/peripheral_integration/navigation/P01_组合外设源码阅读索引.md#1.1_版本与证据范围)、[绑定与别名](../../knowledge/driver_model/device_tree/P11_从设备描述到驱动服务怎样逐层确认.md#11.1_有描述不等于已经有可供匹配的设备)、[OF身份源码](../../research/source_reading/of_binding/navigation/P01_OF绑定与别名源码阅读索引.md#1.1_版本与阅读任务) |
| 字符设备 | [专题大纲](../../knowledge/driver_model/character_device/大纲.md)、[读写契约与复制状态模型](../../knowledge/driver_model/character_device/P05_文件操作契约与数据路径.md)、[有限窗口模板](../../knowledge/driver_model/character_device/P10_字符设备驱动模板.md)、[环形流与等待模板](../../knowledge/driver_model/character_device/P13_流式字符设备与等待通知模板.md)、[实验材料](../../labs/kernel/character_device/materials/README.md)、[固定版本 I/O 源码入口](../../research/source_reading/character_device/navigation/P01_Linux_6.12_字符设备源码阅读索引.md#1.2_由问题进入模块导读) |
| GPIO | [GPIO 专题](../../knowledge/driver_model/gpio/大纲.md)、[标准 GPIO Consumer](../../knowledge/driver_model/gpio_consumers/大纲.md)、[gpiolib源码索引](../../research/source_reading/gpiolib/navigation/P01_Linux_6.12_GPIO源码阅读索引.md#1.1_版本身份与阅读路线) |
| Input | [专题大纲](../../knowledge/driver_model/input/大纲.md) · [源码阅读索引](../../research/source_reading/input/navigation/P01_Linux_6.12_Input源码阅读索引.md#1.2_按当前问题进入模块与实现) |
| misc 设备 | [完整问候服务与文件寿命](../../knowledge/driver_model/misc/readme.md#1.3_完整程序与返回契约)、[实验材料](../../labs/kernel/driver_entries/materials/README.md) |
| 文件操作 | [打开、迭代与映射教材](../../knowledge/driver_model/file_operations/大纲.md#1.1_沿对象寿命逐步增加约束)、[版本成员参考](../../knowledge/driver_model/fundamentals/kernel_driver_mechanisms/data_strcuture_说明/struct_file_operations.md#1.2_按需求查询当前成员)、[完整实验材料](../../labs/kernel/file_operations/materials/README.md)、[源码导读](../../research/source_reading/character_device/navigation/P03_文件操作与打开寿命导读.md) |

## 1.4\_系统软件

| 领域 | 当前内容入口 |
| --- | --- |
| 用户态设备策略 | [事件、规则与访问结果](../../knowledge/system_software/P01_设备事件与用户态策略.md#1.1_定义与分层位置) |
| Buildroot | [学习地图](../../knowledge/system_software/buildroot/大纲.md)、[固定资料](../../research/source_reading/buildroot/SOURCE_BASELINE.md) |
| Kconfig | [基础语法](../../knowledge/system_software/kconfig/基础语法.md) · [固定工具阅读索引](../../research/source_reading/kconfig/navigation/P01_Linux_6.12_配置求值工具阅读索引.md#1.2_从定义输入到输出文件的阅读顺序) |
| 链接脚本 | [链接脚本与运行布局](../../knowledge/system_software/linker/P01_从目标文件到运行布局的链接脚本.md) |
| U-Boot | [连续学习大纲](../../knowledge/system_software/uboot/大纲.md)：布局、构建、早期环境、SPL、异常入口、搬迁、命令与内核交接；[固定源码索引](../../research/source_reading/uboot/navigation/README.md)按阶段进入实现 |

## 1.5\_平台\_实验\_研究与参考

| 类型 | 当前内容入口 |
| --- | --- |
| 内核模块工程 | [构建与部署专题](../../engineering/build/kernel_modules/大纲.md#1.1_四章怎样连起来)：目标身份、多文件组织、Kconfig 集成、部署与符号排错 |
| i.MX6ULL 平台 | [U-Boot 与内核移植](../../platforms/arm/nxp/imx6ull/porting/大纲.md) |
| i.MX8 ARM64平台 | [imx_v8配置与历史5.4.70构建](../../platforms/arm/nxp/imx8/P01_imx_v8配置与ARM64内核构建.md) |
| RK3566 平台 | [旧版Linux SDK构建与排障](../../platforms/arm/rockchip/rk3566/environment/大纲.md) |
| RK3588 与 Arm 中断控制器 | [GICv3 物理中断专题](../../platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图)、[RK3588 集成与手册核对](../../platforms/arm/rockchip/rk3588/P01_RK3588的GIC-600集成与手册核对.md#1.2_怎样确认控制器身份)；十六篇主线正文与[第十七章 UART 综合复盘](../../platforms/arm/architecture/gic/P17_UART中断的端到端流程复盘.md#17.1_阶段导航)已入库，配有章内练习与主机模型；源码与板上实验仍需固定目标环境 |
| 内存顺序实验 | [访问宽度与 ARM 反汇编](../../labs/foundations/computer_architecture/memory_ordering/P01_访问宽度_对齐与ARM反汇编/README.md)、[READ_ONCE 编译器访问](../../labs/kernel/memory_ordering/P01_READ_ONCE_编译器访问实验/README.md)、[LKMM Litmus](../../labs/kernel/memory_ordering/P02_LKMM_Litmus_消息传递与屏障/README.md) |
| C 语言静态分析实验 | [Sparse 地址空间与上下文记账](../../labs/foundations/c_language/P01_Sparse地址空间与上下文记账/README.md#1.1_实验目标) |
| Lockdep 实验 | [锁顺序反转与报告解读](../../labs/kernel/lockdep/P01_锁顺序反转与报告解读/README.md) |
| 链表实验与源码 | [完整模型与模块材料](../../labs/kernel/linked_list/materials/README.md)、[固定版本总索引](../../research/source_reading/linked_list/navigation/P01_Linux_6.12_链表源码阅读索引.md#1.2_由结论进入唯一实现)、[拓扑与发布模块导读](../../research/source_reading/linked_list/navigation/P02_拓扑修改与发布边界导读.md#2.1_先找到连接状态的地址) |
| 哈希计算与节点源码 | [C 模型与 RCU 模块](../../labs/kernel/hash_table/materials/README.md)、[固定版本索引](../../research/source_reading/hash_table/navigation/P01_Linux_6.12_哈希计算源码阅读索引.md#1.2_从问题选择入口)、[键位宽与落桶导读](../../research/source_reading/hash_table/navigation/P02_键位宽与落桶导读.md#2.1_从调用表达式追到计算路径)、[节点连接与并发边界](../../research/source_reading/hash_table/navigation/P03_节点连接与并发边界导读.md#3.1_节点与桶数组分别负责什么)、[动态表迁移](../../research/source_reading/hash_table/navigation/P04_动态表迁移与接口边界导读.md#4.2_沿R0到R5追踪一次迁移)、[子系统身份与寿命](../../research/source_reading/hash_table/navigation/P05_子系统索引身份与寿命导读.md#5.1_先区分索引任务与业务结论) |
| i.MX6ULL 实验 | [驱动实验目录](../../labs/platforms/nxp/imx6ull/drivers) |
| rbtree 查找源码 | [固定版本索引](../../research/source_reading/rbtree/navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)、[路径与返回边界导读](../../research/source_reading/rbtree/navigation/P02_查找路径与返回边界导读.md#2.2_按一次查找定位源码)、[rbtree.h 唯一实现](../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.1_rb_find的任意匹配)、[插入修复导读](../../research/source_reading/rbtree/navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)、[五组插入模块](../../knowledge/linux/data_structures/红黑树_rb-tree/P26_Linux红叶接入与插入修复.md#26.3.15_在内核模块中观察五组插入) |
| Maple 范围查询 | [G/H 区间实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P14_从地址查询到VMA范围修改.md#14.8_运行G与H的区间模型)、[源码阅读路线](../../research/source_reading/maple_tree/大纲.md#1.1_从查询差异进入源码)、[固定版本总索引](../../research/source_reading/maple_tree/navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据) |
| 调查 | [investigations](../../research/investigations/README.md) |
| 源码阅读 | [Linux 源码阅读基线](../../research/source_reading/linux/SOURCE_BASELINE.md#1.1_当前来源)、[Linux 6.12 编译器与 Sparse 注解导读](../../research/source_reading/compiler_annotations/navigation/P01_Linux_6.12_编译器与Sparse注解源码导读.md#1.1_基线与阅读任务)、[Linux 6.12 LKMM 导读](../../research/source_reading/memory_ordering/navigation/P01_Linux_6.12_LKMM_源码与模型导读.md)、[Linux 6.12 RCU 总阅读索引](../../research/source_reading/rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.6_建议的源码阅读顺序)、[Linux 6.12 Lockdep 总阅读索引](../../research/source_reading/lockdep/navigation/P01_Linux_6.12_Lockdep源码导读.md#1.1_基线与阅读目标) |
| 参考资料总入口 | [参考资料导航](../../reference/README.md#1.1_定位与使用边界) |
| 标准 | [GPL 协议说明](../../reference/standards/gpl/GPL协议说明.md) |
| 外部资料 | [Arm、RK3588 与 i.MX6ULL 外部资料索引](../../reference/external_resources/arm/README.md#1.1_索引定位与存储边界) |

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
- [GIC 专题建设与阅读路线](../roadmaps/gic_learning_plan.md#1.2_从零开始的阅读地图)
- [仓库信息架构设计](../../governance/architecture/repository_information_architecture.md)
- [Git 协作与提交规范](../../governance/conventions/git_guide.md)
- [全量目录重构记录](../../governance/migration/P01_全量目录重构记录.md)
- [并发与竞争专题迁移记录](../../governance/migration/P02_并发与竞争专题迁移地图.md)
