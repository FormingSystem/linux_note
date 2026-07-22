---
id: research.source_reading.linux.gpio.6_12_core_paths
title: "Linux 6.12 GPIO 核心路径源码证据"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - driver
topics:
  - gpio
  - gpiolib
source_project: linux
source_version: "6.12.20"
source_tree: "NXP linux-imx-6.12"
---

# 第1章\_Linux\_6.12\_GPIO\_核心路径源码证据

本文记录 [GPIO 专题](../../../../knowledge/driver_model/gpio/大纲.md) 所用的版本化事实。源码树顶层 `Makefile` 标识 Linux `6.12.20`，默认 `ARCH ?= arm`。这里只记录公共 gpiolib；NXP BSP 和 ARM 专属行为需另行核对。

## 1.1\_核心对象

| 对象 | Linux 6.12.20 路径 | 关键事实 |
| --- | --- | --- |
| `struct gpio_chip` | `include/linux/gpio/driver.h:417` | Provider 回调、`base`、`ngpio`、`can_sleep`、IRQ 集成 |
| `struct gpio_device` | `drivers/gpio/gpiolib.h:57` | `device/cdev/id/chip/descs`、SRCU、notifier、pin ranges |
| `struct gpio_desc` | `drivers/gpio/gpiolib.h:175` | `gdev`、flags、RCU label、line name、可选 hog |
| `struct gpio_irq_chip` | `include/linux/gpio/driver.h:50` | irqchip、domain、父 IRQ 与 handler 集成 |

`gpio_device.chip` 是 `struct gpio_chip __rcu *`。源码注释说明 `gpio_device` 可以在 chip 被移除后因用户空间引用继续存活；它不是 `gpio_chip` 的同义包装。

## 1.2\_描述符状态位

Linux 6.12.20 `drivers/gpio/gpiolib.h:175-205` 定义：

| flag | 语义 |
| --- | --- |
| `FLAG_REQUESTED` | line 已被请求 |
| `FLAG_IS_OUT` | 软件记录为输出 |
| `FLAG_ACTIVE_LOW` | 普通值接口需要逻辑反相 |
| `FLAG_OPEN_DRAIN/OPEN_SOURCE` | 电气驱动语义 |
| `FLAG_USED_AS_IRQ` | line 已连接 IRQ |
| `FLAG_IRQ_IS_ENABLED` | 对应 IRQ 已使能 |
| `FLAG_EDGE_RISING/FALLING` | cdev 边沿事件配置 |
| `FLAG_TRANSITORY` | line 可能在睡眠或复位时丢值 |

这些位是每线共享软件状态，不等同硬件寄存器。外部直接写寄存器不会自动同步它们。

## 1.3\_控制器注册

`drivers/gpio/gpiolib.c:920` 的 `gpiochip_add_data_with_key()`：

1. `kzalloc()` 分配 `gpio_device`；
2. 建立 device type、bus、parent；
3. 用 RCU 把 `gdev->chip` 指向 `gpio_chip`；
4. 设置 firmware node；
5. 分配 gpiochip id 并命名；
6. 按 `ngpio` 分配 `gdev->descs`；
7. 复制 `ngpio` 和 `can_sleep`；
8. 在全局锁下分配/检查整数 base 并加入控制器列表；
9. 逐项设置 `desc.gdev`；
10. 初始化 line/device notifier 和 SRCU。

同一函数的注释明确希望长期移除全局整数空间；`struct gpio_chip.base` 的注释把非负静态 base 标记为 deprecated。

## 1.4\_请求与释放

`drivers/gpio/gpiolib.c:2387` 的 `gpiod_request()`：

- 先验证描述符；
- 取得 Provider module 引用；
- 调用内部 request commit；
- 成功后增加 `gpio_device` 引用；
- Provider 不可用时初始错误语义为 `-EPROBE_DEFER`。

`gpiod_free_commit()` 在 Provider 仍存在且 `FLAG_REQUESTED` 已置位时调用可选 `gpio_chip.free()`，随后清除 active-low、requested、open-drain/source、bias、edge、hog 和 label，并发出 `GPIOLINE_CHANGED_RELEASED`。`gpiod_free()` 再释放 module 与 `gpio_device` 引用。

## 1.5\_方向和逻辑值

