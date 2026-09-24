---
id: research.source_reading.memory_ordering.arm_barrier_impl
title: "ARM barrier.h 屏障映射源码实现"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
---

# 第1章\_ARM屏障映射源码实现

## 1.1\_ARMv7指令封装与内部SMP原语

上游位置arch/arm/include/asm/barrier.h，固定NXP linux-imx提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20。[原文件](../../../../../../linux/arch/arm/include/asm/barrier.h)保留完整分支，本页仅展开ARMv7的DMB/DSB及基础、DMA、内部SMP映射；不声称覆盖旧架构指令、推测执行防护或事件等待接口。回到[模块A0～A3](../../../../../navigation/P07_ARM屏障与配置边界导读.md#7.3_沿一条调用走完配置与运行路径)或[总索引](../../../../../navigation/P01_Linux_6.12_LKMM_源码与模型导读.md#1.3.3_ARMv7_映射)获得阅读上下文。

```c
/**
 * @brief 仓库补充阅读说明：下列两定义裁自ARMv7分支。
 * @param option 被字符串化的指令选项；空实参保留汇编缺省形式。
 * @details memory约束面向编译器，指令本身承担架构约束。
 */
#define dsb(option) __asm__ __volatile__ ("dsb " #option : : : "memory")
#define dmb(option) __asm__ __volatile__ ("dmb " #option : : : "memory")

/* 内部接口在本头直接给出；公共smp接口稍后再由通用头选配置。 */
#define __smp_mb() dmb(ish)
#define __smp_rmb() __smp_mb()
#define __smp_wmb() dmb(ishst)
```

这组宏的实现原理是把ish等标记字符串化为指令文本，不会在运行时查表选择范围。__smp_rmb复用全方向内部屏障，是本版本ARM实现强于读方向最小契约的例子，不是其他架构也必须如此的理由。调用公共smp接口时，还要经过[通用配置包装](../../../../include/asm-generic/barrier.h.md#1.3_三种公共SMP屏障的配置分支)：UP不因这些内部定义存在就自动执行DMB。

## 1.2\_基础与DMA接口的配置选择

沿模块已建立的三条构建开关阅读：CONFIG_SMP表示多处理器支持，CONFIG_ARM_DMA_MEM_BUFFERABLE参与DMA可缓冲访问的屏障选择，CONFIG_ARM_HEAVY_MB控制DSB后是否进入平台补充函数。这些条件在编译时决定分支，不会每次执行屏障再读取一份配置对象。

```c
/**
 * @brief 仓库补充阅读说明：heavy开关控制DSB后的平台补充调用。
 * @details 本宏不注册soc_mb，不等待设备完成，也不访问业务描述符。
 */
#ifdef CONFIG_ARM_HEAVY_MB
extern void (*soc_mb)(void);
extern void arm_heavy_mb(void);
#define __arm_heavy_mb(x...) do { dsb(x); arm_heavy_mb(); } while (0)
#else
#define __arm_heavy_mb(x...) dsb(x)
#endif

/* A1：DMA可缓冲或SMP任意一项开启，选择下列硬件路径。 */
#if defined(CONFIG_ARM_DMA_MEM_BUFFERABLE) || defined(CONFIG_SMP)
#define mb() __arm_heavy_mb()
#define rmb() dsb()
#define wmb() __arm_heavy_mb(st)
#define dma_rmb() dmb(osh)
#define dma_wmb() dmb(oshst)
#else
#define mb() barrier()
#define rmb() barrier()
#define wmb() barrier()
#define dma_rmb() barrier()
#define dma_wmb() barrier()
#endif
```

先看外层条件，再看heavy开关：外层两项均关闭时，基础接口直接选择barrier，heavy即使开启也不会从这些调用点被调用。硬件路径下，mb的空选项DSB在本次ARMv7汇编输出中显示为dsb sy；wmb传st，rmb也使用空选项，却没有走heavy包装。DMA读写接口用osh/oshst，不调用heavy函数。

A2的DSB与A3的函数调用在宏中有明确先后。编译器可能把最后一次函数调用优化成尾跳转，因此核对汇编应关注“DSB后进入arm_heavy_mb”，不能只搜bl并把合法的b误报成缺失。函数内部条件与作用见[唯一回调实现](../../mm/flush.c.md#1.1_DSB之后还有什么)。

原头最后包含asm-generic/barrier.h，使公共SMP配置包装接到上面的内部原语。本页节选没有重复该通用实现。交叉编译检查使用固定完整通用头和受控编译器/KCSAN依赖；没有链接内核，也没有运行ARM目标。

## 1.3\_从展开结果回到接口契约

同一条wmb在不同配置下可以是空编译器屏障、DSB，或DSB加平台补充调用。调用者应表达所需接口契约，而不是把某次汇编抄成通用内联指令。抄写会绕开配置和平台补充层，也容易把普通CPU内存协议变成不必要的更广约束。

本页不凭指令名字证明MMIO写到达某设备，也不据DMB/DSB推断streaming DMA的缓存维护完成。若问题是设备是否还持有缓冲区，应继续读设备协议及DMA API，不能在这里把更强屏障当成释放许可。
