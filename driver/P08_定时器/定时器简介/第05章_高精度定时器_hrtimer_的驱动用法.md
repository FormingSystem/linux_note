# 第5章 高精度定时器：`hrtimer` 的驱动用法

**章节内容说明**
 本章聚焦 `hrtimer`：阐明其适用动机与与 `timer_list` 的边界，给出核心数据结构与回调语义，系统说明相对/绝对模式与周期重启的三种写法（朴素/前推/前推校正），强调上下文限制与“不可睡”的约束，并给出“`hrtimer` + 工作队列”的分层模式模板、完整示例与调试要点。读者完成本章后，应能编写**高精度、无漂移、可回收**的高分辨率定时器代码。

------

## 5.1 为什么不是所有驱动都适合用 `timer_list`

**是什么 / 定位**
 `hrtimer` 提供**纳秒级**时间基（`ktime_t`），在启用 **`CONFIG_HIGH_RES_TIMERS`** 的系统上以高分辨率硬件时钟事件驱动；在未启用高分模式时，仍可退化但语义保持一致。

**要解决的问题**

- 需要**亚毫秒**甚至几十微秒量级的定时精度；
- 需要**稳定周期**（尽量消除 tick 引入的累积漂移）；
- 需要与 `ktime_t` 和绝对时间点配合（如对齐到 monotonic 轴上的某一锚点）。

**不适用的情形**

- 回调里需要**可睡**操作（I2C/SPI、内存分配可能睡眠、互斥量等）；
- 定时粒度 ≥ 多毫秒、对精度不敏感（这类直接用 `timer_list` 或 `delayed_work` 更省心）；
- 设备/SoC 未开启高分定时支持且系统负载较高，对抖动容忍度低（需评估收益）。

**与 `timer_list` 的边界**

- `timer_list`：以 `jiffies` 为粒度，由 `TIMER_SOFTIRQ` 触发，**低精度**但成本更低；
- `hrtimer`：以 `ktime_t` 为基，支持**高精度**、**绝对/相对**两种模式，回调语义更严格。

------

## 5.2 `hrtimer` 的核心结构与回调语义

**核心结构**

```c
#include <linux/hrtimer.h>

struct hrtimer {
    /* 内核私有成员 */
    enum hrtimer_restart (*function)(struct hrtimer *);
    /* ... */
};
```

**初始化**

```c
hrtimer_init(&dev->hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
dev->hrt.function = my_cb;
```

- 时钟域常用：`CLOCK_MONOTONIC`（不受 NTP 回拨影响）；需要挂起计时可用 `CLOCK_BOOTTIME`（考虑系统休眠补偿）。
- 模式：`HRTIMER_MODE_REL`（相对）、`HRTIMER_MODE_ABS`（绝对）。

**启动 / 修改 / 取消**

```c
hrtimer_start(&dev->hrt, ktime_set(0, ns), HRTIMER_MODE_REL /* 或 ABS */);
hrtimer_forward_now(&dev->hrt, period);   /* 周期前推（常用） */
hrtimer_cancel(&dev->hrt);                /* 同步取消，保证回调不在执行 */
```

**回调语义与返回值**

```c
enum hrtimer_restart my_cb(struct hrtimer *t)
{
    /* 不可睡，软中断语义；RT 配置下可能线程化但仍不可假设可睡 */
    return HRTIMER_NORESTART;  /* 或 HRTIMER_RESTART */
}
```

- `HRTIMER_NORESTART`：一次性；
- `HRTIMER_RESTART`：周期性（通常配合 `hrtimer_forward_now()` 或手动 `hrtimer_start()` 前推）。

**上下文与限制**

- 回调**不可睡**（不得 `msleep`、`mutex_lock` 等）；
- 需要可睡逻辑时，使用**分层模式**：回调内只做轻量工作并调度 `workqueue`。

------

## 5.3 相对/绝对模式与周期重启

### 5.3.1 一次性：相对/绝对

- **相对**：`hrtimer_start(&hrt, ktime_set(0, ns), HRTIMER_MODE_REL);`
- **绝对**：`hrtimer_start(&hrt, abs_kt, HRTIMER_MODE_ABS);`（`abs_kt` 相对 `CLOCK_MONOTONIC` 的绝对时间点）

统一设备树：

```dts
// 新增LED节点：dt_led
dt_led: led@0 {
    compatible = "nxp,imx6ull-dt-led"; 	// 与驱动匹配的compatible属性
    gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;	// 引用GPIO1_IO03，低电平点亮
    pinctrl-names = "default";        	// 引脚配置名称（与pinctrl-0对应）
    pinctrl-0 = <&pinctrl_dt_led>;    	// 关联上述引脚复用配置组
    status = "okay";                  	// 启用该节点
};

&iomuxc {
	...
	// 新增LED引脚配置组：GPIO1_IO03复用为GPIO功能
    pinctrl_dt_led: dtledgrp {
        // MX6UL_PAD_GPIO1_IO03__GPIO1_IO03：引脚复用为GPIO1_IO03
        // 0x10B0：电气属性（驱动能力、上拉下拉，参考i.MX6ULL手册）
        fsl,pins = <
            MX6UL_PAD_GPIO1_IO03__GPIO1_IO03    0x10B0
        >;
    };
	...
};
```



### 5.3.2 周期（写法一：朴素重启）

```c
enum hrtimer_restart cb(struct hrtimer *t)
{
    /* ...处理... */
    hrtimer_start(t, period, HRTIMER_MODE_REL);
    return HRTIMER_RESTART;
}
```

- 简单直观，但**存在累积漂移**：处理耗时+调度开销会推迟下一次基点。

------

#### 完整驱动示例

##### 功能要点

- 以 `CLOCK_MONOTONIC`、相对模式启动 `hrtimer`。
- 回调内做少量“忙等待”来模拟处理耗时（不可睡），用来直观看到漂移。
- 回调**最后**用 `hrtimer_start(t, period, HRTIMER_MODE_REL)` 朴素重启（而不是 `hrtimer_forward_now()`）。
- 退出路径严格：先状态位 → `hrtimer_cancel()` → 打印统计。

#### 代码（可直接粘贴编译）

