# 第27章　生命周期与引用（kobject/device、devres、kref、get/put）

## 章节内容说明

> 本章从**kobject/device**为入口，因为这是 Linux 设备模型的“存在点”。
>  然后才引出 **devres** —— “资源随 device 生命周期自动收尾”的自动化层。
>  最后落点 **kref / get_put 引用计数** —— device 能“存活多久”的数学。

DMA（第26章）解决的是“这段 buffer 在这轮 DMA 中谁是 owner”。
 本章解决的是 ——
 **“这个 device / driver 实例本身，能活多久、谁在管、谁能放”**。

本章结构依旧 8 步：

- step1 是什么
- step2 干什么
- step3 怎么实现
- step4 怎么用
- step5 表格组
- step6 对比避免
- step7 完整示例
- step8 总结

------

## 27.1　step1：是什么（历史/定位）

> kobject 是 Linux 设备模型的“基础元素”。
>  它不是“一个结构体”，而是“内核统一管理对象生命周期的根节点”。

**kobject 的本体：**

| kobject 是？                 | 正语义                                     |
| ---------------------------- | ------------------------------------------ |
| 不是“文件系统目录”           | 那只是 sysfs 的投影                        |
| 不是“device 的属性”          | 相反，device 是“挂在 kobject 上的高阶实体” |
| 它是“生命周期框架的统一父类” | 所有“可被内核引用与释放的对象”的基础抽象   |

> kobject = 能被 **引用计数**、能出现在 sysfs、能被统一的“存在/销毁”规则管理的内核对象。

device 为什么必须依附 kobject？
 因为 device 是“在系统里有存在感”的东西：

- 可以从总线被 enumerate 出来
- 可以被 open()/close() 交互
- 可以在线插拔 / 移除
- 可以被其他 subsystem 持有引用

**所有这些都要求：**
 device 必须是一个**可被 reference 追踪的实体**。
 因此：
 `struct device` 内部有一个 `struct kobject kobj` 成员。

> device 并不是 “在 C struct 上直接 new 出来的”。
>  device 是”一个 kobject 包装出的生命周期语义实体”。

这一点是本章根底：
 **device 生命周期在“引用计数框架”里，不在“driver 自己管 malloc/free”里。**

------

> 下一节（27.2 step2）
>  我将回答一句话总结：

- 为什么 device 不允许你“直接 free”它？
- 为什么所有子系统都必须“通过 get/put”延长/回收 device 的存在期？



------

## 27.2　step2：干什么（要解决的问题）

一句话：
 **device 不能被“malloc/free”式地随便 new / free，它必须被严格的引用计数机制约束。**

因为 device 不是“代码里的一块内存”，
 device 是整个内核设备模型生态里的 **存在点（identity）**：

- 可以被 driver 框架引用
- 可以被 class 框架引用
- 可以被 sysfs 投影引用
- 可以被 open() 的 file_handle 持有引用
- 可以被异步工作（workqueue / dmaengine）持有引用
- 可以被 cdev、misc、netdev 等子系统持有引用
- 可以被 secondary path 意外延迟持有（例如异步 completion ）

### 所以需要解决什么？

| 问题                              | 为什么 “free(dev)” 不够         |
| --------------------------------- | ------------------------------- |
| 你无法知道它是否 **仍被别处持有** | kobject/device 是“多入口”可访问 |
| 你无法知道 **引用何时减少到 0**   | 有可能 completion、WQ 未 drain  |
| 你无法知道 **什么时候能安全销毁** | 移除 vs fs操作 vs 异步 I/O race |

如果你直接 free，
 你不是释放内存，
 你是在“拆掉一个可能仍被其他子系统引用的 identity”。

→ 这会使任何延迟访问路径直接读取 UAF（use-after-free）。

### Linux device model 的答案

1. **kobject** 提供统一的 “存在计数（kref）”
2. **get/put** 是引用计数的接口语义
3. **devres** 提供 “当 device 生命周期结束时 → 自动释放已登记的资源”
4. 整套东西统一保证：
    device **何时诞生**、**何时存在**、**何时被析构**
    → 不是 driver 自己决定，是引用计数决定

