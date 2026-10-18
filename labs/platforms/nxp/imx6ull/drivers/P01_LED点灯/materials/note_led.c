// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "note_led_protocol.h"

struct note_led_state {
    struct kref ref;
    struct mutex gate;
    bool closing;
    unsigned int nr_leds;
    struct gpio_descs *gpios; /* 只在gate内、closing为false时供业务使用。 */
};
static DEFINE_MUTEX(entry_lock);
static struct note_led_state *led_entry; /* 本实验只支持一个绑定实例。 */

static void note_led_free(struct kref *ref)
{
    struct note_led_state *state = container_of(ref, struct note_led_state, ref);
    kfree(state); /* 描述符由devm随解绑释放，这里不再操作硬件。 */
}

static void note_led_put(struct note_led_state *state)
{
    kref_put(&state->ref, note_led_free);
}

static int note_led_open(struct inode *inode, struct file *file)
{
    struct note_led_state *state;
    int result;

    (void)inode;
    file->private_data = NULL; /* 替换misc核心放入的静态miscdevice地址。 */
    mutex_lock(&entry_lock);
    state = led_entry;
    if (state)
        kref_get(&state->ref); /* 入口锁内仍有绑定份额，不能从悬空指针加引用。 */
    mutex_unlock(&entry_lock);
    if (!state)
        return -ENODEV;
    mutex_lock(&state->gate);
    result = state->closing ? -ENODEV : 0;
    mutex_unlock(&state->gate);
    if (result) {
        note_led_put(state); /* 失败open不会得到release回调，必须自行回滚。 */
        return result;
    }
    file->private_data = state;
    return nonseekable_open(inode, file);
}

static int note_led_release(struct inode *inode, struct file *file)
{
    struct note_led_state *state = file->private_data;
    (void)inode;
    file->private_data = NULL;
    note_led_put(state);
    return 0;
}

static ssize_t note_led_write(struct file *file, const char __user *buf,
                              size_t count, loff_t *position)
{
    struct note_led_state *state = file->private_data;
    char command[NOTE_LED_COMMAND_MAX];
    unsigned int index = 0;
    int on = 0;
    int result;

    (void)position;
    if (!count)
        return 0; /* 空写入不触碰用户地址，也不操作GPIO。 */
    if (count > sizeof(command))
        return -E2BIG;
    if (copy_from_user(command, buf, count))
        return -EFAULT;
    result = note_led_parse(command, count, &index, &on);
    if (result < 0)
        return result;
    mutex_lock(&state->gate);
    if (state->closing)
        result = -ENODEV;
    else if (index >= state->nr_leds)
        result = -EINVAL;
    else {
        /* 描述符处理有效极性；这里绝不再对active-low取反。 */
        gpiod_set_value_cansleep(state->gpios->desc[index], on);
        result = (int)count;
    }
    mutex_unlock(&state->gate);
    return result;
}

static const struct file_operations note_led_fops = {
    .owner = THIS_MODULE,
    .open = note_led_open,
    .release = note_led_release,
    .write = note_led_write,
};

static struct miscdevice note_led_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "note_led",
    .fops = &note_led_fops,
    .mode = 0600,
};

static int note_led_probe(struct platform_device *pdev)
{
    struct note_led_state *state;
    int count;
    int result = 0;

    state = kzalloc(sizeof(*state), GFP_KERNEL);
    if (!state)
        return -ENOMEM;
    kref_init(&state->ref); /* 绑定管理者的初始份额。 */
    mutex_init(&state->gate);
    mutex_lock(&entry_lock);
    if (led_entry) {
        result = -EBUSY;
        goto fail;
    }
    count = gpiod_count(&pdev->dev, "led");
    if (count <= 0 || count > (int)NOTE_LED_LIMIT) {
        result = count < 0 ? count : -EINVAL;
        goto fail;
    }
    state->gpios = devm_gpiod_get_array(&pdev->dev, "led", GPIOD_OUT_LOW);
    if (IS_ERR(state->gpios)) {
        result = PTR_ERR(state->gpios);
        goto fail;
    }
    state->nr_leds = state->gpios->ndescs;
    if (!state->nr_leds || state->nr_leds > NOTE_LED_LIMIT) {
        result = -EINVAL;
        goto fail;
    }
    platform_set_drvdata(pdev, state);
    led_entry = state; /* 硬件准备成功后才发布，可见性由入口锁建立。 */
    mutex_unlock(&entry_lock);
    dev_info(&pdev->dev, "ready: %u logical LEDs\n", state->nr_leds);
    return 0;
fail:
    mutex_unlock(&entry_lock);
    note_led_put(state);
    return dev_err_probe(&pdev->dev, result, "LED resources unavailable\n");
}

/* 调用方持有gate；在devm释放前关闭业务，并使全部LED逻辑关闭。 */
static void note_led_stop(struct note_led_state *state)
{
    unsigned int i;
    if (state->closing)
        return;
    state->closing = true;
    for (i = 0; i < state->nr_leds; ++i)
        gpiod_set_value_cansleep(state->gpios->desc[i], 0);
    state->gpios = NULL; /* 留给旧文件的对象不再保存可用硬件入口。 */
}

static void note_led_remove(struct platform_device *pdev)
{
    struct note_led_state *state = platform_get_drvdata(pdev);
    mutex_lock(&entry_lock);
    led_entry = NULL;
    mutex_lock(&state->gate);
    note_led_stop(state);
    mutex_unlock(&state->gate);
    mutex_unlock(&entry_lock);
    platform_set_drvdata(pdev, NULL);
    note_led_put(state); /* 交还绑定份额；旧文件可能继续保留对象。 */
}

static void note_led_shutdown(struct platform_device *pdev)
{
    struct note_led_state *state = platform_get_drvdata(pdev);
    mutex_lock(&state->gate);
    note_led_stop(state); /* shutdown不是remove，不提前归还绑定份额。 */
    mutex_unlock(&state->gate);
}

static const struct of_device_id note_led_match[] = {
    { .compatible = "linux-note,led-lab" },
    { }
};
MODULE_DEVICE_TABLE(of, note_led_match);

static struct platform_driver note_led_driver = {
    .probe = note_led_probe,
    .remove = note_led_remove,
    .shutdown = note_led_shutdown,
    .driver = {
        .name = "note_led",
        .of_match_table = note_led_match,
    },
};

static int __init note_led_init(void)
{
    int result = platform_driver_register(&note_led_driver);
    if (result)
        return result;
    result = misc_register(&note_led_device);
    if (result)
        platform_driver_unregister(&note_led_driver);
    return result;
}

static void __exit note_led_exit(void)
{
    misc_deregister(&note_led_device);
    platform_driver_unregister(&note_led_driver);
}
module_init(note_led_init);
module_exit(note_led_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO描述符与解绑后文件生命周期实验");
