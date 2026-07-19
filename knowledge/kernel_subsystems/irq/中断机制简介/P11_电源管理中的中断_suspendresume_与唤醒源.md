# 第11章_电源管理中的中断_suspendresume_与唤醒源

## 11.1_电源管理中的中断_suspend/resume_与唤醒源

### 11.1.1_章节内容说明

本章讨论的是：系统进入休眠（suspend-to-RAM / system sleep）或运行时节能（runtime PM）后，中断为什么“不来了”“一直来”“一唤就醒”“唤不醒”，以及 Linux 是怎么用“唤醒源（wakeup source）+ 中断标志位 + pinctrl-sleep 配置”把这些状态串起来的。本章重点不是 PM 框架本身，而是**中断在 PM 状态切换期间的可用性与语义变化**，这是驱动作者最容易遗漏的部分。

本章目标：

1. 说明 suspend/resume 时内核会对中断做哪些限制和重配置；
2. 说明 IRQF_NO_SUSPEND、wakeirq、wakeup-source 各自的语义差异；
3. 说明 GPIO 中断为什么在 suspend 后经常“唤不醒”或“醒得太频繁”；
4. 说明 pinctrl 的 active/sleep 状态与中断触发之间的关系；
5. 给出驱动在有/无电源管理情况下的推荐写法和排查步骤。

本章结构：

- 11.1 suspend/resume 下的中断生存期问题
- 11.2 唤醒源（wakeup source）与中断的绑定方式
- 11.3 GPIO 中断唤醒与 pinctrl-sleep 的协同
- 11.4 runtime PM 场景下的中断可用性
- 11.5 常见问题与排查清单



------

### 11.1.2_suspend/resume_下的中断生存期问题

#### (1)_核心现象

在实际开发中你会遇到几种极常见的现象：

1. 系统一进 suspend，某些中断就完全没了，resume 之后又恢复了；
2. 系统已经休眠，但一有 GPIO 抖动就被“莫名其妙唤醒”；
3. 同一块板子上，一个设备能唤醒，另一个设备不能唤醒，DTS 看起来却是同一写法；
4. resume 之后第一次中断能进，第二次就进不来了，或者进来的是别的中断。

要理解这些现象，得先接受一个前提：**进入系统级 suspend 时，内核有权利屏蔽一部分 IRQ**，因为这些 IRQ 所在的设备、时钟域、电源域已经被关了，再响应它们既没意义，还可能把系统拉醒。

#### (2)_suspend_阶段内核对中断的处理思路

简化来看，进入 suspend 时对 IRQ 的处理大致是：

1. 把“不允许在 suspend 状态触发”的 IRQ 屏蔽掉；
2. 把“允许唤醒系统的 IRQ”保留下来；
3. 在硬件允许的情况下，把能作为唤醒源的 GPIO / 外部中断保持在工作状态；
4. 把和当前电源域一起关掉的中断一并关掉，防止空中断、虚假中断。

所以你会看到一个事实：**不是所有已经 request_irq 成功的中断都能在 suspend 状态下用**，只有被内核/驱动标记为“这条中断是唤醒路径的一部分”的 IRQ，才能在 suspend 期间保持活动。

#### (3)_IRQF_NO_SUSPEND_的真实语义

很多人以为加了 IRQF_NO_SUSPEND 就“一定能在 suspend 期间响应”。不对，真实语义要更谨慎：

- IRQF_NO_SUSPEND 的主要作用是：告诉内核“即使系统要进 suspend，这条 IRQ 也不要跟着一起被挂起来”；
- 它更像是“禁止在 suspend 阶段把我排除掉”，而不是“我能唤醒系统”；
- 真正的“能唤醒系统”要结合唤醒源（wakeup source）或 wakeirq 来说。

因此，如果只是简单地给一个普通外设中断加了 IRQF_NO_SUSPEND，很可能你只是让它在 suspend 流程里不被静态屏蔽，但并没有真正把它接入 “wake → resume” 那条系统唤醒路径。

#### (4)_为什么有的中断进不了_suspend

有一类中断，在设备 runtime suspend 甚至 system suspend 时会被内核强行忽略，这是因为：

- 该中断所在的设备/控制器即将掉电
- 再响应它没有意义
- 甚至会产生“掉电控制器上报中断”这种错误路径

