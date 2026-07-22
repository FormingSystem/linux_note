---
id: atlas.tracks.linux_driver
title: "Linux驱动开发学习路线"
kind: track
status: maintained
domains:
  - navigation
  - linux
  - driver
---

# 第1章\_Linux驱动开发学习路线

## 1.1\_路线目标

本路线以“能写、能解释、能排错”为目标，从最小模块和字符设备逐步进入设备模型、设备树、GPIO、中断与 Input 子系统。默认具备 C 语言、基本 Linux 命令和交叉编译经验。

## 1.2\_第一阶段\_模块与字符设备

1. [Linux 内核模块与设备节点操作入门](../../knowledge/linux/architecture/modules_and_device_nodes/Linux_内核模块与设备节点操作入门.md)。
2. [Linux 驱动开发 Makefile 指南](../../knowledge/linux/architecture/modules_and_device_nodes/Linux_驱动开发_Makefile_指南.md)。
3. 先阅读[VFS 的对象拓扑](../../knowledge/kernel_subsystems/vfs/P03_VFS_状态与对象拓扑.md)、[open 状态机](../../knowledge/kernel_subsystems/vfs/P12_open状态机.md)和[fd/file 生命周期](../../knowledge/kernel_subsystems/vfs/P13_fd_table与file生命周期.md)，明确字符设备接入的上游边界；VFS 自身的完整路线仍以[VFS 专题大纲](../../knowledge/kernel_subsystems/vfs/大纲.md)为准。
4. 按序完成[字符设备专题](../../knowledge/driver_model/character_device/大纲.md)。
5. 结合[字符设备驱动模板](../../knowledge/driver_model/character_device/P10_字符设备驱动模板.md)完成最小驱动。

阶段验收：能独立完成模块装卸、设备号分配、`cdev` 注册、设备节点创建和基础文件操作。

## 1.3\_第二阶段\_驱动所需通用机制

1. [Linux 同步机制总纲](../../knowledge/linux/synchronization/大纲.md)。
2. [错误指针机制](../../knowledge/linux/error_handling/error_pointer/错误指针机制简介.md)。
3. [devres API](../../knowledge/linux/object_lifetime/devres/devres_API说明.md)。
4. [驱动中的时间问题](../../knowledge/linux/time_management/定时器简介/P01_驱动中的_时间问题_概述.md)，再按需要学习睡眠、timer、hrtimer 和 delayed work。
5. [poll 与 epoll](../../knowledge/linux/io_model/blocking_io/poll与epoll的区别.md)及[异步通知](../../knowledge/linux/io_model/async_notification/大纲.md)。

阶段验收：能为共享状态选择同步方法，正确管理失败路径和卸载路径，并为设备事件选择用户态通知方式。

## 1.4\_第三阶段\_设备模型与Platform

1. 按序完成[Linux 设备模型专题](../../knowledge/linux/device_model/大纲.md)。
2. 阅读[驱动框架模型](../../knowledge/driver_model/fundamentals/framework_model/P01_驱动框架模型.md)。
3. 按序完成[设备树与 Platform 开发](../../knowledge/driver_model/device_tree/设备树+platform开发)。
4. 选读[GPIO 与 pinctrl 设备树示例](../../knowledge/driver_model/device_tree/设备树语法专题-04-gpio+pinctrl+interrupt.md)。

阶段验收：能解释 device、driver、bus 的匹配过程，并把寄存器、中断和 GPIO 等硬件资源从设备树传递给驱动。

## 1.5\_第四阶段\_GPIO\_中断与Input

1. 按序完成 [GPIO 专题](../../knowledge/driver_model/gpio/大纲.md)。
2. 阅读 [标准 GPIO Consumer 专题](../../knowledge/driver_model/gpio_consumers/大纲.md)，理解何时复用 `gpio-keys`、`gpio-leds`、regulator 等领域驱动。
3. 阅读[Linux 驱动中的中断注册与接口](../../knowledge/kernel_subsystems/irq/中断机制简介/P04_Linux_驱动中的中断注册与接口.md)。
4. 阅读[GPIO 与触发语义](../../knowledge/kernel_subsystems/irq/中断机制简介/P05_GPIO_与触发语义_电平_边沿_DTS_与_只来一次一直来_为何出现.md)。
5. 按序完成 [Input 子系统](../../knowledge/driver_model/input/大纲.md)。

阶段验收：能实现 GPIO 输入输出、中断与去抖，并能说明 Input 事件从驱动上报到用户态读取的路径。

## 1.6\_第五阶段\_平台验证

建议依次完成现有 i.MX6ULL 实验：

1. [LED 的 ioremap 实现](../../labs/platforms/nxp/imx6ull/drivers/P01_LED点灯/P01_LED点灯+ioremap.md)。
2. [LED 的设备树实现](../../labs/platforms/nxp/imx6ull/drivers/P01_LED点灯/P02_LED点灯+dts.md)。
3. [BEEP 的设备树实现](../../labs/platforms/nxp/imx6ull/drivers/P02_BEEP/P01_beep+dts.md)。
4. [KEY 中断](../../labs/platforms/nxp/imx6ull/drivers/P03_KEY_LED_interrupt/P01_key_led_interrupt.md)及[中断唤醒内核](../../labs/platforms/nxp/imx6ull/drivers/P03_KEY_LED_interrupt/P03_key_interrupt_wakeup_kernel.md)。
5. [Input 按键实验](../../labs/platforms/nxp/imx6ull/drivers/P04_Input子系统/key-input.md)。

每个实验都应核对加载与卸载、失败清理、并发访问、设备树绑定和用户态验证，不只以“现象出现”作为完成标准。

## 1.7\_扩展方向

完成主线后，可进入[i.MX6ULL 的 U-Boot 与内核移植](../../platforms/arm/nxp/imx6ull/porting/imx6ull-移植u-boot-2025.04_and_kernel-6.1.md)和[Buildroot](../../knowledge/system_software/buildroot/P00_全书学习地图.md)，把单个驱动放回完整嵌入式 Linux 系统中理解。
