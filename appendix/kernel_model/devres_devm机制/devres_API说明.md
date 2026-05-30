# devm API接口文档说明

提问：

* 那么我在哪里可以看到更全面的devm接口或者相关的文档说明呢？



下面这些来源最系统、最权威，按优先级给你列好，并说明怎么查：

## 官方内核文档（首选）

- **Devres 总文档**（在线）：*“Devres – Managed Device Resource”*（driver-model/devres）。这是 `devm`/devres 的权威说明，含工作机制、调用时机、释放顺序等。([Linux Kernel 文档](https://docs.kernel.org/driver-api/driver-model/devres.html?utm_source=chatgpt.com))
- **历史版纯文本**（同内容的早期文档）：`Documentation/driver-model/devres.txt`。适合快速浏览。([kernel.org](https://www.kernel.org/doc/Documentation/driver-model/devres.txt?utm_source=chatgpt.com))

> 如果你要看**内核 6.1 对应版本**的在线文档，也可以在 6.1 文档站点按主题跳转；例如 auxiliary bus 文档中也示例性提到 `devm_add_action_or_reset()` 的使用场景。([kernel.org](https://www.kernel.org/doc/html/v6.1/driver-api/auxiliary_bus.html?utm_source=chatgpt.com))

## 源码位置（实现 & 接口宣告）

- **实现**：`lib/devres.c`（核心 devres 实现、释放流程、分组接口等）。可以直接对照阅读。([docs.huihoo.com](https://docs.huihoo.com/doxygen/linux/kernel/3.7/lib_2devres_8c.html?utm_source=chatgpt.com))

- **API 参考方式**：内核树内大量 `devm_*` 接口分散在各子系统头文件/源文件里（如 `include/linux/…`、`drivers/*`）。建议在内核源码根目录用：

  ```bash
  rg -n "devm_[a-z0-9_]+\("  # 或者 git grep
  ```

  结合 `scripts/kernel-doc` 注释阅读（本地生成 API HTML 见下）。

## 本地生成可浏览 HTML 文档

- 在内核源码根目录执行：

  ```bash
  make htmldocs
  # 生成后从 Documentation/output/html/index.html 进入
  ```

  这样能得到与你的源码版本**完全一致**的 `devm`/devres 文档与各子系统的 API 页面（比网上搜索更对版）。该文档集合的入口是 *The Linux driver implementer’s API guide*。([infradead.org](https://www.infradead.org/~mchehab/rst_conversion/driver-api/index.html?utm_source=chatgpt.com))

## 扩展阅读（背景与实践）

- **LWN 文章**：对设备模型与 devres 的背景有不错的讲解与历史线索，适合理解设计动机与使用边界。([LWN.net](https://lwn.net/Articles/645810/?utm_source=chatgpt.com))
- **演示/讲义**：Haifux 的 *Managed Resource Allocation in Linux Device Drivers*（讲座 PDF），用实例解释 devres 的数据结构与调用路径。([haifux.org](https://www.haifux.org/lectures/323/haifux-devres.pdf?utm_source=chatgpt.com))
- **子系统文档中的交叉引用**：例如 auxiliary bus 文档里用到 `devm_add_action_or_reset()` 的正确用法，便于理解“没有 devm 版本时用 action 封装”的思路。([Linux Kernel 文档](https://docs.kernel.org/driver-api/auxiliary_bus.html?utm_source=chatgpt.com))
- **DRM 的对比说明**：DRM 文档说明了何时应使用 `drmm_*` 而非通用 `devm_*`（面向 userspace 可见对象的释放时机差异），有助于把握生命周期边界。([static.lwn.net](https://static.lwn.net/kerneldoc/gpu/drm-internals.html?utm_source=chatgpt.com))

------

好的。下面按子系统分类，**逐个接口给出：作用、与同族接口的区别、典型使用场景、返回值与释放语义、常见误用**。基线为 Linux 6.1。只给必要信息，避免冗余。

------

# `devm` 接口：作用与区别（按子系统）

## 0. 核心机制 / 分组接口

### `devm_add_action`

**功能**：为“没有 `devm_*` 版本”的资源注册一个**受设备生命周期管理**的释放回调。
 **原型**：`int devm_add_action(struct device *dev, void (*action)(void *), void *data);`
 **参数**：

- `dev`：目标设备；
- `action`：释放回调；
- `data`：回调的私有参数。
   **返回值/错误**：`0` 成功；`-ENOMEM` 等。
   **释放语义**：设备解绑/注销、或 `probe()` 早退时，按 **LIFO** 调用 `action(data)`。
   **使用要点**：回调内必须满足可在解绑路径**同步**执行（不可睡眠要求取决于上下文，一般可睡眠）。
   **常见误用**：把“运行状态复位”（如时钟关闭）只放进 `action` 而不在 `remove()`/PM 配对；应在 `remove()`/PM 明确回退状态。

### `devm_add_action_or_reset`

**功能**：与 `devm_add_action` 相同，但**注册失败**时会**立即**执行一次 `action(data)`，避免半初始化。
 **原型**：`int devm_add_action_or_reset(struct device *dev, void (*action)(void *), void *data);`
 **差异点**：失败时“即时回滚”。
 **适用**：没有 `devm_*` 版本、且初始化流程中间失败风险高的资源。

### `devres_open_group`

**功能**：开启一个 devres 资源分组，便于阶段化回滚。
 **原型**：`struct devres_group *devres_open_group(struct device *dev, void *id, gfp_t gfp);`
 **参数**：`id` 可自定义用于后续引用；`gfp` 分配标志。
 **返回值**：组指针或 `NULL`。
 **使用要点**：在阶段开始处调用，随后登记的 `devm_*` 资源会进入该组。

### `devres_close_group`

**功能**：关闭先前 `open_group` 的分组，固化该组资源。
 **原型**：`void devres_close_group(struct device *dev, struct devres_group *grp);`
 **使用要点**：阶段成功后调用，使该组不再被 `remove_group` 撤销。

### `devres_remove_group`

**功能**：撤销（回滚）`open_group` 之后登记的资源。
 **原型**：`void devres_remove_group(struct device *dev, void *id);`
 **使用要点**：阶段失败时调用，实现“一键回滚”。

------

## 1. 内存与字符串

### `devm_kzalloc`

**功能**：分配零清内存，绑定设备生命周期。
 **原型**：`void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);`
 **返回**：成功返回指针，失败 `NULL`。
 **释放**：解绑/失败时自动释放。
 **要点**：仅用于**随设备生命周期**存在的内存；跨设备/全局内存不要使用。

### `devm_kcalloc`

**功能**：分配 `n * size` 零清数组，带溢出检查。
 **原型**：`void *devm_kcalloc(struct device *dev, size_t n, size_t size, gfp_t gfp);`
 **返回/释放**：同上。
 **要点**：用于数组元素计数明确的场景；避免整数溢出。

### `devm_kmemdup`

**功能**：分配并拷贝指定大小的缓冲区。
 **原型**：`void *devm_kmemdup(struct device *dev, const void *src, size_t size, gfp_t gfp);`
 **返回/释放**：同上。

### `devm_kstrdup`

**功能**：复制以 `\0` 结尾字符串。
 **原型**：`char *devm_kstrdup(struct device *dev, const char *s, gfp_t gfp);`
 **返回/释放**：同上。
 **误用**：对非 `\0` 终止数据使用，应改用 `kmemdup`。

------

## 2. I/O 资源与寄存器映射

### `devm_ioremap`

**功能**：将物理地址映射为内核虚拟地址。
 **原型**：`void __iomem *devm_ioremap(struct device *dev, resource_size_t offset, size_t size);`
 **返回**：`__iomem` 指针或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 `iounmap()`。
 **要点**：**不**做资源冲突检查；通常更推荐使用 `_resource` 族。

### `devm_ioremap_resource`

**功能**：对 `struct resource` 指定的区域进行**冲突检查**后映射。
 **原型**：`void __iomem *devm_ioremap_resource(struct device *dev, const struct resource *res);`
 **返回**：同上。
 **区别**：比 `devm_ioremap` 多了资源有效性/冲突检测；**优先使用**。

### `devm_platform_ioremap_resource`

**功能**：对 `platform_device` 的第 `index` 个内存资源进行检查并映射（简写）。
 **原型**：`void __iomem *devm_platform_ioremap_resource(struct platform_device *pdev, unsigned int index);`
 **返回/释放**：同上。
 **要点**：适用于平台驱动；`index` 自 0 起。

### `devm_platform_ioremap_resource_byname`

**功能**：按资源名进行检查并映射。
 **原型**：`void __iomem *devm_platform_ioremap_resource_byname(struct platform_device *pdev, const char *name);`
 **要点**：与设备树/板文件中命名一致时使用；便于可读性。

------

## 3. GPIO（gpiod 消费者）

### `devm_gpiod_get`

**功能**：按连接 ID 获取一个 GPIO 描述符，并可指定初始方向/电平。
 **原型**：`struct gpio_desc *devm_gpiod_get(struct device *dev, const char *con_id, enum gpiod_flags flags);`
 **返回**：`gpio_desc *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 `gpiod_put()`。
 **要点**：`flags` 常用 `GPIOD_OUT_LOW/HIGH`、`GPIOD_IN`；与 DT 的 `*-gpios` 属性匹配。
 **误用**：使用旧整数 GPIO 接口；未考虑极性导致上电瞬态错误。

### `devm_gpiod_get_optional`

**功能**：同 3.1，但**资源可缺省**。
 **原型**：`struct gpio_desc *devm_gpiod_get_optional(struct device *dev, const char *con_id, enum gpiod_flags flags);`
 **区别**：资源不存在时可能返回 `NULL`（具体取决于解析路径），需在调用者做 `NULL` 判定。
 **适用**：硬件版本差异导致 GPIO 可有可无。

### `devm_gpiod_get_index`

**功能**：获取同一连接 ID 下第 `index` 个 GPIO。
 **原型**：`struct gpio_desc *devm_gpiod_get_index(struct device *dev, const char *con_id, unsigned int index, enum gpiod_flags flags);`
 **适用**：多 GPIO（如 `reset-gpios` 多路）。

------

## 4. IRQ

### `devm_request_irq`

**功能**：申请中断线并注册**顶半部**处理函数。
 **原型**：`int devm_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev_id);`
 **返回**：`0` 或 `-Exxx`（如 `-EINVAL/-EBUSY/-ENXIO/-ENOMEM`）。
 **释放**：解绑/失败时自动 `free_irq()`。
 **要点**：`handler` 中不得执行可睡眠操作。

### `devm_request_threaded_irq`

**功能**：申请中断线，注册**顶半部**与**线程化底半部**。
 **原型**：`int devm_request_threaded_irq(struct device *dev, unsigned int irq, irq_handler_t handler, irq_handler_t thread_fn, unsigned long flags, const char *name, void *dev_id);`
 **返回/释放**：同 4.1。
 **要点**：`thread_fn` 可睡眠；常配合 `IRQF_ONESHOT`。
 **误用**：在 `handler` 执行可睡眠 API；未正确设置触发类型导致抖动。

### `devm_free_irq`

**功能**：**提前**释放由 `devm_request_*_irq` 申请的中断。
 **原型**：`void devm_free_irq(struct device *dev, unsigned int irq, void *dev_id);`
 **适用**：需要在解绑前停止中断服务的场合。

------

## 5. 时钟（Common Clock Framework）

### `devm_clk_get`

**功能**：获取一个时钟**句柄**。
 **原型**：`struct clk *devm_clk_get(struct device *dev, const char *id);`
 **返回**：`struct clk *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 `clk_put()`。
 **要点（关键）**：`devm` **只托管句柄**；`clk_prepare_enable()` / `clk_disable_unprepare()`（**状态**）需在 `probe/remove/PM` 显式配对。

### `devm_clk_bulk_get`

**功能**：批量获取多个时钟句柄并在失败时统一回滚。
 **原型**：`int devm_clk_bulk_get(struct device *dev, int num_clks, struct clk_bulk_data *clks);`
 **返回**：`0` 或 `-Exxx`。
 **释放**：解绑/失败时自动 put。
 **适用**：多时钟域的外设。

------

## 6. 电源（Regulator）

### `devm_regulator_get`

**功能**：获取一个 regulator 句柄。
 **原型**：`struct regulator *devm_regulator_get(struct device *dev, const char *id);`
 **返回**：`regulator *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 put。
 **要点（关键）**：`regulator_enable()`/`regulator_disable()`（**状态**）需在 `probe/remove/PM` 显式配对；`devm` 不托管电源启停。

### `devm_regulator_get_optional`

**功能**：与 6.1 相同，但资源可缺省。
 **原型**：`struct regulator *devm_regulator_get_optional(struct device *dev, const char *id);`
 **适用**：硬件版本差异。

### `devm_regulator_bulk_get`

**功能**：批量获取 regulator。
 **原型**：`int devm_regulator_bulk_get(struct device *dev, int num_consumers, struct regulator_bulk_data *consumers);`
 **返回**：`0` 或 `-Exxx`。
 **释放**：解绑/失败时自动 put。
 **要点**：启停同样需要批量 `enable/disable` 自行配对。

### `devm_regulator_put`（少用）

**功能**：**提前**释放一个 `devm` 获取的 regulator 引用。
 **原型**：`void devm_regulator_put(struct regulator *regulator);`
 **适用**：特殊情况下提前放弃句柄；一般不必调用。

------

## 7. Reset 控制

### `devm_reset_control_get`

**功能**：获取复位控制句柄。
 **原型**：`struct reset_control *devm_reset_control_get(struct device *dev, const char *id);`
 **返回**：`reset_control *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 put。
 **要点**：具体复位时序（assert/deassert/pulse）由驱动控制；状态需在 `remove()/PM` 按需要复位。

### `devm_reset_control_get_exclusive`

**功能**：获取**独占**复位控制句柄。
 **原型**：`struct reset_control *devm_reset_control_get_exclusive(struct device *dev, const char *id);`
 **差异**：拒绝共享。适用于硬件要求严格独占的复位线。

### `devm_reset_control_get_shared`

**功能**：获取**共享**复位控制句柄。
 **原型**：`struct reset_control *devm_reset_control_get_shared(struct device *dev, const char *id);`
 **差异**：允许共享；注意并发与引用计数。

### `devm_reset_control_get_optional`

**功能**：可缺省版本。
 **原型**：`struct reset_control *devm_reset_control_get_optional(struct device *dev, const char *id);`

------

## 8. DMA 引擎

### `devm_dma_request_chan`

**功能**：按名称从 DMA 引擎请求一个通道。
 **原型**：`struct dma_chan *devm_dma_request_chan(struct device *dev, const char *name);`
 **返回**：`dma_chan *` 或 `ERR_PTR(-ENODEV/-EPROBE_DEFER/…)`。
 **释放**：解绑/失败时自动释放引用。
 **要点**：可能返回 `-EPROBE_DEFER`；与设备树 `dmas`/`dma-names` 匹配。

------

## 9. PHY

### `devm_phy_get`

**功能**：获取 PHY 句柄。
 **原型**：`struct phy *devm_phy_get(struct device *dev, const char *string);`
 **返回**：`phy *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 put。
 **要点**：`phy_power_on/off`、`phy_init/exit` 属于**状态/阶段操作**，需在 `probe/remove/PM` 明确配对。

------

## 10. pinctrl

### `devm_pinctrl_get`

**功能**：获取 pinctrl 句柄。
 **原型**：`struct pinctrl *devm_pinctrl_get(struct device *dev);`
 **返回**：`pinctrl *` 或 `ERR_PTR(-Exxx)`。
 **释放**：解绑/失败时自动 put。
 **要点**：`pinctrl_lookup_state()` + `pinctrl_select_state()` 的状态切换（如 `"default"`/`"sleep"`）**不受 devm 托管**，需在 `remove()/PM` 配对。

------

## 11. 平台辅助：中断号/资源获取（非 devm，但常与 devm 组合）

> 以下接口不是 `devm_*`，但与上面接口配合频繁，单独列出以免混淆。

### `platform_get_irq`

**功能**：从 `platform_device` 获取中断号。
 **原型**：`int platform_get_irq(struct platform_device *pdev, unsigned int num);`
 **返回**：`>=0` 的 IRQ 号或 `-Exxx`。
 **组合**：获取到 IRQ 后，**再**调用 `devm_request_*_irq` 进行托管。

### `platform_get_resource`

**功能**：从 `platform_device` 获取 `struct resource`。
 **原型**：`struct resource *platform_get_resource(struct platform_device *pdev, unsigned int type, unsigned int num);`
 **组合**：配合 `devm_ioremap_resource` 或 `devm_platform_ioremap_resource(_byname)`。

------

## 12. 注册类（示例）

### `devm_led_classdev_register`

**功能**：注册 LED class 设备，解绑自动注销。
 **原型**：`int devm_led_classdev_register(struct device *dev, struct led_classdev *led_cdev);`
 **返回**：`0` 或 `-Exxx`。
 **要点**：并发访问的同步由驱动负责。

### `devm_thermal_zone_of_sensor_register`

**功能**：向 thermal 框架注册 OF 传感器，解绑自动注销。
 **原型**：`int devm_thermal_zone_of_sensor_register(struct device *dev, int id, void *data, const struct thermal_zone_of_device_ops *ops);`
 **返回**：`0` 或 `-Exxx`。
 **要点**：`ops` 回调需要保证热路径稳定。

（其它如 `devm_extcon_dev_register`、IIO 的 `devm_*` 注册接口，语义一致：**注册成功 → 解绑自动注销**；差异体现在各子系统的回调与数据结构，按需查阅子系统文档。）

------

## 全局注意事项（统一要求）

- `devm` 仅托管**对象/句柄/映射**的释放；**不托管运行状态**（时钟启停、电源上/下电、pinctrl 状态、PHY 电源/初始化、工作队列/定时器等）。
- `probe()` 任意位置失败可直接返回；已登记的 `devm` 资源将按 LIFO 回滚。
- `remove()` 只做**状态回退**；**不要**在 `remove()` 再释放 `devm_*` 资源，避免二次释放。
- 生命周期**跨设备/全局**的资源不要使用 `devm_*`。
- 需要在解绑前**提前释放**个别资源的场景，使用对应的 `devm_*_put()`/`devm_free_*()` 或 `devm_add_action_or_reset()` 封装。
- 错误码：注意识别 `-EPROBE_DEFER`（依赖尚未就绪），按要求返回上层等待重试。

------

如果你需要，我可以把以上逐接口说明导出为 **Markdown/PDF 速查表**（每页左列“接口名/原型”，右列“语义/参数/错误/注意事项”），便于打印或团队内共享。

------

# 关键区别总表（同族接口横向对比）

| 类别         | 接口                                              | 主要区别点             | 推荐                          |
| ------------ | ------------------------------------------------- | ---------------------- | ----------------------------- |
| I/O 映射     | `devm_ioremap` vs `devm_ioremap_resource`         | 是否检查资源冲突       | **`_resource` 优先**          |
| 平台映射     | `devm_platform_ioremap_resource` vs `_byname`     | 按索引/按名称获取      | 依 DTS 命名使用               |
| GPIO         | `devm_gpiod_get` vs `_optional` vs `_index`       | 资源可缺省；多路索引   | 资源可选用 `_optional`        |
| IRQ          | `devm_request_irq` vs `devm_request_threaded_irq` | 是否提供线程化处理     | 需要可睡眠操作用 **threaded** |
| CLK          | `devm_clk_get` vs `devm_clk_bulk_get`             | 单个/批量获取          | 多时钟用 **bulk**             |
| REGULATOR    | `devm_regulator_get` vs `_optional` vs `bulk_get` | 可缺省/批量            | 按依赖关系选择                |
| RESET        | `get` vs `get_exclusive` vs `get_shared`          | 所有权模式             | 按硬件要求选择                |
| 平台回滚     | `devres_open_group/close/remove`                  | 阶段化回滚控制         | 大型 `probe()` 使用           |
| 无 devm 资源 | `devm_add_action` vs `_or_reset`                  | 注册失败时是否立即回滚 | **`_or_reset` 优先**          |

------

# 常见误用与修正

1. **在 `remove()` 手动释放 `devm_\*` 资源** → 可能二次释放。
    **修正**：`remove()` 只回退“状态”，对象/句柄由 devres 回收。
2. **将状态当成托管对象**（如把关电放在 `devm_add_action`） → 状态没有在 PM 路径配对。
    **修正**：状态在 `remove()`/suspend 显式回退，与 `devm` 解耦。
3. **跨设备/全局共享资源用 `devm_\*`** → 另一使用方仍需资源时被提前释放。
    **修正**：此类资源使用旧机制，明确所有权与释放时机。
4. **顶半部执行可睡眠操作** → 中断处理异常。
    **修正**：使用 `devm_request_threaded_irq()`，在线程函数内执行可睡眠操作。
5. **未处理 `-EPROBE_DEFER`** → 设备随机初始化失败。
    **修正**：对依赖型资源获取失败时识别 `-EPROBE_DEFER` 并返回上层，等待重试。

------

# 最小决策规则（学习与实战）

- 句柄/映射/对象在**同一设备生命周期内** → 使用 `devm_*`。
- 需要**阶段化回滚** → 使用分组接口。
- 需要在 `probe()` 中某点**提前释放**或生命周期**跨设备/全局** → 使用旧机制或 `devm_add_action_or_reset()` 封装。
- 所有**运行状态**（clk/regulator/pinctrl/PHY power/任务）**不由 `devm` 托管**，在 `remove()`/PM 路径显式回退。

如果你希望，我可以把这份清单转成 **PDF/Markdown 速查表**（按子系统分栏，附常见错误码对照），方便打印或内网Wiki收录。