这种情况下，正确的做法是“把能做唤醒的外侧 GPIO/PMIC-IRQ 留在电源外侧”，而不是强行给一个会被关电的控制器加 IRQF_NO_SUSPEND。

#### (5)_总结一句话

suspend 阶段，中断不是“能用就都用”，而是“只保留被声明为唤醒路径的那一小部分”，其余的会被暂时冻结；resume 后才恢复原本的中断分发。驱动不能假设“我既然 request_irq 成功了，就一定会在 suspend 里也触发”。

------

### 11.1.3_唤醒源(wakeup_source)与中断的绑定方式

#### (1)_唤醒的基本模型

从内核视角看，“唤醒”是一条完整的链路：

硬件中断源
 → 中断控制器（可能分层）
 → 被允许在 suspend 期间工作的那一组 IRQ
 → PM 核心判断“这是允许唤醒的事件”
 → 触发系统 resume

只要这条链中有一段没打通（比如 GPIO 进不来、该 IRQ 没被标成 wakeup、控制器 suspend 时被关掉），最终就不能唤醒。

#### (2)_wakeup_source_的标记方式

Linux 用“设备是否能唤醒系统”这个属性来描述是否允许该设备的中断在 suspend 时生效。典型方式有三种：

1. DTS 中写 `wakeup-source;` 或类似属性（不同 SoC 有不同叫法）
2. 驱动在 probe 时调用 `device_init_wakeup(dev, true);`
3. 用户态通过 sysfs 把设备设成可以唤醒

这三种方式的目的都是：**让内核在进入 suspend 时，不要把这条中断关掉**，并在真正触发时，把它当成“唤醒事件”处理。

#### (3)_wakeirq(专门的唤醒中断)

有的设备实际上有两条中断线：

- 一条是“正常工作时的中断”
- 一条是“唤醒用的中断”

Linux 为这种场景提供了 wakeirq 机制，把“唤醒线”和“工作线”区分开。这样可以做到：

- 平时用工作中断，功能全；
- suspend 时只保留唤醒中断，这条中断可以挂在一个永远有电的 GPIO/PMIC-IRQ 上；
- resume 后再恢复工作中断。

这是一种很实用的分离方式，避免了“为了能唤醒而让整个功能中断一直保持激活”这种能源浪费。

#### (4)_能唤醒_与_IRQF_NO_SUSPEND_的差别

可以这样记：

- “能唤醒”是**PM 语义**：系统睡着了，这条 IRQ 还能把系统叫醒；
- IRQF_NO_SUSPEND 是**中断控制语义**：别在 suspend 阶段把我挂起来
- wakeirq 是**硬件线路级别的分离语义**：我有一条专门唤醒的中断线

它们可以配合，但不要混淆。最常见的错误是：只加了 IRQF_NO_SUSPEND，以为就能唤醒，结果系统只是在 suspend 里没有马上把中断关掉，但真正睡下去后，硬件那条线也进不来。

#### (5)_开发者要写在驱动里的最少内容

对一个“要支持唤醒”的外设驱动，最少要做到：

1. 在 probe 里打开设备的 wakeup 能力，例如：

   ```c
   device_init_wakeup(dev, true);
   ```

2. 如果硬件是分成“工作中断 + 唤醒中断”，就用 wakeirq 把唤醒线补上；

3. 不在中断处理函数里随意 disable_irq() 而忘了在 resume 里恢复，否则会表现成“能唤醒一次，以后都不行”。



### 11.1.4_GPIO_中断唤醒与_pinctrl-sleep_的协同

#### (1)_为何_GPIO_中断能用_但不能唤醒

这类情况最典型：系统正常运行时，GPIO 中断一切正常；一进入 suspend，再按同一个按键却不能唤醒。根本原因通常是以下两点之一：

1. suspend 时该 GPIO 所在的 pin 被切到了 sleep 配置，**方向/上下拉/中断功能**被改掉了；
2. 该 GPIO 所在的控制器被整体 gate 掉（时钟或电源关闭），所以它根本不能在低功耗里采样电平变化。

也就是说：**正常态能中断 ≠ 低功耗态能唤醒**，中间差了一个 pinctrl-sleep 与电源域的匹配问题。

#### (2)_active_/_sleep_两套_pin_配置的作用

Linux 的 pinctrl 子系统允许你给同一组引脚写两套配置：

