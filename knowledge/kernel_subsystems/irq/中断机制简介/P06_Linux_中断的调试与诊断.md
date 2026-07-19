---
id: knowledge.kernel_subsystems.irq.中断机制简介.p06_linux_中断的调试与诊断
title: "Linux 中断的调试与诊断"
kind: subsystem
status: evolving
domains:
  - linux
  - kernel
---

# 第6章\_Linux\_中断的调试与诊断

> 这一章的定位和前面几章不一样：前面是在“会写”，这一章是在“怎么确认它真的在跑、跑得对不对、哪一步卡了”。很多人卡在“我确信我写对了，但板子就是没反应”这一步，本章就是解决这个阶段的问题。

------

## 6.1\_章节内容说明

本章围绕三个问题展开：

1. **怎么确认真的进中断了？**（别还没确认就去怀疑 GIC、怀疑时钟、怀疑驱动）
2. **怎么判断是“根本没进”还是“进了但被拖住了”？**（这就是你前面那个 IRQF_ONESHOT + printk 的典型场景）
3. **怎么快速定位是 DTS/引脚复用/irqdomain/驱动回调 哪一层有问题？**

本章的视角仍然是“驱动开发者视角”，不从内核开发者抽象层讲 irq_desc 的细节实现，而是从“我现在这块板子这根 GPIO 中断”为起点，一层一层往上排查。

后面内容会跟第5章的 5.8 综合案例打通：我们将用第5章那三种写法来解释为什么有的版本 `/proc/interrupts` 能看到中断计数在涨、有的版本能进回调但计数不涨、有的版本看起来像“死在中断线程里了”。

------

## 6.2\_第一观察口\_/proc/interrupts

这是所有中断调试的**起点**，不是可选项。

```bash
cat /proc/interrupts
```

你能看到的典型字段大概是这样（ARM 板上会略有不同）：

```text
           CPU0  CPU1
 25:        30     0  GICv2  29  ...  gpio1
 46:         0     0  GICv2  56  ...  timer
...
```

重点看四件事：

1. **有没有这一号？**
    你驱动里最后申请到的 IRQ 号（比如 46），在这里要能看到；如果没有，先别怀疑驱动，先怀疑“你拿到的那个号根本就不是内核里注册的那个号”（常见于 DTS 写错 interrupt-parent / interrupts）。
2. **计数会不会涨？**
    按一次键，这一行的计数要 +1；如果不涨，说明硬件没进中断（或者进了别的号）。
3. **是不是在别的 CPU 上涨？**
    SMP 下有时中断会跑到别的 CPU，上面你只看 CPU0 的列，就以为没进，其实在 CPU1 那列在涨。
4. **名字对不对？**
    你 request_irq()/devm_request_irq() 时给的名字，会出现在最后一列；如果没出现，说明你的 handler 根本没挂上去，可能被别的共享中断盖掉了。

⚠️ 你之前提到过：

> `cat /proc/irq/$IRQ/type` 报 “No such file or directory”

这个很典型：**不是所有内核都会把触发类型都暴露成 `/proc/irq/N/type`**，很多时候你根本看不到这一项。所以不要把“没有这个文件”当成“没有这个中断”。这就是为什么我们一定要从 `/proc/interrupts` 开始——它是“有没有进中断”的最低保真度入口。

------

## 6.3\_第二观察口\_/proc/irq\_/sys/kernel/irq\_与\_看不到\_type\_怎么办

很多人一上来就：

```bash
cat /proc/irq/46/type
```

结果是：

```text
cat: can't open '/proc/irq/46/type': No such file or directory
```

然后就开始怀疑“是不是定时器坏了”“是不是 GIC 不支持”——这一步的结论是错的。

要点是：

1. `/proc/irq/N/...` 是内核按“有没有导出”来决定的，不是每个控制器都导；

2. gpio → irqchip → GIC 这种级联结构里，GPIO 这层的触发类型可能只在 GPIO 控制器自己那一层生效，不一定在通用的 `/proc/irq` 下面能看到；

3. 你要确认触发类型，**最好还是在驱动里再钉一次**：

   ```c
   irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);
   ```

   这就是我们在第5章所有示例里都要做的事。

如果你真的想确认当前平台有哪些 IRQ、怎么映射，也可以去看：

```bash
ls -R /sys/kernel/irq
```

有的 SoC 会在这里给出更多调试信息，比 `/proc/irq` 更全。

------

## 6.4\_第三观察口\_引脚复用和\_GPIO\_状态

很多“中断没进”的原因其实根本不是中断，是**引脚没复用到 GPIO/IRQ 功能**。所以要配套看下面几个点：

