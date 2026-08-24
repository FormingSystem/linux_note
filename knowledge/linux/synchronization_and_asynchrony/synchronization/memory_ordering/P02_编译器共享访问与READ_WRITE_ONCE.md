---
id: knowledge.linux.memory_ordering.compiler_access_once
title: "编译器共享访问与 READ WRITE ONCE"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
topics:
  - synchronization
  - memory_ordering
  - compiler
---

# 第2章\_编译器共享访问与READ\_WRITE\_ONCE

## 2.1\_问题发生在机器指令出现之前

CPU 只能执行编译器生成的指令。若编译器已经把两次共享读取合并成一次、把轮询值长期放在寄存器、删除它认为无用的写，后续再讨论 CPU 屏障和缓存传播已经太晚。

```mermaid
flowchart LR
    C[并发 C 源码] -->|as-if 优化| IR[编译器中间表示]
    IR -->|合并／消除／移动访问| ASM[机器指令]
    ASM -->|体系结构内存模型| CPU[CPU 可观察结果]
```

Linux 使用 ONCE、编译器屏障、原子和子系统 API 向编译器表达并发访问意图，不把普通 C 表达式默认当成“汇编加语法糖”。

## 2.2\_轮询为什么可能只读一次

```c
while (dev->state != READY)
    cpu_relax();
```

如果当前执行流中没有编译器可见的写入，优化器可能把 `dev->state` 读到寄存器后反复测试。另一个 CPU 或中断处理程序修改内存，不在普通单线程 as-if 推理中自动构成约束。

```c
while (READ_ONCE(dev->state) != READY)
    cpu_relax();
```

ONCE 告诉编译器每次求值都要执行一次受约束的共享访问。它没有让 CPU 立即从远端 cache 取数，也没有建立“看到 READY 后其他字段有序”的发布协议。

## 2.3\_两次普通读取为什么可能被合并

```c
int plain_sum(void)
{
    return shared + shared;
}
```

若编译器只生成一次 Load 再把结果加倍，单线程语义不变；但并发协议若要求两次读取可能观察不同状态，这种合并会破坏意图。

```c
int once_sum(void)
{
    return READ_ONCE(shared) + READ_ONCE(shared);
}
```

两个 ONCE 表达式要求两个访问实例。配套[编译器访问实验](../../../../../labs/kernel/memory_ordering/P01_READ_ONCE_编译器访问实验/README.md)同时用 GCC 和 Clang 的 `-O0/-O2` 生成汇编，要求读者亲自定位 Load 数量，而不是只记结论。

## 2.4\_宏实现承担什么

Linux 6.12.20 的 [`include/asm-generic/rwonce.h`](../../../../../research/source_reading/linux/include/asm-generic/rwonce.h) 中：

```c
#define __READ_ONCE(x) (*(const volatile __unqual_scalar_typeof(x) *)&(x))

#define READ_ONCE(x) ({
    compiletime_assert_rwonce_type(x);
    __READ_ONCE(x);
})
```

`WRITE_ONCE()` 使用对应的 volatile 类型访问。这里的 volatile cast 是内核实现手段，不等于“把整个共享对象类型声明为 volatile 就完成同步”。宏还组合了类型/大小检查，并与 KASAN/KCSAN 等内核工具约定配合。

源码允许原生机器字以及 `long long` 大小通过检查，但注释明确指出某些 32 位体系结构上的 64 位访问仍可能拆分。Linux 接受该访问大小，不代表所有目标硬件都保证不撕裂；该问题回到[访问粒度与对齐专题](../../../../foundations/computer_architecture/memory_ordering/P02_访问粒度_对齐与撕裂.md)。

## 2.5\_ONCE\_防止哪些典型优化

在具体上下文和编译器规则允许时，ONCE 用于防止或限制：

- 把多次访问合并成一次；
- 把一次源码访问拆成不符合 ONCE 契约的多个编译器访问；
- 从内存重新取值或省略本应存在的取值；
- 把写入认定为不可观察而删除；
- 在轮询中把共享值永久缓存于寄存器；
- 让相邻 ONCE 访问脱离内核原语所表达的顺序关系。

但 ONCE 不禁止所有普通指令调度，也不是通用编译器全栅栏。需要阻止相关普通访问跨越某点时使用 `barrier()` 或携带相应 compiler semantics 的更高层原语。

