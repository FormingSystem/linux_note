---
id: labs.kernel.file_operations.materials
title: "文件操作实验材料"
kind: lab
status: evolving
domains:
  - linux
  - driver
---

# 第1章\_文件操作实验材料

本目录配套[文件操作教材](../../../../knowledge/driver_model/file_operations/大纲.md)。三个模块分别验证打开上下文、请求位置和映射引用，不是一个不断叠加功能的设备模板。

阅读正文后，先写下本轮希望区分的两种结果，再选择对应模块。例如，关闭 dup 的一个入口后，另一入口的关闭计数应改变，独立打开的计数应保持不变；映射程序则故意保留映射而关闭数字描述符。构建成功只说明已经产生供目标使用的文件，不能代替这些观察。每次只装载要验证的模块，记录操作、返回值和预测是否一致，再清理实验；不要把三个模块一次装入后只看有没有节点出现。

## 1.1\_材料对应关系

| 文件 | 正文与用途 |
| --- | --- |
| [note_session.c](note_session.c) | [第一章](../../../../knowledge/driver_model/file_operations/P01_一次打开与最后一次释放.md)：dup、flush、release 与 fdinfo |
| [note_iter.c](note_iter.c) | [第二章](../../../../knowledge/driver_model/file_operations/P02_迭代读取与请求位置.md)：readv、pread 与实际复制进度 |
| [note_mapping.c](note_mapping.c)、[mapping_probe.c](mapping_probe.c) | [第三章](../../../../knowledge/driver_model/file_operations/P03_只读映射与后备页寿命.md)：专用只读页与关闭 fd 后的映射 |
| [Makefile](Makefile) | 用匹配目标的 Kbuild 构建三个独立模块，用户程序另用用户空间编译器 |

接口按 NXP 官方 Linux 6.12.20 固定提交核对，目标要求 MODULES，fdinfo 要求 PROC_FS，映射实验要求 MMU。设置 KDIR、装卸及失败恢复均见正文；ARM 语法和宿主替身不等于目标 Kbuild 或实际装载。
