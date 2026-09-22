---
id: labs.kernel.character_device.materials
title: "字符设备教材实验材料"
kind: lab
status: evolving
domains:
  - linux
  - kernel
  - driver
---

# 第1章\_字符设备教材实验材料

这里保存字符设备教材的完整源文件。操作步骤、预期结果和解释保留在正文，材料目录不维护第二套实验手册。

| 文件 | 用途与正文归属 |
| --- | --- |
| [decode_minor.c](decode_minor.c) | 普通 C 的连续/稀疏次号解码模型，不参与 Kbuild；[多实例设备的身份与组织](../../../../knowledge/linux/architecture/modules_and_device_nodes/Linux_内核模块与设备节点操作基础.md) |
| [inspect_null.c](inspect_null.c) | [第二章](../../../../knowledge/driver_model/character_device/P02_设备号_注册与设备节点.md#2.7_做一次只读的身份观察)：Linux 用户空间的 stat/fstat 与只读身份观察 |
| [note_registration.h](note_registration.h) | 整组设备注册、失败回滚和卸载；[第十章](../../../../knowledge/driver_model/character_device/P10_字符设备驱动模板.md) |
| [note_window.c](note_window.c) | 有限内存窗口，位置、覆盖、追加及部分复制；第十章 |
| [Makefile](Makefile) | 两个独立模块的 Kbuild 目标；[第十一章](../../../../knowledge/driver_model/character_device/P11_构建运行与验证.md) |
| [window_probe.c](window_probe.c) | 检查有限窗口的字节与位置；第十一章 |
| [note_stream.c](note_stream.c) | 环形流、阻塞与非阻塞、poll；[第十三章](../../../../knowledge/driver_model/character_device/P13_流式字符设备与等待通知模板.md) |
| [stream_probe.c](stream_probe.c) | 第十三章的八字节空队列观察；检查 EAGAIN、回绕短传输与 poll，失败后不自动重放 |

模块接口针对 NXP 官方 Linux 6.12.20 固定提交；目标配置与构建输出必须匹配运行内核。源码语法检查不等于内核链接、装载或目标实验已经执行。请从对应正文开始，按其条件恢复现场。