> 简洁且极重要的一句话：

> **device 的生命周期 = “引用计数＝0” 的时刻**
>  不是 “remove() 返回的时刻”

→ remove() 不等于 free()

------

下一节开始进入 step3：
 **27.3 怎么实现（底层原理 & kobject/kref 真实语义 & 为什么不是 atomic_t 简单计数）**

这节会把 `struct kobject` / `struct device` / `struct kref` 三个东西在结构成员层面，
 **一张对照表 + 内核结构体展开**来讲 ——
 从“本质”说明为什么引用计数不是普通整数。



------

## 27.3　step3：怎么实现（底层原理、处理逻辑）

### 27.3.1 关键三件事

| 构件             | 本体是什么                         | 在本章中的职责                |
| ---------------- | ---------------------------------- | ----------------------------- |
| `struct kref`    | 原子计数 + 回调                    | 定义“引用归零点”              |
| `struct kobject` | 包含 kref + sysfs 名字 + parent 链 | device 依附于它               |
| `struct device`  | 高阶实体（bus/class/driver）       | 生命周期由内部的 kobject 驱动 |

→ 设备模型所有对象最终都有一个 kobject。
 → kobject 内有 kref，kref 决定“啥时候可以销毁”。
 → “销毁”不是 free 调用点，是 kref=0 的回调点。

### 27.3.2 结构体展开（关键字段）

（kernel/include/linux/kobject.h）

```c
struct kobject {
    const char      *name;
    struct list_head entry;
    struct kobject  *parent;
    struct kset     *kset;
    struct kobj_type *ktype;
    struct kernfs_node *sd;    // sysfs 节点
    struct kref      kref;     // 引用计数
};
```

（kernel/include/linux/kref.h）

```c
struct kref {
    refcount_t refcount;       // atomic-based counter
};
```

（kernel/include/linux/device.h）

```c
struct device {
    struct kobject kobj;       // ← 内含 kref
    struct device *parent;
    /* ... class/bus/driver ... */
};
```

> device 自身没有“自己的引用计数字段”
>  device 的引用计数 = **它内部那个 kobject 的 kref.refcount**

### 27.3.3 get / put 不是“函数语义”，是“生命周期合同”

| 调用                             | 语义                                    |
| -------------------------------- | --------------------------------------- |
| `kobject_get()` / `get_device()` | “我要持有这个 device，在我手里必须活着” |
| `kobject_put()` / `put_device()` | “我释放这个拥有权”                      |
| kref hit zero                    | “可以执行该对象的 release 回调”         |

### 27.3.4 为什么不是 atomic_t？

因为**不是单纯计数**，而是“计数→到0→触发 destructor 回调”
 kref = 一个“有 destructor 回调的 refcount”

atomic_t 做不到这个“到 0 时执行钩子”。

kref 解决的是：

- 不是“计数为0 → free”
- 是“计数为0 → 执行 ktype->release”

这个 release 才是**真正决定“这个对象如何被销毁”**的代码逻辑。

> 也就是：
>  内核不是把 free 写在 driver remove()
>  是把 free 写在 ktype->release()

### 27.3.5 device 为什么不能被 free()？

因为它不是“内存块”。
 它必须在 kref=0 时执行 `device_release()` 回调。
 这回调里通常包含：

- 结构体的回收
- 子对象的回收
- sysfs 节点收尾
- 与 bus/class 的 detach

### 27.3.6 所以整个路径是：

```
谁需要 device → get_device()
不用了       → put_device()
最后 put 使 refcount 到 0 → 内核执行 release → 最终 free
```

**不是**：

```
probe malloc
remove free
```

remove() 只是“你不再注册 / 不再暴露这个 device”
 不是“这个 device 对象此刻已经无人在引用”。

很多延迟工作路径仍可能持有它。

> DMA completion callback、workqueue、timer
>  都可能导致 device 引用**延迟释放**。



------

## 27.4 step4：怎么用（方法与步骤）

