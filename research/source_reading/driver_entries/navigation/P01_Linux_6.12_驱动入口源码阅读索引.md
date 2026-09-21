---
id: research.source_reading.driver_entries.index
title: "Linux 6.12 驱动入口源码阅读索引"
kind: source
status: evolving
domains:
  - linux
  - source_reading
---

# 第1章\_Linux\_6.12\_驱动入口源码阅读索引

教材已经建立了“目录可见”“属性可读”“字符入口可分派”和“对象仍存活”的区别。阅读源码时要继续保持这几个问题分开：公共框架省去的是哪些重复步骤，哪些寿命仍由调用方管理？

## 1.1\_版本与范围

官方来源为 `https://github.com/nxp-imx/linux-imx.git`，分支 `lf-6.12.y`，发布标签 `lf-6.12.20-2.0.0`，固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，Linux 6.12.20。以[源码基线](../../linux/SOURCE_BASELINE.md)定位；工作树的实验提交不作为证据。

本次观察环境启用 ARM、MODULES、SYSFS、DEVTMPFS；这些是本地配置，不是发布配置。kobject 的 SYSFS 配置桩、DEBUG_KOBJECT_RELEASE 延迟销毁以及节点挂载会改变观察条件。本文说明公共实现，不推断其他架构的硬件行为。

## 1.2\_从现象进入文件

| 问题 | 阅读入口 | 固定源码位置 |
| --- | --- | --- |
| 为什么一个创建引用足以管理动态对象 | [对象属性导读](P02_对象属性与字符入口导读.md#2.1_对象属性的两个寿命) · [创建实现](../source_explanations/P01_对象创建与misc分派.md#1.1_动态kobject的创建引用) | lib/kobject.c、include/linux/kobject.h |
| 为什么移除属性要等待活动调用 | [排空路径导读](P02_对象属性与字符入口导读.md#2.2_移除属性怎样等待读者) | fs/sysfs/file.c、fs/kernfs/dir.c |
| 谁维护实际次号和打开分派 | [字符入口导读](P02_对象属性与字符入口导读.md#2.3_misc的共享状态与分派) · [登记实现](../source_explanations/P01_对象创建与misc分派.md#1.2_misc登记与失败回滚) | drivers/char/misc.c、include/linux/miscdevice.h |
| 已打开文件为何不因注销而自动关闭 | [打开与注销实现](../source_explanations/P01_对象创建与misc分派.md#1.3_misc打开与注销的交接) | misc_open、misc_deregister、file 的操作表 |
| 有分类目录为何不必有节点 | [分类导读](P03_分类对象与属性事务导读.md#3.1_分类集合与设备创建) · [设备创建实现](../source_explanations/drivers/base/core.c.md#1.1_设备创建便利函数的所有权) | class.c、base.h、core.c、device/class.h |
| 谁建节点，谁消费事件 | [发布通信导读](P03_分类对象与属性事务导读.md#3.2_发布的两条通信路径) | core.c、devtmpfs.c、kobject_uevent.c |
| 属性写如何找到设备及业务状态 | [请求导读](P03_分类对象与属性事务导读.md#3.3_一次属性写入的状态落点) · [属性分派实现](../source_explanations/drivers/base/core.c.md#1.2_从通用属性回到设备回调) | kernfs/file.c、sysfs/file.c、core.c、kstrtox.c |

驱动绑定的 S0～S4 沿已有[返回值与清理路径导读](../../error_pointer/navigation/P02_返回值与清理路径导读.md)追踪 dd.c。平台身份比较另只读核对 drivers/base/platform.c 的 platform_match：覆盖名、固件匹配、ID 表和名字回退按实际分支选择；不是所有设备都由设备树扫描出来。本组不复制驱动核心的逐句实现。

返回[教材地图](../../../../knowledge/driver_model/fundamentals/framework_model/大纲.md)或[源码大纲](../大纲.md)。