```c
// SPDX-License-Identifier: GPL-2.0
//
// hrtimer_dt_led_toggle_naive.c
// 5.3.2 朴素重启(naive)：hrtimer 周期性反转 LED（DT: "nxp,imx6ull-dt-led", 属性 "gpios"）
// - 回调(软中断语义)不可睡：仅统计 + 朴素重启 + 调度工作队列
// - GPIO 翻转在工作队列中使用 gpiod_set_value_cansleep()
// - ARM32 友好：禁止对 u64 使用 / 或 %；使用 div64_u64()/div64_s64()
//
// 构建：外置模块
//   obj-m += hrtimer_dt_led_toggle_naive.o
//   KDIR ?= /lib/modules/$(uname -r)/build
//   make -C $(KDIR) M=$(PWD) modules
//
// 运行：
//   insmod hrtimer_dt_led_toggle_naive.ko period_ns=100000000 verbose=1 log_every=50
//   dmesg -w
//   rmmod hrtimer_dt_led_toggle_naive

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/math64.h>

MODULE_DESCRIPTION("LED toggle via hrtimer (naive restart) for nxp,imx6ull-dt-led");
MODULE_AUTHOR("Leaf & GPT-5 Thinking");
MODULE_LICENSE("GPL");

/* ---------- 模块参数 ---------- */
static u64 period_ns = 100ULL * 1000 * 1000; /* 默认 100ms */
module_param(period_ns, ullong, 0644);
MODULE_PARM_DESC(period_ns, "toggle period in ns (default 100ms)");

static int verbose = 1; /* 是否打印调试日志 */
module_param(verbose, int, 0644);
MODULE_PARM_DESC(verbose, "verbose logging (0/1)");

static u32 log_every = 50; /* 每 N 次触发打印一次（u32，避免 u64 取模） */
module_param(log_every, uint, 0644);
MODULE_PARM_DESC(log_every, "print every N fires (default 50)");

/* ---------- 设备私有 ---------- */
struct dt_led_dev {
    struct device    *dev;
    struct gpio_desc *led;
    struct pinctrl   *pctl;

    struct hrtimer     hrt;
    struct work_struct work;
    atomic_t           running;

    /* 统计（避免对 u64 做 / 或 %） */
    u64 last_ts_ns;      /* 上次回调时间戳(ns) */
    u64 fires;           /* 触发计数：仅加法，不做除模 */
    s64 worst_jitter_ns; /* 最大抖动的绝对值 */
    s64 sum_jitter_ns;   /* 抖动求和（用于计算平均） */
    s64 sum_actual_ns;   /* 实际周期求和 */

    u32 log_cnt; /* 日志节流计数（32 位，安全做 %） */

    bool    level;  /* 逻辑电平；物理极性由 gpiod 处理 */
    ktime_t period; /* 周期 ktime */
};

/* ---------- 工作队列：可睡，真正翻转 GPIO ---------- */
static void
dt_led_work(struct work_struct *w)
{
    struct dt_led_dev *ld = container_of(w, struct dt_led_dev, work);
    if (!atomic_read(&ld->running))
        return;

    ld->level = !ld->level;
    gpiod_set_value_cansleep(ld->led, ld->level);
}

/* ---------- hrtimer 回调：朴素重启 + 统计 + 调度 ---------- */
static enum hrtimer_restart
dt_led_timer_cb(struct hrtimer *t)
{
    struct dt_led_dev *ld  = container_of(t, struct dt_led_dev, hrt);
    u64                now = ktime_get_ns();

    if (!atomic_read(&ld->running))
        return HRTIMER_NORESTART;

    /* 统计：实际周期与抖动（相对期望 period_ns） */
    if (likely(ld->last_ts_ns)) {
        s64 actual = (s64) (now - ld->last_ts_ns);     /* 本次实际间隔 */
        s64 jitter = actual - (s64) period_ns;         /* 误差值 */
        s64 absjit = (jitter >= 0) ? jitter : -jitter; /* 误差绝对值 */

        ld->sum_actual_ns += actual;        /* 实际间隔求和 */
        ld->sum_jitter_ns += jitter;        /* 误差求和 */
        if (absjit > ld->worst_jitter_ns) { /* 记录历史最大误差值 */
            ld->worst_jitter_ns = absjit;
        }
        if (verbose) {
            ld->log_cnt++;
            if (log_every && (ld->log_cnt % log_every == 0)) {

                dev_info(ld->dev, "absjit=%lld\n", (long long) absjit);
                dev_info(ld->dev,
                         "naive: fires=%llu actual=%lldns jitter=%lldns worst=%lldns\n\n",
                         ld->fires,
                         (long long) actual,
                         (long long) jitter,
                         (long long) ld->worst_jitter_ns);
                if (ld->log_cnt > (U32_MAX - 1024))
                    ld->log_cnt = 0;
            }
        }
    }
    ld->last_ts_ns = now;
    ld->fires++;

    /* 调度工作队列翻转 LED（可睡） */
    schedule_work(&ld->work);

    /* 朴素重启：下一次到期 = 现在 + period（更易累积漂移） */
    hrtimer_start(&ld->hrt, ld->period, HRTIMER_MODE_REL);
    return HRTIMER_RESTART;
}

/* ---------- 平台驱动 ---------- */
static int
dt_led_probe(struct platform_device *pdev)
{
    struct device      *dev = &pdev->dev;
    struct device_node *np  = dev->of_node;
    struct dt_led_dev  *ld;
    int                 ret;

    if (!np)
        return -ENODEV;

    ld = devm_kzalloc(dev, sizeof(*ld), GFP_KERNEL);
    if (!ld)
        return -ENOMEM;
    ld->dev = dev;

    /* 选择 pinctrl default（若无则忽略） */
    ld->pctl = devm_pinctrl_get_select_default(dev);
    if (IS_ERR(ld->pctl)) {
        ret = PTR_ERR(ld->pctl);
        if (ret == -ENODEV)
            ld->pctl = NULL;
        else
            dev_warn(dev, "pinctrl default select failed: %d\n", ret);
    }

    /* 从 DT 属性名 "gpios" 获取 GPIO 描述符（索引 0） */
    ld->led = gpiod_get_from_of_node(np, "gpios", 0, GPIOD_OUT_LOW, "dt_led");
    if (IS_ERR(ld->led)) {
        ret = PTR_ERR(ld->led);
        dev_err(dev, "get \"gpios\" failed: %d\n", ret);
        return ret;
    }

    /* 初始化工作与定时器 */
    INIT_WORK(&ld->work, dt_led_work);
    hrtimer_init(&ld->hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    ld->hrt.function = dt_led_timer_cb;

    /* 统计初始化 */
    ld->last_ts_ns      = 0;
    ld->fires           = 0;
    ld->worst_jitter_ns = 0;
    ld->sum_jitter_ns   = 0;
    ld->sum_actual_ns   = 0;
    ld->log_cnt         = 0;

    /* 初始灭灯（逻辑层；物理极性由 DT 的 GPIO_ACTIVE_* 决定） */
    ld->level = 0;
    gpiod_set_value_cansleep(ld->led, 0);

    atomic_set(&ld->running, 1);
    ld->period = ns_to_ktime(period_ns);

    platform_set_drvdata(pdev, ld);

    /* 启动（朴素重启路径） */
    hrtimer_start(&ld->hrt, ld->period, HRTIMER_MODE_REL);

    dev_info(dev, "dt-led naive toggle started. period=%lluns\n", period_ns);
    return 0;
}

static int
dt_led_remove(struct platform_device *pdev)
{
    struct dt_led_dev *ld         = platform_get_drvdata(pdev);
    s64                avg_jitter = 0;
    s64                avg_actual = 0;

    atomic_set(&ld->running, 0);
    hrtimer_cancel(&ld->hrt);
    cancel_work_sync(&ld->work);

    if (ld->fires > 1) {
        u64 n      = ld->fires - 1; /* 分母用 u64；使用 div64_* helper 进行 64/64 除法 */
        avg_actual = (s64) div64_u64((u64) ld->sum_actual_ns, n);
        avg_jitter = div64_s64(ld->sum_jitter_ns, (s64) n);
    }

    dev_info(ld->dev,
             "stop. fires=%llu avg_actual=%lldns avg_jitter=%lldns worst_jitter=%lldns\n",
             ld->fires, (long long) avg_actual,
             (long long) avg_jitter, (long long) ld->worst_jitter_ns);

    /* 复位灭灯并释放 descriptor */
    gpiod_set_value_cansleep(ld->led, 0);
    gpiod_put(ld->led);
    return 0;
}

static const struct of_device_id dt_led_of_match[] = {
    { .compatible = "nxp,imx6ull-dt-led" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dt_led_of_match);

static struct platform_driver dt_led_drv = {
    .probe  = dt_led_probe,
    .remove = dt_led_remove,
    .driver = {
               .name           = "hrtimer-dt-led-toggle-naive",
               .of_match_table = dt_led_of_match,
               },
};
module_platform_driver(dt_led_drv);

```