- active 配置：系统正常运行时用，功能最全，复用成 GPIO 中断、外设 IO、上拉/下拉齐全；
- sleep 配置：系统进入 suspend 时自动切换，目的是把引脚调到“省电、安全、不乱触发”的状态。

问题就出在：**如果你在 sleep 配置里把这个 GPIO 从“中断输入”改成了“普通输入/上拉/下拉/甚至输出”，那么 suspend 后这条中断自然进不来**。

下面用真实的设备树节点来演示。原始节点是：

```dts
demo_led_key_int: led_key_int@0 {
	compatible = "nxp,imx6ull-led_key_int";
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_led_key_int>;

	led-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;
	key-gpios = <&gpio1 18 GPIO_ACTIVE_LOW>;

	interrupt-parent = <&gpio1>;
	interrupts = <18 IRQ_TYPE_EDGE_FALLING>;

	nxp,debounce-ms = <30>;
	status = "okay";
};
```

这个写法在“运行态”(active)下是够用的：LED 一个 GPIO，按键一个 GPIO，中断从 gpio1_18 进来，下降沿触发，带软件去抖。但是它只有 `"default"`（active）这一套 pin 配置，没有给出“睡眠态也要保留按键中断”的配置。所以我们要在这个基础上扩成“两套 pin”。

下面是“扩展后”的写法。

##### 1)_设备节点扩展成_active_+_sleep

```dts
demo_led_key_int: led_key_int@0 {
	compatible = "nxp,imx6ull-led_key_int";

	/* 运行态 + 休眠态两套 */
	pinctrl-names = "default", "sleep";
	pinctrl-0 = <&pinctrl_led_key_int_active>;
	pinctrl-1 = <&pinctrl_led_key_int_sleep>;

	/* LED 仍然是 GPIO1_IO03，低电平点亮 */
	led-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;

	/* 按键走 GPIO1_IO18，低电平有效 */
	key-gpios = <&gpio1 18 GPIO_ACTIVE_LOW>;

	/* 中断来自 gpio1 控制器的 18 号引脚，下降沿 */
	interrupt-parent = <&gpio1>;
	interrupts = <18 IRQ_TYPE_EDGE_FALLING>;

	/* 软件去抖动 30ms，由驱动消费 */
	nxp,debounce-ms = <30>;

	status = "okay";
};
```

要点：

- 我们没改你已有的字段名，还是 led-gpios / key-gpios / nxp,debounce-ms；
- 只是把 `pinctrl-names` 从一个改成了两个；
- 中断来源、触发类型都保持你提供的版本：`&gpio1` + `<18 IRQ_TYPE_EDGE_FALLING>`。

##### 2)_active_态的_pinctrl(运行时)

```dts
pinctrl_led_key_int_active: led_key_int-active {
	fsl,pins = <
		/* LED: GPIO1_IO03 → GPIO1,3，输出，默认拉高灭灯 */
		MX6UL_PAD_GPIO1_IO03__GPIO1_IO03  0x000110B0

		/* KEY: UART1_CTS_B → GPIO1_IO18，做成中断输入并上拉，防抖交给驱动 */
		MX6UL_PAD_UART1_CTS_B__GPIO1_IO18 0x000110B0
	>;
};
```

说明：

- 第一行是 LED 用的管脚，还是你原来用的 `&gpio1 3 GPIO_ACTIVE_LOW` 那根；
- 第二行是按键对应的 GPIO1_IO18，我们在 active 态下把它复用成 GPIO 输入，并保持上拉（0x000110B0 只是典型 i.MX6ULL 的 mux+pad 配置位，你的实际值按 BSP 改）。

##### 3)_sleep_态的_pinctrl(休眠也要能中断)

```dts
pinctrl_led_key_int_sleep: led_key_int-sleep {
	fsl,pins = <
		/* LED：可以保持原样，或者改成纯 GPIO 输出高，避免休眠乱闪 */
		MX6UL_PAD_GPIO1_IO03__GPIO1_IO03  0x000110B0

		/* KEY：关键是这行，休眠态仍然保持成 GPIO 输入 + 上拉，不能换成别的功能 */
		MX6UL_PAD_UART1_CTS_B__GPIO1_IO18 0x000110B0
	>;
};
```

