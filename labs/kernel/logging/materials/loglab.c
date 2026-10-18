// SPDX-License-Identifier: GPL-2.0-only
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kstrtox.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/printk.h>

static struct device *loglab_dev;
static DEFINE_MUTEX(loglab_lock);

static int trigger_set(const char *value, const struct kernel_param *parameter)
{
    static const u8 sample[] = {0xde, 0xad, 0xbe, 0xef, 0, 1, 2, 3};
    unsigned int count, index;
    int result;

    /* 只接受有限次数，避免把教学触发器变成无限刷屏入口。 */
    (void)parameter;
    result = kstrtouint(value, 0, &count);
    if (result)
        return result;
    if (count < 1 || count > 32)
        return -EINVAL;

    /* 串行化演示，并与退出时的设备撤销互斥。 */
    mutex_lock(&loglab_lock);
    if (!loglab_dev) {
        result = -ENODEV;
        goto out;
    }
    pr_info("trigger count=%u\n", count);
    printk(KERN_DEBUG "loglab: direct debug\n");
    dev_info(loglab_dev, "device trigger\n");
    for (index = 0; index < count; ++index) {
        pr_debug("debug index=%u\n", index);
        dev_dbg(loglab_dev, "device debug index=%u\n", index);
        pr_warn_once("once at this callsite\n");
        pr_info_ratelimited("limited index=%u\n", index);
    }
    /* 直接转储不由动态调试开关控制，仍带 DEBUG 级别。 */
    print_hex_dump(KERN_DEBUG, "loglab dump: ", DUMP_PREFIX_OFFSET,
                   16, 1, sample, sizeof(sample), true);
    result = 0;
out:
    mutex_unlock(&loglab_lock);
    return result;
}

static const struct kernel_param_ops trigger_ops = {
    .set = trigger_set,
};
module_param_cb(trigger, &trigger_ops, NULL, 0200);
MODULE_PARM_DESC(trigger, "写入1到32触发日志，模块加载后使用");

static int __init loglab_init(void)
{
    struct device *device;

    /* 根设备仅承载日志身份，不申请硬件，也不创建字符设备接口。 */
    device = root_device_register("loglab");
    if (IS_ERR(device))
        return PTR_ERR(device);
    mutex_lock(&loglab_lock);
    loglab_dev = device;
    mutex_unlock(&loglab_lock);
    pr_info("ready; write the trigger parameter\n");
    return 0;
}

static void __exit loglab_exit(void)
{
    mutex_lock(&loglab_lock);
    root_device_unregister(loglab_dev);
    loglab_dev = NULL;
    mutex_unlock(&loglab_lock);
    pr_info("exit\n");
}

module_init(loglab_init);
module_exit(loglab_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("可重复触发的内核日志教学模块");