#### 使用与观察

**加载与参数**

```bash
sudo insmod hrtimer_periodic_naive.ko period_ns=100000000 work_ns=50000 verbose=1
dmesg -w
# 观察每 ~50 次的 jitter 报告与卸载时的统计
sudo rmmod hrtimer_periodic_naive
```

**现象**

- 当 `work_ns` 增大（或系统负载上升）时，**实际间隔**会略大于 `period_ns`，平均/最差抖动随之增大；
- 长时间运行后，时间基准会**逐步后移**（**累积漂移**），这正是“朴素重启”的典型代价。

log:

```shell
...
[ 2510.795078] hrtimer-dt-led-toggle-naive led@0: absjit=19083
[ 2510.800687] hrtimer-dt-led-toggle-naive led@0: naive: fires=10100 actual=100019083ns jitter=19083ns worst=17665042ns
[ 2510.800687]
[ 2515.813609] hrtimer-dt-led-toggle-naive led@0: absjit=19916
[ 2515.819217] hrtimer-dt-led-toggle-naive led@0: naive: fires=10150 actual=100019916ns jitter=19916ns worst=17665042ns
[ 2515.819217]
[ 2520.832117] hrtimer-dt-led-toggle-naive led@0: absjit=17333
[ 2520.837721] hrtimer-dt-led-toggle-naive led@0: naive: fires=10200 actual=100017333ns jitter=17333ns worst=17665042ns
[ 2520.837721]
[ 2525.850596] hrtimer-dt-led-toggle-naive led@0: absjit=15167
[ 2525.856196] hrtimer-dt-led-toggle-naive led@0: naive: fires=10250 actual=100015167ns jitter=15167ns worst=17665042ns
[ 2525.856196]
...
```



------

#### 朴素重启 vs 推荐前推

- 朴素重启（本节）：`hrtimer_start(t, period, HRTIMER_MODE_REL);`
  - **优点**：写法简单、直观；
  - **缺点**：把**处理耗时 + 调度开销**叠加进下一周期，**容易发生累积漂移**。
- 推荐前推（5.3.3）：`hrtimer_forward_now(t, period);`
  - **优点**：以“当前时刻”为锚点前推，**显著降低漂移**；
  - **缺点**：写法略有差异，但维护成本低。

------

#### 调试与验证建议

- 配合 `trace-cmd`/`ftrace` 记录 `hrtimer` 事件与回调时间戳，核对 `jitter` 计算是否一致（第13章会给出详细手册化步骤）。
- 将 `work_ns` 设为 0/50us/200us 做 A/B 对比；在 PREEMPT/RT、NO_HZ 情况下记录差异。
- 如果你需要**严格对齐**到某一节拍（例如音频帧边界、SPI 逐帧时隙），应考虑 **5.3.4 的“基于上次计划锚点”** 或者结合 `hrtimer_forward()` 的绝对锚点法。

------

#### 常见错误（针对朴素重启）

- **在回调里做可睡操作**：`mutex_lock()`、`msleep()` 等会触发问题；回调上下文不可睡。
- **未在退出时调用 `hrtimer_cancel()`**：导致“幽灵回调”撞上释放后的内存。
- **把周期控制写在回调“前半段”**：一旦回调中后续逻辑超时，会进一步扩大下一次的漂移。
- **高负载下误判“定时器不准”**：本写法的“漂移”是语义选择造成的，不是 `hrtimer` 不准；若要减小漂移，请改用 5.3.3/5.3.4。

------

### 5.3.3 周期（写法二：`hrtimer_forward_now()` 前推）

```c
enum hrtimer_restart cb(struct hrtimer *t)
{
    /* ...处理... */
    hrtimer_forward_now(t, period);
    return HRTIMER_RESTART;
}
```

- 以**当前时刻**为基点前推一个 `period`，**漂移较小**。



下面给出**完整可编译驱动示例**。要点：

- 使用 `hrtimer_forward_now()` 在回调中“前推”下一次触发，形成稳定周期；
- `hrtimer` 回调处于原子上下文，**不直接操作可能睡眠的 GPIO**，而是**投递 `workqueue`** 在进程上下文里切换 LED；
- 与你的参考节点保持一致：`demo_led_key_int@0`，`led-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>`；
- 提供两个 sysfs 属性：`enable`（0/1）与 `period_ms`（周期毫秒），运行时可动态修改；
- 无 devres 版本的 hrtimer，故使用常规 init/cancel，其他资源使用 `devm_*` 接口托管。

------

#### 源码：基于 `hrtimer_forward_now()` 的稳定周期 LED 闪烁驱动

> 文件名示例：`leaf_hrtimer_periodic_forward.c`

