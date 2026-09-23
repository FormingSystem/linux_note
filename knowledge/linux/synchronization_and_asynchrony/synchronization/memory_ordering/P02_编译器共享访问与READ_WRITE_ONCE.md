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

上一章的[消息发布](P01_READ_WRITE_ONCE_与_SMP_内存顺序原语.md#1.2.1_从准备到使用的四个阶段)把载荷和就绪标志接成一条有序链。本章只追问更早的一层：编译器是否生成了协议所需的那些访问。

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
    ; /* 本反例刻意没有屏障、外部调用或其他限制优化的动作。 */
```

如果当前执行流中没有编译器可见的写入，优化器可能把 `dev->state` 读到寄存器后反复测试。另一个 CPU 或中断处理程序修改内存，不在普通单线程 as-if 推理中自动构成约束。

这里的READY只是应用定义的就绪值，不能直接把这个片段当用户空间线程同步程序运行。特别要检查循环体：固定ARM版本中cpu_relax的通常分支本身就是barrier，另有架构/勘误分支使用更强动作。因此不能拿“已经包含cpu_relax”的循环证明读取必然会外提；屏障或未知函数调用都可能改变优化前提。

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

两个ONCE表达式要求两个访问实例，但`+`两边不是按源码左到右建立的跨CPU顺序协议。我们现在只检查生成了几次内存读取，不据此宣称读到了同一版值或建立了发布关系。

### 2.3.1\_完整编译材料

下面的文件没有main，实验只生成汇编，不启动会无限轮询的程序。__typeof__(x)取得x的类型，&(x)取得它的地址，转换只在这次解引用上添加volatile访问约束，不把整个对象类型改掉。局部volatile转换只模拟ONCE的编译器访问形态；LAB_BARRIER则声明普通内存可能受影响，使优化器必须在它周围重新考虑读取。两者不能代替真实内核宏的类型检查、架构条件和检查器接入。

```c
#if defined(__GNUC__) || defined(__clang__)
#define LAB_READ_ONCE(x) (*(volatile __typeof__(x) *)&(x))
#define LAB_WRITE_ONCE(x, value) (*(volatile __typeof__(x) *)&(x) = (value))
#define LAB_BARRIER() __asm__ __volatile__("" : : : "memory")
#else
#error "本实验需要 GCC 或 Clang 的 __typeof__ 扩展"
#endif

int shared;

/* 普通表达式允许编译器合并两次读取。 */
int plain_sum(void)
{
    return shared + shared;
}

/* 两个 ONCE 表达式要求保留两个访问实例。 */
int once_sum(void)
{
    return LAB_READ_ONCE(shared) + LAB_READ_ONCE(shared);
}

/* 普通轮询可能只在进入循环前读取一次。 */
int plain_poll(void)
{
    while (shared == 0)
        ;

    return shared;
}

/* ONCE 轮询要求循环中重新读取共享值。 */
int once_poll(void)
{
    while (LAB_READ_ONCE(shared) == 0)
        ;

    return LAB_READ_ONCE(shared);
}

/* 编译器屏障也会改变普通轮询的优化条件，不等于CPU屏障。 */
int barrier_poll(void)
{
    while (shared == 0)
        LAB_BARRIER();
    return shared;
}

/* 前一个普通写可能被后一个覆盖，外部只留下最终值。 */
void plain_stores(void)
{
    shared = 1;
    shared = 2;
}

/* 两次局部volatile访问保留写入动作，不意味着读者必定观察到1。 */
void once_stores(void)
{
    LAB_WRITE_ONCE(shared, 1);
    LAB_WRITE_ONCE(shared, 2);
}
```

### 2.3.2\_生成并阅读汇编

在仓库根目录先进入[配套实验](../../../../../labs/kernel/memory_ordering/P01_READ_ONCE_编译器访问实验/README.md)目录，再运行Bash驱动。需要GCC和Clang以及支持GNU扩展的C模式；Windows使用MSYS2 Bash，把两种编译器加入该终端PATH。只装了一种时可以先观察该工具链，不能把另一个跳过项记作通过。

```bash
cd labs/kernel/memory_ordering/P01_READ_ONCE_编译器访问实验
bash run.sh
```

脚本把两种优化级别的汇编和版本、目标三元组、完整命令写入本目录generated。它不运行目标程序，Bash只组织编译器调用；教学材料是上面的完整C文件。先定位plain_sum和once_sum的函数边界：在本批x86-64的GCC与Clang O2结果中，前者一次读后加倍，后者保留两次读；Clang的第二次读可以嵌在add的内存操作数里，所以不能只数mov指令。

再沿跳转标签找到循环回边：plain_poll在循环前读一次，零值分支空转；once_poll回边包含重新读取。barrier_poll也保留循环内的读取，因为本实验显式告诉编译器屏障会影响内存判断。这个第三组正是对上面cpu_relax反例的校正，不是说普通读从此获得了Linux ONCE完整契约。

最后比较plain_stores和once_stores：本批O2前者只留下写2，后者有写1和写2。生成两次写仍不保证另一CPU能够捕捉到中间值1，更不是消息队列。O0用于对照源代码形状，O2用于观察优化后的合法变换；若换版本后结果不同，记录完整输出和优化条件，不能修改汇编来迎合预期。

## 2.4\_宏实现承担什么

Linux 6.12.20的版本入口从[源码与模型导读](../../../../../research/source_reading/memory_ordering/P01_Linux_6.12_LKMM_源码与模型导读.md#1.3.1_READ_ONCE_WRITE_ONCE)进入。保存的include/asm-generic/rwonce.h中，公开READ_ONCE先做compiletime_assert_rwonce_type检查，再由__READ_ONCE把目标地址转换为适当的const volatile标量指针并读取。这里描述其职责顺序，不把省略续行符的多行宏伪装成可编译上游代码。

`WRITE_ONCE()` 使用对应的 volatile 类型访问。这里的 volatile cast 是内核实现手段，不等于“把整个共享对象类型声明为 volatile 就完成同步”。宏还组合了类型/大小检查，并与 KASAN/KCSAN 等内核工具约定配合。

源码允许原生机器字以及 `long long` 大小通过检查，但注释明确指出某些 32 位体系结构上的 64 位访问仍可能拆分。Linux 接受该访问大小，不代表所有目标硬件都保证不撕裂；该问题回到[访问粒度与对齐专题](../../../../foundations/computer_architecture/memory_ordering/P02_访问粒度_对齐与撕裂.md)。

## 2.5\_ONCE\_防止哪些典型优化

在具体上下文和编译器规则允许时，ONCE 用于防止或限制：

- 把多次访问合并成一次；
- 把一次源码访问拆成不符合 ONCE 契约的多个编译器访问；
- 从内存重新取值或省略本应存在的取值；
- 把写入认定为不可观察而删除；
- 在轮询中把共享值永久缓存于寄存器；
- 破坏内核原语及表达式求值规则已经要求的访问关系；不能只凭两个ONCE的文本相邻推导完整编译器或CPU屏障。

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

先在七个函数旁标出“预期读取次数、写入次数、回边是否再次触及shared”，再和两种O2输出比对。把barrier_poll的屏障去掉后重新生成，哪一个函数可以作为参照？把once_sum改为先读取一次到局部变量，再返回局部变量加自身，为什么只有一次共享读取仍符合你的新源码？

第一题应该回到plain_poll的控制流；第二题把契约改成了一次读取和两次使用局部值，编译器不欠你第二次共享读取。最后给once_stores增加一个读者，要求它一定消费到1和2：仅有两个WRITE_ONCE不能满足，必须另建确认或队列协议。不要把“生成动作存在”和“远端必然观察每个中间状态”混为一谈。

1. 能解释轮询值为什么可能被寄存器化。
2. 能从反汇编识别两次普通读取被合并、ONCE 保留两次访问。
3. 能说明 ONCE 的 volatile cast 是局部实现手段，不是通用 volatile 同步。
4. 能区分 ONCE 接受的宽度与硬件不撕裂保证。
5. 能说明 `data_race()` 为什么不增加同步语义。
6. 能识别 ONCE 无法解决的原子 RMW、快照和发布问题。

上一篇：[READ/WRITE_ONCE 与 SMP 内存顺序原语](P01_READ_WRITE_ONCE_与_SMP_内存顺序原语.md)。

下一篇：[Linux SMP 屏障与顺序域](P03_Linux_SMP屏障与顺序域.md)。