关键点只有一个：**sleep 态也要让 GPIO1_IO18 维持“可中断的输入”状态**，不能因为想省电就把它改成普通输入/复用成别的功能，否则 suspend 之后这条 `<18 IRQ_TYPE_EDGE_FALLING>` 就起不来了。

##### 4)_如果要做到_真能唤醒

上面只是把 DTS 的 active/sleep 两套 pin 配好了，还要加两件事（你驱动里要支持）：

1. 节点可以再加一行，让 PM 知道它是唤醒源：

   ```dts
   wakeup-source;
   ```

2. 驱动的 probe 里要配合：

   ```c
   device_init_wakeup(dev, true);
   ```

这样 suspend 时内核就不会把这条来自 `&gpio1 18` 的中断直接关掉。

##### 5)_下面是具体驱动示例代码

```c
// SPDX-License-Identifier: GPL-2.0
//
// demo_led_key_int.c
// 支持 DTS 节点：demo_led_key_int: led_key_int@0
// compatible = "nxp,imx6ull-led_key_int";
// LED:  led-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;
// KEY:  key-gpios = <&gpio1 18 GPIO_ACTIVE_LOW>;
// IRQ:  interrupt-parent = <&gpio1>; interrupts = <18 IRQ_TYPE_EDGE_FALLING>;
// nxp,debounce-ms = <30>;

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pm_wakeup.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>

struct demo_led_key {
	struct device       *dev;
	struct gpio_desc    *led_gpiod;
	struct gpio_desc    *key_gpiod;
	int                  irq;
	u32                  debounce_ms;
	unsigned long        last_jiffies;
	struct mutex         lock;     /* 保护 last_jiffies/LED 等 */
	bool                 wakeup_en;
};

/* 简单：LED 低电平点亮，高电平熄灭 */
static void demo_led_key_set_led(struct demo_led_key *dlk, bool on)
{
	if (!dlk->led_gpiod)
		return;
	/* 你的 DTS 写的是 GPIO_ACTIVE_LOW → 点亮 = 拉低 */
	gpiod_set_value_cansleep(dlk->led_gpiod, on ? 0 : 1);
}

/*
 * 顶半部：尽量快，只做“能不能进线程”的判断
 * 我们用 threaded irq，不在这里做去抖
 */
static irqreturn_t demo_led_key_irq(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

/*
 * 线程中断处理：做去抖 + 业务
 */
static irqreturn_t demo_led_key_thread(int irq, void *dev_id)
{
	struct demo_led_key *dlk = dev_id;
	unsigned long now = jiffies;
	unsigned long delta_ms;
    struct device *dev = dlk->dev;

	mutex_lock(&dlk->lock);

	if (dlk->debounce_ms) {
		delta_ms = jiffies_to_msecs(now - dlk->last_jiffies);
		if (delta_ms < dlk->debounce_ms) {
			/* 抖动太快，丢掉 */
			mutex_unlock(&dlk->lock);
			return IRQ_HANDLED;
		}
	}

	dlk->last_jiffies = now;

	/* 这里写你的按键业务：比如按一下就翻转 LED */
	if (dlk->led_gpiod) {
		int cur = gpiod_get_value_cansleep(dlk->led_gpiod);
		/* 当前是 0 = 亮，我们翻转 */
		demo_led_key_set_led(dlk, cur ? true : false);
	}

	/* 如果这是唤醒中断，通知 PM 有活动 */
	if (device_may_wakeup(dlk->dev))
		pm_wakeup_event(dlk->dev, 0);

	mutex_unlock(&dlk->lock);
    dev_info(dev, "into thread interrupt!\r\n");
	return IRQ_HANDLED;
}

static int demo_led_key_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct demo_led_key *dlk;
	int ret;

	dlk = devm_kzalloc(dev, sizeof(*dlk), GFP_KERNEL);
	if (!dlk)
		return -ENOMEM;

	dlk->dev = dev;
	mutex_init(&dlk->lock);
	dlk->last_jiffies = jiffies;

	/* LED GPIO，低电平点亮 */
	dlk->led_gpiod = devm_gpiod_get_optional(dev, "led", GPIOD_OUT_HIGH);
	if (IS_ERR(dlk->led_gpiod)) {
		ret = PTR_ERR(dlk->led_gpiod);
		dev_err(dev, "failed to get led-gpios: %d\n", ret);
		return ret;
	}
	/* 默认熄灭 */
	demo_led_key_set_led(dlk, false);

	/* KEY GPIO，主要是为了读当前电平，触发还是靠 IRQ */
	dlk->key_gpiod = devm_gpiod_get_optional(dev, "key", GPIOD_IN);
	if (IS_ERR(dlk->key_gpiod)) {
		ret = PTR_ERR(dlk->key_gpiod);
		dev_err(dev, "failed to get key-gpios: %d\n", ret);
		return ret;
	}

	/* 读去抖时间 */
	ret = device_property_read_u32(dev, "nxp,debounce-ms", &dlk->debounce_ms);
	if (ret)
		dlk->debounce_ms = 0;

	/* 解析 IRQ：走的就是你 DTS 里 <18 IRQ_TYPE_EDGE_FALLING> 那个 */
	dlk->irq = platform_get_irq(pdev, 0);
	if (dlk->irq < 0)
		return dlk->irq;

	/* 让设备具备唤醒能力（对应 DTS 里可以再加 wakeup-source;） */
	device_init_wakeup(dev, true);
	dlk->wakeup_en = true;

	/*
	 * 注册中断：
	 * - 用 threaded，方便做去抖
	 * - 加 IRQF_NO_SUSPEND，允许 suspend 阶段也保留
	 *   （如果你要更严格，就去掉这 flag，在 PM 回调里配合 enable_irq_wake）
	 */
	ret = devm_request_threaded_irq(dev, dlk->irq,
					demo_led_key_irq,
					demo_led_key_thread,
					IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
					dev_name(dev), dlk);
	if (ret) {
		dev_err(dev, "failed to request irq %d: %d\n", dlk->irq, ret);
		device_init_wakeup(dev, false);
		return ret;
	}

	platform_set_drvdata(pdev, dlk);

	dev_info(dev, "demo_led_key_int probed, irq=%d, debounce=%u ms\n",
		 dlk->irq, dlk->debounce_ms);
	return 0;
}

static int demo_led_key_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
	struct demo_led_key *dlk = platform_get_drvdata(pdev);

	if (dlk->wakeup_en)
		device_init_wakeup(dlk->dev, false);
    dev_info(dev, "remove the driver!\r\n");
	return 0;
}

/* system sleep: 保留为 wake 中断 */
#ifdef CONFIG_PM_SLEEP
static int demo_led_key_suspend(struct device *dev)
{
	struct demo_led_key *dlk = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(dlk->irq);

	return 0;
}

static int demo_led_key_resume(struct device *dev)
{
	struct demo_led_key *dlk = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(dlk->irq);

	return 0;
}

static const struct dev_pm_ops demo_led_key_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(demo_led_key_suspend, demo_led_key_resume)
};
#define DEMO_LED_KEY_PM_OPS   (&demo_led_key_pm_ops)
#else
#define DEMO_LED_KEY_PM_OPS   NULL
#endif

static const struct of_device_id demo_led_key_of_match[] = {
	{ .compatible = "nxp,imx6ull-led_key_int", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, demo_led_key_of_match);

static struct platform_driver demo_led_key_driver = {
	.probe  = demo_led_key_probe,
	.remove = demo_led_key_remove,
	.driver = {
		.name           = "demo_led_key_int",
		.of_match_table = demo_led_key_of_match,
		.pm             = DEMO_LED_KEY_PM_OPS,
	},
};
module_platform_driver(demo_led_key_driver);

MODULE_AUTHOR("leaf-example");
MODULE_DESCRIPTION("i.MX6ULL demo LED+KEY interrupt driver with debounce and wake");
MODULE_LICENSE("GPL");

```