`gpiod_direction_input()` 位于 `gpiolib.c:2674`：调用 Provider `direction_input()` 或验证固定输入能力，成功后清除 `FLAG_IS_OUT`。

`gpiod_direction_output_raw()` 位于 `gpiolib.c:2782`，把物理值交给 raw commit。`gpiod_direction_output()` 位于 `gpiolib.c:2802`，先读取 `desc->flags`，根据 `FLAG_ACTIVE_LOW` 反相逻辑值，并禁止把已启用 IRQ 的 GPIO 改成输出，然后进入输出提交路径。

## 1.6\_can\_sleep\_契约

`include/linux/gpio/driver.h:380-384` 对 `gpio_chip.can_sleep` 的注释：当 get/set 会睡眠时必须设置，典型场景是 I²C/SPI GPIO 扩展器；若此 chip 支持 IRQ，读取 IRQ 状态也可能睡眠，因此 IRQ 需要 threaded。

源码用的是 must，而不是 may。正文据此把它解释为正确性契约。

## 1.7\_devres\_请求

`drivers/gpio/gpiolib-devres.c:112` 的 `devm_gpiod_get_index()`：

1. 调用 `gpiod_get_index()`；
2. 非独占请求检查同 device 是否已有相同 devres；
3. 为 `devm_gpiod_release` 分配 devres；
4. 分配失败时立即 `gpiod_put()`；
5. 成功则 `devres_add()`。

因此 devm 是在普通请求外增加 device 生命周期释放，不是另一套 GPIO 查找或配置机制。

## 1.8\_GPIO\_到\_IRQ

`drivers/gpio/gpiolib.c:3690` 的 `gpiod_to_irq()`：

- NULL/错误描述符返回 `-EINVAL`；
- 在 `gpio_device.srcu` 下读取 `gdev->chip`；
- chip 已移除返回 `-ENODEV`；
- 计算硬件 offset；
- 调用 `gc->to_irq()`；
- Provider 返回 0 时转换为 `-ENXIO`。

该函数只返回映射结果，不申请 handler，不配置 Consumer 生命周期。

## 1.9\_用户空间字符设备

`drivers/gpio/gpiolib-cdev.c:2595` 把 `GPIO_V2_GET_LINE_IOCTL` 分派到 `linereq_create()`。`linereq_create()` 位于 1733 行附近：

1. 校验 `num_lines`、padding 和 line config；
2. 分配 `struct linereq` 并取得 `gpio_device` 引用；
3. 初始化每线 delayed work、配置 mutex、waitqueue、事件 kfifo 和序号；
4. 循环 offset，调用 `gpio_device_get_desc()` 和 `gpiod_request_user()`；
5. 将 v2 flags 写入 `gpio_desc.flags`；
6. 按配置调用 `gpiod_direction_output()` 或 `gpiod_direction_input()`，输入事件再建立 edge detector；
7. 注册 device unregister notifier；
8. 最后分配 fd 并安装匿名 file。

`line_fileops` 的 `.release = linereq_release`、`.read = linereq_read`、`.poll = linereq_poll`、`.unlocked_ioctl = linereq_ioctl`，说明 request fd 同时承载释放、事件读取、等待和再配置生命周期。内部逐根请求意味着“一个 ioctl 请求一组 line”提供失败回滚和统一 fd，不自动等价于硬件同时切换。

## 1.10\_控制器注销

`drivers/gpio/gpiolib.c:1161` 的 `gpiochip_remove()` 注释规定仍有 requested GPIO 的 chip 不得移除。函数依次：撤销 sysfs、hog 和剩余 IRQ；从全局 list 删除并 `synchronize_srcu(gpio_devices_srcu)`；把 `gdev->chip` 设为 `NULL` 并等待 `gdev->srcu`；拆除 irqchip、ACPI、OF、pin ranges、valid mask 和 driver data；最后 `gcdev_unregister()`、`gpio_device_put()`。源码注释明确：有用户 client 时 cdev/device 会悬挂到最后一个用户离开。

## 1.11\_待继续核对的边界

- OF、ACPI 和 software node 查找的错误优先级；
- `gpio_irq_chip` 的不同父 IRQ 拓扑；
- NXP i.MX6ULL `gpio-mxc.c` 的寄存器、锁和唤醒实现；
- PREEMPT_RT 下 GPIO IRQ 回调的具体上下文变化。

这些项目在完成源码核对前不写成跨版本通用结论。