> **你写 driver 时，device 生命周期不是你 free 的**
>  你做的是：
>
> - **持有权**：get
> - **不再持有**：put
> - 自动回收资源：devres

典型 driver 路径：

```
probe()
    device_register()   ←（框架调好）
    devm_* 申请资源     ← devres 绑定 device kobject
    ...
    return 0

remove()
    driver 停机逻辑     ← 不 free 资源
    return 0

release()              ← 引用归零后才发生
    free(device struct)
```

### 四个关键动作落点

| 事情                             | 时机     | 用什么                             |
| -------------------------------- | -------- | ---------------------------------- |
| 声明“我现在必须保证 device 活着” | get      | `get_device()`（本质 kobject_get） |
| 我用完这个持有权了               | put      | `put_device()`（本质 kobject_put） |
| 资源随 device 生命周期回收       | devres   | `devm_*` 家族 API                  |
| 真正的析构（driver 不直接调用）  | 内核触发 | `release()`（kobj_type->release）  |

### **probe — remove — release** 三者不是同一层

| 层级        | 解释                          |
| ----------- | ----------------------------- |
| `probe()`   | “我进入世界”                  |
| `remove()`  | “我退出总线管理与暴露范围”    |
| `release()` | “引用数归零，identity 被销毁” |

错误：把 remove 当 free
 正确：remove 时不能 free，只能停止

### get/put 使用场景标准模板

> 只要你把 device 指针传给异步路径，就必须在传出前 get

例子（workqueue）：

```c
static void workfn(struct work_struct *wk)
{
    struct my_ctx *ctx = container_of(wk, struct my_ctx, work);
    struct device *dev = ctx->dev;

    /* 使用 dev ... */

    put_device(dev);     // 用完，释放持有权
}

void kick_async(struct device *dev)
{
    /* 异步路径将会持有 dev */
    get_device(dev);     // 声明“我持有”
    ctx->dev = dev;
    queue_work(wq, &ctx->work);
}
```

> get_device 在“发起异步”时
>  put_device 在“回调使用完”时
>
> remove **不干涉**这个：remove 不减少引用计数

如果 remove 时 work 仍在队列 → device 不释放
 等 work 结束 put → 才触发 release

这就是**引用计数**的意义

### devres 的实用指导

| 行为                | 用法    | 为什么                         |
| ------------------- | ------- | ------------------------------ |
| probe 里申请资源    | `devm_` | 把资源与 device 生命周期绑定   |
| remove 不 free      | 不 free | devres 一旦 kref=0 → 自动 free |
| 异步路径持有 device | get/put | devres 不做“引用计数管理”      |

> devres 和 get/put 是两条正交的轴
>  devres = “释放资源”
>  get/put = “控制 device identity 的存在”

### 软件架构人一生必须刻下的一句：

> remove 不是“销毁”
>  remove 是“退出可见范围”
>  销毁是 kref=0 → release

“什么时候销毁”是引用计数数学结果



------

## 27.5 step5：通用接口 / 工具方法表与逐步详解

### 27.5.1 概念区分表

| 术语    | 是什么                      | 谁控制它                | 为什么存在                       |
| ------- | --------------------------- | ----------------------- | -------------------------------- |
| kref    | “有 destructor 的 refcount” | 调用 get/put 的所有路径 | 把“对象能活多久”变成数学         |
| kobject | kref + sysfs identity       | kobject 框架            | 使对象成为“世界里的实体”         |
| device  | kobject 上的更高层实体      | device/core 子系统      | 提供 bus/class/driver 对象化     |
| devres  | 自动 free 资源的快照系统    | devm_* API              | 让资源随 device 生命周期自动回收 |
| remove  | 退出总线/对外暴露           | driver                  | “不再服务”不是“销毁”             |
| release | kref=0 时执行的 destructor  | 内核（kref→0）          | 真正 free 结构体的地方           |

------

### 27.5.2 用法速览表

