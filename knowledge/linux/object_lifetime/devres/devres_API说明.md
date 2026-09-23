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

这组接口管理绑定期内存，声明位于`include/linux/device.h`，主要实现位于`drivers/base/devres.c`。非零分配失败用NULL表示；它不是错误指针接口。内存复制也不自动取得结构体内部指针所指对象的引用。源码顺序见[资源族导读](../../../../research/source_reading/devres/navigation/P03_内存映射与中断资源导读.md#3.1_内存失败与零大小)。

### 2.2.1\_devm\_kzalloc

原型：`void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)`。它对devm_kmalloc加入清零标志。申请非零大小时成功返回可用内存，失败返回NULL；清理设备记录时回收，不因外部还有device引用而延长。

零大小在本版本返回`ZERO_SIZE_PTR`，没有提供可读写字节，不能用“非NULL”证明至少有一个有效元素。私有状态仍须在回调可能访问以前完成初始化；零清不会自动把mutex、list_head等对象初始化成合法机制状态。

### 2.2.2\_devm\_kcalloc

原型：`void *devm_kcalloc(struct device *dev, size_t n, size_t size, gfp_t gfp)`。先检查`n * size`是否溢出，再按总字节数分配并清零；溢出返回NULL。总大小为零时沿相同零大小约定，不允许访问元素。

它保护的是分配长度计算，不替调用者校验索引、外部长度或元素内部资源。提前回收必须使用对应托管接口，不能直接kfree留下旧登记。

### 2.2.3\_devm\_kmemdup

原型：`void *devm_kmemdup(struct device *dev, const void *src, size_t size, gfp_t gfp)`。分配后复制指定字节，非零大小失败返回NULL。调用者须保证src在复制期间有效、长度正确且所需字段一致；复制只是字节级浅拷贝，不为内部指针追加引用，也不自动添加字符串终止字节。

### 2.2.4\_devm\_kstrdup

原型：`char *devm_kstrdup(struct device *dev, const char *s, gfp_t gfp)`。对有效、以零字节终止的字符串分配`strlen(s) + 1`字节并复制；分配失败返回NULL，本版本输入s为NULL也返回NULL。调用者若把NULL输入作为业务错误，须自行区分，不能仅凭结果判定内存不足。

非终止字节序列应按明确长度选择kmemdup；字符串源的读期限和并发写入也必须由调用者保障。

## 2.3\_I/O\_资源与寄存器映射

映射给出的是内核访问设备地址空间的入口，类型`__iomem`用来标注I/O地址，不应当作普通RAM指针随意解引用。基础映射和resource包装的失败表示 **不同**：前者NULL，后者错误指针。固定实现见[映射返回值导读](../../../../research/source_reading/devres/navigation/P03_内存映射与中断资源导读.md#3.2_映射与区域占用是两条责任)。

### 2.3.1\_devm\_ioremap

原型：`void __iomem *devm_ioremap(struct device *dev, resource_size_t offset, resource_size_t size)`。注意size也是resource_size_t。头文件为`include/linux/io.h`，实现为`lib/devres.c`；成功登记映射的iounmap责任，分配记录或底层映射失败均返回NULL，使用空指针判断。

它不同时申请该物理资源区间的独占使用权。适用于区间所有权已由其他正确协议建立的情况；不是只要获得映射就能安全访问任意设备地址。具体记录失败与映射失败分支见[基础映射实现](../../../../research/source_reading/devres/source_explanations/lib/devres.c.md#1.1_基础映射失败保持NULL)。

### 2.3.2\_devm\_ioremap\_resource

原型：`void __iomem *devm_ioremap_resource(struct device *dev, const struct resource *res)`，声明位于`include/linux/device.h`。检查非空内存资源、申请区间后再映射；成功返回I/O地址，失败返回编码错误，包括无效资源`-EINVAL`、申请区间失败`-EBUSY`、记录或映射失败`-ENOMEM`等，使用IS_ERR/PTR_ERR。

区间申请和映射分别留下责任；映射失败时会撤回已经申请的区间。资源名字等更早的托管分配仍由账本后续清理，因此“本次返回失败”不等于“设备链和调用前逐字相同”。有合适struct resource且需要申请其使用权时通常选择此族；不要在已经独占申请相同区域后再次申请造成自冲突。见[resource包装实现](../../../../research/source_reading/devres/source_explanations/lib/devres.c.md#1.2_资源包装将失败编码并撤回区域)。

### 2.3.3\_devm\_platform\_ioremap\_resource

原型：`void __iomem *devm_platform_ioremap_resource(struct platform_device *pdev, unsigned int index)`。声明位于`include/linux/platform_device.h`；获取编号index的内存资源后进入resource包装，沿同一错误指针契约检查。index从0开始，查找不到资源也不是成功的NULL映射。

### 2.3.4\_devm\_platform\_ioremap\_resource\_byname

原型：`void __iomem *devm_platform_ioremap_resource_byname(struct platform_device *pdev, const char *name)`。按资源名查找后进入同一包装，适合已经定义稳定命名的资源。名称要与最终platform资源表一致，不能只凭任意设备树字符串猜测。

两种platform入口在无HAS_IOMEM配置下有返回错误的头文件桩，不能从编译通过推断硬件映射存在。主动提前iounmap只处理映射；区间占用是另一条责任，应按实际需要使用对应托管区间释放接口。解绑前仍须阻止IRQ、工作或用户继续使用该I/O地址。

## 2.4\_GPIO(gpiod\_消费者)

GPIO描述符代表设备请求的信号线及相关状态。这里使用消费者接口，不用旧整数编号表达归属；方向与输出值是逻辑语义，需结合active-low极性理解真实电平。完整消费者机制由[GPIO专题](../../../driver_model/gpio/大纲.md)组织，返回与登记边界见[资源族导读](../../../../research/source_reading/devres/navigation/P03_内存映射与中断资源导读.md#3.3_GPIO可选缺席与记录失败)。

### 2.4.1\_devm\_gpiod\_get

原型：`struct gpio_desc *devm_gpiod_get(struct device *dev, const char *con_id, enum gpiod_flags flags)`，头文件`include/linux/gpio/consumer.h`。按连接标识con_id取得第0个描述符，可由GPIOD_IN或GPIOD_OUT_LOW/HIGH等指定初始方向和逻辑输出。

启用GPIOLIB时，成功返回描述符，未分配指定GPIO返回`ERR_PTR(-ENOENT)`，其他失败保持对应错误指针。先判IS_ERR再使用；devm成功路径登记gpiod_put责任，不保证驱动自己遗漏的工作退出或板级安全电平策略。

### 2.4.2\_devm\_gpiod\_get\_optional

原型参数与普通get相同。启用实现只把“未分配该GPIO”的`-ENOENT`转换为NULL，资源存在时仍返回描述符，其他失败（包括适用的延迟探测）仍是错误指针。顺序应是先IS_ERR处理真实失败，再按NULL选择无此功能的业务分支。

未启用GPIOLIB时，optional桩返回NULL，而非可选get桩返回`-ENOSYS`。因此NULL不能单独证明实际电路板没有这根线；可选策略还须与驱动配置依赖及硬件要求一致。

### 2.4.3\_devm\_gpiod\_get\_index

原型：`struct gpio_desc *devm_gpiod_get_index(struct device *dev, const char *con_id, unsigned int index, enum gpiod_flags flags)`。选择同一连接中的第index项；普通get是index为0的入口，成功/错误和自动put契约相同。

固定实现先取得GPIO，再分配管理记录；后者失败会先gpiod_put，再返回`-ENOMEM`。非独占标志还有复用已有登记的分支，不能把每次get都假定成新建一条记录。提前释放应使用匹配的devm_gpiod_put，并停止其他使用者，不能直接gpiod_put却保留托管记录。

## 2.5\_IRQ

中断请求接口登记处理函数；注册成功以后处理路径就可能开始使用dev_id及其指向的状态。申请前必须完成处理函数所需初始化，清理时必须使IRQ及其派生工作先于相关内存或映射退出。源码配合见[IRQ登记导读](../../../../research/source_reading/devres/navigation/P03_内存映射与中断资源导读.md#3.4_IRQ记录不替代使用者退出)。

### 2.5.1\_devm\_request\_irq

原型：`int devm_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev_id)`，头文件`include/linux/interrupt.h`。本版本是传thread_fn为NULL的内联包装。成功返回0，失败返回负错误；硬中断handler不得调用可睡眠接口。

dev_id是交回处理函数的身份参数，共享IRQ还依它区分申请者。它不是自动被devres保活的引用；资源和结构体存储必须由调用者安排好期限。

### 2.5.2\_devm\_request\_threaded\_irq

原型：`int devm_request_threaded_irq(struct device *dev, unsigned int irq, irq_handler_t handler, irq_handler_t thread_fn, unsigned long flags, const char *name, void *dev_id)`。成功0，失败负错误。handler承担必要的硬中断判断，thread_fn在线程化上下文执行，可使用相应允许睡眠的操作；IRQF_ONESHOT等标志按底层request_threaded_irq契约选择，不意味着任何共享线都能不作来源判断。

管理包装先分配irq_devres，再调用底层申请；失败释放尚未登记的记录，成功才保存irq/dev_id并登记free_irq回调。若底层申请已经允许中断执行，驱动不得等函数返回后才初始化处理函数所需字段。

### 2.5.3\_devm\_free\_irq

原型：`void devm_free_irq(struct device *dev, unsigned int irq, void *dev_id)`。用相同设备、IRQ号和身份参数移除对应记录，再执行free_irq，适合必须在默认账本顺序之前结束IRQ责任的情况。不能用普通free_irq留下记录，也不能将同一份IRQ反复释放当作幂等接口。

IRQ处理退出不等于它排出的work或其他异步活动也退出。需要关闭硬件来源并按依赖排空这些活动；devm管理包装没有因此接管所有后续使用者。真实硬件顺序须在匹配平台和配置验证，本页的类型检查不作该证明。


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