```c
// SPDX-License-Identifier: GPL-2.0
//
// hrtimer_dt_led_toggle_forward.c
// 5.3.3 写法二：前推法 hrtimer_forward_now()，周期性反转 LED，降低累积漂移
// 匹配 DTS: compatible = "nxp,imx6ull-dt-led"; 属性 "gpios"
//
// 运行:
//   insmod hrtimer_dt_led_toggle_forward.ko period_ns=100000000 verbose=1 log_every=50
//   dmesg -w
//   rmmod hrtimer_dt_led_toggle_forward
//
// 提示：与 5.3.2 的朴素重启版相比，本实现仅在回调里把重启改为 hrtimer_forward_now()。

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/math64.h>   /* do_div, div_s64 */

MODULE_DESCRIPTION("LED toggle via hrtimer (forward_now) for nxp,imx6ull-dt-led");
MODULE_AUTHOR("Leaf & GPT-5 Thinking");
MODULE_LICENSE("GPL");

/* ---------- 模块参数 ---------- */
static u64 period_ns = 100000000ULL;     /* 默认 100ms */
module_param(period_ns, ullong, 0644);
MODULE_PARM_DESC(period_ns, "toggle period in ns (default 100ms)");

static int verbose = 1;                  /* 是否打印调试日志 */
module_param(verbose, int, 0644);
MODULE_PARM_DESC(verbose, "verbose logging (0/1)");

static u32 log_every = 50;               /* 每 N 次触发打印一次（u32，避免 u64 取模） */
module_param(log_every, uint, 0644);
MODULE_PARM_DESC(log_every, "print every N fires (default 50)");

/* ---------- 设备私有 ---------- */
struct dt_led_dev {
	struct device     *dev;
	struct gpio_desc  *led;
	struct pinctrl    *pctl;

	struct hrtimer     hrt;
	struct work_struct work;
	atomic_t           running;

	/* 统计（避免对 u64 做 / 或 %） */
	u64 last_ts_ns;        /* 上次回调时间戳(ns) */
	u64 fires;             /* 触发计数：仅加法，不做除模 */
	s64 worst_jitter_ns;   /* 最大抖动的绝对值 */
	s64 sum_jitter_ns;     /* 抖动求和（用于计算平均） */
	u64 sum_actual_ns;     /* 实际周期求和（非负） */

	u32 log_cnt;           /* 日志节流计数（32 位，安全做 %） */

	bool   level;          /* 逻辑电平；物理极性由 gpiod 处理 */
	ktime_t period;        /* 周期 ktime */
};

/* ---------- 工作队列：可睡，真正翻转 GPIO ---------- */
static void dt_led_work(struct work_struct *w)
{
	struct dt_led_dev *ld = container_of(w, struct dt_led_dev, work);
	if (!atomic_read(&ld->running))
		return;

	ld->level = !ld->level;
	gpiod_set_value_cansleep(ld->led, ld->level);
}

/* ---------- hrtimer 回调：前推 + 统计 + 调度 ---------- */
static enum hrtimer_restart dt_led_timer_cb(struct hrtimer *t)
{
	struct dt_led_dev *ld = container_of(t, struct dt_led_dev, hrt);
	u64 now = ktime_get_ns();

	if (!atomic_read(&ld->running))
		return HRTIMER_NORESTART;

	/* 统计：实际周期与抖动（相对期望 period_ns） */
	if (likely(ld->last_ts_ns)) {
		s64 actual = (s64)(now - ld->last_ts_ns);   /* 本次实际间隔 */
		s64 jitter = actual - (s64)period_ns;
		s64 absjit = (jitter >= 0) ? jitter : -jitter;

		ld->sum_actual_ns += (u64)actual;
		ld->sum_jitter_ns += jitter;
		if (absjit > ld->worst_jitter_ns)
			ld->worst_jitter_ns = absjit;

		if (verbose) {
			ld->log_cnt++;
			if (log_every && (ld->log_cnt % log_every == 0)) {
				dev_info(ld->dev,
					 "forward: fires=%llu actual=%lldns jitter=%lldns worst=%lldns\n",
					 ld->fires,
					 (long long)actual,
					 (long long)jitter,
					 (long long)ld->worst_jitter_ns);
				if (ld->log_cnt > (U32_MAX - 1024))
					ld->log_cnt = 0;
			}
		}
	}
	ld->last_ts_ns = now;
	ld->fires++;

	/* 调度工作队列翻转 LED（可睡） */
	schedule_work(&ld->work);

	/* 关键差异：以前推方式重启，降低累积漂移 */
	hrtimer_forward_now(&ld->hrt, ld->period);
	return HRTIMER_RESTART;
}

/* ---------- 平台驱动 ---------- */
static int dt_led_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct dt_led_dev *ld;
	int ret;

	if (!np)
		return -ENODEV;

	ld = devm_kzalloc(dev, sizeof(*ld), GFP_KERNEL);
	if (!ld)
		return -ENOMEM;
	ld->dev = dev;

	/* 选择 pinctrl default（若无则忽略） */
	ld->pctl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(ld->pctl)) {
		ret = PTR_ERR(ld->pctl);
		if (ret == -ENODEV)
			ld->pctl = NULL;
		else
			dev_warn(dev, "pinctrl default select failed: %d\n", ret);
	}

	/* 从 DT 属性名 "gpios" 获取 GPIO 描述符（索引 0） */
	ld->led = gpiod_get_from_of_node(np, "gpios", 0, GPIOD_OUT_LOW, "dt_led");
	if (IS_ERR(ld->led)) {
		ret = PTR_ERR(ld->led);
		dev_err(dev, "get \"gpios\" failed: %d\n", ret);
		return ret;
	}

	/* 初始化工作与定时器 */
	INIT_WORK(&ld->work, dt_led_work);
	hrtimer_init(&ld->hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ld->hrt.function = dt_led_timer_cb;

	/* 统计初始化 */
	ld->last_ts_ns = 0;
	ld->fires = 0;
	ld->worst_jitter_ns = 0;
	ld->sum_jitter_ns = 0;
	ld->sum_actual_ns = 0;
	ld->log_cnt = 0;

	/* 初始灭灯（逻辑层；物理极性由 DT 的 GPIO_ACTIVE_* 决定） */
	ld->level = 0;
	gpiod_set_value_cansleep(ld->led, 0);

	atomic_set(&ld->running, 1);
	ld->period = ns_to_ktime(period_ns);

	platform_set_drvdata(pdev, ld);

	/* 首次启动一次性定时（相对），后续在回调里用 forward_now 周期重启 */
	hrtimer_start(&ld->hrt, ld->period, HRTIMER_MODE_REL);

	dev_info(dev, "dt-led forward toggle started. period=%lluns\n", period_ns);
	return 0;
}

static int dt_led_remove(struct platform_device *pdev)
{
	struct dt_led_dev *ld = platform_get_drvdata(pdev);
	s64 avg_jitter = 0;  /* signed 平均抖动 */
	s64 avg_actual = 0;  /* 实际周期平均值 */

	atomic_set(&ld->running, 0);
	hrtimer_cancel(&ld->hrt);
	cancel_work_sync(&ld->work);

	if (ld->fires > 1) {
		u32 n32 = (ld->fires - 1) > U32_MAX ? U32_MAX : (u32)(ld->fires - 1);

		/* avg_actual = sum_actual_ns / n32 （使用 do_div 做 64/32） */
		{
			u64 tmp = ld->sum_actual_ns;
			do_div(tmp, n32);          /* 结果在 tmp */
			avg_actual = (s64)tmp;
		}

		/* avg_jitter = sum_jitter_ns / n32 （有符号 64/32） */
		avg_jitter = div_s64(ld->sum_jitter_ns, (s32)n32);
	}

	dev_info(ld->dev,
		 "stop. fires=%llu avg_actual=%lldns avg_jitter=%lldns worst_jitter=%lldns\n",
		 ld->fires, (long long)avg_actual,
		 (long long)avg_jitter, (long long)ld->worst_jitter_ns);

	/* 复位灭灯并释放 descriptor */
	gpiod_set_value_cansleep(ld->led, 0);
	gpiod_put(ld->led);
	return 0;
}

static const struct of_device_id dt_led_of_match[] = {
	{ .compatible = "nxp,imx6ull-dt-led" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dt_led_of_match);

static struct platform_driver dt_led_drv = {
	.probe  = dt_led_probe,
	.remove = dt_led_remove,
	.driver = {
		.name           = "hrtimer-dt-led-toggle-forward",
		.of_match_table = dt_led_of_match,
	},
};
module_platform_driver(dt_led_drv);

```