| 使用场景                                      | 正确写法                                           | 本质                    |
| --------------------------------------------- | -------------------------------------------------- | ----------------------- |
| 把 device 传入异步路径（WQ/Timer/IRQ 离开放） | `get_device()` 在发起前；`put_device()` 在回调结尾 | 声明/释放“我有持有权”   |
| probe 中申请资源                              | `devm_*`                                           | devres = “资源快照”     |
| remove                                        | 不 free device，不 free 资源                       | remove ≠ 销毁           |
| 真正销毁时机                                  | kref 变 0 时，内核走 release                       | 引用计数数学            |
| sysfs entry 还挂着时                          | 不能被 free                                        | kobject 的存在依赖 kref |

------

### 27.5.3 核对表（CHECK）

- [CHECK] device 指针被异步路径持有前，是否 get_device？
- [CHECK] 异步路径使用完，是否 put_device？
- [CHECK] probe 是否用 devm_* 申请资源？
- [CHECK] remove 是否 **不** free 资源？
- [CHECK] release 是否只由内核在 kref=0 时调用？
- [CHECK] 你知道“kref=0 = 销毁”而不是“remove 返回 = 销毁”？
- [CHECK] sysfs 是否也能持有 device？（所以“看起来没引用”不代表 ref=0）

------

### 27.5.4 一句话提示（建议贴在每个 driver 文件头）

> remove 不 free
>  free 在 release
>  release 只在 kref=0
>  kref=0 只有 put 们真的全都 put 了



------

## 27.6 step6：对比 / 避坑 / 限制 / 注意点

### 27.6.1 devres vs get/put —— 两条正交轴，绝不能混

| 东西                              | 作用域       | 它负责什么              | 它**不**负责什么     |
| --------------------------------- | ------------ | ----------------------- | -------------------- |
| devres (`devm_*`)                 | “资源释放”层 | 自动 free 资源          | **不**做引用计数管理 |
| get/put (`get_device/put_device`) | “存在期”层   | 声明/释放 device 生存权 | **不** free 资源     |

> devres = free
>  get/put = alive

它们根本不是同一层
 不要幻想 “devm_xxx 自动减少引用”
 → devres 只登记“release 时要 free 什么”

### 27.6.2 remove 不是析构，最容易被误解

错误句型：

> remove 就是 free

正确语义：

> remove = “退出可见范围”
>  free = “kref=0 → release 由 kernel 执行”

### 27.6.3 最危险的常见 6 错误句式

| 错误习惯语句                          | 为什么危险                                    |
| ------------------------------------- | --------------------------------------------- |
| remove 里 free()                      | 有异步路径未 put，直接 UAF                    |
| 看着没人用就 free                     | sysfs / class / bus 可能仍 hold               |
| devres 自动帮我 put                   | devres 不做引用管理                           |
| get_device 不在发起异步那边写         | 异步路径会使用一个未被声明持“存活权”的 device |
| remove 里 flush_wq() 然后 free device | flush 不保证没有 future put                   |
| release 里做复杂逻辑                  | release 是最后一跳，不可失败，不可阻塞        |

### 27.6.4 release 要写在哪里？

不是写在 remove
 不是写在 probe

是写在 kobject/device type 的 release 钩子里：

```c
static void demo_dev_release(struct device *dev)
{
    /* 这里才能真正 free device 结构本身 */
    kfree(dev);
}
```

注：这不是你主动调的
 是引用计数归零后内核调

### 27.6.5 get/put 不成对会怎样？

不会马上崩
 但会两种结局：

| 情况          | 结局                              |
| ------------- | --------------------------------- |
| 多 get 少 put | 永不释放（内存泄漏 + sysfs 残留） |
| 少 get 多 put | 可能提前 release → 异步路径踩 UAF |

真正毒瘤不是“崩没崩”
 而是“它在 300ms 或 3s 后异步崩”
 你会误以为是“DMA 错”
 但实际上是 **引用计数提前到0** 的逻辑 bug

> get/put bug 的“类时延” → 是最难查的

### 27.6.6 remove 阶段的正确态度

remove 不是 free
 remove 是 “撤掉对外入口 + 停机 + cleanup pending work”

所以 remove 应该做的是：

- 关闭入口 path
- flush async
- drop own get（如果 framework 给你持有）
- **但不 free device**

