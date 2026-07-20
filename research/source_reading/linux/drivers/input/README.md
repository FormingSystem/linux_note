---
id: research.source_reading.linux.input_6_12_20
title: "Linux 6.12.20 Input 子系统源码导读"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - driver
  - source_reading
---

# 第1章\_Linux\_6.12.20\_Input\_子系统源码导读

## 1.1\_版本和范围

本目录中的文件来自仓库源码基线记录的 Linux 6.12.20 厂商树，保持上游相对路径。这里保存的是知识正文实际引用的公共 Input 实现，不包含具体触摸控制器源码，也不把厂商板级行为写成通用契约。

## 1.2\_证据地图

| 文件 | 核对内容 |
| --- | --- |
| [`drivers/input/input.c`](input.c) | 设备/handler 注册、匹配、事件过滤与分发、能力和 devres 接口 |
| [`drivers/input/evdev.c`](evdev.c) | evdev 连接、每客户端缓冲、读取、ioctl 和 `SYN_DROPPED` |
| [`drivers/input/input-mt.c`](input-mt.c) | MT slot 初始化、slot 状态、点匹配和帧同步 |
| [`include/linux/input.h`](../../include/linux/input.h) | `input_dev`、`input_handler`、`input_handle` 和内核接口 |
| [`include/linux/input/mt.h`](../../include/linux/input/mt.h) | MT 状态结构、标志和辅助接口 |
| [`include/uapi/linux/input.h`](../../include/uapi/linux/input.h) | `input_event`、`input_absinfo` 和 evdev ioctl ABI |
| [`include/uapi/linux/input-event-codes.h`](../../include/uapi/linux/input-event-codes.h) | 事件类型、code 和属性的 UAPI 编号 |
| [`Documentation/input/input-programming.rst`](../../Documentation/input/input-programming.rst) | 上游驱动编程说明 |
| [`Documentation/input/multi-touch-protocol.rst`](../../Documentation/input/multi-touch-protocol.rst) | Protocol A/B、slot 和 tracking ID 契约 |

## 1.3\_阅读顺序

先从 `include/linux/input.h` 识别三类核心对象，再在 `input.c` 追踪 `input_register_device()` 和 `input_event()`。随后阅读 `evdev.c`，观察一个 `input_handle` 如何连接设备、handler 和客户端队列。多点设备再进入 `input-mt.c`，把 slot 的存储状态与协议文档对照。

源码只证明 6.12.20 的具体实现。知识正文若只需要稳定契约，应引用 UAPI 或上游文档；涉及锁、字段、过滤算法和缓冲行为时才引用 `.c` 文件，并注明版本。

## 1.4\_关键调用链索引

| 主题 | 入口与方向 |
| --- | --- |
| 设备上线 | `input_register_device()` → 遍历 handler → `input_attach_handler()` → handler `.connect()` |
| handler 上线 | `input_register_handler()` → 遍历 device → `input_attach_handler()` |
| 事件提交 | `input_event()` → `input_handle_event()` → `input_get_disposition()` → `input_event_dispose()` → `input_pass_values()` |
| evdev 缓冲 | evdev handle 的批量事件回调 → `evdev_pass_values()` → `__pass_event()` → client ring buffer |
| 设备打开 | `evdev_open()` → `evdev_open_device()` → `input_open_device()` → 首用户触发 `input_dev->open()` |
| 独占 | `EVIOCGRAB` → `evdev_grab()` → `input_grab_device()` → `input_dev->grab` |
| 缓冲溢出 | `__pass_event()` 检测 head 追上 tail → 保留 `SYN_DROPPED` 和最新事件 |
| MT 帧结束 | `input_mt_sync_frame()` → 可选 `input_mt_drop_unused()` → pointer emulation → frame++ |
| 注销 | `input_unregister_device()` → `input_disconnect_device()` → 各 handler `.disconnect()` |

## 1.5\_版本相关观察

- core 在 `input_dev->vals` 中按帧积累要交给 handler 的值，`SYN_REPORT` 触发批量传递。
- `input_defuzz_abs_event()` 使用多段区间调整新值，不是单一阈值判断。
- managed input device 的撤销分两步：先注销，devres 栈继续展开后再释放分配引用。
- evdev 的溢出处理是客户端级别的；每个打开文件拥有自己的缓冲和读取位置。

这些是 Linux 6.12.20 具体实现。未来版本若改变字段或内部批处理方式，应更新本导读；UAPI 可观察行为则以 `include/uapi` 和正式文档为准。
