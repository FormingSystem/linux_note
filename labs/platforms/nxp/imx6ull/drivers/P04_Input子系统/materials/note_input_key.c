// SPDX-License-Identifier: GPL-2.0
/* 运行态实验：双边沿更新安静期限，将确认后的逻辑按键交给Input。 */
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

struct note_input_key {
    struct device *dev;
    struct gpio_desc *key;
    struct input_dev *input;
    unsigned int keycode;
    struct delayed_work sample_work;
    spinlock_t state_lock;
    u64 generation;
    u64 quiet_until_ns;
    unsigned int debounce_ms;
    int irq;
    bool stopping;
    bool stable_pressed;
};

static void note_key_sample(struct work_struct *work)
{
    struct note_input_key *state = container_of(to_delayed_work(work),
                                              struct note_input_key, sample_work);
    unsigned long flags;
    u64 generation;
    int level;
    bool changed = false;

    spin_lock_irqsave(&state->state_lock, flags);
    if (state->stopping)
        goto unlock_first;
    if (ktime_get_ns() < state->quiet_until_ns) {
        /* jiffies定时负责安排执行，单调时钟才核对实际安静期限。 */
        mod_delayed_work(system_wq, &state->sample_work, 1);
        goto unlock_first;
    }
    generation = state->generation;
    spin_unlock_irqrestore(&state->state_lock, flags);

    level = gpiod_get_value_cansleep(state->key); /* 自旋锁外，允许提供者睡眠。 */

    spin_lock_irqsave(&state->state_lock, flags);
    if (state->stopping)
        goto unlock_second;
    if (generation != state->generation)
        goto unlock_second; /* 新IRQ已排好下一次工作，丢弃这次过时采样。 */
    if (level < 0) {
        mod_delayed_work(system_wq, &state->sample_work,
                         msecs_to_jiffies(state->debounce_ms));
        spin_unlock_irqrestore(&state->state_lock, flags);
        dev_warn_ratelimited(state->dev, "GPIO read failed: %d\n", level);
        return;
    }
    if (!!level != state->stable_pressed) {
        state->stable_pressed = !!level;
        changed = true;
    }
    spin_unlock_irqrestore(&state->state_lock, flags);

    /* 同一工作项不并发执行；输入注销在停止动作之后，报告期间对象仍有效。 */
    if (changed) {
        input_report_key(state->input, state->keycode, !!level);
        input_sync(state->input);
    }
    return;

unlock_second:
    spin_unlock_irqrestore(&state->state_lock, flags);
    return;
unlock_first:
    spin_unlock_irqrestore(&state->state_lock, flags);
}

static irqreturn_t note_key_irq(int irq, void *data)
{
    struct note_input_key *state = data;
    unsigned long flags;
    (void)irq;

    spin_lock_irqsave(&state->state_lock, flags);
    if (!state->stopping) {
        ++state->generation;
        state->quiet_until_ns = ktime_get_ns() +
                               (u64)state->debounce_ms * NSEC_PER_MSEC;
        mod_delayed_work(system_wq, &state->sample_work,
                         msecs_to_jiffies(state->debounce_ms));
    }
    spin_unlock_irqrestore(&state->state_lock, flags);
    return IRQ_HANDLED;
}

static void note_key_stop(void *data)
{
    struct note_input_key *state = data;
    unsigned long flags;

    /* 此动作早于IRQ资源登记，因此正常devres逆序回收时IRQ已先释放并排空。 */
    spin_lock_irqsave(&state->state_lock, flags);
    state->stopping = true;
    spin_unlock_irqrestore(&state->state_lock, flags);
    cancel_delayed_work_sync(&state->sample_work);
}

