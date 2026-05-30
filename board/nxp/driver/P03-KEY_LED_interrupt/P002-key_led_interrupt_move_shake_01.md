# 实验目的：

通过key的中断方式来更改led的输出状态，并且实现key的消抖处理。

* 消抖处理：采用定时器启动工作队列来完成。
  * 软中断中仅仅记录按键的状态，然后紧接着中断线失能。
  * 软中断中启动定时器来延迟 x debounce ms 的时间再执行状态转换。
* 当定时器任务执行后重新使能该中断线，调度工作队列，启动led的转换。



# 电路图：

key，led：

![image](./../../../../images/driver/board/imx6ull/led_key_etc.png)



# 设备树：

```dts
	demo_led_key_int: led_key_int@0 {
		compatible = "nxp,imx6ull-led_key_int"; 	// 与驱动匹配的compatible属性
		pinctrl-names = "default";
		pinctrl-0 = <&pinctrl_led_key_int>;

		led-gpios = <&gpio1 3 	GPIO_ACTIVE_LOW>;	// 引用GPIO1_IO03，低电平点亮，默认关闭状态
		key-gpios = <&gpio1 18 	GPIO_ACTIVE_LOW>;	// UART1_CTS_B → GPIO1_IO18

		interrupt-parent = <&gpio1>;
		interrupts = <18 IRQ_TYPE_EDGE_FALLING>;

		nxp,debounce-ms = <30>;	/* 软件去抖动 */
		status = "okay";
	};
	
&iomuxc {
	...
	pinctrl_led_key_int: ledkey_int{
		fsl,pins = <
			MX6UL_PAD_GPIO1_IO03__GPIO1_IO03        0x10B0
            MX6UL_PAD_UART1_CTS_B__GPIO1_IO18       0x1B0B0
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>

#define DRV_NAME "imx6ull-key-hrtimer-gate"

struct keydev {
    struct device    *dev;
    struct gpio_desc *led;
    struct gpio_desc *key;
    int               irq;

    /* 去抖窗口（ms） */
    unsigned int debounce_ms;

    /* 原子状态 */
    atomic_t latched; /* 已锁存一次事件 */
    atomic_t armed;   /* 已关线并挂了定时器 */

    /* work：真正消费 */
    struct work_struct work;

    /* hrtimer：到点开线 */
    struct hrtimer timer;
};

static void
key_work(struct work_struct *w)
{
    struct keydev *kd = container_of(w, struct keydev, work);

    /* 进程上下文：可睡、可打印、可访问子系统 */
    /* 这里示例为翻转 ACTIVE_LOW LED（可用 _cansleep 版本更保险） */
    static bool on;
    on = !on;
    gpiod_set_value_cansleep(kd->led, on ? 0 : 1);
    dev_info(kd->dev, "work: consume one key event, LED=%d\n", on);
}

/* hrtimer 回调：开线 + 拉起一次消费 */
static enum hrtimer_restart
key_timer_fn(struct hrtimer *t)
{
    struct keydev *kd = container_of(t, struct keydev, timer);

    /* 允许该 IRQ 再次进入 */
    enable_irq(kd->irq);
    atomic_set(&kd->armed, 0);

    /* 窗口内可能来了多次边沿，但我们只消费一次 */
    if (atomic_xchg(&kd->latched, 0))
        schedule_work(&kd->work);

    return HRTIMER_NORESTART;
}

/* 硬中断：第一次边沿即“关线 + 启动定时器 + 锁存一次事件” */
static irqreturn_t
key_isr(int irq, void *data)
{
    struct keydev *kd = data;

    /* 锁存一次事件（无论窗口内是否重复触发） */
    atomic_set(&kd->latched, 1);

    /* 原子设置 armed：只在未 armed→armed=1 的瞬间执行关线与启动定时器 */
    if (!atomic_xchg(&kd->armed, 1)) {
        /* 关键：中断上下文要用 _nosync 变体，避免死等自身 */
        disable_irq_nosync(irq);

        /* 启动一次性 hrtimer（相对时间） */
        hrtimer_start(&kd->timer,
                      ms_to_ktime(kd->debounce_ms),
                      HRTIMER_MODE_REL);
    }
    return IRQ_HANDLED;
}

static int
key_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct keydev *kd;
    u32            ms = 30;
    int            ret;

    kd = devm_kzalloc(dev, sizeof(*kd), GFP_KERNEL);
    if (!kd) return -ENOMEM;
    kd->dev = dev;

    /* 资源 */
    kd->led = devm_gpiod_get(dev, "led", GPIOD_OUT_HIGH); /* ACTIVE_LOW: 高=灭 */
    if (IS_ERR(kd->led))
        return dev_err_probe(dev, PTR_ERR(kd->led), "led-gpios\n");

    kd->key = devm_gpiod_get(dev, "key", GPIOD_IN);
    if (IS_ERR(kd->key))
        return dev_err_probe(dev, PTR_ERR(kd->key), "key-gpios\n");

    /* IRQ：DTS 优先，失败再从 key 推导 */
    kd->irq = platform_get_irq_optional(pdev, 0);
    if (kd->irq < 0) {
        if (kd->irq != -ENXIO) return kd->irq;
        kd->irq = gpiod_to_irq(kd->key);
        if (kd->irq < 0) return kd->irq;
    }
    irq_set_irq_type(kd->irq, IRQ_TYPE_EDGE_FALLING);

    /* 去抖窗口 */
    of_property_read_u32(dev->of_node, "nxp,debounce-ms", &ms);
    kd->debounce_ms = ms;

    /* 初始化状态与定时器、work */
    atomic_set(&kd->latched, 0);
    atomic_set(&kd->armed, 0);

    INIT_WORK(&kd->work, key_work);
    hrtimer_init(&kd->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    kd->timer.function = key_timer_fn;

    /* 申请硬中断（只用 primary handler） */
    ret = devm_request_irq(dev, kd->irq, key_isr,
                           IRQF_TRIGGER_FALLING | IRQF_NO_THREAD,
                           dev_name(dev), kd);
    if (ret)
        return dev_err_probe(dev, ret, "request_irq\n");

    platform_set_drvdata(pdev, kd);
    dev_info(dev, "ready: irq=%d, debounce=%ums (hrtimer gate)\n",
             kd->irq, kd->debounce_ms);
    return 0;
}

static int
key_remove(struct platform_device *pdev)
{
    struct keydev *kd = platform_get_drvdata(pdev);

    /* 收尾：防止定时器/工作仍未结束 */
    hrtimer_cancel(&kd->timer);
    cancel_work_sync(&kd->work);
    return 0;
}

static const struct of_device_id key_of_match[] = {
    { .compatible = "nxp,imx6ull-led_key_int" },
    {}
};
MODULE_DEVICE_TABLE(of, key_of_match);

static struct platform_driver key_drv = {
    .probe  = key_probe,
    .remove = key_remove,
    .driver = {
               .name           = DRV_NAME,
               .of_match_table = key_of_match,
               },
};
module_platform_driver(key_drv);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leaf & ChatGPT");
MODULE_DESCRIPTION("GPIO key: disable_irq_nosync + hrtimer debounce + enable_irq once");
```



## log：

```shell
~ # cd /mnt/nfs/driver/
/mnt/nfs/driver # ls
key_led_int.ko
/mnt/nfs/driver # insmod key_led_int.ko
[   23.370951] key_led_int: loading out-of-tree module taints kernel.
[   23.378828] imx6ull-key-hrtimer-gate led_key_int@0: ready: irq=46, debounce=30ms (hrtimer gate)
/mnt/nfs/driver # [   26.147301] imx6ull-key-hrtimer-gate led_key_int@0: work: consume one key event, LED=1
[   27.438520] imx6ull-key-hrtimer-gate led_key_int@0: work: consume one key event, LED=0
[   27.921904] imx6ull-key-hrtimer-gate led_key_int@0: work: consume one key event, LED=1
[   28.354121] imx6ull-key-hrtimer-gate led_key_int@0: work: consume one key event, LED=0
[   28.750926] imx6ull-key-hrtimer-gate led_key_int@0: work: consume one key event, LED=1
[   29.029896] imx6ull-key-hrtimer-gate led_key_int@0: work: consume one key event, LED=0
```
