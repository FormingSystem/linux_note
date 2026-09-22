---
id: labs.kernel.error_pointer.materials
title: "错误指针教材实验材料"
kind: lab
status: evolving
domains:
  - linux
  - kernel
---

# 第1章\_错误指针教材实验材料

操作、预期和解释保留在对应正文，这里只保存可取用的完整材料。

| 文件 | 用途与正文 |
| --- | --- |
| [lookup_result.c](lookup_result.c) | [第一章](../../../../knowledge/linux/error_handling/error_pointer/错误指针机制简介.md)：普通 C 的状态与输出参数实验 |
| [encoding_model.c](encoding_model.c) | [第二章](../../../../knowledge/linux/error_handling/error_pointer/P02_错误值的编码与判定.md)：C11 固定宽度整数模型，不创建指针 |
| [error_pointer_demo.c](error_pointer_demo.c)、[Makefile](Makefile) | [第四章](../../../../knowledge/linux/error_handling/error_pointer/P04_观察返回值与释放顺序.md)：无硬件依赖的平台设备实验模块 |

整数模型使用 C11，保留 assert 自检，不定义 NDEBUG。模块接口以 NXP 官方 Linux 6.12.20 固定提交为证据，运行必须使用与目标内核匹配的构建产物。静态语法和宿主模型验证不等于目标装载、探测与释放已经实测。
