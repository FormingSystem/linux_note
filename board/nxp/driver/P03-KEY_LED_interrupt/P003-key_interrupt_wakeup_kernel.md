# 实验目的：

通过key的中断方式来更改led的输出状态，并且实现key的唤醒实验。

* 消抖处理：采用定时器启动工作队列来完成。
  * 软中断中仅仅记录按键的状态，然后紧接着中断线失能。
  * 软中断中启动定时器来延迟 x debounce ms 的时间再执行状态转换。
* 当定时器任务执行后重新使能该中断线，调度工作队列，启动led的转换。
* 当配置kernel睡下去之后，通过按键将系统重新唤醒。



# 电路图：

key，led：

![image](./../../../../images/driver/board/imx6ull/led_key_etc.png)



# 设备树：

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
	
&iomuxc {
	...
	pinctrl_led_key_int_active: led_key_int-active {
		fsl,pins = <
			/* LED: GPIO1_IO03 → GPIO1,3，输出，默认拉高灭灯 */
			MX6UL_PAD_GPIO1_IO03__GPIO1_IO03  0x000110B0

			/* KEY: UART1_CTS_B → GPIO1_IO18，做成中断输入并上拉，防抖交给驱动 */
			MX6UL_PAD_UART1_CTS_B__GPIO1_IO18 0x000110B0
		>;
	};
	pinctrl_led_key_int_sleep: led_key_int-sleep {
		fsl,pins = <
			/* LED：可以保持原样，或者改成纯 GPIO 输出高，避免休眠乱闪 */
			MX6UL_PAD_GPIO1_IO03__GPIO1_IO03  0x000110B0

			/* KEY：关键是这行，休眠态仍然保持成 GPIO 输入 + 上拉，不能换成别的功能 */
			MX6UL_PAD_UART1_CTS_B__GPIO1_IO18 0x000110B0
		>;
	};
	...
};
```



# 驱动代码

## makefile

```makefile
tartget_p := key_led_int
# 编译目标：$(tartget_p).c -> $(tartget_p).ko
obj-m := $(tartget_p).o

# 内核源码路径（改成你自己的）
KDIR := /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
PWD  := $(shell pwd)

# 默认目标：交叉编译 ARM 模块
ARCH ?= arm
CROSS_COMPILE ?= arm-none-linux-gnueabihf-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

install:
	make
	cp $(tartget_p).ko ~/nfs/driver/
```



## C源码

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
					IRQF_TRIGGER_FALLING,
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



## log：

```shell
/mnt/nfs/driver # insmod key_led_int.ko				   # 加载驱动
[  729.886417] demo_led_key_int led_key_int@0: demo_led_key_int probed, irq=46, debounce=30 ms
/mnt/nfs/driver # [  731.869970] demo_led_key_int led_key_int@0: into thread interrupt!
[  732.062045] demo_led_key_int led_key_int@0: into thread interrupt!
[  732.226923] demo_led_key_int led_key_int@0: into thread interrupt!
[  732.385781] demo_led_key_int led_key_int@0: into thread interrupt!
/mnt/nfs/driver # echo mem > /sys/power/state			# 让 kernel 进入睡眠
[  734.627317] PM: suspend entry (deep)
[  734.631207] Filesystems sync: 0.000 seconds
[  734.638483] Freezing user space processes
[  734.642621] Freezing user space processes completed (elapsed 0.000 seconds)
[  734.649741] OOM killer disabled.
[  734.652986] Freezing remaining freezable tasks
[  734.658766] Freezing remaining freezable tasks completed (elapsed 0.001 seconds)
[  734.666248] printk: Suspending console(s) (use no_console_suspend to debug)
[  734.773959] fec 20b4000.ethernet eth0: Link is Down
[  734.775367] PM: suspend devices took 0.110 seconds
[  734.778654] Disabling non-boot CPUs ...
[  734.778676] Turn off M/F mix!
[  734.780306] demo_led_key_int led_key_int@0: into thread interrupt!
[  734.880901] demo_led_key_int led_key_int@0: into thread interrupt!
[  735.263782] usb 1-1: reset high-speed USB device number 2 using ci_hdrc
[  735.588366] PM: resume devices took 0.800 seconds
[  735.628930] OOM killer enabled.
[  735.632081] Restarting tasks ... done.
[  735.637134] random: crng reseeded on system resumption
[  735.645137] PM: suspend exit
/mnt/nfs/driver # rmmod key_led_int.ko [  737.043949] fec 20b4000.ethernet eth0: Link is Up - 100Mbps/Full - flow control rx/tx
/mnt/nfs/driver # cat /sys/power/pm_wakeup_irq				# 查看是谁进行的唤醒操作
46
/mnt/nfs/driver #
```

## 注意事项：

* 不要对 devm_request_threaded_irq() 接口采用 irqflag: IRQF_NO_SUSPEND。虽然会有唤醒功能，但是查询的时候会找不到。
* 让kernel睡眠可以使用：`echo mem > /sys/power/state` shell 命令。
* 查看是谁唤醒的采用：`cat /sys/power/pm_wakeup_irq` shell 命令。