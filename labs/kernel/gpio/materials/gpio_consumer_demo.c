// SPDX-License-Identifier: GPL-2.0
/* 仅连接本章创建的 gpio-sim；不用于真实设备的供电或复位。 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct gpio_demo_state {
    struct device *dev;
    struct gpio_desc *enable;
    struct gpio_desc *reset;
    struct gpio_desc *alert;
};

static bool fail_after_start;
module_param(fail_after_start, bool, 0444);
MODULE_PARM_DESC(fail_after_start, "启动后注入probe失败，检查托管停机");

static struct platform_device *demo_device;

/* 测试连接表：key是唯一的chip label，不是动态gpiochipN。 */
static struct gpiod_lookup_table demo_lookup = {
    .dev_id = "gpio_consumer_demo",
    .table = {
        GPIO_LOOKUP("linux-note-consumer-sim", 0, "enable", GPIO_ACTIVE_HIGH),
        GPIO_LOOKUP("linux-note-consumer-sim", 1, "reset", GPIO_ACTIVE_LOW),
        GPIO_LOOKUP("linux-note-consumer-sim", 2, "alert", GPIO_ACTIVE_LOW),
        { }
    }
};

static void gpio_demo_stop(void *data)
{
    struct gpio_demo_state *state = data;

    /* 没有异步使用者；按教学协议先断言复位，再关闭使能。 */
    gpiod_set_value_cansleep(state->reset, 1);
    gpiod_set_value_cansleep(state->enable, 0);
    dev_info(state->dev, "stop requested: reset=1 enable=0 (logical)\n");
}

static int gpio_demo_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct gpio_demo_state *state;
    int ret;

    state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
    if (!state)
        return -ENOMEM;
    state->dev = dev;

    state->enable = devm_gpiod_get(dev, "enable", GPIOD_OUT_LOW);
    if (IS_ERR(state->enable))
        return dev_err_probe(dev, PTR_ERR(state->enable), "enable request failed\n");
    state->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(state->reset))
        return dev_err_probe(dev, PTR_ERR(state->reset), "reset request failed\n");
    state->alert = devm_gpiod_get(dev, "alert", GPIOD_IN);
    if (IS_ERR(state->alert))
        return dev_err_probe(dev, PTR_ERR(state->alert), "alert request failed\n");

    /* 在全部描述符之后登记，资源清理时先停机，再释放这些描述符。 */
    ret = devm_add_action_or_reset(dev, gpio_demo_stop, state);
    if (ret)
        return ret;

    gpiod_set_value_cansleep(state->enable, 1);
    /* 1ms仅是教学时序，不是任何真实传感器的参数。 */
    usleep_range(1000, 1500);
    gpiod_set_value_cansleep(state->reset, 0);
    ret = gpiod_get_value_cansleep(state->alert);
    if (ret < 0)
        return dev_err_probe(dev, ret, "alert read failed\n");
    dev_info(dev, "started: alert=%d (logical)\n", ret);

    if (fail_after_start)
        return dev_err_probe(dev, -EIO, "injected failure after start\n");
    platform_set_drvdata(pdev, state);
    return 0;
}

static void gpio_demo_shutdown(struct platform_device *pdev)
{
    struct gpio_demo_state *state = platform_get_drvdata(pdev);

    /* 系统关机不依赖devres解绑；动作在本例中可重复执行。 */
    if (state)
        gpio_demo_stop(state);
}

static struct platform_driver demo_driver = {
    .probe = gpio_demo_probe,
    .shutdown = gpio_demo_shutdown,
    .driver = {
        .name = "gpio_consumer_demo",
    },
};

static int __init gpio_demo_init(void)
{
    int ret;

    /* 本实验自己枚举模拟消费者；真实设备由对应总线/固件枚举。 */
    gpiod_add_lookup_table(&demo_lookup);
    ret = platform_driver_register(&demo_driver);
    if (ret)
        goto remove_lookup;
    demo_device = platform_device_register_simple("gpio_consumer_demo", -1, NULL, 0);
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

static void __exit gpio_demo_exit(void)
{
    /* 先撤销消费者并完成devres清理，最后撤销它使用的连接表。 */
    platform_device_unregister(demo_device);
    platform_driver_unregister(&demo_driver);
    gpiod_remove_lookup_table(&demo_lookup);
}

module_init(gpio_demo_init);
module_exit(gpio_demo_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO consumer lifecycle lesson using gpio-sim only");
