---
id: labs.kernel.class_sysfs.materials
title: "class 与 sysfs 实验材料"
kind: lab
status: evolving
domains: [linux, kernel, driver]
---

# 第1章\_class与sysfs实验材料

[教材](../../../../knowledge/linux/device_model/class_sysfs/大纲.md)已包含完整程序、构建前提、预测、解释和练习；本目录提供可直接取用的同一份代码。不要把两份源码拼成一个模块。

| 材料 | 对应观察 |
| --- | --- |
| [note_class.c](note_class.c) | 两个零 devt 对象，属性共享回调、实例数据分离、按指针回滚 |
| [note_control.c](note_control.c) | 文本校验后提交，属性与字符读取共用锁，禁用不改变文件位置 |
| [control_probe.c](control_probe.c) | 第二章的用户程序：建立启用初值、验证禁用错误与位置保留、恢复为启用 |
| [cell_decode.c](cell_decode.c) | 第五章的用户程序：按字节解释大端 cell，不访问真实设备 |
| [Makefile](Makefile) | 外部模块构建目标，使用正文传入的 KDIR |

目标需要匹配的 NXP Linux 6.12.20 构建与配置，启用模块、SYSFS、字符设备相关能力；自动节点观察还依赖 DEVTMPFS 与正确挂载。工具语法和模块契约以[固定源码基线](../../../../research/source_reading/linux/SOURCE_BASELINE.md)为界。教学设备没有真实硬件、后台工作或热拔插，用它不能证明生产驱动的断电与移除协议。

Makefile 只构建两个内核模块；两个用户程序按对应正文中的 C11 命令单独编译。control_probe 需要在目标 Linux 运行，并要求没有其他程序同时修改 enabled；cell_decode 只使用标准 C，可以在宿主机核对编码结果。不要把用户程序加入 obj-m，也不要把用户程序编译成功当作内核模块已通过 Kbuild。

本批 ARM 语法与宿主分支检查已完成，范围见[工作记录](../../../../governance/migration/repository_textbook_refactor.md#1.4.8_B02h分类对象与属性事务)；目标 Kbuild、MODPOST、装卸、并发、活动排空和权限策略的真实运行仍待执行。运行正文前保存工作，实验结束关闭文件并正常卸载，不使用强制卸载。