1. **pinctrl 是否真的生效**
    你在驱动里已经做了这一句：

   ```c
   devm_pinctrl_get_select_default(dev);
   ```

   这是对的，能减少“还没复用中断就到了”的概率。

2. **看 debugfs 的 pin 配置**
    如果你的内核打开了 pinctrl 的 debugfs，可以：

   ```bash
   ls /sys/kernel/debug/pinctrl/*/pinmux-pins
   ```

   看这个 pin 到底是不是你想要的那个功能。

3. **用 gpioinfo 看当前 GPIO 的方向/占用/标志**
    （需要 libgpiod 工具）

   ```bash
   gpioinfo
   ```

   看看你这个 gpio1_18（或者对应的数字）到底是不是 input、是不是被内核占了、是不是被别的驱动抢走了。

结论：
 “中断不进”里，**至少有一半**是 pin / DTS / mux 没落地，不是中断子系统的问题。

------

## 6.5\_区分两类\_没反应\_根本没进\_vs\_进了但卡在线程里

这是你前面那三个驱动示例要解决的核心点，所以在调试章必须把这两类分开说。

### 6.5.1\_根本没进

表现：

- `/proc/interrupts` 不涨
- dmesg 没你的 dev_info
- 驱动断点/trace 都看不到
- 重复按很多次都没用

这种情况要往下排查：DTS → pinctrl → interrupt-parent → interrupts → irq_domain

### 6.5.2\_进了\_但卡在线程里

表现：

- `/proc/interrupts` 会涨（至少第一次会涨）
- 驱动的 hardirq 能打印（如果你在 hardirq 里打了 pr_info）
- 但后面“就是不再响应了”
- 你用的是：**request_threaded_irq + IRQF_ONESHOT + 串口很慢的 printk**

和第5.8节的第二个示例对上了。

所以这里给一个非常实用的排查套路：

1. 在线程 handler 的**第一行**先把 `printk()` 改成 `trace_printk()`
2. 再试几次按键
3. 如果变得灵了，就说明你原来就是 console 慢
4. 如果还是不灵，再看是不是你把“去抖的时间窗”写在线程里了（我们第5章反复说过，“写在线程里”的去抖，在 ONESHOT 下会变成“这条 IRQ 在你线程还没走完之前都不会再进”）

------

## 6.6\_高阶手段\_trace\_printk\_/\_ftrace\_/\_irqsoff

真正要查“是不是中断路径里有慢操作”的时候，`trace_printk()` 是最省事的：

1. 不堵 console；
2. 能在 `/sys/kernel/debug/tracing/trace` 里看到；
3. 能看到时间差。

在我们第5.8.5 的“推荐版”里，你已经看到了典型写法：

```c
if (atomic_xchg(&lk->pending, 0)) {
    trace_printk("... delta=%lums\n",
                 jiffies_to_msecs(now - lk->last_edge_j));
    ...
}
```

如果你要更狠的，还可以开 irqsoff / function tracer，看是不是线程化中断后面跑了别的路径；但这部分可以放到“内核性能调试”里，不一定要在驱动篇铺开。

------

## 6.7\_常见误区汇总

1. **把 printk 当作“无害操作”写进 threaded IRQ**
    在慢串口上绝对不是无害的。
2. **以为 `/proc/irq/N/type` 没有就说明中断没注册**
    不是，没有这个节点只能说明它没导出来，先看 `/proc/interrupts`。
3. **在 threaded IRQ 里做 msleep / 延时去抖**
    ONESHOT 下就是“整条 IRQ 等你睡完”。
4. **DTS 写了下降沿就以为一定是下降沿**
    不一定，驱动最好再钉一次 `irq_set_irq_type()`，特别是 GPIO → GIC 级联时。
5. **不看引脚复用就怀疑 GIC**
    至少一半的问题是 pin 没复用好。

------

## 6.8\_小结

- 调试中断，一定从 `/proc/interrupts` 开始；
- 确认能不能进中断，要用“硬中断里直接翻 LED”这种最短路径的版本；
- 确认“为什么进了一次就不进了”，要看你是不是用了 `request_threaded_irq()` + `IRQF_ONESHOT` + 任意慢操作（printk、msleep、i2c 读写）；
- 真正推荐的 GPIO 按键中断写法，是：**硬中断锁存 + 线程消费 + trace_printk + DTS 可配置的去抖窗口**；
- 没有 `/proc/irq/N/type` 不要慌，驱动里手动钉一次触发类型即可。

------

如果你要，我们下一章可以进到：

**第7章 中断下半部机制与驱动中的选择：threaded IRQ、tasklet、workqueue 的取舍**
 保持同样的排版。