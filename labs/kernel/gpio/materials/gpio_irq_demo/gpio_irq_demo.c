// SPDX-License-Identifier: GPL-2.0
/* 只连接教学gpio-sim，观察输入IRQ；不控制真实外设。 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct gpio_irq_state {
    struct device *dev;
    struct gpio_desc *alert;
};

static bool fail_after_irq;
module_param(fail_after_irq, bool, 0444);
MODULE_PARM_DESC(fail_after_irq, "申请IRQ后故意让probe失败");

static struct platform_device *demo_device;
static struct gpiod_lookup_table demo_lookup = {
    .dev_id = "gpio_irq_demo",
    .table = {
        GPIO_LOOKUP("linux-note-consumer-sim", 2, "alert", GPIO_ACTIVE_LOW),
        { }
    }
};

static irqreturn_t gpio_irq_thread(int irq, void *data)
{
    struct gpio_irq_state *state = data;
    int value = gpiod_get_value_cansleep(state->alert);

    /* 这里是线程上下文；采到的是当前逻辑值，不是边沿历史。 */
    if (value < 0)
        dev_err_ratelimited(state->dev, "alert read failed: %d\n", value);
    else
        dev_info(state->dev, "irq=%d alert=%d (logical sample)\n", irq, value);
    /* 返回处理过本次通知，不把读取失败伪装成有效电平。 */
    return IRQ_HANDLED;
}

static int gpio_irq_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct gpio_irq_state *state;
    int irq, ret;

    state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
    if (!state)
        return -ENOMEM;
    state->dev = dev;
    state->alert = devm_gpiod_get(dev, "alert", GPIOD_IN);
    if (IS_ERR(state->alert))
        return dev_err_probe(dev, PTR_ERR(state->alert), "alert request failed\n");

    irq = gpiod_to_irq(state->alert);
    if (irq < 0)
        return dev_err_probe(dev, irq, "alert mapping failed\n");

    /* 处理者可能在申请返回前执行，依赖的state和输入必须先就绪。 */
    ret = devm_request_threaded_irq(dev, irq, NULL, gpio_irq_thread,
                                  IRQF_ONESHOT | IRQF_TRIGGER_RISING |
                                  IRQF_TRIGGER_FALLING, dev_name(dev), state);
    if (ret)
        return dev_err_probe(dev, ret, "alert IRQ request failed\n");
    if (fail_after_irq)
        return dev_err_probe(dev, -EIO, "injected failure after IRQ request\n");

    dev_info(dev, "ready: irq=%d, both physical edges\n", irq);
    return 0;
}

static struct platform_driver demo_driver = {
    .probe = gpio_irq_probe,
    .driver = { .name = "gpio_irq_demo" },
};

static int __init gpio_irq_init(void)
{
    int ret;

    gpiod_add_lookup_table(&demo_lookup);
    ret = platform_driver_register(&demo_driver);
    if (ret)
        goto remove_lookup;
    demo_device = platform_device_register_simple("gpio_irq_demo", -1, NULL, 0);
    if (IS_ERR(demo_device)) {
        ret = PTR_ERR(demo_device);
        platform_driver_unregister(&demo_driver);
        goto remove_lookup;
    }
    return 0;
remove_lookup:
    gpiod_remove_lookup_table(&demo_lookup);
    return ret;
}

static void __exit gpio_irq_exit(void)
{
    /* devres先释放并等IRQ处理结束，再放描述符，最后释放state。 */
    platform_device_unregister(demo_device);
    platform_driver_unregister(&demo_driver);
    gpiod_remove_lookup_table(&demo_lookup);
}

module_init(gpio_irq_init);
module_exit(gpio_irq_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO IRQ lifecycle lesson using gpio-sim only");
