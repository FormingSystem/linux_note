---
id: engineering.driver_development.gpio.debug_migration
title: "GPIO 调试、迁移与工程模板"
kind: engineering
status: evolving
domains:
  - linux
  - kernel
  - driver
topics:
  - gpio
  - debugging
  - migration
depends_on:
  - knowledge.driver_model.gpio.userspace_abi_selection
---

# 第1章\_GPIO\_调试\_迁移与工程模板

本文解决“如何证明 GPIO 路径正确”，不重复 GPIO 机制。机制与接口保证见 [GPIO 专题](../../../knowledge/driver_model/gpio/大纲.md)。命令随内核配置和 libgpiod 版本变化，执行前应查看本机 `--help`。

## 1.1\_按状态层定位

不要从“GPIO 不工作”直接跳到换 API。先确定故障位于哪一层：

| 层 | 要验证的状态 | 主要证据 |
| --- | --- | --- |
| pad/pinctrl | pad 是否复用为 GPIO，bias 是否正确 | pinctrl debugfs、设备树、示波器 |
| Provider | gpiochip 是否注册、line 数量和名称 | `/sys/kernel/debug/gpio`、`gpioinfo`、内核日志 |
| 连接解析 | `*-gpios` 是否指向正确 Provider/offset/flags | 运行时设备树、probe 错误码 |
| 所有权 | line 被谁请求 | debugfs、line info consumer label |
| 方向与逻辑 | 输入/输出、active-low、初始值 | line info、驱动日志、物理电平 |
| IRQ | pending、Linux IRQ、计数、触发类型 | `/proc/interrupts`、trace、控制器状态 |
| 生命周期 | probe 失败或 fd 关闭后是否释放 | 重复绑定、重复 request、状态通知 |

## 1.2\_最小观察闭环

```bash
# 内核登记与 Consumer 标签
sudo cat /sys/kernel/debug/gpio

# libgpiod 工具名称和参数取决于安装版本
gpiodetect
gpioinfo

# 中断映射和计数
cat /proc/interrupts
```

看到 `active-low` 时，工具显示的逻辑值和示波器看到的物理电平可能相反。调试记录必须明确标注“逻辑值”或“物理电平”。

## 1.3\_设备树与运行时连接

检查编译输入 DTS 不足以证明运行内核使用同一版本。应反编译 `/sys/firmware/fdt` 或读取 `/proc/device-tree`，核对：

- 属性名是否为驱动请求的 `<con_id>-gpios`；
- phandle 是否指向已注册 Provider；
- offset 是否在 `ngpio` 和 valid mask 范围内；
- active-low/open-drain 等 flags 是否符合电路；
- pinctrl default/sleep 状态是否包含对应 pad。

`-ENOENT` 指向连接缺失，`-EPROBE_DEFER` 指向依赖尚未就绪，`-EBUSY` 指向所有权冲突；不要统一改成 optional 来掩盖错误。

## 1.4\_动态调试与\_trace

```bash
# 开启 gpiolib 动态调试；具体文件名以目标内核为准
echo 'file drivers/gpio/gpiolib*.c +p' |
    sudo tee /sys/kernel/debug/dynamic_debug/control

# 查看可用 GPIO trace event 后按目标开启
sudo sh -c 'grep -i gpio /sys/kernel/tracing/available_events'
```

动态调试用于确认 S1～S7 的控制流，示波器/逻辑分析仪用于确认物理波形。软件日志成功不能证明 pad mux、外部上拉或电源域正确。

## 1.5\_中断故障矩阵

| 现象 | 优先检查 |
| --- | --- |
| 完全没有计数 | pinmux、输入方向、父 IRQ、mask、触发类型 |
| 只触发一次 | ack/mask 顺序、设备内部状态是否清除 |
| 中断风暴 | 电平条件未解除、极性相反、浮空输入 |
| 偶发重复 | 机械抖动、边沿配置、去抖窗口 |
| 扩展器 IRQ 卡死 | 线程化路径、I²C 错误、pending 缓存同步 |
| 能中断但不能唤醒 | wake 配置、父 IRQ、电源域、sleep pinctrl |

## 1.6\_迁移验收表

### 1.6.1\_整数接口到描述符接口

- 全局编号已从驱动源码移出；
- 请求使用功能名和 index；
- 初始方向和值在 get flags 中建立；
- 逻辑值与 raw 值没有混用；
- `can_sleep` 与执行上下文匹配；
- deferred probe 没有被转换成永久失败；
- probe 每个失败点都能释放已取得资源；
- IRQ 经 `gpiod_to_irq()` 映射，而非复用 GPIO number。

### 1.6.2\_sysfs\_到字符设备

- 明确目标 libgpiod major version；
- 明确 request fd 持有时长；
- 不假定命令退出后输出保持；
- 多线初值在同一 request 中建立；
- consumer label 能识别业务进程；
- busy 被当作所有权冲突处理，而非强行绕过；
- 事件缓冲、时钟和溢出策略符合业务需求。

## 1.7\_无板验证

内核 `gpio-sim` 和 `tools/testing/selftests/gpio/` 可建立虚拟 gpiochip，适合验证请求冲突、active-low、方向、字符设备事件和释放行为。它不能验证真实 pad mux、电气特性、控制器寄存器时序和板级中断连线；这些仍需目标板和仪器。

## 1.8\_交付证据

一次可审查的 GPIO 变更至少保存：

1. 内核版本、配置和设备树版本；
2. Provider 与 line info 快照；
3. 请求者、方向、active-low 和 fd/device 生命周期；
4. 正常值、边界值和所有权冲突结果；
5. IRQ 计数及触发类型；
6. suspend/resume 前后结果；
7. 必要时的示波器或逻辑分析仪波形。