## 2.6\_为什么不能把整个结构体声明为\_volatile

```c
volatile struct device_state state;
```

这种做法把每个访问都变成过宽的编译器约束，却仍没有说明：

- 哪个字段是发布点；
- 哪些访问要在发布之前；
- 哪个消费者读取与之配对；
- 多写者怎样串行化；
- 多字段如何形成同一快照；
- 对象何时可以释放。

Linux 用窄而明确的 ONCE、acquire/release、锁、原子和 RCU 接口表达这些责任。`volatile` 仍用于 MMIO accessor 内部、特殊低层实现等受控位置，但不作为普通内存并发协议的替代品。

## 2.7\_普通访问\_data\_race\_与\_KCSAN

LKMM 允许内核在经过严格证明的场景使用 plain access，但并发的 plain access 尤其是至少一方为写时，需要同时考虑编译器优化和数据竞争检测。调用方不应因为“内核不是用户态 C11”就任意写数据竞争。

`data_race(expr)` 用于明确标记一个已审查、允许竞态且不需要 KCSAN 报告的表达式；它不是同步原语，不增加原子性或顺序。`READ_ONCE()` / `WRITE_ONCE()` 也能向 KCSAN 和代码审查者表达访问意图，但是否允许该竞态仍由外围协议决定。

正确审查顺序是：

1. 证明竞态在业务上允许；
2. 证明访问宽度/对齐满足需求；
3. 证明不需要与其他地址建立顺序，或已有外部原语；
4. 再选择 ONCE、`data_race()` 或 plain access。

## 2.8\_同一\_CPU\_的中断和信号式并发

ONCE 的一个重要用途并不要求多个 CPU：进程上下文和中断/NMI 可以在同一 CPU 上交替访问状态。即使硬件不存在跨 CPU 传播，编译器仍需要知道值可能在当前控制流之外变化。

这解释了为什么 `CONFIG_SMP=n` 时 ONCE 和编译器屏障仍有意义；SMP 硬件屏障可以退化，不代表编译器约束可以全部删除。

## 2.9\_错误用法逐项拆解

```c
/* 错误 1：两个访问不是一个原子 RMW。 */
WRITE_ONCE(counter, READ_ONCE(counter) + 1);

/* 错误 2：分别完整，不代表同一快照。 */
base = READ_ONCE(range->base);
len  = READ_ONCE(range->len);

/* 错误 3：指针单次取得，不代表对象已发布完成。 */
p = READ_ONCE(global_ptr);
use(p->field);
```

分别需要原子 RMW/锁、多字段版本协议、以及 release/acquire 或 RCU 指针接口。

## 2.10\_源码和实验核对点

版本化证据：

- [`include/asm-generic/rwonce.h`](../../../../../research/source_reading/linux/include/asm-generic/rwonce.h)：宏与访问大小检查；
- [`include/linux/compiler.h`](../../../../../research/source_reading/linux/include/linux/compiler.h)：`barrier()`、`data_race()` 等编译器接口；
- [`include/linux/compiler_types.h`](../../../../../research/source_reading/linux/include/linux/compiler_types.h)：类型属性和编译器基础。

实验必须保存两个维度：普通/ONCE 代码差异，以及 `-O0/-O2`、GCC/Clang 差异。只展示一种编译器的一段汇编，不足以理解 ONCE 是防御编译器变换的契约。

## 2.11\_本章验收

1. 能解释轮询值为什么可能被寄存器化。
2. 能从反汇编识别两次普通读取被合并、ONCE 保留两次访问。
3. 能说明 ONCE 的 volatile cast 是局部实现手段，不是通用 volatile 同步。
4. 能区分 ONCE 接受的宽度与硬件不撕裂保证。
5. 能说明 `data_race()` 为什么不增加同步语义。
6. 能识别 ONCE 无法解决的原子 RMW、快照和发布问题。

上一篇：[READ/WRITE_ONCE 与 SMP 内存顺序原语](P01_READ_WRITE_ONCE_与_SMP_内存顺序原语.md)。

下一篇：[Linux SMP 屏障与顺序域](P03_Linux_SMP屏障与顺序域.md)。
