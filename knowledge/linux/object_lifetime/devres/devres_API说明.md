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

时钟框架向设备提供运行所需时钟；“取得时钟句柄”与“准备并使能时钟”是不同责任。声明在`include/linux/clk.h`，管理包装在`drivers/clk/clk-devres.c`。沿[句柄与状态导读](../../../../research/source_reading/devres/navigation/P04_句柄启停与注册契约导读.md#4.1_时钟把退出动作放进同一记录)对照普通和enabled两条路径。

### 2.6.1\_devm\_clk\_get

原型：`struct clk *devm_clk_get(struct device *dev, const char *id)`。启用相应实现时返回句柄或错误指针，托管的是clk_put，未自动调用clk_prepare_enable。若自行使能，就要按使用期限配对disable/unprepare，不能只依赖句柄回收。

未启用对应时钟支持的头文件有空操作桩；NULL在部分桩或optional接口中可以表示无须实际操作的时钟，不应将所有NULL都编造成同一种硬件错误。驱动对真实时钟的要求还须由配置依赖和平台描述保障。

### 2.6.2\_devm\_clk\_bulk\_get

原型：`int devm_clk_bulk_get(struct device *dev, int num_clks, struct clk_bulk_data *clks)`。获取多项句柄，成功0，失败负错误；已取得项在批量获取失败时按底层契约回滚，成功登记后清理调用clk_bulk_put。这一接口不自动enable整组。

管理记录保存的是调用者的clks数组地址，而不是复制整个数组，因此数组和其中所需数据必须活到清理完成。不能把probe栈上的临时数组交给它后返回。参数批量化只减少代码重复，不消除启停、共享及异步使用的约束。

### 2.6.3\_devm\_clk\_get\_enabled

原型：`struct clk *devm_clk_get_enabled(struct device *dev, const char *id)`。先获取，再clk_prepare_enable；成功记录清理时先clk_disable_unprepare，再clk_put。使能失败会归还已取得句柄并释放未登记记录。它证明“devm永远不托管运行状态”是不成立的，但也不会代替每次PM暂停/恢复的策略。

需要整个绑定期持续使能的简单责任可选择此包装；需要频繁电源转换或精确启停顺序时，须另行组织状态责任，避免在remove中重复disable已由托管回调承担的那一份。optional_enabled还有可选获取语义，不能仅凭名称当作必需时钟一定存在。

## 2.7\_电源(Regulator)

regulator消费者句柄代表设备对供电资源的请求，获取和使能同样分开。共享供电轨的实际电平还取决于提供者约束和其他消费者；归还一个句柄不等于已经证明硬件断电。声明在`include/linux/regulator/consumer.h`，管理包装在`drivers/regulator/devres.c`，见[供电责任导读](../../../../research/source_reading/devres/navigation/P04_句柄启停与注册契约导读.md#4.2_供电的句柄记录与disable动作)。

### 2.7.1\_devm\_regulator\_get

原型：`struct regulator *devm_regulator_get(struct device *dev, const char *id)`。正常实现返回句柄或错误指针，成功登记regulator_put责任，不自动regulator_enable。普通获取的提供者查找政策可能包含dummy替代，不能把非错误返回当作实测电压已经符合要求。

无REGULATOR时普通获取桩返回NULL。驱动应遵循相应配置契约，不能把“编译通过且非错误”提升为存在真实供电控制器的结论。

### 2.7.2\_devm\_regulator\_get\_optional

原型与普通get相同，名字为`devm_regulator_get_optional`。可选指不应强行采用普通获取的dummy替代；**缺席不是GPIO optional式的NULL契约**，启用实现保留相应错误指针，调用者须按供电依赖策略区分缺席、延迟探测和其他失败。无REGULATOR的optional桩返回`-ENODEV`错误指针。

因此不可把所有子系统的optional写进同一个“NULL表示缺席”辅助函数。明确哪一类缺席允许继续工作，再处理其余错误。

### 2.7.3\_devm\_regulator\_bulk\_get

原型：`int devm_regulator_bulk_get(struct device *dev, int num_consumers, struct regulator_bulk_data *consumers)`。成功0，失败负错误；获取失败时归还本次先前取得的项，成功后登记regulator_bulk_free责任。此函数没有隐含批量enable。

consumers数组地址被管理记录保存，必须具有足够存储期限；批量启停若另行调用，也要按其成功/失败契约和资源依赖组织配对，不能仅在remove中无条件循环disable。

### 2.7.4\_devm\_regulator\_put(少用)

原型：`void devm_regulator_put(struct regulator *regulator)`。通过句柄关联的设备找到管理记录，执行释放并移除该记录；它的参数里没有额外dev。用于提前结束由普通托管获取建立的句柄责任，不能用来自动撤销仍需要该句柄的其他action或使用者。

### 2.7.5\_devm\_regulator\_get\_enable

原型：`int devm_regulator_get_enable(struct device *dev, const char *id)`。这里返回整数状态，**不返回供驱动继续操作的句柄**。成功先登记获取责任，再登记disable action，逆序时先disable后put；enable或action登记失败会回滚当前责任，action登记失败的reset路径已执行disable。

适用于绑定期使能的明确需求；不能把它和“自己保存句柄、按PM反复启停”的模型混用。其optional变体仍按供电optional获取契约传播错误，不照搬GPIO的NULL规则。

## 2.8\_Reset\_控制

复位控制描述硬件模块怎样被置于或释放出复位状态；句柄的共享政策与硬件当前是否处于复位是两组状态。公共包装在`include/linux/reset.h`选择shared、optional、acquired参数，核心在`drivers/reset/core.c`登记reset_control_put。见[复位及其他资源导读](../../../../research/source_reading/devres/navigation/P04_句柄启停与注册契约导读.md#4.3_复位所有权与缺席)。

### 2.8.1\_devm\_reset\_control\_get

原型：`struct reset_control *devm_reset_control_get(struct device *dev, const char *id)`。本版本是exclusive获取的包装，成功返回控制句柄，失败错误指针；取得句柄不等于已经执行assert、deassert或reset操作。硬件复位时序须按设备协议实施。

### 2.8.2\_devm\_reset\_control\_get\_exclusive

原型：`struct reset_control *devm_reset_control_get_exclusive(struct device *dev, const char *id)`。请求已取得操作资格的独占控制，不能和其他不相容的持有方式同时使用同一控制。它管理最终put，不替驱动决定退出时应保持复位还是解除复位。

### 2.8.3\_devm\_reset\_control\_get\_shared

原型：`struct reset_control *devm_reset_control_get_shared(struct device *dev, const char *id)`。面向硬件模块共享同一复位控制的情形。共享assert/deassert受复位核心的deassert_count等协议约束，不能把自己的assert当作不顾其他用户、立即拉动共享线的命令；必须按实际操作契约配对。

“多个代码位置都想访问”本身不是选择shared的理由，应先证明硬件和消费者之间允许这种共享语义。

### 2.8.4\_devm\_reset\_control\_get\_optional

原型：`struct reset_control *devm_reset_control_get_optional(struct device *dev, const char *id)`。本版本转到optional_exclusive；允许描述中没有对应复位时返回NULL，其他失败仍是错误指针。底层返回NULL时管理包装释放尚未登记的记录，不创建一条假资源责任。

未启用RESET_CONTROLLER时optional桩返回NULL，非optional返回`-ENOTSUPP`错误指针。调用者要让配置要求与硬件必需条件一致，不能用optional掩盖缺失的控制器支持。

## 2.9\_DMA\_引擎

DMAengine管理的是传输通道与提交给它的工作，不是CPU通过普通指针访问内存的同义操作。通道句柄、传输结束、回调退出及缓冲区寿命必须分别证明。

### 2.9.1\_dma\_request\_chan与显式管理

本基线的公共入口为`struct dma_chan *dma_request_chan(struct device *dev, const char *name)`，头文件`include/linux/dmaengine.h`；它不是devm接口。成功返回通道，失败错误指针，包括依赖未就绪时可能出现的`-EPROBE_DEFER`。按设备的通道描述和名称获取，退出通过dma_release_channel归还。

若用devm_add_action_or_reset封装归还，必须先证明action参数到清理时仍有效，并在释放通道或缓冲区以前停止新提交、等待或终止已提交传输及其回调。dmaengine_terminate_sync成功返回才形成它所承诺的同步终止证据；其错误不能忽略，也不能在原子上下文或同通道完成回调里调用。这里给出管理边界，不提供省略停止流程的伪“自动DMA”模板。固定入口见[其他资源导读](../../../../research/source_reading/devres/navigation/P04_句柄启停与注册契约导读.md#4.4_DMA和PHY的独立阶段)。

## 2.10\_PHY

这里指通用PHY框架管理的物理层部件，不把所有网络PHY接口都混为同一套API。取得部件句柄与初始化、上电等阶段是不同操作。

### 2.10.1\_devm\_phy\_get

原型：`struct phy *devm_phy_get(struct device *dev, const char *string)`，头文件`include/linux/phy/phy.h`。成功返回PHY句柄，失败错误指针；管理包装记录phy_put，未自动代办phy_init/exit或phy_power_on/off。

驱动按实际成功阶段回滚，并按PM和硬件依赖安排退出；不能在某次power_on失败以后假定它已建立需要power_off的责任。若关闭通用PHY支持，头文件桩有单独契约，须同时核对驱动的配置依赖。

## 2.11\_pinctrl

pinctrl组织设备引脚复用和配置状态；句柄、某个命名状态对象、选择状态的结果分别属于不同步骤。

### 2.11.1\_devm\_pinctrl\_get

原型：`struct pinctrl *devm_pinctrl_get(struct device *dev)`，头文件`include/linux/pinctrl/consumer.h`。启用实现成功返回句柄，失败错误指针，登记pinctrl_put；不因这次获取就自动完成驱动期待的default/sleep切换。

pinctrl_lookup_state返回状态对象或错误，pinctrl_select_state返回整数结果，二者的错误也要处理。框架已有自动状态选择路径与驱动的PM策略应协调，不能不看总线及驱动核心行为就在所有remove里重复切换。提前结束句柄使用devm_pinctrl_put；这不代替设备安全电平时序。源码入口见[状态与注册导读](../../../../research/source_reading/devres/navigation/P04_句柄启停与注册契约导读.md#4.5_pinctrl与注册类返回值)。

## 2.12\_平台辅助\_中断号/资源获取(非\_devm\_但常与\_devm\_组合)

以下是platform资源查询入口，不承担devres自动登记。先完成查询，再把合法结果交给相应申请或映射接口。

### 2.12.1\_platform\_get\_irq

原型：`int platform_get_irq(struct platform_device *pdev, unsigned int num)`，头文件`include/linux/platform_device.h`。成功返回有效正IRQ号，失败负错误；本实现会把不应出现的0当作无效IRQ处理。先判断负值并传播适用的延迟错误，再调用devm_request_irq或threaded变体；取得IRQ号本身未注册handler。

### 2.12.2\_platform\_get\_resource

原型：`struct resource *platform_get_resource(struct platform_device *pdev, unsigned int type, unsigned int num)`。返回设备资源表中的匹配描述，找不到返回NULL；这是借用描述，不是新分配的托管资源。对内存区域可交给devm_ioremap_resource检查和申请，也可用组合platform映射入口；不要对该描述执行自创的free。

## 2.13\_注册类(示例)

注册接口可能把驱动提供的对象、操作表或数据暴露给其他执行者。必须在注册允许调用之前完成初始化，并保持它们活到注销所要求的活动退出点；“自动注销”不说明所有对象都由框架代为分配和回收。

### 2.13.1\_devm\_led\_classdev\_register

原型：`int devm_led_classdev_register(struct device *dev, struct led_classdev *led_cdev)`，头文件`include/linux/leds.h`。本版本是ext注册接口的内联包装，成功0，失败负错误。成功登记led_classdev_unregister责任；led_cdev由调用者提供，不能指向probe返回后消失的栈变量。

失败时也要按真实注册结果处理，不能仅根据“devm”就当作已经有注销记录。并发回调及硬件访问的驱动约束仍需满足。

### 2.13.2\_devm\_thermal\_of\_zone\_register

本版本原型：`struct thermal_zone_device *devm_thermal_of_zone_register(struct device *dev, int id, void *data, const struct thermal_zone_device_ops *ops)`，头文件`include/linux/thermal.h`。成功返回热区对象，失败错误指针，使用IS_ERR/PTR_ERR；不是返回0/负错误的整数API。

包装成功登记thermal_of_zone_unregister清理，data和ops涉及的状态要活过回调使用期限。无THERMAL_OF配置时桩返回`-ENOTSUPP`错误指针。不要把旧函数名、旧ops类型或遗留源码注释中的名称拼成当前原型；应以声明和函数定义共同定位。

其他如extcon、IIO及DRM有各自对象与清理期限，按对应子系统继续查阅；尤其drmm的生命周期对象不能直接等同于通用devm设备资源链。


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
| I/O 映射     | `devm_ioremap` vs `devm_ioremap_resource`         | 前者失败为NULL；后者校验并申请区域，失败为错误指针 | 需要同时取得区域占用责任时用`_resource`；不能重复申请已占用区域 |
| 平台映射     | `devm_platform_ioremap_resource` vs `_byname`     | 按索引/按名称获取      | 依 DTS 命名使用               |
| GPIO         | `devm_gpiod_get` vs `_optional` vs `_index`       | optional允许规定的缺席，index选择多路中的一项 | 按硬件依赖选取；仍传播其他错误，并核对配置桩 |
| IRQ          | `devm_request_irq` vs `devm_request_threaded_irq` | 是否提供线程化处理     | 需要可睡眠操作用 **threaded** |
| CLK          | `devm_clk_get`、`devm_clk_bulk_get`与`devm_clk_get_enabled` | 普通单个/批量仅取得句柄；enabled同时准备使能并登记逆操作 | 按句柄与运行阶段责任选择；批量数组须活到清理结束 |
| REGULATOR    | `devm_regulator_get`、`_optional`、`bulk_get`与`get_enable` | optional缺席按错误指针处理；get_enable返回整数并登记disable和put | 分清可选供电政策与运行阶段；批量数组期限由调用者保证 |
| RESET        | `get` vs `get_exclusive` vs `get_shared`          | 此版本普通get即exclusive；shared受共享状态配对规则约束 | 依据真实复位线使用者与硬件协议选择，不能只为避开忙错误而改shared |
| 阶段回滚     | `devres_open_group/close/remove/release`                  | close限定范围；remove仅撤标记；release实际清理组内资源 | 保留阶段成果用remove，撤销阶段责任用release |
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
