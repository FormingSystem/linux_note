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

本目录保存教材中完整展示的模块、观察程序及整数模型。实验步骤、预测、回滚说明及解答以正文为准。

## 1.1\_按正文取用

| 材料 | 使用位置 |
| --- | --- |
| [note_sysfs.c](note_sysfs.c) | [只读属性](../../../../knowledge/driver_model/fundamentals/framework_model/kobject讲解.md#1.3_完整的只读属性模块) |
| [note_misc.c](note_misc.c) | [misc 问候服务](../../../../knowledge/driver_model/misc/readme.md#1.3_完整程序与返回契约) |
| [sysfs_view.cpp](sysfs_view.cpp) | [只读设备观察](../../../../knowledge/driver_model/fundamentals/framework_model/P01_驱动框架模型.md#1.4_在自己的机器上寻找对象)：C++17 路径与目录标准库 |
| [misc_probe.c](misc_probe.c)、[hold_open.c](hold_open.c) | [misc 位置与模块引用观察](../../../../knowledge/driver_model/misc/readme.md#1.5_按位置观察内容) |
| [control_probe.c](control_probe.c) | [属性开关与保留位置](../../../../knowledge/linux/device_model/class_sysfs/P02_让属性控制字符读取.md#2.4_一次保留文件位置的观察)；退出时尝试恢复 enabled=1 |
| [cell_decode.c](cell_decode.c) | [设备树 cell 观察](../../../../knowledge/linux/device_model/class_sysfs/P05_系统属性与接口选择参考.md#5.2_设备树输入不等于运行时控制值)：C11 大端组装，不读取真实硬件 |
| [Makefile](Makefile) | 以 Kbuild 同时生成两个独立模块；执行命令见两篇正文 |

材料接口以 NXP 官方 Linux 6.12.20 固定提交为准，身份见[源码基线](../../../../research/source_reading/linux/SOURCE_BASELINE.md)。目标须具有匹配构建树、MODULES 与 SYSFS 支持；节点自动出现还取决于 devtmpfs 和目标节点管理。没有进行硬件访问；不要把宿主模型或 ARM 语法检查称为目标装载成功。

[跨层访问案例](../../../../knowledge/linux/object_lifetime/devres/P03_驱动资源与用户态访问的协作.md#3.16_用实际读取补齐策略检查)复用note_misc与misc_probe，分别检查接口发布、实际节点权限和读取位置；没有为了规则演示复制或改写驱动。本次只复核材料及既有契约，未新增目标加载、规则应用或卸载结论。
