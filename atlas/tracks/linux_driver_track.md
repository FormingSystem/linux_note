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

开始前建议先在[知识库专题阅读与评审地图](../maps/knowledge_review_map.md)查看各专题和章节的人工确认程度；路线中的阶段验收是学习目标，不等同于内容评审状态。

## 1.2\_第一阶段\_模块与字符设备

1. 先完成[内核模块构建与部署](../../engineering/build/kernel_modules/大纲.md#1.1_四章怎样连起来)，从 Hello 模块建立目标身份、构建和装卸闭环，再学习多文件与配置集成。
2. 阅读[模块装载与设备访问入口](../../knowledge/linux/architecture/modules_and_device_nodes/Linux_内核模块与设备节点操作入门.md)，观察 Hello 模块与现有文件，区分代码、服务和路径；沿[模块入口大纲](../../knowledge/linux/architecture/modules_and_device_nodes/大纲.md#1.1_沿三个问题进入正文)进入号码登记与多实例身份。
3. 先阅读[VFS 的对象拓扑](../../knowledge/kernel_subsystems/vfs/P03_VFS_状态与对象拓扑.md)、[open 状态机](../../knowledge/kernel_subsystems/vfs/P12_open状态机.md)和[fd/file 生命周期](../../knowledge/kernel_subsystems/vfs/P13_fd_table与file生命周期.md)，明确字符设备接入的上游边界；VFS 自身的完整路线仍以[VFS 专题大纲](../../knowledge/kernel_subsystems/vfs/大纲.md)为准。
4. 按序完成[字符设备专题](../../knowledge/driver_model/character_device/大纲.md)。
5. 用[有限窗口模板](../../knowledge/driver_model/character_device/P10_字符设备驱动模板.md)和[运行验证](../../knowledge/driver_model/character_device/P11_构建运行与验证.md)观察位置、内容与节点关系；再以[环形流模板](../../knowledge/driver_model/character_device/P13_流式字符设备与等待通知模板.md)引入消费、等待和 poll，不把两种 EOF 规则混用。

阶段验收：能独立完成模块装卸、设备号分配、`cdev` 注册、设备节点创建和基础文件操作。

## 1.3\_第二阶段\_驱动所需通用机制

1. [Linux 同步和异步机制总纲](../../knowledge/linux/synchronization_and_asynchrony/大纲.md)。
2. [错误指针专题](../../knowledge/linux/error_handling/error_pointer/大纲.md#1.1_四次认识变化)。
3. [devres资源账本与分组](../../knowledge/linux/object_lifetime/devres/大纲.md#1.1_从退出责任进入资源接口)：先运行六条C回滚路径，再查具体资源接口。
4. [驱动中的时间问题](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/P01_驱动中的_时间问题_概述.md)，再按需要学习睡眠、timer、hrtimer 和 delayed work。
5. [poll 与 epoll](../../knowledge/linux/io_model/blocking_io/大纲.md)及[异步通知](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/大纲.md)。

阶段验收：能为共享状态选择同步方法，正确管理失败路径和卸载路径，并为设备事件选择用户态通知方式。

## 1.4\_第三阶段\_设备模型与Platform

1. 从[驱动框架学习地图](../../knowledge/driver_model/fundamentals/framework_model/大纲.md#1.1_四个问题怎样接起来)进入：先分离实例和代码，再观察登记与绑定，完成只读 sysfs 属性和 misc 字符入口，区分对象引用、活动回调和旧打开者。
   完成 misc 后，沿[文件操作教材](../../knowledge/driver_model/file_operations/大纲.md#1.1_沿对象寿命逐步增加约束)观察 dup 与独立打开、readv 与 pread、关闭 fd 后的映射引用；不要把 file_operations 当作必须全部填写的成员清单。
2. 按序完成[Linux 设备模型专题](../../knowledge/linux/device_model/大纲.md)，继续研究匹配、依赖、热拔插和完整生命周期；小型入口不能代替这部分协议。
   第 11 章进入[class 与 sysfs 分支](../../knowledge/linux/device_model/class_sysfs/大纲.md#1.1_从分类观察到可控数据入口)，先观察无节点的两个分类实例，再运行属性控制字符读取，解释输入失败、权限和撤销边界。
3. 按序完成[设备树与 Platform 开发](../../knowledge/driver_model/device_tree/设备树+platform开发)。
4. 选读[GPIO 与 pinctrl 设备树示例](../../knowledge/driver_model/device_tree/设备树语法专题-04-gpio+pinctrl+interrupt.md)。

阶段验收：能解释 device、driver、bus 的匹配过程，并把寄存器、中断和 GPIO 等硬件资源从设备树传递给驱动。

## 1.5\_第四阶段\_GPIO\_中断与Input

1. 按序完成 [GPIO 专题](../../knowledge/driver_model/gpio/大纲.md)。
2. 阅读 [标准 GPIO Consumer 专题](../../knowledge/driver_model/gpio_consumers/大纲.md)，理解何时复用 `gpio-keys`、`gpio-leds`、regulator 等领域驱动。
3. 阅读[Linux 驱动中的中断注册与接口](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P04_Linux_驱动中的中断注册与接口.md)。
4. 阅读[GPIO 与触发语义](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/P05_GPIO_与触发语义_电平_边沿_DTS_与_只来一次一直来_为何出现.md)。
5. 按序完成 [Input 子系统](../../knowledge/driver_model/input/大纲.md)。

阶段验收：能实现 GPIO 输入输出、中断与去抖，并能说明 Input 事件从驱动上报到用户态读取的路径。

## 1.6\_第五阶段\_平台验证

建议依次完成现有 i.MX6ULL 实验：

开始实验前，先从[Arm、GIC、RK3588 与 i.MX6ULL 外部资料索引](../../reference/external_resources/arm/README.md#1.4_版本选择与阅读顺序)确认处理器架构、GIC 代际和芯片手册版本。外部资料给出硬件契约，下面的实验记录给出特定板卡与软件版本上的观测结果，二者不能互相替代。面向 RK3588 的控制器学习按 [GICv3 独立专题](../../platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图)，先学 GIC-600 r1p6 / GICv3.0；不要求先掌握 GIC 代际差异。

1. [LED 的 ioremap 实现](../../labs/platforms/nxp/imx6ull/drivers/P01_LED点灯/P01_LED点灯+ioremap.md)。
2. [LED 的设备树实现](../../labs/platforms/nxp/imx6ull/drivers/P01_LED点灯/P02_LED点灯+dts.md)。
3. [BEEP 的设备树实现](../../labs/platforms/nxp/imx6ull/drivers/P02_BEEP/P01_beep+dts.md)。
4. [KEY 中断](../../labs/platforms/nxp/imx6ull/drivers/P03_KEY_LED_interrupt/P01_key_led_interrupt.md)及[中断唤醒内核](../../labs/platforms/nxp/imx6ull/drivers/P03_KEY_LED_interrupt/P03_key_interrupt_wakeup_kernel.md)。
5. [Input 按键实验](../../labs/platforms/nxp/imx6ull/drivers/P04_Input子系统/key-input.md)。

每个实验都应核对加载与卸载、失败清理、并发访问、设备树绑定和用户态验证，不只以“现象出现”作为完成标准。

## 1.7\_扩展方向

完成主线后，可进入[i.MX6ULL 的 U-Boot 与内核移植](../../platforms/arm/nxp/imx6ull/porting/imx6ull-移植u-boot-2025.04_and_kernel-6.1.md)和[Buildroot](../../knowledge/system_software/buildroot/P00_全书学习地图.md)，把单个驱动放回完整嵌入式 Linux 系统中理解。
