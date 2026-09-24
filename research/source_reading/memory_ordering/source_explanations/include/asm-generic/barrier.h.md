---
id: research.source_reading.memory_ordering.smp_fence_impl
title: "barrier.h SMP 屏障与发布取得源码实现"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
---

# 第1章\_barrier.hSMP屏障与发布取得源码实现

## 1.1\_从公共调用找到真正执行的分支

[屏障模块导读](../../../navigation/P03_SMP屏障的配置与调用层次.md#3.3_一次公共屏障的路径)已经建立F0～F3：架构先提供定义，配置选择包装，检测与功能路径分别执行，调用方继续。本页先沿这条路径解释实现原理，再结合[发布取得模块](../../../navigation/P04_发布取得的访问与配置导读.md#4.3_让实现回到同一组S阶段)把顺序绑定到一次访问。范围包括三种公共SMP屏障与发布取得回退，不声称覆盖整个barrier.h。

上游位置为 `include/asm-generic/barrier.h`，编译器barrier定义来自 `include/linux/compiler.h`；版本为NXP官方linux-imx、lf-6.12.20-2.0.0、dfaf2136deb2af2e60b994421281ba42f1c087e0、Linux 6.12.20。[总索引](../../../navigation/P01_Linux_6.12_LKMM_源码与模型导读.md#1.3.2_通用屏障)定位相关模块，[原始通用头](../../../../linux/include/asm-generic/barrier.h)和[compiler.h](../../../../linux/include/linux/compiler.h)用于逐项核对。以下Doxygen与中文行注均为仓库补充阅读说明；剪裁掉文件外壳和其他API，没有改变条件分支。

## 1.2\_内部接口如何获得缺省实现

先在F0看定义是否已存在。内部宏__smp_mb/rmb/wmb连接到架构实现；未定义时，通用头才退回相应基础屏障接口。前缀不是功能强弱的证明，实际定义和调用层次才是依据。

```c
/**
 * __smp_mb - 未被架构提供时，采用基础全屏障
 * __smp_rmb - 未被架构提供时，采用基础读屏障
 * __smp_wmb - 未被架构提供时，采用基础写屏障
 * 仓库阅读说明：这里只补缺，不重新定义已有底层实现。
 */
#ifndef __smp_mb
#define __smp_mb() mb()
#endif
#ifndef __smp_rmb
#define __smp_rmb() rmb()
#endif
#ifndef __smp_wmb
#define __smp_wmb() wmb()
#endif
```

`#ifndef`由预处理器判断，不是运行时if。若架构已定义__smp_wmb，调用它不会先绕到本段的wmb回退；若未定义，才沿基础接口继续查。这仍不是最终机器指令：基础接口本身也可能是架构定义或通用回退，读者必须顺着包含顺序继续展开。

## 1.3\_三种公共SMP屏障的配置分支

公共宏smp_mb、smp_rmb、smp_wmb是调用方通常使用的入口。F1首先选择是否启用CONFIG_SMP；F2再由所选包装决定是否经过检测接口及内部功能实现。

```c
/**
 * smp_mb / smp_rmb / smp_wmb - 按配置提供公共屏障
 * 仓库阅读说明：SMP分支先进入对应检测入口，再进入功能原语。
 * 已有公共定义时，通用头不再覆盖；UP分支只采用编译器屏障。
 */
#ifdef CONFIG_SMP

#ifndef smp_mb
#define smp_mb() do { kcsan_mb(); __smp_mb(); } while (0)
#endif
#ifndef smp_rmb
#define smp_rmb() do { kcsan_rmb(); __smp_rmb(); } while (0)
#endif
#ifndef smp_wmb
#define smp_wmb() do { kcsan_wmb(); __smp_wmb(); } while (0)
#endif

#else /* !CONFIG_SMP：保留编译器约束。 */

#ifndef smp_mb
#define smp_mb() barrier()
#endif
#ifndef smp_rmb
#define smp_rmb() barrier()
#endif
#ifndef smp_wmb
#define smp_wmb() barrier()
#endif

#endif /* CONFIG_SMP */
```

在通用SMP包装中，检测入口位于功能原语之前。kcsan_mb/rmb/wmb属于内核并发检测器KCSAN（Kernel Concurrency Sanitizer）的屏障接入层；这三个调用不取得业务锁，不发送IPI，也不等待另一个线程读取flag。

固定 `include/linux/kcsan-checks.h` 进一步区分弱内存检测配置及插桩来源：编译器线程插桩路径借用signal fence表达检测事件，显式屏障插桩路径调用检测辅助函数，其他分支为空。这里仅解释接入边界，不把三种名称统一描述成每次都发生真实函数调用；检测器状态推进仍属检测器实现。尤其不能删除__smp_mb后用kcsan_mb补偿：一个记录检查语义，另一个承担功能顺序。

公共宏外层的ifndef也有意义。如果架构预先定义了smp_mb，通用头不会在它外面再次添加kcsan_mb。要判断这种覆盖是否正确接入检测，必须继续读覆盖实现，不能根据本段没有生效的代码下结论。

## 1.4\_barrier怎样约束编译器

F3在通用UP分支落到compiler.h的编译器屏障。下列定义与硬件屏障不是同一层接口。

```c
/**
 * barrier - 编译器优化屏障（仓库补充阅读说明）
 * 空汇编没有指定硬件指令；memory clobber使编译器顾及内存影响。
 */
#ifndef barrier
# define barrier() __asm__ __volatile__("": : :"memory")
#endif
```

三个冒号分隔GNU内联汇编的输出、输入和clobber部分；前两项为空，最后的memory声明内存影响。编译器不能像完全没有该影响时那样跨越此处保留其对普通内存的全部假设。volatile防止把这段汇编当作无用表达式删掉；空模板本身不会生成dmb、mfence或其他指定的处理器屏障指令。

这不意味着为每个局部变量都生成一次写回，也不意味着强制处理器按源程序顺序完成所有访存，更不是刷新整个缓存。共享访问本身仍需要ONCE等适合内核契约的标记；[唯一ONCE实现](rwonce.h.md#1.3_READ_ONCE与内部读取)负责访问形态，本页负责不同访问间的编译与配置边界。

## 1.5\_修改与验证的边界

以上九项分支定义加一个编译器屏障定义，分别落在三个内部回退、三个SMP公共包装、三个UP公共包装和barrier。改变ifndef、把检测入口移到功能原语后面，或让UP公共入口直接调用内部__smp原语，都可能改变实际生成路径；不能仅凭宏体很短就当作无语义整理。

可以用受控记录函数替代检测入口和底层原语，检查“公共调用选择了哪条路径、先调用谁”。这种单线程夹具只能验证展开与调用顺序，不验证处理器内存重排，也不能证明KCSAN在真实内核中发现了竞态。硬件保证必须继续核对目标架构，模型保证须运行相应Linux内核内存模型（Linux Kernel Memory Model，LKMM）测试；本页不以替身记录代替二者。

回到[模块的配置比较](../../../navigation/P03_SMP屏障的配置与调用层次.md#3.5_把配置比较变成可检验问题)：默认三分支先闭合，公共覆盖再单独检查。接下来把顺序绑定到发布取得；atomic辅助、设备屏障与ARM具体指令仍需各自的前提和证据，不能从三个入口机械外推。

## 1.6\_发布取得的内部回退

发布方向要在写入发布位置之前约束先前访问，取得方向要在读出该位置之后约束后续访问。通用实现采用全屏障来兑现单向契约，这是实现选择，不把接口契约提升为任意位置的全屏障。

类型辅助宏compiletime_assert_atomic_type位于compiler_types.h。它使用[已解释的__native_word](rwonce.h.md#1.2_类型工具与尺寸门槛)按尺寸判断；这里没有额外接受long long大小的或分支。

```c
/**
 * compiletime_assert_atomic_type - 约束访问为本机基本整数尺寸
 * @t: 待访问表达式；断言发生于编译期，不是运行时地址检查
 * 仓库阅读说明：不包含rwonce尺寸检查中的额外long long例外。
 */
#define compiletime_assert_atomic_type(t) \
    compiletime_assert(__native_word(t), \
        "Need native word sized stores/loads for atomicity.")

/**
 * __smp_store_release - 通用发布回退
 * @p: 有效发布位置的指针
 * @v: 发布值；其赋值表达式位于内部屏障之后
 * 仓库阅读说明：S1先检查类型，再约束此前访问，最后写入发布位置。
 */
#ifndef __smp_store_release
#define __smp_store_release(p, v) \
do { \
    compiletime_assert_atomic_type(*p); \
    __smp_mb(); \
    WRITE_ONCE(*p, v); \
} while (0)
#endif

/**
 * __smp_load_acquire - 通用取得回退
 * @p: 有效发布位置的指针
 * 返回S2读取并保存的值；不在屏障之后重新读取发布位置。
 */
#ifndef __smp_load_acquire
#define __smp_load_acquire(p) \
({ \
    __unqual_scalar_typeof(*p) ___p1 = READ_ONCE(*p); \
    compiletime_assert_atomic_type(*p); \
    __smp_mb(); \
    (typeof(*p))___p1; \
})
#endif
```

取得宏把读值放到局部___p1，再执行内部屏障，最后将同一个值转回表达式类型。若把返回表达式改成再次READ_ONCE，屏障前后可能读到不同轮次的ready，便改变了当前证明绑定的访问。断言虽然在源码中排在读取语句之后，却仍是编译期诊断；编译失败时没有先执行一次读取再报告错误的运行流程。

发布宏也不是普通函数调用。v真正参与赋值位于__smp_mb之后；调用方应先完成S0载荷准备，再用稳定指针和值进入S1，不把需要被发布覆盖的副作用隐藏在v中。指针有效性、对齐及对象存活仍由外层协议提供，尺寸断言不负责这些事实。

## 1.7\_发布取得的公共配置分支

公共包装smp_store_release和smp_load_acquire决定上述内部回退是否成为当前调用路径。下面保留原来的CONFIG_SMP条件，只裁掉同一条件块里的其他接口。

```c
/**
 * smp_store_release / smp_load_acquire - 选择发布取得的公共路径
 * 仓库阅读说明：SMP发布侧有release检测入口，取得侧直接调用内部实现；
 * UP分支不调用这两个内部宏，使用编译器屏障与ONCE访问组合。
 */
#ifdef CONFIG_SMP

#ifndef smp_store_release
#define smp_store_release(p, v) do { kcsan_release(); __smp_store_release(p, v); } while (0)
#endif
#ifndef smp_load_acquire
#define smp_load_acquire(p) __smp_load_acquire(p)
#endif

#else

#ifndef smp_store_release
#define smp_store_release(p, v) \
do { \
    barrier(); \
    WRITE_ONCE(*p, v); \
} while (0)
#endif
#ifndef smp_load_acquire
#define smp_load_acquire(p) \
({ \
    __unqual_scalar_typeof(*p) ___p1 = READ_ONCE(*p); \
    barrier(); \
    (typeof(*p))___p1; \
})
#endif

#endif
```

两边公共入口并不对称。发布侧的kcsan_release表达检测事件，功能路径仍必须执行内部发布；取得侧此处没有一个额外kcsan_acquire包装。检测器如何识别访问及屏障，要继续依据其插桩实现，不能据宏行数推测“取得不受任何检测”。

UP路径保留读写方向，但不执行内部回退中的compiletime_assert_atomic_type。READ_ONCE/WRITE_ONCE仍有自己的门槛，所以不是无限制访问。若代码仅在UP配置通过编译，应再次核对切换SMP或更换架构后的尺寸与原子性；不能以这一配置差异规避正确协议的要求。

## 1.8\_沿同一发布周期核对结果

对S1，检查屏障是否在发布写前、发布位置是否就是消费者读取的地址；对S2，检查保存值、屏障与返回的顺序，并确认只有取得协议认可的发布才进入S3。读到0仍会正常返回，没有自动登记等待者或请求生产者重试。

这套实现不增加缓冲区复用资格。消费者取得发布后，生产者若立刻写下一轮payload，仍可能覆盖消费者正在使用的数据；应回到[发布取得模块](../../../navigation/P04_发布取得的访问与配置导读.md#4.5_检查调用现场而非只背宏体)及完整单槽交还教材建立反向协议。类型通过、检测未告警、宿主替身轨迹通过都不能代替这份责任。
