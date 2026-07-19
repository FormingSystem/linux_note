---
id: knowledge.kernel_subsystems.irq.中断机制简介.p04_linux_驱动中的中断注册与接口
title: "Linux 驱动中的中断注册与接口"
kind: subsystem
status: evolving
domains:
  - linux
  - kernel
---

# 第4章\_Linux\_驱动中的中断注册与接口

本章是在前面三章的基础上继续往“能写驱动”这一步走的。

- 第1章解决了“为什么一定要有中断”；
- 第2章解决了“在一块 SoC 上中断为什么一定是分层的、为什么要有中断控制器”；
- 第3章解决了“Linux 为什么要用 irq_chip / irq_domain / irq_desc 这三层来把杂乱的硬件中断变成统一的 Linux IRQ”。

到了本章，我们终于可以站到“驱动开发者”的视角，用上一章准备好的那条抽象链，来讨论：**驱动到底怎么拿 IRQ 号、用哪个注册函数、要不要用 devm、什么时候用 threaded IRQ、共享中断怎么写、怎么把 GPIO 中断也变成可用的 IRQ。**

------

## 4.1\_章节内容说明

本章分成几条线并行展开：

1. **获取 IRQ 号的所有常见路径**：不仅有 `platform_get_irq()`，还要讲字符设备/misc 设备怎么拿中断号，把“我没有 platform_device”这个坑一次说清；
2. **核心注册接口的语义**：`request_irq()`、`devm_request_irq()`、`request_threaded_irq()` 到底各管什么，什么时候该选哪一个；
3. **标志位与共享中断**：`IRQF_SHARED`、`IRQF_ONESHOT`、触发类型的再次钉定；
4. **配套接口**：`enable_irq()` / `disable_irq_nosync()` / `irq_set_irq_type()` 这些在 GPIO/去抖场景下一定会遇到的要一并讲；
5. **一个完整的、可以落地的示例**：GPIO 按键中断，从 DTS 到注册到 handler 到调试。

这样写的目的，是让读者看完这一章就能写出一个真正能在板子上“按一下就进中断”的驱动，而不是只停留在“我知道有个 request_irq()”。

------

## 4.2\_驱动视角下的中断处理路径回顾

在进具体 API 之前，先把上一章的路径用驱动的口吻再说一遍，免得弄混：

1. **硬件世界**里有很多中断源（GIC、GPIO irqchip、外设自己的中断）；
2. Linux 启动时，用 **irq_domain** 把这些“硬件号”翻成一个个统一的 **Linux IRQ（整数）**；
3. 每一个 Linux IRQ 在内核里都有一个 **irq_desc** 作为它的“运行时家”；
4. 驱动调用 **request_irq()/request_threaded_irq()**，其实就是“往 irq_desc 上挂一个自己的处理函数（irqaction）”；
5. 真正要去 mask/unmask/ack 的时候，内核会去找这条中断线所绑定的 **irq_chip**，由它去操作真实的中断控制器。

所以，从驱动的视角看，**你只需要两件事**：

1. 拿到一个“正确的、已经被内核映射过的 Linux IRQ 整数”；
2. 把你想做的处理函数挂上去。

本章就按这两件事来写。

------

## 4.3\_获取\_IRQ\_号的几种路径

这一小节专门把“怎么拿到那个 IRQ 整数”说全，因为这一步经常让人卡住，尤其是“我写了一个纯字符设备，怎么也拿不到 platform_get_irq()”。

### 4.3.1\_标准场景\_platform\_设备\_用\_platform\_get\_irq()

**前提**：你的设备是 DTS/ACPI 里描述出来的，内核已经把它变成了一个 `struct platform_device`，也就是你是在 `.probe()` 里写代码。

**写法**：

```c
static int my_probe(struct platform_device *pdev)
{
    int irq, ret;

    irq = platform_get_irq(pdev, 0);     /* 取第0个中断 */
    if (irq < 0)
        return irq;

    ret = devm_request_irq(&pdev->dev, irq,
                           my_isr_thread_func, 0,
                           dev_name(&pdev->dev), pdev);
    if (ret)
        return ret;

    return 0;
}
```

**特点**：

- IRQ 信息是跟设备描述（DTS/ACPI）绑在一起的；
- 不需要自己解析设备树；
- 可以直接用 devm 版本，资源管理省心；
- 嵌入式平台驱动首选这种方式。

------

### 4.3.2\_同样是\_platform\_但要\_再钉一次类型

DTS 里一般已经写了触发方式，但驱动也常常这么做：

```c
irq = platform_get_irq(pdev, 0);
if (irq < 0)
    return irq;

/* 确保和硬件手册一致 */
irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);
```

