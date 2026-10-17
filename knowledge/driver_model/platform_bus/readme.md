---
id: knowledge.driver_model.platform_bus.readme
title: "平台总线阅读入口"
kind: reference
status: evolving
domains:
  - linux
  - kernel
  - driver
---

# 第1章\_平台总线阅读入口

平台总线把不能由通用硬件枚举协议完整发现的设备实例与对应驱动接起来。设备实例保存资源和固件关联，驱动保存匹配表与 probe/remove 路径；总线执行匹配和生命周期交接。它不要求设备必须来自设备树，也不表示所有 DT 节点都属于平台总线。

## 1.1\_从配对实验进入真实资源

| 阅读任务 | 正文入口 |
| --- | --- |
| 分清 platform_device、platform_driver 与资源登记 | [板级资源与无硬件配对实验](../device_tree/设备树+platform开发/P01_旧式平台设备与资源机制.md) |
| 追踪 DTB 怎样进入设备模型 | [节点枚举与匹配](../device_tree/设备树+platform开发/P02_设备树驱动开发方式.md#2.2_DTB中的节点什么时候才成为Linux设备) |
| 区分匹配成功与自动装入模块 | [模块别名与用户空间加载链](../device_tree/设备树+platform开发/P02_设备树驱动开发方式.md#2.4_匹配成功与模块自动装入之间还有一条通信链) |
| 取得 MMIO、IRQ、GPIO、供电和时钟 | [设备树与平台资源路线](../device_tree/大纲.md#1.1_沿描述怎样成为可用资源逐步阅读) |
| 接入用户文件接口并处理解绑 | [字符设备与设备模型交接](../character_device/P08_设备模型与硬件子系统交接.md#8.1_字符设备不是设备发现机制) |

这里保留平台总线的定位与跳转，完整入门正文由设备树目录中的前两章承担，避免维护两套相互漂移的设备/驱动配对教程。I2C、SPI 子设备通常由各自总线枚举，不能仅凭节点出现在 DTS 中，就给它套一个 platform_driver。