static int note_key_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct note_input_key *state;
    unsigned long flags;
    int level, irq, result;

    state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
    if (!state)
        return -ENOMEM;
    state->dev = dev;
    state->debounce_ms = 10;
    state->keycode = KEY_F13;
    if (device_property_present(dev, "debounce-ms")) {
        result = device_property_read_u32(dev, "debounce-ms", &state->debounce_ms);
        if (result)
            return dev_err_probe(dev, result, "invalid debounce-ms\n");
    }
    if (!state->debounce_ms || state->debounce_ms > 1000)
        return dev_err_probe(dev, -EINVAL, "debounce-ms must be 1..1000\n");
    if (device_property_present(dev, "linux,code")) {
        result = device_property_read_u32(dev, "linux,code", &state->keycode);
        if (result)
            return dev_err_probe(dev, result, "invalid linux,code\n");
    }
    if (!state->keycode || state->keycode > KEY_MAX)
        return dev_err_probe(dev, -EINVAL, "linux,code must be 1..KEY_MAX\n");
    state->key = devm_gpiod_get(dev, "key", GPIOD_IN);
    if (IS_ERR(state->key))
        return dev_err_probe(dev, PTR_ERR(state->key), "key-gpios\n");
    level = gpiod_get_value_cansleep(state->key);
    if (level < 0)
        return dev_err_probe(dev, level, "initial key read\n");
    state->stable_pressed = !!level; /* 描述符已经应用极性，不能再手动反相。 */
    spin_lock_init(&state->state_lock);
    INIT_DELAYED_WORK(&state->sample_work, note_key_sample);
    state->quiet_until_ns = ktime_get_ns() +
                           (u64)state->debounce_ms * NSEC_PER_MSEC;
    state->input = devm_input_allocate_device(dev);
    if (!state->input)
        return -ENOMEM;
    state->input->name = "note_input_key";
    state->input->id.bustype = BUS_HOST;
    input_set_capability(state->input, EV_KEY, state->keycode);
    result = input_register_device(state->input);
    if (result)
        return result;
    /* 发布后先对齐core初始状态，再允许IRQ生产者运行。 */
    input_report_key(state->input, state->keycode, !!level);
    input_sync(state->input);

    result = devm_add_action_or_reset(dev, note_key_stop, state);
    if (result)
        return result;
    irq = gpiod_to_irq(state->key);
    if (irq < 0)
        return dev_err_probe(dev, irq, "key IRQ mapping\n");
    state->irq = irq;
    result = devm_request_irq(dev, irq, note_key_irq,
                              IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                              dev_name(dev), state);
    if (result)
        return dev_err_probe(dev, result, "request key IRQ\n");

    /* 再次安排采样，覆盖首次读取与IRQ申请之间可能发生的电平变化。 */
    spin_lock_irqsave(&state->state_lock, flags);
    mod_delayed_work(system_wq, &state->sample_work,
                     msecs_to_jiffies(state->debounce_ms));
    spin_unlock_irqrestore(&state->state_lock, flags);
    platform_set_drvdata(pdev, state);
    dev_info(dev, "ready: IRQ=%d quiet=%u ms, code=%u initial=%u\n",
             irq, state->debounce_ms, state->keycode, !!level);
    return 0;
}

static void note_key_shutdown(struct platform_device *pdev)
{
    struct note_input_key *state = platform_get_drvdata(pdev);
    /* shutdown不自动执行解绑的devres逆序清理，因此显式停止生产者。 */
    devm_free_irq(&pdev->dev, state->irq, state);
    note_key_stop(state);
}

static int __maybe_unused note_key_suspend(struct device *dev)
{
    dev_info(dev, "runtime lab: unbind before system sleep\n");
    return -EBUSY; /* 不把没有PM交接的运行态样例伪装成唤醒驱动。 */
}
static DEFINE_SIMPLE_DEV_PM_OPS(note_key_pm_ops, note_key_suspend, NULL);

static const struct of_device_id note_key_match[] = {
    { .compatible = "linux-note,input-key-lab" },
    { }
};
MODULE_DEVICE_TABLE(of, note_key_match);

static struct platform_driver note_key_driver = {
    .probe = note_key_probe,
    .shutdown = note_key_shutdown,
    .driver = {
        .name = "note_input_key",
        .of_match_table = note_key_match,
        .pm = pm_sleep_ptr(&note_key_pm_ops),
    },
};
module_platform_driver(note_key_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO稳定按键到Input事件的运行态实验");
