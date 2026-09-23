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
| 内核架构 | [从读取文件认识内核](../../knowledge/linux/architecture/kernel_composition/linux内核概貌.md#1.1_先让程序读到几个字)、[从问题定位源码](../../knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md#1.1_先区分源码目录与正在运行的系统)、[模块与设备入口](../../knowledge/linux/architecture/modules_and_device_nodes/大纲.md#1.1_沿三个问题进入正文) |
| 数据结构 | [Linux 双向循环链表](../../knowledge/linux/data_structures/单链表_linked_list/大纲.md#1.1_从一组任务走到容器选择)、[哈希表专题](../../knowledge/linux/data_structures/哈希表_Hash_Table/大纲.md#1.1_沿问题增加约束)、[树结构路线](../../knowledge/linux/data_structures/红黑树_rb-tree/大纲.md#1.1_沿问题进入现有章节)、[普通树表示实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P16_普通树的表示与构建实验.md#16.7_运行预测与资源回收)、[二叉树查询反例](../../knowledge/linux/data_structures/红黑树_rb-tree/P21_递归状态与二叉树基本查询.md#21.8_让查询接受一个反例)、[BST 取值边界验证](../../knowledge/linux/data_structures/红黑树_rb-tree/P23_BST验证与高度边界.md#23.6_运行全子树边界反例)、[退化访问计数](../../knowledge/linux/data_structures/红黑树_rb-tree/P04_为什么_BST_会退化.md#4.2.4_用节点访问次数观察退化)、[左右旋与根引用](../../knowledge/linux/data_structures/红黑树_rb-tree/P05_旋转的作用与局部重排.md#5.4.11_用根引用运行一对互逆动作)、[组合与失衡反例](../../knowledge/linux/data_structures/红黑树_rb-tree/P24_组合旋转与形状判断.md#24.11_内部子树上的_LR_重排反例)、[AVL 高度传播](../../knowledge/linux/data_structures/红黑树_rb-tree/P25_AVL高度诊断与更新传播.md#25.23_运行完整高度维护程序)、[重复键与旧路径](../../knowledge/linux/data_structures/红黑树_rb-tree/P10_Linux_6.12_内核_rbtree_查找与返回边界.md#10.2.10_用完整C程序观察相等节点和旧路径)、[取消请求与缺黑](../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.3.12_用完整模块观察取消请求)、[删除源码模块](../../research/source_reading/rbtree/navigation/P04_对象摘除与缺黑修复导读.md#4.2_从对象到缺黑父槽)、[遍历与整树销毁](../../knowledge/linux/data_structures/红黑树_rb-tree/P27_Linux有序遍历与整树销毁.md#27.2.9_运行完整遍历与销毁模块)、[同键替换与旧对象](../../knowledge/linux/data_structures/红黑树_rb-tree/P28_Linux同键替换与旧对象退出.md#28.2.7_运行同键替换观察模块)、[旋转完成边界](../../knowledge/linux/data_structures/红黑树_rb-tree/P29_普通旋转与Linux修复的完成边界.md#29.1_为什么没有一一对应的旋转调用)、[多路节点查找](../../knowledge/linux/data_structures/红黑树_rb-tree/P06_2-3-4_树_从多路平衡到红黑树的结构桥梁.md#6.4.7_用完整C程序观察区间下行)、[多路插入与资源预留](../../knowledge/linux/data_structures/红黑树_rb-tree/P30_2-3-4树插入与分裂时机.md#30.3_运行完整的两种插入)、[多路预修复删除](../../knowledge/linux/data_structures/红黑树_rb-tree/P31_2-3-4树预修复删除.md#31.3_运行完整预修复删除)、[下溢回溯与根收缩](../../knowledge/linux/data_structures/红黑树_rb-tree/P32_2-3-4树下溢回溯与根收缩.md#32.4_运行完整回溯删除)、[多路与红黑缺口](../../knowledge/linux/data_structures/红黑树_rb-tree/P33_从多路删除到红黑缺口.md#33.3_用黑高收支检查父层是否仍有缺口)、[页请求与驻留模型](../../knowledge/linux/data_structures/红黑树_rb-tree/P34_从多路节点到页级索引.md#34.3_运行页请求与未命中的计数模型)、[红黑性质与高度检查](../../knowledge/linux/data_structures/红黑树_rb-tree/P07_红黑树_把_2-3-4_树映射成二叉表示.md#7.3.9_让程序区分三种非法结构)、[红红冲突与完整插入](../../knowledge/linux/data_structures/红黑树_rb-tree/P35_红黑插入与红红冲突上推.md#35.3_运行完整插入程序)、[缺黑位置与完整删除](../../knowledge/linux/data_structures/红黑树_rb-tree/P36_红黑删除与缺黑位置传播.md#36.3_用完整程序删除图中的对象)、[多路分组与区间折叠](../../knowledge/linux/data_structures/红黑树_rb-tree/P07_红黑树_把_2-3-4_树映射成二叉表示.md#7.5.12_保持键区间的折叠实验)、[任务排序契约](../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#8.2.7_把排序契约变成可观察结果)、[根值与对象寿命](../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#2%29_观察根值复制与对象存活)、[父色编码与布局](../../research/source_reading/rbtree/navigation/P07_节点布局与编码状态导读.md#7.1_先识别三个存储对象)、[嵌入成员与所有者](../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_两个嵌入成员还原同一个任务)、[比较政策与调用成本](../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_让同一个比较规则走两种调用路径)、[入口与对象寿命](../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_两个入口关闭之后谁还在使用对象)、[调用者完整框架](../../knowledge/linux/data_structures/红黑树_rb-tree/P37_构建rbtree调用者接口.md#37.16_运行完整的私有调用者框架)、[缓存入口一致性](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行缓存一致性实验)、[区间摘要维护](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行完整区间摘要实验)、[复制与回收](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_用两个线程观察复制值与删除)、[调用者示例回访](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#12.5_Linux_内核_rbtree_示例代码)、[有界快照验证](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行有界快照检查器)、[固定调用场景](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#12.8_Linux_rbtree_在内核中的典型使用场景)、[B/B+ 页布局](../../knowledge/linux/data_structures/红黑树_rb-tree/P13_再扩展到_B_树与_B+_树.md#%281%29_运行等值路由与叶分裂模型)、[Maple 树根与模式](../../knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.3_struct_maple_tree_树对象本身)、[Maple 节点与空洞](../../knowledge/linux/data_structures/红黑树_rb-tree/P38_Maple节点中的范围与空洞.md#38.5_运行包含空槽的分区程序)、[Maple 字段编码](../../knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.5.4_用定宽整数观察错误掩码)、[Maple 游标周期](../../knowledge/linux/data_structures/红黑树_rb-tree/P39_Maple操作游标的暂停与继续.md#39.3_沿S0到S6比较暂停与重置)、[Maple 普通接口](../../knowledge/linux/data_structures/红黑树_rb-tree/P40_Maple普通接口中的范围与查询.md#40.2_同一棵树中的覆盖与拒绝覆盖)、[Maple 写入准备](../../knowledge/linux/data_structures/红黑树_rb-tree/P41_Maple写入准备与锁边界.md#41.3_沿S0到S5区分位置与资源)、[VMA 边界实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.11.4_运行边界等价性实验) |
| 同步和异步机制 | [总纲](../../knowledge/linux/synchronization_and_asynchrony/大纲.md)、[同步机制](../../knowledge/linux/synchronization_and_asynchrony/synchronization/大纲.md)、[异步机制](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/大纲.md)、[锁](../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/大纲.md)、[序列计数器](../../knowledge/linux/synchronization_and_asynchrony/synchronization/sequence_counters/大纲.md)、[等待与完成量](../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/大纲.md)、[RCU](../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/大纲.md)、[Lockdep](../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/大纲.md)、[工作队列](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/workqueue/大纲.md) |
| 对象生命周期 | [kref](../../knowledge/linux/object_lifetime/kref)、[devres](../../knowledge/linux/object_lifetime/devres) |
| I/O 模型 | [阻塞 I/O](../../knowledge/linux/io_model/blocking_io)、[MMIO](../../knowledge/linux/io_model/mmio/大纲.md)、[DMA](../../knowledge/linux/io_model/dma/大纲.md) |
| 设备模型 | [设备模型专题](../../knowledge/linux/device_model/大纲.md) |
| class 与 sysfs | [分类、属性与节点教材](../../knowledge/linux/device_model/class_sysfs/大纲.md#1.1_从分类观察到可控数据入口)、[成员参考](../../knowledge/driver_model/fundamentals/kernel_driver_mechanisms/data_strcuture_说明/struct_class.md#1.2_按职责查询当前成员)、[实验材料](../../labs/kernel/class_sysfs/materials/README.md)、[源码导读](../../research/source_reading/driver_entries/navigation/P03_分类对象与属性事务导读.md) |
| 错误处理 | [错误指针](../../knowledge/linux/error_handling/error_pointer/大纲.md#1.1_四次认识变化) · [固定版本源码](../../research/source_reading/error_pointer/navigation/P01_Linux_6.12_错误指针源码阅读索引.md#1.2_按问题进入源码) |

## 1.3\_内核子系统与驱动模型

| 领域 | 当前内容入口 |
| --- | --- |
| VFS | [VFS 子系统专题](../../knowledge/kernel_subsystems/vfs/大纲.md) |
| 日志与跟踪 | [Linux 内核日志](../../knowledge/kernel_subsystems/tracing/logging/Linux_内核日志.md) |
| 驱动基础 | [框架学习地图](../../knowledge/driver_model/fundamentals/framework_model/大纲.md#1.1_四个问题怎样接起来)、[只读属性实验](../../knowledge/driver_model/fundamentals/framework_model/kobject讲解.md#1.3_完整的只读属性模块)、[固定版本入口源码](../../research/source_reading/driver_entries/navigation/P01_Linux_6.12_驱动入口源码阅读索引.md#1.2_从现象进入文件) |
| 字符设备 | [专题大纲](../../knowledge/driver_model/character_device/大纲.md)、[读写契约与复制状态模型](../../knowledge/driver_model/character_device/P05_文件操作契约与数据路径.md)、[有限窗口模板](../../knowledge/driver_model/character_device/P10_字符设备驱动模板.md)、[环形流与等待模板](../../knowledge/driver_model/character_device/P13_流式字符设备与等待通知模板.md)、[实验材料](../../labs/kernel/character_device/materials/README.md)、[固定版本 I/O 源码入口](../../research/source_reading/character_device/navigation/P01_Linux_6.12_字符设备源码阅读索引.md#1.2_由问题进入模块导读) |
| 设备树与 Platform | [device_tree](../../knowledge/driver_model/device_tree)、[platform_bus](../../knowledge/driver_model/platform_bus/readme.md) |
| GPIO | [GPIO 专题](../../knowledge/driver_model/gpio/大纲.md)、[标准 GPIO Consumer](../../knowledge/driver_model/gpio_consumers/大纲.md) |
| Input | [专题大纲](../../knowledge/driver_model/input/大纲.md) |
| misc 设备 | [完整问候服务与文件寿命](../../knowledge/driver_model/misc/readme.md#1.3_完整程序与返回契约)、[实验材料](../../labs/kernel/driver_entries/materials/README.md) |
| 文件操作 | [打开、迭代与映射教材](../../knowledge/driver_model/file_operations/大纲.md#1.1_沿对象寿命逐步增加约束)、[版本成员参考](../../knowledge/driver_model/fundamentals/kernel_driver_mechanisms/data_strcuture_说明/struct_file_operations.md#1.2_按需求查询当前成员)、[完整实验材料](../../labs/kernel/file_operations/materials/README.md)、[源码导读](../../research/source_reading/character_device/navigation/P03_文件操作与打开寿命导读.md) |

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
| 内核模块工程 | [构建与部署专题](../../engineering/build/kernel_modules/大纲.md#1.1_四章怎样连起来)：目标身份、多文件组织、Kconfig 集成、部署与符号排错 |
| i.MX6ULL 平台 | [U-Boot 与内核移植](../../platforms/arm/nxp/imx6ull/porting/imx6ull-移植u-boot-2025.04_and_kernel-6.1.md)、[内核配置编译](../../platforms/arm/nxp/imx6ull/porting/imx_v8_config_kernel编译说明.md) |
| RK3566 平台 | [Linux SDK 编译](../../platforms/arm/rockchip/rk3566/environment/linux_sdk编译说明.md) |
| RK3588 与 Arm 中断控制器 | [GICv3 物理中断专题](../../platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图)、[RK3588 集成与手册核对](../../platforms/arm/rockchip/rk3588/P01_RK3588的GIC-600集成与手册核对.md#1.2_怎样确认控制器身份)；十篇主线正文与[UART 流程复盘草稿](../../platforms/arm/architecture/gic/P11_UART中断的端到端流程复盘.md#11.1_阶段导航)已入库，源码与板上实验仍需固定目标环境 |
| 内存顺序实验 | [访问宽度与 ARM 反汇编](../../labs/foundations/computer_architecture/memory_ordering/P01_访问宽度_对齐与ARM反汇编/README.md)、[READ_ONCE 编译器访问](../../labs/kernel/memory_ordering/P01_READ_ONCE_编译器访问实验/README.md)、[LKMM Litmus](../../labs/kernel/memory_ordering/P02_LKMM_Litmus_消息传递与屏障/README.md) |
| C 语言静态分析实验 | [Sparse 地址空间与上下文记账](../../labs/foundations/c_language/P01_Sparse地址空间与上下文记账/README.md#1.1_实验目标) |
| Lockdep 实验 | [锁顺序反转与报告解读](../../labs/kernel/lockdep/P01_锁顺序反转与报告解读/README.md) |
| 链表实验与源码 | [完整模型与模块材料](../../labs/kernel/linked_list/materials/README.md)、[固定版本总索引](../../research/source_reading/linked_list/navigation/P01_Linux_6.12_链表源码阅读索引.md#1.2_由结论进入唯一实现)、[拓扑与发布模块导读](../../research/source_reading/linked_list/navigation/P02_拓扑修改与发布边界导读.md#2.1_先找到连接状态的地址) |
| 哈希计算与节点源码 | [C 模型与 RCU 模块](../../labs/kernel/hash_table/materials/README.md)、[固定版本索引](../../research/source_reading/hash_table/navigation/P01_Linux_6.12_哈希计算源码阅读索引.md#1.2_从问题选择入口)、[键位宽与落桶导读](../../research/source_reading/hash_table/navigation/P02_键位宽与落桶导读.md#2.1_从调用表达式追到计算路径)、[节点连接与并发边界](../../research/source_reading/hash_table/navigation/P03_节点连接与并发边界导读.md#3.1_节点与桶数组分别负责什么)、[动态表迁移](../../research/source_reading/hash_table/navigation/P04_动态表迁移与接口边界导读.md#4.2_沿R0到R5追踪一次迁移)、[子系统身份与寿命](../../research/source_reading/hash_table/navigation/P05_子系统索引身份与寿命导读.md#5.1_先区分索引任务与业务结论) |
| i.MX6ULL 实验 | [驱动实验目录](../../labs/platforms/nxp/imx6ull/drivers) |
| rbtree 查找源码 | [固定版本索引](../../research/source_reading/rbtree/navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)、[路径与返回边界导读](../../research/source_reading/rbtree/navigation/P02_查找路径与返回边界导读.md#2.2_按一次查找定位源码)、[rbtree.h 唯一实现](../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.1_rb_find的任意匹配)、[插入修复导读](../../research/source_reading/rbtree/navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)、[五组插入模块](../../knowledge/linux/data_structures/红黑树_rb-tree/P26_Linux红叶接入与插入修复.md#26.3.15_在内核模块中观察五组插入) |
| Maple 范围查询 | [G/H 区间实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P14_Maple_Tree_与_VMA_管理.md#14.9.3_运行G与H的区间模型)、[源码阅读路线](../../research/source_reading/maple_tree/大纲.md#1.1_从查询差异进入源码)、[固定版本总索引](../../research/source_reading/maple_tree/navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据) |
| 调查 | [investigations](../../research/investigations/README.md) |
| 源码阅读 | [Linux 源码阅读基线](../../research/source_reading/linux/SOURCE_BASELINE.md#1.1_当前来源)、[Linux 6.12 编译器与 Sparse 注解导读](../../research/source_reading/compiler_annotations/navigation/P01_Linux_6.12_编译器与Sparse注解源码导读.md#1.1_基线与阅读任务)、[Linux 6.12 LKMM 导读](../../research/source_reading/memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md)、[Linux 6.12 RCU 总阅读索引](../../research/source_reading/rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.6_建议的源码阅读顺序)、[Linux 6.12 Lockdep 总阅读索引](../../research/source_reading/lockdep/navigation/P01_Linux_6.12_Lockdep源码导读.md#1.1_基线与阅读目标) |
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