##### 6)_查看唤醒源

能看唤醒源，不用靠猜 log。直接几招，挑你内核里有的用。

1. 看“最后一次是谁把我叫醒的”（很多 5.x/6.x 内核都有）：

   ```sh
   cat /sys/power/pm_wakeup_irq
   ```

   会给你一个 IRQ 号，比如 `46`。你再去：

   ```sh
   cat /proc/interrupts | grep 46
   ```

   就知道是哪个控制器/驱动的 IRQ 把它叫醒的。

2. 看“最近哪些唤醒源在计数”：

   ```sh
   cat /sys/kernel/debug/wakeup_sources
   ```

   里面有名字、active_count、event_count、last_change。你刚按完键，它对应的那一行 event_count 会 +1。
    没有这个文件就先：

   ```sh
   mount -t debugfs none /sys/kernel/debug
   ```

3. 看单个设备是不是被允许唤醒（确认你说的那个节点到底开没开）：

   ```sh
   cat /sys/devices/platform/demo_led_key_int/power/wakeup
   ```

   显示 `enabled` 就说明 suspend 时它被当成唤醒源；`disabled` 就不是。

4. 一次性看所有能唤醒的设备：

   ```sh
   find /sys/devices -path '*power/wakeup' -exec echo {} \; -exec cat {} \;
   ```

   哪个是 enabled，哪个在捣乱，一眼就能看出来。