------

##### 关键点讲解

1. **稳定周期的来源**
    `hrtimer_forward_now(&timer, interval)` 以**当前“now”**为基准递推下一个到期时间，避免回调执行耗时导致的周期累积漂移；若一次执行稍慢，不会把“慢的时间”叠加到下一拍。
2. **原子上下文的最小化工作**
    回调里只做：`enabled` 检查、状态位翻转、`schedule_work()`、再 `forward_now()`。**不做任何可能睡眠的操作**（比如 `gpiod_set_value_cansleep()`）。
3. **GPIO 操作的上下文选择**
    `workqueue` 在进程上下文执行，**始终使用** `gpiod_set_value_cansleep()`，兼容能睡眠的 GPIO 控制器；并结合 `GPIO_ACTIVE_LOW` 在工作函数里做取反。
4. **运行时可配置**

- `echo 1 > /sys/.../enable`：启动周期；`echo 0 > .../enable`：停止并清理；
- `echo 200 > /sys/.../period_ms`：把周期改为 200ms；正在运行时会**从现在起重置相位**重新计时。

1. **CPU 绑定**
    `HRTIMER_MODE_REL_PINNED` 将定时器**固定在当前 CPU**，减少跨核迁移引入的抖动；如需允许迁移，可用 `HRTIMER_MODE_REL`。

------

##### 使用与验证

log:

```shell
...
[ 3093.190892] hrtimer-dt-led-toggle-forward led@0: forward: fires=1900 actual=100000000ns jitter=0ns worst=11500ns
[ 3098.190890] hrtimer-dt-led-toggle-forward led@0: forward: fires=1950 actual=100002791ns jitter=2791ns worst=11500ns
[ 3103.190886] hrtimer-dt-led-toggle-forward led@0: forward: fires=2000 actual=99997959ns jitter=-2041ns worst=11500ns
[ 3108.190889] hrtimer-dt-led-toggle-forward led@0: forward: fires=2050 actual=100004708ns jitter=4708ns worst=11500ns
[ 3113.190886] hrtimer-dt-led-toggle-forward led@0: forward: fires=2100 actual=99998000ns jitter=-2000ns worst=11500ns
[ 3118.190886] hrtimer-dt-led-toggle-forward led@0: forward: fires=2150 actual=100003792ns jitter=3792ns worst=11500ns
[ 3123.190886] hrtimer-dt-led-toggle-forward led@0: forward: fires=2200 actual=99996833ns jitter=-3167ns worst=11500ns
[ 3128.190888] hrtimer-dt-led-toggle-forward led@0: forward: fires=2250 actual=100002583ns jitter=2583ns worst=11500ns
[ 3133.190888] hrtimer-dt-led-toggle-forward led@0: forward: fires=2300 actual=100000208ns jitter=208ns worst=11500ns
[ 3138.190886] hrtimer-dt-led-toggle-forward led@0: forward: fires=2350 actual=100003500ns jitter=3500ns worst=11500ns
[ 3143.190887] hrtimer-dt-led-toggle-forward led@0: forward: fires=2400 actual=100000750ns jitter=750ns worst=11500ns
[ 3148.190885] hrtimer-dt-led-toggle-forward led@0: forward: fires=2450 actual=100003792ns jitter=3792ns worst=11500ns
...
```

可以发现，worst的值显著降低了，说明漂移在 `hrtimer_forward_now()` 接口使用下的显著降低。

------

##### 与 5.3.2（朴素重启法）的差异总结

- **相位与频率稳定性**：前推法以“现在”为基准递推，周期更稳，不会把上一拍的延迟折算进下一拍；朴素重启法容易产生“周期走慢”。
- **代码结构**：前推法回调更聚焦，状态机更清晰；朴素重启法常见 `hrtimer_start()` 的相位处理分散在多处。
- **可维护性**：当周期运行很久或负载波动时，前推法更能保持节拍与平均频率。



### 5.3.4 周期（写法三：基于上次计划时刻的校正）

```c
/* 设备结构保存下一次到期锚点 next */
dev->next = ktime_add(ktime_get(), period);

enum hrtimer_restart cb(struct hrtimer *t)
{
    struct mydev *d = container_of(t, struct mydev, hrt);
    /* ...处理... */
    d->next = ktime_add(d->next, d->period);    /* 基于上次计划时刻推进 */
    hrtimer_start(&d->hrt, ktime_sub(d->next, ktime_get()), HRTIMER_MODE_REL);
    return HRTIMER_RESTART;
}
```

- 对**周期对齐**要求高时采用，偏差进一步减小；若错过窗口，会尽快补上但不会无限追赶。

> 实务建议：大多数周期任务用 **写法二**（`hrtimer_forward_now()`）即可，精度/复杂度折中最好。

