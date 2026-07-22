---
id: knowledge.driver_model.gpio.gpio子模块.p04_provider_控制器_驱动编写
title: "GPIO Provider 控制器实现"
kind: subsystem
status: evolving
domains:
  - linux
  - kernel
  - driver
topics:
  - gpio
  - provider
  - gpio_chip
depends_on:
  - knowledge.driver_model.gpio.gpiolib_implementation
---

# 第6章\_GPIO\_Provider\_控制器实现

## 6.1\_Provider\_兑现什么保证

Provider 把一组 offset 为 `0..ngpio-1` 的硬件 line 注册为 `gpio_chip`。它不决定哪一个设备把某根线叫作 reset；它负责在合法上下文中兑现方向、读写、配置和可选 IRQ 能力，并维护寄存器、总线缓存、锁和电源状态。

Linux 6.12.20 的 `struct gpio_chip` 在 `include/linux/gpio/driver.h` 中包含 `request/free`、`get_direction`、`direction_input/output`、`get/set`、批量回调、`set_config`、`to_irq`、`base`、`ngpio`、`can_sleep` 和内嵌 `gpio_irq_chip`。

## 6.2\_MMIO\_与总线扩展器的共同接口\_不同成本

| 条件 | SoC MMIO GPIO | I²C/SPI GPIO 扩展器 |
| --- | --- | --- |
| 状态位置 | 控制器寄存器 | 外设寄存器，常伴软件缓存 |
| 单次访问 | CPU MMIO | 总线消息、控制器完成、可能调度 |
| `can_sleep` | 通常 false | 必须 true（若 get/set 睡眠） |
| 并发保护 | raw spinlock、原子 set/clear 寄存器 | mutex、regmap/总线锁 |
| IRQ pending | 直接读状态寄存器 | 父 IRQ 后通过总线读取 |
| 失电影响 | 取决于 SoC 电源域 | 取决于扩展器电源和缓存恢复 |

`driver.h` 对 `can_sleep` 的注释是强制条件：get/set 会睡眠时必须设置；若该 chip 支持 IRQ，读取 IRQ 状态也可能睡眠，因此 IRQ 需要线程化。这是源码给出的精确契约，而非一般性能建议。

## 6.3\_S1\_注册所需的最小状态

```c
struct my_gpio {
    void __iomem *base;
    raw_spinlock_t lock;
    struct gpio_chip chip;
};

static int my_gpio_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct my_gpio *priv;

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->base = devm_platform_ioremap_resource(pdev, 0);
    if (IS_ERR(priv->base))
        return PTR_ERR(priv->base);

    raw_spin_lock_init(&priv->lock);
    priv->chip.label = dev_name(dev);
    priv->chip.parent = dev;
    priv->chip.owner = THIS_MODULE;
    priv->chip.base = -1;
    priv->chip.ngpio = 32;
    priv->chip.can_sleep = false;
    priv->chip.get_direction = my_gpio_get_direction;
    priv->chip.direction_input = my_gpio_direction_input;
    priv->chip.direction_output = my_gpio_direction_output;
    priv->chip.get = my_gpio_get;
    priv->chip.set = my_gpio_set;

    return devm_gpiochip_add_data(dev, &priv->chip, priv);
}
```

固定非负 `base` 在 Linux 6.12.20 的 `gpio_chip` 注释和注册路径中都被标记为 deprecated；新驱动使用 `base = -1` 请求动态分配。稳定连接应依赖描述符和固件引用，不依赖全局整数 base。

## 6.4\_方向输出必须处理初值顺序

`direction_output(gc, offset, value)` 同时接收方向和初值。Provider 应按硬件手册选择避免毛刺的顺序，例如先写输出数据锁存器，再打开输出驱动。若硬件提供原子 set/clear 寄存器，应优先使用，避免读—改—写覆盖其他 CPU 修改的位。

若多个 line 共用一个数据寄存器，朴素的：

```text
read register → 修改一位 → write register
```

必须由锁保护，或者使用硬件原子寄存器。锁保护的状态地址通常是 Provider 私有结构中的 raw spinlock；gpiolib 的请求锁不能替代寄存器级并发保护，因为 S5 的不同 line 可以同时由不同 Consumer 合法访问。

## 6.5\_批量操作的保证与限制

`get_multiple`/`set_multiple` 允许 Provider 在一次寄存器或总线事务中处理多根 line。它减少 I²C 消息和 MMIO 锁操作，并能在硬件支持时让多位同时变化。

但“调用批量 API”不自动保证物理同步：若线路跨越多个 `gpio_chip`，公共层仍需分组调用多个 Provider；若硬件只能逐位更新，Provider 也无法制造原子切换。调用者必须按所在 chip 和硬件能力判断保证边界。

## 6.6\_pinctrl\_交界

pinctrl 决定 pad 复用和电气配置，GPIO Provider 决定作为 GPIO 后的方向和值。`gpio-ranges` 可描述 GPIO offset 与 pinctrl pin 的映射，使请求/释放 GPIO 时 pinctrl 知道对应 pin。

专题在此给出完整判断：

1. pad 未复用为 GPIO 时，Provider 写数据寄存器可能不改变引脚；
2. GPIO 请求成功只证明 gpiolib 所有权成功，不证明 pinmux 和 bias 正确；
3. `set_config` 可把部分 bias、drive 等请求转给 Provider/pinctrl，但支持集合由控制器决定；
4. `pinctrl-0`/sleep 状态的具体语法和 SoC pin 编号属于 pinctrl 实现，不复制到 GPIO Provider 正文。

## 6.7\_总线型\_Provider\_的缓存与错误

扩展器常用 regmap 或私有缓存减少总线访问。缓存是 Provider 私有状态，不等同 `gpio_desc.flags`。写事务失败时不能无条件把缓存当作硬件已更新；恢复供电后也要按驱动 PM 策略同步缓存。

正常 S5 路径可能睡眠等待总线完成；总线超时返回错误的回调应向上传播。注意 `gpio_chip.set()` 返回 `void`，某些输出错误只能记录或通过更高层状态发现，这是接口保证的限制，不能在正文中宣称所有写失败都同步返回 Consumer。

## 6.8\_注销和在途引用

Linux 6.12.20 的 `gpio_device` 注释明确：它保存多数运行时可变状态，并可在 `gpio_chip` 已移除后因用户空间引用继续存活。其 `chip` 字段是 `struct gpio_chip __rcu *`，访问路径通过 SRCU 取得；`gpiod_to_irq()` 在解引用后若 chip 已不存在返回 `-ENODEV`。

`gpiochip_remove()` 的源码注释要求仍有 requested GPIO 的 chip 不得移除。实际注销先撤销 sysfs、hog 和剩余 IRQ，把 `gpio_device` 从全局列表删除并等待查找侧 SRCU；随后以 RCU 把 `gdev->chip` 设为 `NULL`，再次等待在途访问，才拆除 IRQ、OF/ACPI、pin ranges 和私有 data。最后注销 cdev 并放下 gpiochip 自身引用；若仍有用户空间 client，`gpio_device` 会悬挂到最后一个用户离开。

这说明注销不是立即释放所有内部对象，而是先阻止新查找并切断 Provider 能力，再由引用生命周期决定 `gpio_device` 最终释放。Provider 私有结构不能在 SRCU 等待完成前销毁。

下一篇在 Provider IRQ 能力之上展开事件链：[GPIO 中断桥接与事件传播](P07_GPIO_中断桥接与事件传播.md)。
