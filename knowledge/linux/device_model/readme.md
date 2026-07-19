---
id: knowledge.linux.device_model.readme
title: "readme"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
---

好的，按你给的34章内容，**分为 6 个部分**，不使用表格，只用数字分段：

------

### 1.1.1_基础与对象模型(第1–4章)

1. 第1章　Linux 设备模型系统（Device Model System）
2. 第2章　内核对象（kobject/kset）机制
3. 第3章　bus_type 总线机制与匹配流程详解
4. 第4章　device 与 device_driver 的匹配与绑定过程

### 1.1.2_设备树与_Platform_框架(第5–10章)

1. 第5章　of_device_id 与 of_match_table 匹配机制详解
2. 第6章　platform_device 与 platform_driver 框架
3. 第7章　class 与设备分类机制
4. 第8章　device_attribute 与驱动属性文件机制
5. 第9章　devres 资源管理机制（devm 系列）
6. 第10章　driver core 核心管理机制

### 1.1.3_电源_sysfs_与层级事件(第11–14章)

1. 第11章　电源管理（PM）在设备模型中的集成
2. 第12章　sysfs 与 kobject 内核对象模型
3. 第13章　uevent 与 udev：内核事件通知机制
4. 第14章　device 与 subsystem 层级管理机制

### 1.1.4_加载与匹配进阶(第15–19章)

1. 第15章　设备模型与模块加载：从 insmod 到 probe 的全流程
2. 第16章　driver core 设备匹配机制详解：from of_match_device() 到 of_match_node()
3. 第17章　设备树匹配的高级机制与多层继承（compatible 列表与 fallback 匹配）
4. 第18章　从 device_node 到 platform_device：设备树节点在设备模型中的注册过程
5. 第19章　platform_device 与 platform_driver 的绑定过程（device_attach 与 probe 调用）

### 1.1.5_生命周期_依赖与属性体系(第20–29章)

1. 第20章　设备模型中的引用计数与生命周期管理（get_device / put_device / kref 机制）
2. 第21章　设备模型中的资源管理与 devm_* 接口机制
3. 第22章　device_link 与电源依赖关系（Power Domain Dependencies）
4. 第23章　bus_type 与 class 的深入实现机制（从 bus_register 到 sysfs 呈现）
5. 第24章　driver core 与模块系统的交互机制（THIS_MODULE 与引用保护）
6. 第25章　热插拔与设备动态创建机制（Hotplug & Dynamic Device）
7. 第26章　device_create() 与 sysfs 属性自动生成机制
8. 第27章　sysfs 属性机制的底层原理与实现（kobj_attribute 与属性访问路径）
9. 第28章　class_attribute 与 driver_attribute 的实现与差异
10. 第29章　bus_attribute 与子系统属性机制（总线级属性注册）

### 1.1.6_高级协作与全景整合(第30–34章)

1. 第30章　设备模型与模块依赖：modalias 与 modprobe 自动加载机制
2. 第31章　驱动卸载与 remove() 调用链
3. 第32章　多总线协作与复合设备机制（Composite / MFD / Component 框架）
4. 第33章　ACPI/OF 抽象与 fwnode_handle 框架
5. 第34章　Driver Core 全景总结与架构整合

如果你要，我可以在每个部分后面补一行“这一部分的学习目标/产出物”，方便教学或写书导读。