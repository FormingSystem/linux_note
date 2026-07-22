---
id: knowledge.driver_model.gpio_consumers.standard_implementations
title: "输入、指示、电源与控制类 GPIO Consumer"
kind: subsystem
status: evolving
domains:
  - linux
  - kernel
  - driver
topics:
  - gpio
  - input
  - led
  - regulator
source_version: "6.12.20"
---

# 第2章\_输入\_指示\_电源与控制类\_GPIO\_Consumer

## 2.1\_源码范围

本文核对 Linux 6.12.20：

| 功能 | 源码 |
| --- | --- |
| 按键/开关 | `drivers/input/keyboard/gpio_keys.c` |
| LED | `drivers/leds/leds-gpio.c` |
| 开关式背光 | `drivers/video/backlight/gpio_backlight.c` |
| 固定稳压器 | `drivers/regulator/fixed.c` |
| GPIO mux | `drivers/mux/gpio.c` |
| 关机/重启 | `drivers/power/reset/gpio-poweroff.c`、`gpio-restart.c` |

这些文件证明具体实现，不代表对应子系统的全部契约。

## 2.2\_gpio-keys\_线路事件转换成\_input\_event

`gpio_keys.c` 为每个 button 保存 `struct gpio_button_data`，其中关联按钮配置、`gpio_desc`、input device、IRQ、timer/delayed work 和禁用状态。Linux 6.12.20 在子节点初始化中调用 `devm_fwnode_gpiod_get(..., GPIOD_IN, ...)` 取得输入描述符。

事件路径根据 `gpiod_cansleep()` 选择 `gpiod_get_value()` 或 `_cansleep()`；去抖可由硬件配置或 timer/delayed work 承担；稳定值经 `input_event()`/同步路径上报。这里至少有三层状态：GPIO line 电平、button 去抖状态、input key 状态，不能把一次 IRQ 直接等同一次按键事件。

## 2.3\_gpio-leds\_brightness\_转换成逻辑电平

`leds-gpio.c` 的 `struct gpio_led_data` 保存 LED class device、描述符、`can_sleep` 和 blink 状态。创建时读取 `gpiod_cansleep()`；brightness 更新据此选择 `gpiod_set_value()` 或 `_cansleep()`。初始状态最终通过 `gpiod_direction_output(desc, state)` 建立。

active-low 由描述符转换，因此 LED core 的 brightness 语义不应关心物理高低。硬件 blink 回调存在时可下沉闪烁，否则 LED core/工作路径承担定时切换成本。

## 2.4\_gpio-backlight\_只有开和关的背光

`gpio_backlight.c` 用 `devm_gpiod_get(..., GPIOD_ASIS)` 取得 line，结合初始 brightness 后调用 `gpiod_direction_output()`；更新路径使用 `gpiod_set_value_cansleep()`。

它适合 GPIO 只控制背光使能的二值设备。需要 PWM 亮度等级时应使用 PWM backlight 等模型，不能用高频用户态翻转 GPIO 模拟稳定调光。

## 2.5\_fixed\_regulator\_GPIO\_进入电源依赖图

`drivers/regulator/fixed.c` 把可选 enable GPIO 放入 `regulator_config.ena_gpiod`，由 regulator core 按 enable polarity、startup delay、off-on delay 和消费者引用关系控制。Linux 6.12.20 源码使用 `gpiod_get_optional()` 取得 enable line。

直接在设备驱动中拉 enable 只能改变电平，无法自动协调多个 regulator consumer、延时和电源拓扑。电源线语义匹配固定稳压器时，应让 regulator core 拥有 GPIO。

## 2.6\_gpio-mux\_一组\_line\_共同编码选择状态

`drivers/mux/gpio.c` 先用 `gpiod_count(dev, "mux")` 得到位数，再用 `devm_gpiod_get_array(..., GPIOD_OUT_LOW)` 请求数组，选择路径调用 `gpiod_set_array_value_cansleep()`。

数组 API 可让同一 Provider 使用批量回调，但跨 chip 或不支持批量的硬件不保证所有选择位同时变化。若外部 mux 对中间编码敏感，需要 enable/gate 信号或硬件锁存保证，而不能仅凭 array API 宣称无毛刺。

## 2.7\_gpio-poweroff\_和\_gpio-restart

两者向 sys-off 框架注册处理者，在关机或重启阶段按配置产生 GPIO 电平序列。Linux 6.12.20 的 poweroff 路径使用 `_cansleep` 写值，restart 路径使用普通 `gpiod_set_value()`；这反映两种回调上下文和 Provider 约束不同。

选择前必须核对控制器在系统关机/重启末期是否仍有时钟和电源，以及回调上下文是否允许目标 Provider 访问。把 reset 接到会睡眠且可能先被关闭的扩展器，可能在最后阶段失去控制能力。

## 2.8\_统一选择表

| 硬件功能 | 优先驱动 | 不适用信号 |
| --- | --- | --- |
| 人机按键/开关 | `gpio-keys` | 私有高速数据流 |
| 状态指示 | `gpio-leds` | 复杂波形协议 |
| 二值背光使能 | `gpio-backlight` | 多级 PWM 调光 |
| 固定电源使能 | fixed regulator | 可编程电压 PMIC 协议 |
| 二进制选择外部 mux | gpio mux | 对中间编码无毛刺有严格要求且无 gate |
| 最终关机/重启脉冲 | gpio poweroff/restart | Provider 在 sys-off 阶段不可访问 |