free 在 release



------

## 27.7 step7：完整示例与讲解

### 27.7.1 基础版：无异步路径（不需要显式 get/put）

> 只要**没有把 device 指针传给异步**，
>  probe/remove 都不用 get/put，
>  devres 就足够。

```c
// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

static void demo_dev_release(struct device *dev)
{
    /* 真正 free device 的地方 */
    kfree(dev);
}

static int demo_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;

    /* normal devm usage */
    void *buf = devm_kmalloc(dev, 1024, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    pr_info("demo: probe ok\n");
    return 0;
}

static int demo_remove(struct platform_device *pdev)
{
    /* 不 free 资源！devres 自动收尾 */
    pr_info("demo: remove\n");
    return 0;
}

static const struct of_device_id demo_of_match[] = {
    { .compatible = "demo,life-demo", },
    {}
};
MODULE_DEVICE_TABLE(of, demo_of_match);

static struct platform_driver demo_drv = {
    .probe  = demo_probe,
    .remove = demo_remove,
    .driver = {
        .name = "demo-life-demo",
        .of_match_table = demo_of_match,
    },
};
module_platform_driver(demo_drv);

MODULE_LICENSE("GPL");
```

这个例子展示：

- release 在 device 本体里
- 驱动 probe/remove 不显式 free
- devres 自动释放资源
- **没有异步路径** → 不需要 get/put

------

### 27.7.2 有异步：必须 get/put

只要把 dev pointer 放到一个延迟执行路径 → 必须 get/put

```c
static void workfn(struct work_struct *wk)
{
    struct demo_ctx *ctx = container_of(wk, struct demo_ctx, work);
    struct device *dev = ctx->dev;

    /* 使用 dev ... */

    put_device(dev); /* 对称释放持有权 */
}

static void kick_async(struct demo_ctx *ctx, struct device *dev)
{
    get_device(dev);         /* 声明：我要持有 dev */
    ctx->dev = dev;
    queue_work(ctx->wq, &ctx->work);
}
```

→ remove() 里：
 需要 flush_workqueue() → 阻塞直到 workfn 跑完（put 归还）
 **但 remove 仍不 free device**
 最后 device 的 kref 归零时，内核调用 release → free

------

### 27.7.3 用一句话总结 step7

> 只要把 device 传给了“未来某个地方”，
>  就必须 get；
>  那个未来的地方用完后必须 put；
>  free 只在 release。



------

## 27.8　step8：总结

### 27.8.1 三条不可折叠的根契约

1）**remove != free**
 remove 只是“退出可见范围”，**不是销毁设备对象**。
 销毁点 = **kref = 0 时内核调用 release**

2）**devres != 引用计数**
 devres 解决 “资源随着 device 销毁自动 free”
 它不做 “device 是否仍被谁持有” 的判断
 引用计数 = get/put

3）**get/put = 持有与归还 device 生命权**
 凡是把 device 放到异步，就必须 get
 异步回调里用完就必须 put
 驱动不 free，free 在 release

------

### 27.8.2 本章一页速查表（最终版）

| 主题                | 必做                                     | 错做就炸点                       |
| ------------------- | ---------------------------------------- | -------------------------------- |
| 异步路径持有 dev    | 发起前 get_device()，回调后 put_device() | 否则异步延迟 UAF                 |
| probe 中申请资源    | 用 devm_*                                | 否则 remove 后资源泄漏           |
| remove              | 不 free，不销毁 device                   | free 在 release                  |
| release             | kref=0 → 内核自动调用                    | release 不可失败也不可再持有引用 |
| sysfs / class / bus | 也会持有 device                          | 用眼看不一定没引用               |
| devres vs get/put   | free vs alive                            | 永不混为一谈                     |

------

### 27.8.3 本章结束句

> device 不是 malloc/free 对象。
>  device 是“多路径可被引用的 identity”。
>  生命周期必须交给 kobject/kref 数学收口；
>  devres 只是“资源清单”。
>  驱动写作者只做两件事：
>  **声明我要持有（get）**
>  **使用完归还（put）**