**原因**：DTS 是“板子怎么接的”，驱动是“设备需要什么触发语义”，两头对一下是最稳的；很多 GPIO→GIC 的板子恰恰是这里不一致才导致“只来一次”或者“疯了一样来”。

------

### 4.3.3\_字符设备\_/\_misc\_设备\_用\_of\_irq\_get()

**场景**：你注册的是 `miscdevice` / `cdev`，但这个设备在 DTS 里是有一个节点的。那就可以这样拿：

```c
static struct miscdevice my_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "mychardev",
    .fops  = &my_fops,
};

static int __init my_init(void)
{
    int ret, irq;
    struct device *dev;

    ret = misc_register(&my_misc);
    if (ret)
        return ret;

    dev = my_misc.this_device;           /* misc 会给你一个 struct device */
    irq = of_irq_get(dev->of_node, 0);   /* 从 DTS 拿第0个中断 */
    if (irq < 0)
        return irq;

    ret = devm_request_irq(dev, irq, my_isr, 0,
                           dev_name(dev), dev);
    return ret;
}
```

**要点**：

- 前提是你的设备 **真的** 有 DTS 节点；
- 有了 `struct device *`，就能走 `devm_` 路线；
- 这是“不是 platform_driver，但我就是想从设备树拿中断”的标准写法。

------

### 4.3.4\_没有\_struct\_device\_但知道\_DTS\_路径\_用\_of\_find\_node\_*\_+\_irq\_of\_parse\_and\_map()

**场景**：老项目、实验模块、不想/不能重构成 platform 驱动，但又想从 DTS 拿 IRQ。

```c
struct device_node *np;
int irq;

np = of_find_node_by_path("/soc/my-int-demo");
if (!np)
    return -ENODEV;

irq = irq_of_parse_and_map(np, 0);
if (!irq)
    return -EINVAL;

ret = request_irq(irq, my_isr, 0, "my-int-demo", NULL);
```

**缺点**：

- 硬编码 DTS 路径，可移植性差；
- 不能自动释放（除非你自己封装）；
- 适合板级小工具、临时驱动。

------

### 4.3.5\_最兜底\_模块参数传\_IRQ

**场景**：完全没 DTS，也不是 platform，就想测一下一条中断。

```c
static int irq = -1;
module_param(irq, int, 0444);

static int __init my_init(void)
{
    if (irq < 0)
        return -EINVAL;
    return request_irq(irq, my_isr, 0, "my-test", NULL);
}
```

这不是工程推荐方案，但说明了一个原则：**IRQ 号必须来自“能描述硬件”的那一层，而不是“字符设备”这个纯软件抽象。**

------

### 4.3.6\_获取\_IRQ\_号的选择顺序

可以总结成一张小表：

| 层次/场景               | 推荐接口                                    |
| ----------------------- | ------------------------------------------- |
| 标准 platform 驱动      | `platform_get_irq()`                        |
| misc/cdev 但有 DTS 节点 | `of_irq_get(dev->of_node, idx)`             |
| 只有 DTS 路径           | `of_find_node_*` + `irq_of_parse_and_map()` |
| 调试/通用工具           | 模块参数传 IRQ                              |

记住一句话就够：**想用得像 platform，就给自己造一个能看到 DTS 的 struct device。**

------

## 4.4\_中断注册的核心接口

拿到 IRQ 号以后，下一步就是“把我的处理函数挂上去”。Linux 给的选择不止一个，我们分三档讲。

### 4.4.1\_最原始的\_request\_irq()

```c
int request_irq(unsigned int irq,
                irq_handler_t handler,
                unsigned long flags,
                const char *name,
                void *dev);
```

- `irq`：刚才那几节拿到的 Linux IRQ 号；
- `handler`：硬中断处理函数（上半部），不能睡；
- `flags`：`IRQF_SHARED`、触发类型、`IRQF_ONESHOT` 等；
- `name`：出现在 `/proc/interrupts` 里的名字；
- `dev`：共享中断时用来区分不同设备的 cookie。

适合：**简单、不能睡、时间短** 的处理；也适合你还没想好要不要线程化的时候。

------

### 4.4.2\_更省心的\_devm\_request\_irq()

```c
int devm_request_irq(struct device *dev,
                     unsigned int irq,
                     irq_handler_t handler,
                     unsigned long irqflags,
                     const char *devname,
                     void *dev_id);
```

差别只有一个：**挂在 devres 上，设备移除时自动 free_irq**。
 在平台驱动、绑定了 `struct device` 的 misc 驱动里，优先用这个，少写一堆 remove 代码。

------

### 4.4.3\_能线程化的\_request\_threaded\_irq()

```c
int request_threaded_irq(unsigned int irq,
                         irq_handler_t handler,     /* 可以为 NULL */
                         irq_handler_t thread_fn,   /* 必须能睡 */
                         unsigned long flags,
                         const char *name,
                         void *dev);
```