5. 想看“PM 框架自己说是谁唤醒的”：
    开一下动态调试：

   ```sh
   echo 'file drivers/base/power/wakeup.c +p' > /sys/kernel/debug/dynamic_debug/control
   ```

   再睡再醒，dmesg 会加一句，直接告诉你哪个 wakeup source 触发的。

所以你不用把那几行 PM 日志当圣旨，你只要拿到 **IRQ号** 或 **wakeup_sources 里的名字**，就能反证到底是不是你这个 `demo_led_key_int@0` 把它叫醒的。

```shell
/mnt/nfs/driver # echo mem > /sys/power/state			# 配置linux kernel睡眠
[  157.780683] PM: suspend entry (deep)
[  157.784714] Filesystems sync: 0.000 seconds
[  157.791836] Freezing user space processes
[  157.796108] Freezing user space processes completed (elapsed 0.000 seconds)
[  157.803196] OOM killer disabled.
[  157.806565] Freezing remaining freezable tasks
[  157.812305] Freezing remaining freezable tasks completed (elapsed 0.001 seconds)
[  157.819918] printk: Suspending console(s) (use no_console_suspend to debug)
[  157.924278] fec 20b4000.ethernet eth0: Link is Down
[  157.925692] PM: suspend devices took 0.100 seconds
[  157.928986] Disabling non-boot CPUs ...
[  157.929009] Turn off M/F mix!
[  157.930635] demo_led_key_int led_key_int@0: into thread interrupt!
[  158.413882] usb 1-1: reset high-speed USB device number 2 using ci_hdrc
[  158.738486] PM: resume devices took 0.800 seconds
[  158.772767] OOM killer enabled.
[  158.775937] Restarting tasks ... done.
[  158.780923] random: crng reseeded on system resumption
[  158.788942] PM: suspend exit
/mnt/nfs/driver # [  160.164075] fec 20b4000.ethernet eth0: Link is Up - 100Mbps/Full - flow control rx/tx

/mnt/nfs/driver # cat /sys/power/pm_wakeup_irq				# 查看唤醒源
46
/mnt/nfs/driver #
```



#### (3)_唤醒用的_GPIO_必须在_sleep_状态下仍然保持为_能触发中断的输入

所以对于“要用来唤醒的按键、外部唤醒引脚、PMIC 中断引脚”，要做到：

1. 在 DTS 里给它单独写一个 sleep 状态；
2. 这个 sleep 状态必须仍然配置成输入、仍然允许中断/边沿检测、仍然保持正确的上下拉；
3. 该 GPIO 控制器在 suspend 时不要被整体关掉，或者它所在的电源域要标成“保持”。

示例思路（不写具体 DTS，只说语义）：

- active: 配成 GPIO 中断 + 上拉
- sleep: 还是 GPIO 中断 + 上拉（不要改成纯输入）
- pinctrl-0 / pinctrl-1 分别对应 active/sleep
- 在设备节点里把这两个状态都声明出来，内核才能在 suspend/resume 期间自动切换

#### (4)_触发类型与噪声

低功耗状态下，外部信号往往更“脏”：有抖动、有毛刺、有慢变沿。如果 sleep 配置里还是保持“边沿触发”，就有可能出现：

- 轻微抖动就唤醒 → 表现为“太敏感”
- 唤醒后没有实际事件可读 → 表现为“假唤醒”
   解决方式有两种：

1. 在 sleep 态改成电平触发，只要按键保持按下就能唤醒；
2. 在唤醒 ISR 里做一次软件确认（比如用 hrtimer 再读一次 GPIO）。

这两种都要看具体硬件能不能支持在低功耗里做这种配置。