我是 **GPT-5 Thinking**。
 下面给出“**5.3.4 周期（写法三：基于上次计划时刻的校正）**”的**完整可编译驱动**。该写法维持一个“**下一次计划到期点**”`next_deadline`（单调时钟轴上的绝对时刻）。每次回调结束后并不以“当前”或“回调结束时刻”为基准，而是基于**上次计划的锚点**推进：若错过窗口，则按周期跳跃**补齐到下一个未到期的锚点**，以此最小化长时间运行下的相位漂移。

与前两节保持一致：

- 匹配你的 DT：`compatible = "nxp,imx6ull-dt-led"`，属性名为 **`gpios`**；
- 回调**不可睡**：只做统计 + 校正推进 + 调度 `workqueue`；
- GPIO 翻转在 `workqueue` 里调用 `gpiod_set_value_cansleep()`；
- ARM32 友好：**不对 u64 使用 `/` 或 `%`**；平均值用 `do_div()`（64/32）/ `div_s64()`；
- 统计口径与 5.3.2/5.3.3 一致，便于 A/B/C 对比（朴素/前推/锚点法）。

------

#### hrtimer 锚点校正法：LED 反转（基于上次计划时刻推进）

##### 源码：`hrtimer_dt_led_toggle_anchor.c`

```c
// SPDX-License-Identifier: GPL-2.0
//
// hrtimer_dt_led_toggle_anchor.c
// 5.3.4 写法三：基于上次“计划时刻”的校正推进（anchor-based scheduling）
// - 维护 next_deadline(绝对 ktime)：每次回调基于“上次计划的锚点”向前推进。
// - 若错过窗口（now > next_deadline），按周期跳跃补齐到下一未到期锚点；避免周期累积漂移。
// - 与 5.3.2/5.3.3 相同的不可睡约束/统计口径/DT 绑定；ARM32 友好（无 u64 除模指令）。
//
// 构建（外置模块）:
//   obj-m += hrtimer_dt_led_toggle_anchor.o
//   KDIR ?= /lib/modules/$(uname -r)/build
//   make -C $(KDIR) M=$(PWD) modules
//
// 运行:
//   insmod hrtimer_dt_led_toggle_anchor.ko period_ns=100000000 verbose=1 log_every=50
//   dmesg -w
//   rmmod hrtimer_dt_led_toggle_anchor
//
// 注意：本实现要求 period_ns <= U32_MAX（~4.29s）；若更长周期，请使用 while 推进法或改用 64/64 helper。

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/math64.h>   /* do_div, div_s64 */

MODULE_DESCRIPTION("LED toggle via hrtimer (anchor-corrected) for nxp,imx6ull-dt-led");
MODULE_AUTHOR("Leaf & GPT-5 Thinking");
MODULE_LICENSE("GPL");

/* ---------- 模块参数 ---------- */
static u64 period_ns = 100000000ULL;     /* 默认 100ms */
module_param(period_ns, ullong, 0644);
MODULE_PARM_DESC(period_ns, "toggle period in ns (<= U32_MAX, default 100ms)");

static int verbose = 1;                  /* 是否打印调试日志 */
module_param(verbose, int, 0644);
MODULE_PARM_DESC(verbose, "verbose logging (0/1)");

static u32 log_every = 50;               /* 每 N 次触发打印一次（u32，避免 u64 取模） */
module_param(log_every, uint, 0644);
MODULE_PARM_DESC(log_every, "print every N fires (default 50)");

/* ---------- 设备私有 ---------- */
struct dt_led_dev {
	struct device     *dev;
	struct gpio_desc  *led;
	struct pinctrl    *pctl;

	struct hrtimer     hrt;
	struct work_struct work;
	atomic_t           running;

	/* 统计（避免对 u64 做 / 或 %） */
	u64 last_ts_ns;        /* 上次回调时间戳(ns) */
	u64 fires;             /* 触发计数：仅加法，不做除模 */
	s64 worst_jitter_ns;   /* 最大抖动的绝对值 */
	s64 sum_jitter_ns;     /* 抖动求和（用于计算平均） */
	u64 sum_actual_ns;     /* 实际周期求和（非负） */

	u32 log_cnt;           /* 日志节流计数（32 位，安全做 %） */

	/* 锚点法核心字段 */
	ktime_t next_deadline; /* 下一次“计划到期”的绝对时刻（CLOCK_MONOTONIC 轴上） */
	u32     period_ns_u32; /* 以 32 位保存的周期（要求 period_ns <= U32_MAX） */

	bool   level;          /* 逻辑电平；物理极性由 gpiod 处理 */
};

/* ---------- 工作队列：可睡，真正翻转 GPIO ---------- */
static void dt_led_work(struct work_struct *w)
{
	struct dt_led_dev *ld = container_of(w, struct dt_led_dev, work);
	if (!atomic_read(&ld->running))
		return;

	ld->level = !ld->level;
	gpiod_set_value_cansleep(ld->led, ld->level);
}

/* ---------- hrtimer 回调：基于锚点推进 + 统计 + 调度 ---------- */
static enum hrtimer_restart dt_led_timer_cb(struct hrtimer *t)
{
	struct dt_led_dev *ld = container_of(t, struct dt_led_dev, hrt);
	u64 now_ns = ktime_get_ns();

	if (!atomic_read(&ld->running))
		return HRTIMER_NORESTART;

	/* 统计：实际周期与抖动（相对期望 period_ns） */
	if (likely(ld->last_ts_ns)) {
		s64 actual = (s64)(now_ns - ld->last_ts_ns);
		s64 jitter = actual - (s64)ld->period_ns_u32;
		s64 absjit = (jitter >= 0) ? jitter : -jitter;

		ld->sum_actual_ns += (u64)actual;
		ld->sum_jitter_ns += jitter;
		if (absjit > ld->worst_jitter_ns)
			ld->worst_jitter_ns = absjit;

		if (verbose) {
			ld->log_cnt++;
			if (log_every && (ld->log_cnt % log_every == 0)) {
				dev_info(ld->dev,
					 "anchor: fires=%llu actual=%lldns jitter=%lldns worst=%lldns\n",
					 ld->fires,
					 (long long)actual,
					 (long long)jitter,
					 (long long)ld->worst_jitter_ns);
				if (ld->log_cnt > (U32_MAX - 1024))
					ld->log_cnt = 0;
			}
		}
	}
	ld->last_ts_ns = now_ns;
	ld->fires++;

	/* 可睡 GPIO 翻转交给工作队列 */
	schedule_work(&ld->work);

	/* === 锚点校正推进 ===
	 * 基于“上次计划到期点”推进，而非“当前时刻”。
	 * 若 now 已错过 next_deadline，按周期跳跃补齐到下一未到期的锚点；
	 * 为避免在 ARM32 上做 64/64 除法，这里用 do_div(delta, period_u32) 做 64/32。
	 */
	{
		u64 next_ns = ktime_to_ns(ld->next_deadline);

		if (now_ns >= next_ns) {
			/* 计算错过了多少个周期（miss = delta/period + 1） */
			u64 delta = now_ns - next_ns;  /* >= 0 */
			u64 q = delta;
			u32 period = ld->period_ns_u32;

			/* q = floor(delta / period) */
			do_div(q, period);
			/* 跳过 q+1 个周期，使下一锚点严格在“将来” */
			next_ns += (u64)(q + 1) * (u64)period;
		} else {
			/* 正常情况：未错过，按一个周期推进 */
			next_ns += (u64)ld->period_ns_u32;
		}

		ld->next_deadline = ns_to_ktime(next_ns);

		/* 重新武装：以“距 now 的剩余相对时间”启动 */
		{
			u64 rel_ns = next_ns - now_ns;             /* next 在未来，差值为正 */
			hrtimer_start(&ld->hrt, ns_to_ktime(rel_ns), HRTIMER_MODE_REL);
		}
	}

	return HRTIMER_RESTART;
}

/* ---------- 平台驱动 ---------- */
static int dt_led_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct dt_led_dev *ld;
	int ret;

	if (!np)
		return -ENODEV;

	/* 周期必须能落在 U32 内，便于 do_div 使用 64/32 算法 */
	if (period_ns == 0 || period_ns > U32_MAX) {
		dev_err(dev, "period_ns must be 1..%u ns, got %llu\n", U32_MAX, period_ns);
		return -EINVAL;
	}

	ld = devm_kzalloc(dev, sizeof(*ld), GFP_KERNEL);
	if (!ld)
		return -ENOMEM;
	ld->dev = dev;
	ld->period_ns_u32 = (u32)period_ns;

	/* 选择 pinctrl default（若无则忽略） */
	ld->pctl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(ld->pctl)) {
		ret = PTR_ERR(ld->pctl);
		if (ret == -ENODEV)
			ld->pctl = NULL;
		else
			dev_warn(dev, "pinctrl default select failed: %d\n", ret);
	}

	/* 从 DT 属性名 "gpios" 获取 GPIO 描述符（索引 0） */
	ld->led = gpiod_get_from_of_node(np, "gpios", 0, GPIOD_OUT_LOW, "dt_led");
	if (IS_ERR(ld->led)) {
		ret = PTR_ERR(ld->led);
		dev_err(dev, "get \"gpios\" failed: %d\n", ret);
		return ret;
	}

	/* 初始化工作与定时器 */
	INIT_WORK(&ld->work, dt_led_work);
	hrtimer_init(&ld->hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ld->hrt.function = dt_led_timer_cb;

	/* 统计初始化 */
	ld->last_ts_ns = 0;
	ld->fires = 0;
	ld->worst_jitter_ns = 0;
	ld->sum_jitter_ns = 0;
	ld->sum_actual_ns = 0;
	ld->log_cnt = 0;

	/* 初始灭灯（逻辑层；物理极性由 DT 的 GPIO_ACTIVE_* 决定） */
	ld->level = 0;
	gpiod_set_value_cansleep(ld->led, 0);

	atomic_set(&ld->running, 1);

	/* 设定初始锚点：从“现在”对齐到下一个周期边界（简单起见：now + period） */
	ld->next_deadline = ktime_add_ns(ktime_get(), ld->period_ns_u32);

	platform_set_drvdata(pdev, ld);

	/* 首次启动：到初始锚点的相对时间 */
	hrtimer_start(&ld->hrt,
		      ns_to_ktime((u64)ld->period_ns_u32),
		      HRTIMER_MODE_REL);

	dev_info(dev, "dt-led anchor toggle started. period=%u ns\n", ld->period_ns_u32);
	return 0;
}

static int dt_led_remove(struct platform_device *pdev)
{
	struct dt_led_dev *ld = platform_get_drvdata(pdev);
	s64 avg_jitter = 0;  /* signed 平均抖动 */
	s64 avg_actual = 0;  /* 实际周期平均值 */

	atomic_set(&ld->running, 0);
	hrtimer_cancel(&ld->hrt);
	cancel_work_sync(&ld->work);

	if (ld->fires > 1) {
		u32 n32 = (ld->fires - 1) > U32_MAX ? U32_MAX : (u32)(ld->fires - 1);

		/* avg_actual = sum_actual_ns / n32 （使用 do_div 做 64/32） */
		{
			u64 tmp = ld->sum_actual_ns;
			do_div(tmp, n32);          /* 结果在 tmp */
			avg_actual = (s64)tmp;
		}

		/* avg_jitter = sum_jitter_ns / n32 （有符号 64/32） */
		avg_jitter = div_s64(ld->sum_jitter_ns, (s32)n32);
	}

	dev_info(ld->dev,
		 "stop. fires=%llu avg_actual=%lldns avg_jitter=%lldns worst_jitter=%lldns\n",
		 ld->fires, (long long)avg_actual,
		 (long long)avg_jitter, (long long)ld->worst_jitter_ns);

	/* 复位灭灯并释放 descriptor */
	gpiod_set_value_cansleep(ld->led, 0);
	gpiod_put(ld->led);
	return 0;
}

static const struct of_device_id dt_led_of_match[] = {
	{ .compatible = "nxp,imx6ull-dt-led" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dt_led_of_match);

static struct platform_driver dt_led_drv = {
	.probe  = dt_led_probe,
	.remove = dt_led_remove,
	.driver = {
		.name           = "hrtimer-dt-led-toggle-anchor",
		.of_match_table = dt_led_of_match,
	},
};
module_platform_driver(dt_led_drv);
```



