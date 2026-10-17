---
id: research.source_reading.gpiolib.irq_mapping_simulation
title: "GPIO映射与模拟事件怎样交接"
kind: source
status: evolving
domains: [linux, kernel, driver]
---

# 第5章\_GPIO映射与模拟事件怎样交接

取得GPIO描述符只建立线路使用关系，取得Linux IRQ则把它接到另一套事件管理对象。本页先追踪 `gpiod_to_irq()` 的能力保护和返回值，再追踪gpio-sim的模拟事件如何变成IRQ调用。先读 [事件模块导读](../navigation/P04_GPIO事件怎样从线路到达处理者.md#4.1_先分开配置路径与通知路径)，版本与配置见 [总索引](../navigation/P01_Linux_6.12_GPIO源码阅读索引.md#1.1_版本身份与阅读路线)。

本页源码来自NXP Linux 6.12.20固定提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 的 `drivers/gpio/gpiolib.c` 与 `kernel/irq/irq_sim.c`；中文Doxygen为仓库补充，摘录沿用原文件许可。它们不是所有GPIO控制器统一的事件实现。

## 5.1\_取得IRQ为何仍需保护可撤销的Provider

已经请求的desc可以长期持有，但gdev上的chip能力可能正在退出。to_irq不能仅凭desc存在就直接跳入旧回调。

```c
/**
 * @brief 在gdev的SRCU保护内调用映射能力，保留错误和延迟探测语义。
 * @note 仓库补充中文说明；原始语句来自固定版本，非独立可编译示例。
 */
int gpiod_to_irq(const struct gpio_desc *desc)
{
	struct gpio_device *gdev;
	struct gpio_chip *gc;
	int offset;

	/*
	 * Cannot VALIDATE_DESC() here as gpiod_to_irq() consumer semantics
	 * requires this function to not return zero on an invalid descriptor
	 * but rather a negative error number.
	 */
	if (IS_ERR_OR_NULL(desc))
		return -EINVAL;

	gdev = desc->gdev;
	/* FIXME Cannot use gpio_chip_guard due to const desc. */
	guard(srcu)(&gdev->srcu);
	gc = srcu_dereference(gdev->chip, &gdev->srcu);
	if (!gc)
		return -ENODEV;

	offset = gpio_chip_hwgpio(desc);
	if (gc->to_irq) {
		int retirq = gc->to_irq(gc, offset);

		/* Zero means NO_IRQ */
		if (!retirq)
			return -ENXIO;

		return retirq;
	}
#ifdef CONFIG_GPIOLIB_IRQCHIP
	if (gc->irq.chip) {
		/*
		 * Avoid race condition with other code, which tries to lookup
		 * an IRQ before the irqchip has been properly registered,
		 * i.e. while gpiochip is still being brought up.
		 */
		return -EPROBE_DEFER;
	}
#endif
	return -ENXIO;
}
```

第一步特意不用通常的desc校验宏，因为这个接口不能把无效描述符返回成0；NULL和错误指针统一为 `-EINVAL`。第二步取gdev，在其srcu读侧保护下读取chip；chip已空就返回 `-ENODEV`。保护覆盖to_irq回调调用，避免控制器撤销与回调使用失去交接。

第三步 `gpio_chip_hwgpio(desc)` 取得该控制器内offset，交给Provider。回调返回0表示没有可用IRQ，转成 `-ENXIO`；其他结果原样返回。若没有to_irq，但启用GPIOLIB_IRQCHIP且irqchip已开始建立，则返回 `-EPROBE_DEFER`，保留“依赖尚未就绪”的可能性；其余无能力情形返回 `-ENXIO`。

整个函数没有设置输入方向、请求handler或配置触发类型。Provider的回调也未被限定成纯查询：gpio-sim调用 `irq_create_mapping()`，其唯一展开在 [模拟器映射入口](P04_模拟控制器的状态与回调.md#4.3_为什么释放后读到的值可能改变)。Consumer还必须完成IRQ申请并检查返回值，不能把一个正数当作事件已经可用。

## 5.2\_模拟中断把待处理状态放在哪里

真实GPIO可能有硬件锁存位，模拟器没有这个硬件。`irq_sim.c` 分开保存整个域的待处理位图和每条IRQ的使能状态：

```c
/**
 * @brief 域的pending位图与每IRQ的enabled状态不是gpio-sim的value位图。
 * @note 仓库补充中文说明；原始语句来自固定版本，非独立可编译示例。
 */
struct irq_sim_work_ctx {
	struct irq_work		work;
	unsigned int		irq_count;
	unsigned long		*pending;
	struct irq_domain	*domain;
	struct irq_sim_ops	ops;
	void			*user_data;
};

struct irq_sim_irq_ctx {
	bool			enabled;
	struct irq_sim_work_ctx	*work_ctx;
};
```

`irq_sim_work_ctx.pending` 是按局部中断号索引的待处理位图，`irq_count`限定范围；`domain`保存局部身份到Linux IRQ的映射，`work`用于排队irq_work。`ops`与 `user_data` 保存模拟器可选的资源申请/释放钩子以及私有对象。每条IRQ的 `irq_sim_irq_ctx` 保存enabled并回指上述域状态，mask/unmask回调分别把enabled设为false/true。

因此至少有三种不同状态：gpio-sim的value表示当前模拟线路值，pending表示待交付通知，IRQ action表示Consumer安装的处理者。value变化不等于每次都新增一个独立通知；只要pending是位图，同一位在被消费前重复置1就不能记录发生次数。

## 5.3\_从输入变化到irq\_work再到处理者

gpio-sim的apply_pull在符合边沿条件时请求置pending，随后由irq_sim处理。输入、方向和pull的判断已经在 [模拟器实现](P04_模拟控制器的状态与回调.md#4.3_为什么释放后读到的值可能改变) 展开，此处从交接点继续。

```c
/**
 * @brief 只有enabled时才更新pending；置真后排队irq_work。
 * @note 仓库补充中文说明；原始语句来自固定版本，非独立可编译示例。
 */
static int irq_sim_set_irqchip_state(struct irq_data *data,
				     enum irqchip_irq_state which, bool state)
{
	struct irq_sim_irq_ctx *irq_ctx = irq_data_get_irq_chip_data(data);
	irq_hw_number_t hwirq = irqd_to_hwirq(data);

	switch (which) {
	case IRQCHIP_STATE_PENDING:
		if (irq_ctx->enabled) {
			assign_bit(hwirq, irq_ctx->work_ctx->pending, state);
			if (state)
				irq_work_queue(&irq_ctx->work_ctx->work);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
```

`which`必须是pending状态类型，否则返回 `-EINVAL`。enabled为false时既不更新位图也不排队，但函数仍返回0。故不能把“接口调用成功”翻译为“事件已经排队”。enabled为true时，`assign_bit`按hwirq写位，置真才调用 `irq_work_queue()`；置假是清位，不新增处理任务。

域创建使用 `IRQ_WORK_INIT_HARD` 初始化这份work，后续执行下面的处理函数。它把待处理身份交给IRQ核心，而不是在持有gpio-sim的mutex时直接调用Consumer读取GPIO。

```c
/**
 * @brief 消费pending位，按域查找Linux IRQ并执行simple流处理。
 * @note 仓库补充中文说明；原始语句来自固定版本，非独立可编译示例。
 */
static void irq_sim_handle_irq(struct irq_work *work)
{
	struct irq_sim_work_ctx *work_ctx;
	unsigned int offset = 0;
	int irqnum;

	work_ctx = container_of(work, struct irq_sim_work_ctx, work);

	while (!bitmap_empty(work_ctx->pending, work_ctx->irq_count)) {
		offset = find_next_bit(work_ctx->pending,
				       work_ctx->irq_count, offset);
		clear_bit(offset, work_ctx->pending);
		irqnum = irq_find_mapping(work_ctx->domain, offset);
		handle_simple_irq(irq_to_desc(irqnum));
	}
}
```

`container_of`从嵌入的work找回域状态。循环找置位的局部offset，清pending，查Linux IRQ，再把对应desc交给 `handle_simple_irq()`。这里的清pending不是清真实外设寄存器；父控制器也不是真实GIC。申请的是线程化Consumer时，IRQ核心进一步调度其线程，线程中的可睡眠GPIO读取属于后续步骤。

此函数只表达固定模拟实现的分派流程，不能据此承诺完整边沿计数：pending位可以合并事件，屏蔽期间置pending请求会被忽略，处理者读取时线路也可能已再次变化。本章实验逐次注入并观察，避免把大量快速写入的次数当成必然相等的IRQ次数。

## 5.4\_映射建立和handler安装仍是两个阶段

irq_sim的domain映射回调在Linux IRQ上安装模拟irqchip和流处理函数：

```c
/**
 * @brief 建立每IRQ私有状态与simple流处理；尚未注册业务action。
 * @note 仓库补充中文说明；原始语句来自固定版本，非独立可编译示例。
 */
static int irq_sim_domain_map(struct irq_domain *domain,
			      unsigned int virq, irq_hw_number_t hw)
{
	struct irq_sim_work_ctx *work_ctx = domain->host_data;
	struct irq_sim_irq_ctx *irq_ctx;

	irq_ctx = kzalloc(sizeof(*irq_ctx), GFP_KERNEL);
	if (!irq_ctx)
		return -ENOMEM;

	irq_set_chip(virq, &irq_sim_irqchip);
	irq_set_chip_data(virq, irq_ctx);
	irq_set_handler(virq, handle_simple_irq);
	irq_modify_status(virq, IRQ_NOREQUEST | IRQ_NOAUTOEN, IRQ_NOPROBE);
	irq_ctx->work_ctx = work_ctx;

	return 0;
}
```

这里分配每IRQ的context，设置irqchip、chip data和 `handle_simple_irq`，调整可申请/自动启用等状态，再关联work_ctx。`irq_modify_status` 的clear参数移除IRQ_NOREQUEST与IRQ_NOAUTOEN，set参数增加IRQ_NOPROBE。它没有安装本章教学模块的 `gpio_irq_thread`；该线程由随后 `devm_request_threaded_irq()` 提交。

Consumer申请前必须准备好回调将读取的state和输入描述符，因为注册过程可能使事件立即进入。解除设备时则反过来：先释放IRQ并等待在途处理完成，再释放描述符，最后释放state。托管IRQ的具体资源登记与释放复用 [devres中断导读](../../devres/navigation/P03_内存映射与中断资源导读.md)，不能另造一套“devm会处理所有异步工作”的保证。若handler还投递了独立work或timer，Consumer仍需显式取消并等待它们。

修改验证可以从三个问题入手：若把to_irq的负数强转为无符号IRQ，会怎样破坏错误处理？若在irq_sim屏蔽时连续注入多次变化，pending为何不等于计数？若把可睡眠GPIO读取放到硬中断主处理者里，为什么irq_work已经异步仍不能保证该调用合法？答案分别是错误身份被当作编号、该实现的使能与位图规则不保存全部历史、硬中断执行上下文仍禁止睡眠。

原件定位：[gpiolib.c的gpiod_to_irq](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/drivers/gpio/gpiolib.c#L3690)、[irq_sim.c](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/kernel/irq/irq_sim.c)。回到 [事件模块导读](../navigation/P04_GPIO事件怎样从线路到达处理者.md#4.3_模拟事件怎样完成通知闭环) 或 [GPIO主线](../../../../knowledge/driver_model/gpio/P07_GPIO_中断桥接与事件传播.md)。