#### (5)_和第9章的联系

第9章说的是“GPIO 中断要先在 GPIO 这一层消化类型，再往上交给 GIC”；
 本节要强调的是：**suspend 时也要在 GPIO 这一层保持“我还能感知到这个引脚的变化”**，否则就算上层 GIC 没被关，这条中断也起不来。

------

### 11.1.5_runtime_PM_场景下的中断可用性

#### (1)_runtime_PM_与系统_suspend_的区别

- 系统 suspend：整个系统要睡，PM 核心要统一管控所有设备的中断开关；
- runtime PM：只是“某个设备暂时没人用，先让它自己省电”，系统本身还是唤醒的。

所以在 runtime PM 下，“中断还要不要接收”是**设备自己要不要接收**的问题，而不是“系统允不允许”的问题。

#### (2)_runtime_suspend_时的中断策略

常见策略有两种：

1. 设备 runtime suspend 时，把本设备的中断关掉，等 runtime resume 时再开；
2. 设备 runtime suspend 时，保留能当作 wakeup 的那条中断，把其他中断关掉。

选择哪一种，取决于这个设备需不需要“用中断把自己叫醒”。典型场景是 UART：空闲时 runtime suspend，但 RX 线有数据来时要能把 UART 自己叫醒，这时就要保留那条 RX 中断。

#### (3)_与_wakeup_概念的配合

如果设备要在 runtime suspend 里靠中断把自己叫醒，就必须：

1. 调 `device_wakeup_enable()` / `device_init_wakeup()`；
2. 不要在 runtime suspend 回调里把这条中断 disable 掉；
3. 驱动逻辑要能在 ISR 里识别“这是我睡着时过来的唤醒事件”，然后调用 `pm_runtime_resume()`（或者排队到工作线程里去做）。

#### (4)_驱动里常见的坑

- 在 probe 里申请了中断，但在 runtime suspend 回调里直接 `disable_irq()`，结果 runtime resume 之后忘了 `enable_irq()`，看起来就像“睡了一次就不能再用”；
- runtime PM 和 system PM 同时启用，但两个路径里都去 enable/disable，同一条 IRQ 被重复操作，最后不确定到底是开还是关；
- 子设备被 runtime suspend 了，但它的中断挂在一个已经被 gate 掉的 GPIO 控制器上，这种情况必须把 GPIO 控制器放在“永远有电”或者“系统级 PM 管的域”里。

------

### 11.1.6_常见问题与排查清单

#### (1)_休眠后不能被_GPIO_唤醒

排查顺序：

1. 看 DTS：该设备是否声明了 `wakeup-source;`
2. 看驱动：是否 `device_init_wakeup(dev, true);`
3. 看 pinctrl：sleep 态是否仍为中断输入，而不是普通输入
4. 看 GPIO 控制器：suspend 时有没有被整体关掉
5. 看中断控制器（GIC）：这一条中断在 suspend 时是否被屏蔽

#### (2)_休眠后频繁被唤醒

排查顺序：

1. 确认触发类型：边沿在低功耗里容易被噪声触发
2. 确认外部上拉/下拉：睡眠态是否仍有稳定电平
3. 确认 PMIC / 其他外设是不是也挂在同一个中断线上
4. 用 trace 看谁先唤醒的，再回到对应 GPIO 去加软件消抖

#### (3)_能唤醒一次_以后不行

典型原因：

- 驱动的 resume 回调里没有把中断重新 enable
- 唤醒 ISR 里做了过度处理（比如直接 disable_irq_nosync），但 resume 后没恢复
- 唤醒线是单次触发的外部器件（比如只拉一次低），但驱动没有在 resume 里清状态，导致第二次不会再起边沿

#### (4)_runtime_suspend_正常_system_suspend_不正常

说明你的驱动只考虑了 runtime PM，没有把 system PM 的 suspend/resume 路径和中断开关做好。两条路径要分别看：

- runtime 回调：只管自己这个设备
- system 回调：要考虑 GPIO 控制器、电源域、唤醒源

#### (5)_最小_checklist

- DTS：有无 wakeup-source
- 驱动：有无 device_init_wakeup
- pinctrl：sleep 态是否保留中断功能
- 控制器：suspend 时是否关电/关时钟
- resume：是否重新 enable / 重新配置触发类型

（第11章完）