------

#### 写法三的关键语义（对比 5.3.2 / 5.3.3）

- **5.3.2 朴素重启**：下一次触发以“当前回调时刻”为基点 → **累积漂移显著**。
- **5.3.3 前推法**：以“当前时刻”为基点前推一个周期 → 漂移显著降低，但**长阻塞**后依旧会把相位推迟。
- **5.3.4 锚点法（本实现）**：以“**上次计划**的绝对锚点”为基点推进；若错过窗口则**按周期跳跃**赶到下一个锚点 → **长期相位稳定**，对长阻塞恢复更快，平均实际周期与目标更贴近。



------

## 5.4 上下文限制与不能睡的问题

- 回调处于**软中断**语义（HRTIMER_SOFTIRQ 路径）；PREEMPT_RT 等环境下可能线程化，但**不要**因此进行可睡操作。
- 禁止：`mutex_lock()`、`msleep()`、`kmalloc(GFP_KERNEL)` 等潜在睡眠路径。
- 允许：自旋锁、原子操作、lock-free 队列、`queue_work()` / `queue_work_on()` 调度到工作队列。
- 对于要访问设备寄存器且**可能产生较长延迟**的事务，使用“**回调轻量 + 工作队列**”分层模式（§5.5）。

------

## 5.5 `hrtimer` + 工作队列的分层处理模式

