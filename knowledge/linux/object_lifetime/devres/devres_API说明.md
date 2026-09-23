---
id: knowledge.linux.object_lifetime.devres.devres_api说明
title: "devm API接口文档说明"
kind: reference
status: evolving
domains:
  - linux
  - kernel
---

# 第1章\_devm\_API接口文档说明

本页用于查询接口，初次学习先读[从失败回滚到设备资源账本](P01_从失败回滚到设备资源账本.md#1.1_从两条退出路径提取同一份责任)。devres保存清理责任，devm包装登记具体资源；调用者仍负责错误判断、资源使用同步、业务关停和未托管责任。

## 1.1\_官方内核文档(首选)

优先阅读已确认版本源码中的`Documentation/driver-api/driver-model/devres.rst`，通过[固定源码索引](../../../../research/source_reading/devres/navigation/P01_Linux_6.12_devres源码阅读索引.md#1.1_固定版本与阅读任务)核对本仓库使用的NXP Linux 6.12.20、不可变提交与实际配置。在线[devres文档](https://docs.kernel.org/driver-api/driver-model/devres.html)方便检索，但滚动页面不能代替固定提交证据；历史`Documentation/driver-model/devres.txt`只作为旧目录线索，不用来证明当前接口原型。

## 1.2\_源码位置(实现\_&\_接口宣告)

核心实现位于`drivers/base/devres.c`，公共声明及action包装位于`include/linux/device.h`。`lib/devres.c`包含I/O等资源包装，不是本版本核心分组实现的位置。具体接口还散布在子系统头文件与实现中，既要核对成功路径，也要核对失败回滚及禁用配置下的桩。

下面在已确认身份的源码仓库只读查询固定版本，不依赖当前实验工作树内容：

```bash
baseline=dfaf2136deb2af2e60b994421281ba42f1c087e0
git grep -n -E 'devm_[a-z0-9_]+\(' "$baseline" -- include/linux drivers lib
git show "$baseline":drivers/base/devres.c
git show "$baseline":include/linux/device.h
```

名字相似不代表返回值或释放责任相同。先找到声明，再沿实现和对应kernel-doc注释确认参数、返回表示、记录登记时点及自动清理动作。

## 1.3\_本地生成可浏览\_HTML\_文档

需要离线HTML时，在自己准备的对应版本文档构建树中安装Sphinx等构建依赖后运行`make htmldocs`；本仓库知识整理不会替读者修改外部核对树。默认输出入口为`Documentation/output/html/index.html`，设置独立输出目录时按构建配置定位。

生成结果与所选源码和文档匹配，但不保证所有API都有完整文档，也不会校验你的驱动使用顺序。本页没有把文档构建列为已执行验证；接口核对以保存的源码证据为准。

## 1.4\_扩展阅读(背景与实践)

先沿[返回值与资源记录导读](../../../../research/source_reading/error_pointer/navigation/P02_返回值与清理路径导读.md#2.1_先把返回值与资源记录分开)区分错误传播和资源清理，再沿[devres模块导读](../../../../research/source_reading/devres/navigation/P02_记录与分组清理导读.md#2.1_从记录地址追踪S0到S4)检查状态落点。driver core对象引用与驱动资源期限的区别见[kref框架边界](../kref/P11_kref_refcount_t_kobject_的边界.md#11.4_driver_core_边界_device_class_bus_不是裸_kref)。

历史背景可继续查[LWN讨论](https://lwn.net/Articles/645810/)与[Haifux讲义](https://www.haifux.org/lectures/323/haifux-devres.pdf)，它们不替代当前实现。auxiliary bus、DRM等子系统还会引入自己的对象寿命；例如`drmm_*`面向DRM对象的管理期限，不能直接当作通用`devm_*`别名。本页后续各族条目用于定位差异，最终以所选子系统版本契约为准。

# 第2章\_devm\_接口\_作用与区别(按子系统)

## 2.1\_核心机制\_/\_分组接口

本节按固定dfaf2136提交核对。模型与完整C实验见[六条回滚路径](P01_从失败回滚到设备资源账本.md#1.5_运行完整C模型观察六条路径)，下列接口只承担查询职责。

### 2.1.1\_devm\_add\_action

调用形式为`int devm_add_action(struct device *dev, void (*action)(void *), void *data)`；本版本实际是宏，转到带调试名称参数的`__devm_add_action()`。成功返回0并登记一条记录；记录分配失败返回`-ENOMEM`，**不执行action**，已经取得的资源仍由调用者负责。登记后的回调在所选devres清理路径执行，参数必须活到该回调结束。

action可封装资源释放，也可封装符合依赖的关停动作。正常解绑的清理不会替代每次PM暂停/恢复；锁外回调也不自动取得任意睡眠资格。具体登记实现见[__devm_add_action](../../../../research/source_reading/devres/source_explanations/drivers/base/devres.c.md#1.1_普通action登记成功才转交责任)。

### 2.1.2\_devm\_add\_action\_or\_reset

调用参数与普通add相同。成功仍只登记，不立即执行；失败则在返回错误以前立即执行`action(data)`，此时不留下待清理记录。调用者在失败分支不能再重复释放同一资源；action须能在登记调用者的上下文执行。见[失败即时回滚实现](../../../../research/source_reading/devres/source_explanations/include/linux/device.h.md#1.1_reset包装失败直接执行)。

### 2.1.3\_devres\_open\_group

原型为`void *devres_open_group(struct device *dev, void *id, gfp_t gfp)`。返回组ID，分配失败返回NULL；**返回类型不是公开的struct devres_group指针**。传入非NULL标识须避免与另一组冲突，传NULL则由核心生成标识。成功只登记开始标记，后续记录沿同一设备链追加。见[open实现](../../../../research/source_reading/devres/source_explanations/drivers/base/devres.c.md#1.2_devres_open_group登记开始标记)。

### 2.1.4\_devres\_close\_group

原型为`void devres_close_group(struct device *dev, void *id)`。给有效未关闭组追加结束标记，此后登记的资源在组外；不执行回调，也不取消之后显式release该组的能力。需要后来选择一个已关闭组时保存并传入明确ID；NULL选择最近未关闭组。不要重复关闭同一个组。见[close实现](../../../../research/source_reading/devres/source_explanations/drivers/base/devres.c.md#1.3_devres_close_group限定范围)。

### 2.1.5\_devres\_remove\_group

原型为`void devres_remove_group(struct device *dev, void *id)`。它只移除该组的开始/结束标记并释放组管理结构，**不释放组内资源**。常用于中间层创建成功以后撤掉临时分组，让已登记资源继续由设备管理；它不是失败回滚接口。组被移除以后不得继续使用生成的ID选择它。见[remove实现](../../../../research/source_reading/devres/source_explanations/drivers/base/devres.c.md#1.4_devres_remove_group只拿走标记)。

### 2.1.6\_devres\_release\_group

原型为`int devres_release_group(struct device *dev, void *id)`。选择从开始标记到结束标记的范围；未关闭组延伸到当前设备链尾。摘出组内普通资源及合法嵌套组，锁外逆序执行资源回调，并移除选中的组；返回释放的非组资源数量。无效ID不是正常重试办法，固定实现会告警，不能依靠它完成幂等业务。

需要失败时只撤销本阶段，就调用它；需要成功后保留资源但不要组标记，则使用remove。见[范围选择实现](../../../../research/source_reading/devres/source_explanations/drivers/base/devres.c.md#1.5_devres_release_group摘取后回调)。

### 2.1.7\_提前清理与撤销action

`devm_release_action(dev, action, data)`匹配一条记录，执行回调并删除记录；`devm_remove_action(dev, action, data)`只删除记录，不执行action，实际资源责任须已被接走或履行。二者都要求函数与参数相符；重复登记同一对不代表一次操作会自动处理所有副本，未找到记录会告警。参见[分组与action选择](../../../../research/source_reading/devres/navigation/P02_记录与分组清理导读.md#2.3_选择接口先确定责任是否保留)。

普通`free/put`不会自动摘掉对应devres记录，随后自动清理可能再次处理已释放资源。需要提前清理时选配套托管接口；新增action也不会撤销原有记录，不能把它当成消除重复清理的补丁。


## 2.2\_内存与字符串

### 2.2.1\_devm\_kzalloc

**功能**：分配零清内存，绑定设备生命周期。
 **原型**：`void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);`
 **返回**：成功返回指针，失败 `NULL`。
 **释放**：解绑/失败时自动释放。
 **要点**：仅用于**随设备生命周期**存在的内存；跨设备/全局内存不要使用。

### 2.2.2\_devm\_kcalloc

**功能**：分配 `n * size` 零清数组，带溢出检查。
 **原型**：`void *devm_kcalloc(struct device *dev, size_t n, size_t size, gfp_t gfp);`
 **返回/释放**：同上。
 **要点**：用于数组元素计数明确的场景；避免整数溢出。

### 2.2.3\_devm\_kmemdup

**功能**：分配并拷贝指定大小的缓冲区。
 **原型**：`void *devm_kmemdup(struct device *dev, const void *src, size_t size, gfp_t gfp);`
 **返回/释放**：同上。

### 2.2.4\_devm\_kstrdup

**功能**：复制以 `\0` 结尾字符串。
 **原型**：`char *devm_kstrdup(struct device *dev, const char *s, gfp_t gfp);`
 **返回/释放**：同上。
 **误用**：对非 `\0` 终止数据使用，应改用 `kmemdup`。

------

## 2.3\_I/O\_资源与寄存器映射

### 2.3.1\_devm\_ioremap

**功能**：将物理地址映射为内核虚拟地址。
 **原型**：`void __iomem *devm_ioremap(struct device *dev, resource_size_t offset, size_t size);`
 **返回**：`__iomem` 指针或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 `iounmap()`。
 **要点**：**不**做资源冲突检查；通常更推荐使用 `_resource` 族。

### 2.3.2\_devm\_ioremap\_resource

**功能**：对 `struct resource` 指定的区域进行**冲突检查**后映射。
 **原型**：`void __iomem *devm_ioremap_resource(struct device *dev, const struct resource *res);`
 **返回**：同上。
 **区别**：比 `devm_ioremap` 多了资源有效性/冲突检测；**优先使用**。

### 2.3.3\_devm\_platform\_ioremap\_resource

**功能**：对 `platform_device` 的第 `index` 个内存资源进行检查并映射（简写）。
 **原型**：`void __iomem *devm_platform_ioremap_resource(struct platform_device *pdev, unsigned int index);`
 **返回/释放**：同上。
 **要点**：适用于平台驱动；`index` 自 0 起。

### 2.3.4\_devm\_platform\_ioremap\_resource\_byname

**功能**：按资源名进行检查并映射。
 **原型**：`void __iomem *devm_platform_ioremap_resource_byname(struct platform_device *pdev, const char *name);`
 **要点**：与设备树/板文件中命名一致时使用；便于可读性。

------

## 2.4\_GPIO(gpiod\_消费者)

### 2.4.1\_devm\_gpiod\_get

**功能**：按连接 ID 获取一个 GPIO 描述符，并可指定初始方向/电平。
 **原型**：`struct gpio_desc *devm_gpiod_get(struct device *dev, const char *con_id, enum gpiod_flags flags);`
 **返回**：`gpio_desc *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 `gpiod_put()`。
 **要点**：`flags` 常用 `GPIOD_OUT_LOW/HIGH`、`GPIOD_IN`；与 DT 的 `*-gpios` 属性匹配。
 **误用**：使用旧整数 GPIO 接口；未考虑极性导致上电瞬态错误。

### 2.4.2\_devm\_gpiod\_get\_optional

**功能**：同 3.1，但**资源可缺省**。
 **原型**：`struct gpio_desc *devm_gpiod_get_optional(struct device *dev, const char *con_id, enum gpiod_flags flags);`
 **区别**：资源不存在时可能返回 `NULL`（具体取决于解析路径），需在调用者做 `NULL` 判定。
 **适用**：硬件版本差异导致 GPIO 可有可无。

### 2.4.3\_devm\_gpiod\_get\_index

**功能**：获取同一连接 ID 下第 `index` 个 GPIO。
 **原型**：`struct gpio_desc *devm_gpiod_get_index(struct device *dev, const char *con_id, unsigned int index, enum gpiod_flags flags);`
 **适用**：多 GPIO（如 `reset-gpios` 多路）。

------

## 2.5\_IRQ

### 2.5.1\_devm\_request\_irq

**功能**：申请中断线并注册**顶半部**处理函数。
 **原型**：`int devm_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev_id);`
 **返回**：`0` 或 `-Exxx`（如 `-EINVAL/-EBUSY/-ENXIO/-ENOMEM`）。
 **释放**：解绑/失败时自动 `free_irq()`。
 **要点**：`handler` 中不得执行可睡眠操作。

### 2.5.2\_devm\_request\_threaded\_irq

**功能**：申请中断线，注册**顶半部**与**线程化底半部**。
 **原型**：`int devm_request_threaded_irq(struct device *dev, unsigned int irq, irq_handler_t handler, irq_handler_t thread_fn, unsigned long flags, const char *name, void *dev_id);`
 **返回/释放**：同 4.1。
 **要点**：`thread_fn` 可睡眠；常配合 `IRQF_ONESHOT`。
 **误用**：在 `handler` 执行可睡眠 API；未正确设置触发类型导致抖动。

### 2.5.3\_devm\_free\_irq

**功能**：**提前**释放由 `devm_request_*_irq` 申请的中断。
 **原型**：`void devm_free_irq(struct device *dev, unsigned int irq, void *dev_id);`
 **适用**：需要在解绑前停止中断服务的场合。

------

## 2.6\_时钟(Common\_Clock\_Framework)

### 2.6.1\_devm\_clk\_get

**功能**：获取一个时钟**句柄**。
 **原型**：`struct clk *devm_clk_get(struct device *dev, const char *id);`
 **返回**：`struct clk *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 `clk_put()`。
 **要点（关键）**：`devm` **只托管句柄**；`clk_prepare_enable()` / `clk_disable_unprepare()`（**状态**）需在 `probe/remove/PM` 显式配对。

### 2.6.2\_devm\_clk\_bulk\_get

**功能**：批量获取多个时钟句柄并在失败时统一回滚。
 **原型**：`int devm_clk_bulk_get(struct device *dev, int num_clks, struct clk_bulk_data *clks);`
 **返回**：`0` 或 `-Exxx`。
 **释放**：解绑/失败时自动 put。
 **适用**：多时钟域的外设。

------

## 2.7\_电源(Regulator)

### 2.7.1\_devm\_regulator\_get

**功能**：获取一个 regulator 句柄。
 **原型**：`struct regulator *devm_regulator_get(struct device *dev, const char *id);`
 **返回**：`regulator *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 put。
 **要点（关键）**：`regulator_enable()`/`regulator_disable()`（**状态**）需在 `probe/remove/PM` 显式配对；`devm` 不托管电源启停。

### 2.7.2\_devm\_regulator\_get\_optional

**功能**：与 6.1 相同，但资源可缺省。
 **原型**：`struct regulator *devm_regulator_get_optional(struct device *dev, const char *id);`
 **适用**：硬件版本差异。

### 2.7.3\_devm\_regulator\_bulk\_get

**功能**：批量获取 regulator。
 **原型**：`int devm_regulator_bulk_get(struct device *dev, int num_consumers, struct regulator_bulk_data *consumers);`
 **返回**：`0` 或 `-Exxx`。
 **释放**：解绑/失败时自动 put。
 **要点**：启停同样需要批量 `enable/disable` 自行配对。

### 2.7.4\_devm\_regulator\_put(少用)

**功能**：**提前**释放一个 `devm` 获取的 regulator 引用。
 **原型**：`void devm_regulator_put(struct regulator *regulator);`
 **适用**：特殊情况下提前放弃句柄；一般不必调用。

------

## 2.8\_Reset\_控制

### 2.8.1\_devm\_reset\_control\_get

**功能**：获取复位控制句柄。
 **原型**：`struct reset_control *devm_reset_control_get(struct device *dev, const char *id);`
 **返回**：`reset_control *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 put。
 **要点**：具体复位时序（assert/deassert/pulse）由驱动控制；状态需在 `remove()/PM` 按需要复位。

### 2.8.2\_devm\_reset\_control\_get\_exclusive

**功能**：获取**独占**复位控制句柄。
 **原型**：`struct reset_control *devm_reset_control_get_exclusive(struct device *dev, const char *id);`
 **差异**：拒绝共享。适用于硬件要求严格独占的复位线。

### 2.8.3\_devm\_reset\_control\_get\_shared

**功能**：获取**共享**复位控制句柄。
 **原型**：`struct reset_control *devm_reset_control_get_shared(struct device *dev, const char *id);`
 **差异**：允许共享；注意并发与引用计数。

### 2.8.4\_devm\_reset\_control\_get\_optional

**功能**：可缺省版本。
 **原型**：`struct reset_control *devm_reset_control_get_optional(struct device *dev, const char *id);`

------

## 2.9\_DMA\_引擎

### 2.9.1\_devm\_dma\_request\_chan

**功能**：按名称从 DMA 引擎请求一个通道。
 **原型**：`struct dma_chan *devm_dma_request_chan(struct device *dev, const char *name);`
 **返回**：`dma_chan *` 或 `ERR_PTR(-ENODEV/-EPROBE_DEFER/…)`。
 **释放**：解绑/失败时自动释放引用。
 **要点**：可能返回 `-EPROBE_DEFER`；与设备树 `dmas`/`dma-names` 匹配。

------

## 2.10\_PHY

### 2.10.1\_devm\_phy\_get

**功能**：获取 PHY 句柄。
 **原型**：`struct phy *devm_phy_get(struct device *dev, const char *string);`
 **返回**：`phy *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 put。
 **要点**：`phy_power_on/off`、`phy_init/exit` 属于**状态/阶段操作**，需在 `probe/remove/PM` 明确配对。

------

## 2.11\_pinctrl

### 2.11.1\_devm\_pinctrl\_get

**功能**：获取 pinctrl 句柄。
 **原型**：`struct pinctrl *devm_pinctrl_get(struct device *dev);`
 **返回**：`pinctrl *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 put。
 **要点**：`pinctrl_lookup_state()` + `pinctrl_select_state()` 的状态切换（如 `"default"`/`"sleep"`）**不受 devm 托管**，需在 `remove()/PM` 配对。

------

## 2.12\_平台辅助\_中断号/资源获取(非\_devm\_但常与\_devm\_组合)

> 以下接口不是 `devm_*`，但与上面接口配合频繁，单独列出以免混淆。

### 2.12.1\_platform\_get\_irq

**功能**：从 `platform_device` 获取中断号。
 **原型**：`int platform_get_irq(struct platform_device *pdev, unsigned int num);`
 **返回**：`>=0` 的 IRQ 号或 `-Exxx`。
 **组合**：获取到 IRQ 后，**再**调用 `devm_request_*_irq` 进行托管。

### 2.12.2\_platform\_get\_resource

**功能**：从 `platform_device` 获取 `struct resource`。
 **原型**：`struct resource *platform_get_resource(struct platform_device *pdev, unsigned int type, unsigned int num);`
 **组合**：配合 `devm_ioremap_resource` 或 `devm_platform_ioremap_resource(_byname)`。

------

## 2.13\_注册类(示例)

### 2.13.1\_devm\_led\_classdev\_register

**功能**：注册 LED class 设备，解绑自动注销。
 **原型**：`int devm_led_classdev_register(struct device *dev, struct led_classdev *led_cdev);`
 **返回**：`0` 或 `-Exxx`。
 **要点**：并发访问的同步由驱动负责。

### 2.13.2\_devm\_thermal\_zone\_of\_sensor\_register

**功能**：向 thermal 框架注册 OF 传感器，解绑自动注销。
 **原型**：`int devm_thermal_zone_of_sensor_register(struct device *dev, int id, void *data, const struct thermal_zone_of_device_ops *ops);`
 **返回**：`0` 或 `-Exxx`。
 **要点**：`ops` 回调需要保证热路径稳定。

（其它如 `devm_extcon_dev_register`、IIO 的 `devm_*` 注册接口，语义一致：**注册成功 → 解绑自动注销**；差异体现在各子系统的回调与数据结构，按需查阅子系统文档。）

------

## 2.14\_全局注意事项(统一要求)

- 每个托管接口只履行其登记的清理责任；action可以封装状态关停，但不自动完成每次PM转换。必须逐项核对资源族的具体契约。
- probe失败由核心清理已经登记的记录；未托管资源、尚未登记责任及已启动的异步使用者仍由驱动正确收束。
- remove按依赖关闭入口、停止使用并处理未托管责任；已登记资源不能直接用普通free/put重复清理，需要提前结束时使用配套托管释放接口。
- 需要活过解绑的资源不能仅依赖本设备devres账本；跨设备使用还要明确谁承担实际存储和资源责任。
- 提前释放使用对应的托管释放或release_action；新登记action不会自动撤销已有释放记录。
- 错误码：注意识别 `-EPROBE_DEFER`（依赖尚未就绪），按要求返回上层等待重试。

------

# 第3章\_关键区别总表(同族接口横向对比)

| 类别         | 接口                                              | 主要区别点             | 推荐                          |
| ------------ | ------------------------------------------------- | ---------------------- | ----------------------------- |
| I/O 映射     | `devm_ioremap` vs `devm_ioremap_resource`         | 是否检查资源冲突       | **`_resource` 优先**          |
| 平台映射     | `devm_platform_ioremap_resource` vs `_byname`     | 按索引/按名称获取      | 依 DTS 命名使用               |
| GPIO         | `devm_gpiod_get` vs `_optional` vs `_index`       | 资源可缺省；多路索引   | 资源可选用 `_optional`        |
| IRQ          | `devm_request_irq` vs `devm_request_threaded_irq` | 是否提供线程化处理     | 需要可睡眠操作用 **threaded** |
| CLK          | `devm_clk_get` vs `devm_clk_bulk_get`             | 单个/批量获取          | 多时钟用 **bulk**             |
| REGULATOR    | `devm_regulator_get` vs `_optional` vs `bulk_get` | 可缺省/批量            | 按依赖关系选择                |
| RESET        | `get` vs `get_exclusive` vs `get_shared`          | 所有权模式             | 按硬件要求选择                |
| 平台回滚     | `devres_open_group/close/remove/release`                  | 划定范围、移除标记或真正回滚         | 大型 `probe()` 使用           |
| 无 devm 资源 | `devm_add_action` vs `_or_reset`                  | 注册失败时是否立即回滚 | **`_or_reset` 优先**          |

------

# 第4章\_常见误用与修正

1. 直接普通free/put一个仍有托管记录的资源，之后可能被再次清理。提前释放要同时处理登记记录，不能只处理底层对象。
2. action负责解绑时关停，不等于它会在每次PM暂停时自动调用。单次退出与重复电源转换应分别设计，避免漏停或重复停。
3. 旧用户能保持device外壳，不代表绑定期的devm内存继续存活。按实际需要建立独立外壳和资源失效协议。
4. 硬中断处理函数仍须遵守不可睡眠约束；线程化IRQ的线程函数允许相应睡眠操作，但不改变顶半部约束。
5. 依赖未就绪的错误要按获取接口契约传播，包括适用的`-EPROBE_DEFER`，不能把可选缺席与所有失败都转换成成功。

# 第5章\_最小决策规则(学习与实战)

先画资源实际使用终点，再判断是否与设备绑定期清理相容。相容时选择对应托管包装；需要阶段回滚时使用release_group；需要保留资源但取消阶段标记时使用remove_group。独立寿命资源应有独立拥有者，不能通过晚登记一个action来延长已登记资源的寿命。

最后把成功、登记失败、后续probe失败、主动提前清理和正常解绑分别走一遍；再检查资源族是否还要求启停、同步、状态恢复或回调退出。返回[devres阅读路线](大纲.md#1.1_从退出责任进入资源接口)。