用来干什么：

- 上半部（handler）里只做“确认/关 IRQ/排队”；
- 下半部（thread_fn）里做“能睡的、慢的、要调用别的子系统的”；
- 配合 `IRQF_ONESHOT`，可以做到：**中断线在下半部没做完之前不会重新进来**，解决了“中断线程里 `msleep()` 把整条线拖死”的问题。

这是后面你要写“去抖 + hrtimer + 再开中断”一类方案时最常用的组合。

------

## 4.5\_常用配套接口

这些接口跟“注册”是绑在一起要讲的，不然你会写出“一进中断就再也出不来了”的代码。

1. **`enable_irq(irq)` / `disable_irq(irq)`**
   - 完整同步屏蔽/打开；
   - `disable_irq()` 会等正在处理的中断结束。
2. **`disable_irq_nosync(irq)`**
   - 在中断上下文里常用；
   - 不会等当前中断处理完再返回，适合“先关上，等会儿自己再开”的去抖方案。
3. **`irq_set_irq_type(irq, type)`**
   - 在驱动里“再钉一次类型”用；
   - DTS 写了也可以再设，确保跟外设手册一致；
   - 常用值：`IRQ_TYPE_EDGE_RISING` / `IRQ_TYPE_EDGE_FALLING` / `IRQ_TYPE_LEVEL_HIGH` / `IRQ_TYPE_LEVEL_LOW`。
4. **`free_irq(irq, dev_id)`**
   - 非 devm 路线必须调用；
   - `dev_id` 要和你 request 时的一样，才能只释放你这一个 handler。

------

## 4.6\_示例\_GPIO\_按键中断驱动(平台版)

这是一个最贴近你板子场景的写法，省略错误处理后大致如下：

```c
static irqreturn_t key_irq_handler(int irq, void *dev_id)
{
    /* 1. 读状态/排队/唤醒 */
    /* ... */
    return IRQ_HANDLED;
}

static int key_probe(struct platform_device *pdev)
{
    int irq;

    irq = platform_get_irq(pdev, 0);
    if (irq < 0)
        return irq;

    /* 再钉一次触发类型，防止 DTS 写得不一致 */
    irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);

    return devm_request_irq(&pdev->dev, irq,
                            key_irq_handler,
                            0,
                            dev_name(&pdev->dev),
                            pdev);
}
```

如果你要在线程里做去抖，就换成：

```c
return devm_request_threaded_irq(&pdev->dev, irq,
                                 NULL,            /* 不需要硬中断部分 */
                                 key_irq_thread,  /* 能睡 */
                                 IRQF_ONESHOT,
                                 dev_name(&pdev->dev),
                                 pdev);
```

------

## 4.7\_调试与排错要点(跟本章内容相关的部分)

1. **拿错 IRQ**：`platform_get_irq()` 返回负数；或拿到的号在 `/proc/interrupts` 里根本没动 → 看 DTS / 看 `interrupt-parent`；
2. **触发类型不对**：能进一次后面不进了 → 很可能是边沿/电平没对上，驱动里再 `irq_set_irq_type()` 一次；
3. **中断线程不返回**：用了 `IRQF_ONESHOT`，但线程里 `msleep(100)` 甚至不 return → 这条中断线就一直被锁住；
4. **共享中断没判别**：多个设备共用一条中断，handler 里不判断是不是自己的，所有人都说 `IRQ_HANDLED` → 另一个设备就没法判断了；
5. **没 devm 就忘记 free**：模块卸载/设备 remove 后中断还在，下一次 request 报错 → 用 devm。

------

## 4.8\_小结

- 想要“像 platform_get_irq() 那样一句话拿到中断”，前提就是：**你的设备要被内核当成一个真正的设备**，也就是你要有 `struct device`；字符设备本身是不带这个能力的；
- 获取 IRQ 的路径可以合在一张图里理解：
   **platform (最推荐) → of_irq_get(已有 device) → irq_of_parse_and_map(知道 DTS) → 模块参数(调试)**；
- 注册接口至少要掌握三个：`request_irq()`、`devm_request_irq()`、`request_threaded_irq()`，其中 threaded + `IRQF_ONESHOT` 是解决“中断里要做慢事”的标准手段；
- 配套接口（`disable_irq_nosync()` / `irq_set_irq_type()`）要跟着讲，不然你一旦写到 GPIO/去抖，就会遇到“只进一次/一直进”的经典坑；
- 下一章就可以专讲**触发类型、电平 vs 边沿、为什么要清中断源、为什么 GPIO 场景里 DTS 写的 ACTIVE_LOW 不是中断的触发极性**，也就是把“我们申请到了一个 IRQ”这件事放到实际硬件语义里去看。