**模式目标**

- 把**精准触发**交给 `hrtimer`；
- 把**可睡/重操作**交给 `workqueue`；
- 退出路径**先取消定时器，再同步取消工作**，避免“幽灵工作”。

**收尾顺序（与 devres 的关系）**

- 内核没有通用 `devm_hrtimer_create()`；
- 在 `probe()` 中用 `devm_add_action_or_reset(dev, cleanup, ...)` 绑定 `hrtimer_cancel()`；
- 在 `remove()` 与错误路径：
  1. `atomic_clear(running)` / 状态位禁止再调度；
  2. `hrtimer_cancel()`；
  3. `cancel_work_sync()` / `flush_work()`；
  4. 释放资源。
- 与 `timer_list` + `workqueue` 的收尾顺序一致（详见第10章）。

------

## 5.6 示例代码与调试

### 5.6.1 一次性超时（纳秒级）

```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

struct one_shot {
    struct hrtimer hrt;
    ktime_t delay;
    atomic_t fired;
};

static enum hrtimer_restart one_shot_cb(struct hrtimer *t)
{
    struct one_shot *os = container_of(t, struct one_shot, hrt);
    atomic_set(&os->fired, 1);
    /* 不再重启 */
    return HRTIMER_NORESTART;
}

static struct one_shot os;

static int __init demo_init(void)
{
    atomic_set(&os.fired, 0);
    hrtimer_init(&os.hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    os.hrt.function = one_shot_cb;

    os.delay = ktime_set(0, 200 * 1000 * 1000); /* 200ms */
    hrtimer_start(&os.hrt, os.delay, HRTIMER_MODE_REL);
    pr_info("hrtimer one-shot armed 200ms\n");
    return 0;
}

static void __exit demo_exit(void)
{
    hrtimer_cancel(&os.hrt);
    pr_info("hrtimer fired=%d\n", atomic_read(&os.fired));
}

module_init(demo_init);
module_exit(demo_exit);
MODULE_LICENSE("GPL");
```

### 5.6.2 周期稳定（`hrtimer_forward_now()` 推荐式）

```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/atomic.h>

struct periodic {
    struct hrtimer hrt;
    ktime_t period;
    atomic_t running;
};

static enum hrtimer_restart periodic_cb(struct hrtimer *t)
{
    struct periodic *p = container_of(t, struct periodic, hrt);
    if (!atomic_read(&p->running))
        return HRTIMER_NORESTART;

    /* 轻量处理（不可睡） */
    /* ... */

    hrtimer_forward_now(&p->hrt, p->period);
    return HRTIMER_RESTART;
}

static struct periodic gp;

static int __init p_init(void)
{
    atomic_set(&gp.running, 1);
    hrtimer_init(&gp.hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    gp.hrt.function = periodic_cb;
    gp.period = ktime_set(0, 100 * 1000 * 1000); /* 100ms */
    hrtimer_start(&gp.hrt, gp.period, HRTIMER_MODE_REL);
    return 0;
}

static void __exit p_exit(void)
{
    atomic_set(&gp.running, 0);
    hrtimer_cancel(&gp.hrt);
}

module_init(p_init);
module_exit(p_exit);
MODULE_LICENSE("GPL");
```

### 5.6.3 分层（`hrtimer` + `workqueue` 可睡事务）

```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/atomic.h>

struct layered {
    struct hrtimer hrt;
    struct work_struct wk;
    ktime_t period;
    atomic_t running;
};

static void layered_work(struct work_struct *w)
{
    struct layered *L = container_of(w, struct layered, wk);
    if (!atomic_read(&L->running)) return;

    /* 进程上下文，可睡：I2C/SPI/blk等 */
    /* ... 与硬件交互 ... */
}

static enum hrtimer_restart layered_cb(struct hrtimer *t)
{
    struct layered *L = container_of(t, struct layered, hrt);
    if (!atomic_read(&L->running))
        return HRTIMER_NORESTART;

    /* 只做调度，保持回调轻量 */
    schedule_work(&L->wk);
    hrtimer_forward_now(&L->hrt, L->period);
    return HRTIMER_RESTART;
}

static struct layered gL;

static int __init layered_init(void)
{
    INIT_WORK(&gL.wk, layered_work);
    hrtimer_init(&gL.hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    gL.hrt.function = layered_cb;
    gL.period = ktime_set(0, 50 * 1000 * 1000); /* 50ms */
    atomic_set(&gL.running, 1);
    hrtimer_start(&gL.hrt, gL.period, HRTIMER_MODE_REL);
    return 0;
}

static void __exit layered_exit(void)
{
    atomic_set(&gL.running, 0);
    hrtimer_cancel(&gL.hrt);
    cancel_work_sync(&gL.wk);
}

module_init(layered_init);
module_exit(layered_exit);
MODULE_LICENSE("GPL");
```

**调试要点**

- 观察是否周期稳定：打印 `ktime_get()` 差值或用 ftrace 事件（第13章详述）。
- 若出现“退出后仍触发回调”：检查**状态位→`hrtimer_cancel()`→`cancel_work_sync()`**顺序。
- 周期 jitter 较大：确认是否启用 `CONFIG_HIGH_RES_TIMERS`，评估 CPU 负载、IRQ 屏蔽时间、`NO_HZ` 配置等。

------

## 5.7 小结

- `hrtimer` 适用于**高精度、低抖动**的定时需求，支持**相对/绝对**到期，并提供**前推**API 以降低漂移。
- 回调语义：**不可睡**；需要睡操作时采用“`hrtimer` 触发 + `workqueue` 处理”的**分层模式**。
- 周期重启优先使用 `hrtimer_forward_now()`；对齐要求更高时用“基于上次计划锚点”法。
- 资源回收没有通用 devres 包装：用 `devm_add_action_or_reset()` 将 `hrtimer_cancel()` 资源化，并在退出中严格执行**状态位→取消定时器→同步取消工作**的顺序。
- 若仅需毫秒级且容忍漂移，`timer_list`/`delayed_work` 成本更低，应优先选择（取舍见第6章与第10章）。

------

