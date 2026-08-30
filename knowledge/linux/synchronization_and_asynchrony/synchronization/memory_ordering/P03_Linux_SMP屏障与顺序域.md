---
id: knowledge.linux.memory_ordering.smp_barriers_domains
title: "Linux SMP 屏障与顺序域"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
topics:
  - synchronization
  - memory_ordering
  - barrier
---

# 第3章\_Linux\_SMP屏障与顺序域

本章中的 SMP 沿用 [Symmetric Multiprocessing（对称多处理）的系统模型](../../../../foundations/computer_architecture/cache_coherence/P01_缓存一致性问题与缓存行.md#1.1.1_SMP的中英文全称与系统模型)：多个逻辑 CPU 由同一个 Linux 内核管理并通过共享内存协作。本章只解释 `smp_*()` 怎样为 CPU—CPU 普通内存协议建立顺序，以及 `CONFIG_SMP=n` 时哪些约束可以退化。

## 3.1\_先按同步域分类

Linux 中都带“barrier”意味的原语，可能约束不同参与者：

| 同步域 | 参与者 | 典型接口 |
| --- | --- | --- |
| 编译器 | 当前编译单元中的优化器 | `barrier()`、ONCE 的 compiler semantics |
| SMP 普通内存 | 多 CPU 访问可缓存普通内存 | `smp_rmb()`、`smp_wmb()`、`smp_mb()` |
| 架构硬件域 | CPU 与架构定义的更广观察者 | `rmb()`、`wmb()`、`mb()` |
| DMA 共享内存 | CPU 与 DMA 设备共享描述符/数据 | `dma_rmb()`、`dma_wmb()` 加 DMA API |
| MMIO | CPU 与设备寄存器/posted write | `readl()`、`writel()`、relaxed 变体及设备协议 |

同一体系结构可能让几个接口映射到相同指令，但调用方仍应按语义域选择。这样配置和架构变化时，代码表达的对象不会改变。

## 3.2\_barrier\_只约束编译器

Linux 6.12.20 的 [`include/linux/compiler.h`](../../../../../research/source_reading/linux/include/linux/compiler.h) 定义：

```c
#define barrier() __asm__ __volatile__("": : :"memory")
```

空内联汇编通常不生成硬件屏障指令，`memory` clobber 告诉编译器相关内存值可能变化，阻止访问越过该点。它适合保护编译器层顺序，例如某些低层状态转换；单独用于两个 CPU 的消息传递仍会留下 Store Buffer 和架构弱序问题。

```c
WRITE_ONCE(data, 42);
barrier();
WRITE_ONCE(flag, 1); /* 不能据此声称 CPU1 看到 flag 后一定看到 data。 */
```

## 3.3\_SMP\_屏障给普通内存增加方向

| 原语 | 最小关注方向 | 典型用途 | 不保证 |
| --- | --- | --- | --- |
| `smp_rmb()` | 屏障前相关读 → 屏障后相关读 | 两阶段读取 | 写者互斥、MMIO 完成 |
| `smp_wmb()` | 屏障前相关写 → 屏障后相关写 | 描述符/标志顺序 | Store→Load 顺序 |
| `smp_mb()` | 屏障前相关读写 → 屏障后相关读写 | SB、复杂状态机 | 自动形成条件和生命周期 |

“最小关注方向”用于理解选择，不应自行假定某款架构实现恰好更强的效果可以成为通用 Linux 契约。

体系结构层为什么需要读、写和全屏障，见[屏障、Acquire/Release 与依赖顺序](../../../../foundations/computer_architecture/memory_ordering/P05_屏障_Acquire_Release与依赖顺序.md)。

## 3.4\_屏障必须成对进入一条协议

MP 使用显式屏障可以写成：

```c
/* CPU0：生产者。 */
WRITE_ONCE(data, 42);
smp_wmb();
WRITE_ONCE(flag, 1);

/* CPU1：消费者。 */
if (READ_ONCE(flag)) {
    smp_rmb();
    use(READ_ONCE(data));
}
```

发布端写屏障排列两次写；取得端读屏障排列两次读；消费者还必须读取到发布标志，才能把两边连接起来。只在生产者加 `smp_wmb()`，不能阻止消费者提前读取载荷；只在消费者加 `smp_rmb()`，不能阻止生产者先传播 flag。

这一模式通常更适合用 `smp_store_release()` / `smp_load_acquire()` 表达，下一章会解释为什么它把顺序直接绑定到发布位置。

## 3.5\_为什么\_SB\_需要全屏障而不是写屏障

```c
/* CPU0 */                         /* CPU1 */
WRITE_ONCE(x, 1);                  WRITE_ONCE(y, 1);
smp_mb();                          smp_mb();
r0 = READ_ONCE(y);                 r1 = READ_ONCE(x);
```

要禁止 `r0 == 0 && r1 == 0`，每边都要限制 Store→Load。`smp_wmb()` 只提供写→写方向，后面却是 Load；`smp_rmb()` 只提供读→读方向，前面却是 Store。接口选择来自事件方向，不来自“越轻越好”的性能直觉。

配套 Litmus 实验把 `SB+poonceonces` 的 `Sometimes` 与 `SB+fencembonceonces` 的 `Never` 并列运行。

## 3.6\_CONFIG\_SMP\_n\_为什么允许退化

[`include/asm-generic/barrier.h`](../../../../../research/source_reading/linux/include/asm-generic/barrier.h) 在非 SMP 构建下允许部分 `smp_*()` 退化为 `barrier()`。原因是同一内核实例不存在另一个 CPU 与本 CPU 形成 SMP 普通内存观察关系，但编译器仍可能重排当前 CPU 与中断等上下文共享的状态。

这不是说 UP 内核中“所有内存顺序都不存在”：

- 设备和 DMA 仍是外部观察者；
- 中断/NMI 可与当前上下文交错；
- 编译器访问约束仍然必要；
- 某些架构硬件序原语面向的域不只 SMP CPU。

所以应使用 `smp_*()` 表达 CPU—CPU 普通内存协议，让配置层做合法退化，而不是在业务代码里自行用 `#ifdef CONFIG_SMP` 删除同步。

## 3.7\_ARMv7\_在本仓库基线中的映射

Linux 6.12.20 的 `arch/arm/include/asm/barrier.h` 对 ARMv7 SMP 定义：

```c
#define __smp_mb()  dmb(ish)
#define __smp_rmb() __smp_mb()
#define __smp_wmb() dmb(ishst)
```

这里 `ish` 表示 inner-shareable 域，`ishst` 只针对 Store 方向。该映射说明在这份 ARM 基线中，`smp_rmb()` 实现得与 `smp_mb()` 一样强；调用方仍应写 `smp_rmb()` 表达只需要读顺序，不能把 ARMv7 当前实现强度外推成所有架构的 Linux 契约。

同一文件还分别定义 `mb/rmb/wmb` 和 `dma_rmb/dma_wmb`，证明“在 ARM 上都是 DMB/DSB”这种压缩说法会丢失 shareability、访问方向、配置和 SoC heavy barrier 等边界。

## 3.8\_mb\_rmb\_wmb\_不是更保险的默认选择

非 `smp_` 原语由架构定义更广硬件域，驱动中的 MMIO 或 DMA 场景可能需要它们或对应 accessor。但对纯 CPU—CPU 可缓存普通内存协议，使用 `smp_*()` 更准确，也允许 UP 构建合法优化。

即便使用 `mb()`：

- posted MMIO write 是否到达设备仍取决于 accessor 和设备协议；
- streaming DMA 缓冲区的所有权转换仍需要 DMA API；
- 等待者不会因为屏障自动被唤醒；
- 对象不会因为屏障自动延长生命期。

P10 会把这些子系统边界放在同一检查表中。

## 3.9\_屏障和原子操作怎样组合

原子 RMW 有 relaxed/acquire/release/fully ordered 变体；还存在 `smp_mb__before_atomic()`、`smp_mb__after_atomic()` 等只在特定原子操作周围补顺序的接口。它们不是给任意代码随手加的半屏障，而是用于外围协议已经明确原子事件位置、只缺某一侧顺序的场景。

具体保证取决于原子 API 是否返回值、条件操作是否成功和后缀，详见 [P06 原子 RMW](P06_原子RMW_顺序后缀与条件成功.md)。

## 3.10\_性能成本怎样分析

屏障开销不能只测空循环中的单条指令。完整成本可能来自：

- 屏障前 Store Buffer 中是否有未完成写；
- 前后是否有缓存 Miss 或所有权竞争；
- 屏障域覆盖多大；
- 后续 Load 是否失去推测/并行机会；
- 当前架构把该 Linux 原语映射得比最小契约更强；
- 更高层算法是否本可用锁、批处理或 per-CPU 状态减少屏障次数。

研究报告应同时给出正确性事件图和负载状态，不能把一次微基准纳秒数当成所有调用点常数。

## 3.11\_本章验收

1. 能按编译器、SMP 普通内存、架构、DMA 和 MMIO 区分顺序域。
2. 能解释 `barrier()` 为什么不能单独完成跨 CPU 发布。
3. 能为 MP 配对写/读屏障，并说明缺一侧会怎样。
4. 能解释 SB 为什么要求 Store→Load 方向。
5. 能说明 `CONFIG_SMP=n` 退化删除了什么、保留了什么。
6. 能读出 ARMv7 `ish/ishst` 映射，同时不外推当前实现强度。

上一篇：[编译器共享访问与 READ/WRITE_ONCE](P02_编译器共享访问与READ_WRITE_ONCE.md)。

下一篇：[release/acquire 发布协议](P04_release_acquire_发布协议.md)。
