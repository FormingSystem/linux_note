// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>

/* 0 为成功，1 为可选对象缺席，2 为提供者失败，3 为后续初始化失败。 */
static unsigned int scenario;
module_param(scenario, uint, 0444);
MODULE_PARM_DESC(scenario, "0=present 1=absent 2=provider_error 3=late_error");

struct note_state {
    int number;
};

static struct platform_device *note_device;
static const int optional_object = 7;

static const int *note_get_optional(void)
{
    if (scenario == 1)
        return NULL;
    if (scenario == 2)
        return ERR_PTR(-EBUSY);
    return &optional_object;
}

static void note_release(void *data)
{
    struct note_state *state = data;

    /* 此回调只作观察；其后的 devm 记录才释放 state 内存。 */
    pr_info("error_pointer_demo: release marker=%d\n", state->number);
}

static int note_probe(struct platform_device *pdev)
{
    struct note_state *first;
    struct note_state *second;
    const int *optional;
    int ret;

    first = devm_kzalloc(&pdev->dev, sizeof(*first), GFP_KERNEL);
    if (!first)
        return -ENOMEM;
    first->number = 1;
    ret = devm_add_action_or_reset(&pdev->dev, note_release, first);
    if (ret)
        return ret;

    optional = note_get_optional();
    if (IS_ERR(optional)) {
        dev_info(&pdev->dev, "provider error=%ld name=%pe\n",
                 PTR_ERR(optional), optional);
        return PTR_ERR(optional);
    }
    if (optional)
        dev_info(&pdev->dev, "optional value=%d\n", *optional);
    else
        dev_info(&pdev->dev, "optional absent; continue initialization\n");

    second = devm_kzalloc(&pdev->dev, sizeof(*second), GFP_KERNEL);
    if (!second)
        return -ENOMEM;
    second->number = 2;
    ret = devm_add_action_or_reset(&pdev->dev, note_release, second);
    if (ret)
        return ret;

    if (scenario == 3) {
        dev_info(&pdev->dev, "late error=%d\n", -EINVAL);
        return -EINVAL;
    }
    dev_info(&pdev->dev, "probe completed\n");
    return 0;
}

static struct platform_driver note_driver = {
    .probe = note_probe,
    .driver = {
        .name = "error_pointer_demo",
        .probe_type = PROBE_FORCE_SYNCHRONOUS, /* 便于按注册时序观察。 */
    },
};

static int __init note_init(void)
{
    int ret;

    if (scenario > 3)
        return -EINVAL;
    ret = platform_driver_register(&note_driver);
    if (ret)
        return ret;

    note_device = platform_device_register_simple("error_pointer_demo", -1,
                                                   NULL, 0);
    if (IS_ERR(note_device)) {
        ret = PTR_ERR(note_device);
        platform_driver_unregister(&note_driver);
        return ret;
    }
    /* 设备注册成功与它的 probe 是否成功是不同的结果。 */
    pr_info("error_pointer_demo: registration completed scenario=%u\n", scenario);
    return 0;
}

static void __exit note_exit(void)
{
    platform_device_unregister(note_device);
    platform_driver_unregister(&note_driver);
    pr_info("error_pointer_demo: exit completed\n");
}

module_init(note_init);
module_exit(note_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Error value and managed cleanup teaching example");
