---
id: labs.kernel.driver_entries.materials
title: "驱动入口实验材料"
kind: lab
status: evolving
domains:
  - linux
  - driver
---

# 第1章\_驱动入口实验材料

本目录保存教材中完整展示的两个模块和一个只读脚本。实验步骤、预测、回滚说明及解答以正文为准。

## 1.1\_按正文取用

| 材料 | 使用位置 |
| --- | --- |
| [note_sysfs.c](note_sysfs.c) | [只读属性](../../../../knowledge/driver_model/fundamentals/framework_model/kobject讲解.md#1.3_完整的只读属性模块) |
| [note_misc.c](note_misc.c) | [misc 问候服务](../../../../knowledge/driver_model/misc/readme.md#1.3_完整程序与返回契约) |
| [sysfs_view.py](sysfs_view.py) | [只读设备观察](../../../../knowledge/driver_model/fundamentals/framework_model/P01_驱动框架模型.md#1.4_在自己的机器上寻找对象) |
| [Makefile](Makefile) | 以 Kbuild 同时生成两个独立模块；执行命令见两篇正文 |

材料接口以 NXP 官方 Linux 6.12.20 固定提交为准，身份见[源码基线](../../../../research/source_reading/linux/SOURCE_BASELINE.md)。目标须具有匹配构建树、MODULES 与 SYSFS 支持；节点自动出现还取决于 devtmpfs 和目标节点管理。没有进行硬件访问；不要把宿主模型或 ARM 语法检查称为目标装载成功。
