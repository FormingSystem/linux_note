---
id: labs.foundations.computer_architecture.memory_ordering.arm_access_width_alignment
title: "访问宽度、对齐与 ARM 反汇编实验"
kind: lab
status: evolving
domains:
  - foundations
  - engineering
topics:
  - computer_architecture
  - memory_ordering
  - arm
---

# 第1章\_访问宽度\_对齐与\_ARM\_反汇编实验

## 1.1\_实验目标

设软件把一个 64 位计数值放在紧凑结构体的第 2 个字节开始处。源码仍只有一次赋值，但读者真正需要知道的是：编译器会发出几次访问，分别访问哪些地址？本实验只拆开这个问题，不运行无同步的多线程 C 代码，也不测硬件撕裂概率。

对同一份 C 源码分别生成 host 和 Cortex-A7 目标汇编，观察：

1. 自然对齐 32 位读取映射为怎样的访问；
2. 64 位读取是否使用一条多寄存器指令或多次访问；
3. packed 结构体中的未对齐 64 位字段怎样被拆分；
4. 为什么“源码只有一次读取”不足以证明硬件不撕裂。

理论前置：[访问粒度、对齐与撕裂](../../../../../knowledge/foundations/computer_architecture/memory_ordering/P02_访问粒度_对齐与撕裂.md)。

## 1.2\_环境

- host GCC（可选）；
- `arm-none-eabi-gcc`（推荐）；
- ARM 目标固定为 `-mcpu=cortex-a7 -marm -O2`，对应 i.MX6ULL 的具体观察载体。

先执行 `gcc --version`、`arm-none-eabi-gcc --version` 核对可用工具；只要求生成其中一个目标时，对应编译器可用即可。不需要开发板、交叉链接库、Python或内核源码，因为 `-S` 在生成汇编后停止，不链接、不执行。正文直接调用C编译器，终端只承担启动工具的职责，没有另加一种语言实现教学模型。

本实验的 ARM 输出只是编译器证据。它不代替 ARMv7 架构手册对原子性、对齐和内存类型的正式保证。

## 1.3\_运行

以下先进入实验目录，命令从仓库根目录执行；之后所有命令都在该实验目录中运行：

```sh
cd labs/foundations/computer_architecture/memory_ordering/P01_访问宽度_对齐与ARM反汇编
```

先创建保存汇编的目录。Linux或Bash终端使用：

```bash
mkdir -p generated
```

Windows PowerShell使用下面这一条替代mkdir命令，其余两条编译命令可以原样执行：

```powershell
New-Item -ItemType Directory -Force -Path ./generated | Out-Null
```

两种创建方式都允许目录已经存在。接下来的同名输出会覆盖，所以要保留上次比较结果，应先另存对应汇编文件。先生成host输出：

```sh
gcc -O2 -S -fno-asynchronous-unwind-tables -fno-ident src/access_width.c -o generated/host_gcc_O2.s
```

`-O2`启用优化，`-S`要求生成汇编后停止，两个`-fno-`选项减少非核心的展开表与编译器标识，`-o`给出输出文件。host指编译器自身的默认目标，不能仅凭在某台机器启动就推断其目标ABI；用 `gcc -dumpmachine`核对。再生成本实验的Cortex-A7输出：

```sh
arm-none-eabi-gcc -O2 -mcpu=cortex-a7 -marm -S -fno-asynchronous-unwind-tables -fno-ident src/access_width.c -o generated/cortex_a7_gcc_O2.s
```

`-mcpu=cortex-a7`指定处理器，`-marm`选择ARM指令状态；这里没有链接或下载到开发板。只观察一个目标时，只执行对应编译命令。每条命令都须检查退出状态；失败后不要打开同名旧文件冒充本轮结果。成功后分别打开 `generated/host_gcc_O2.s`和 `generated/cortex_a7_gcc_O2.s`中本轮实际生成的文件，并保存编译器版本及上述参数。

## 1.4\_观察函数

以下是本实验的完整 [源文件](src/access_width.c)。两个对齐指针参数要求调用者提供满足类型对齐的地址；packed 结构体则显式允许 `value` 从偏移 1 开始。`volatile` 用于观察编译器保留的访问，不提供线程同步，也不承诺访问不可拆分。

```c
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

struct packed_record {
    u8 tag;
    u64 value;
} __attribute__((packed));

/* 自然对齐的 32 位读取。 */
u32 load_aligned_u32(const volatile u32 *value)
{
    return *value;
}

/* 自然对齐的 64 位读取。 */
u64 load_aligned_u64(const volatile u64 *value)
{
    return *value;
}

/* value 位于偏移 1，编译器不能假定自然对齐。 */
u64 load_packed_u64(const volatile struct packed_record *record)
{
    return record->value;
}

/* 未对齐 64 位写入也可能被拆分。 */
void store_packed_u64(volatile struct packed_record *record, u64 value)
{
    record->value = value;
}
```

| 函数 | 预期关注点 |
| --- | --- |
| `load_aligned_u32()` | 自然对齐 32 位 Load |
| `load_aligned_u64()` | Cortex-A7 目标的 64 位取数指令序列 |
| `load_packed_u64()` | 偏移 1 的字段是否拆成多个小访问 |
| `store_packed_u64()` | 未对齐写入的拆分方向和次数 |

同时查看汇编中的地址偏移，确认访问的是 `record.value` 而不是函数序言或栈操作。

## 1.5\_预期结论

- 对齐 u32 通常可由单个字访问表达；
- u64 在 32 位 ARM 上需要两个寄存器承载，即使使用一条 `ldrd/strd` 语法，也必须继续核对体系结构原子性契约；
- packed u64 可能拆成多条字节/半字/字访问；
- 反汇编能证明编译器生成什么，不能证明并发 reader 在体系结构层绝不会观察中间状态。

2026-10-16 重新运行两个目标：host GCC 14.2.0 生成 packed 字段的一条 `movq`；GNU Arm Embedded GCC 10.3.1 为对齐 u32 生成 `ldr`，为对齐 u64 生成 `ldrd`，为 packed u64 生成偏移 `1`、`5` 的两条 `ldr` 或 `str`。这与 [历史观察中的裁剪汇编](expected/2026-08-02_compiler_observation.md)一致。正文中的结论以这两个指定编译配置为界；未来输出不同，应保留实际结果，不必强行改成记录中的样子。

2026-10-17改为正文直接调用后，在PowerShell重新执行两条命令；目标分别为 `x86_64-w64-mingw32`和 `arm-none-eabi`，编译器仍为上述版本。两份汇编与改写前保存结果的SHA-256分别一致，说明移除运行器没有改变本次编译输入与输出。未执行ARM指令或硬件并发测试。

先预测再解释：若给两条 32 位写之间插入屏障，能否把 packed 赋值变成一次 64 位原子写？不能；屏障约束先后，不能把两条独立访问合并成一个原子事务。若 host 只有一条 `movq`，能否宣布它在任意地址都不会撕裂？也不能；还要核对目标架构的宽度、地址边界和内存属性保证。

## 1.6\_失败现象与排查

| 现象 | 排查 |
| --- | --- |
| 找不到 ARM 编译器 | 安装 GNU Arm Embedded Toolchain 并配置PATH，或只执行host编译命令 |
| packed 访问仍像单条指令 | 核对目标选项、汇编地址偏移和编译器版本 |
| 编译失败 | 保存完整命令，确认编译器支持 Cortex-A7 |
| 输出与记录不同 | 这是允许的研究结果，记录版本后按架构手册重新解释 |

## 1.7\_清理

清理不是运行前置步骤。如需保留编译结果作证据，先另存；只删除本实验约定的两个输出文件，不递归清空目录。Linux或Bash终端使用：

```bash
rm -f -- generated/host_gcc_O2.s generated/cortex_a7_gcc_O2.s
```

PowerShell使用：

```powershell
Remove-Item -LiteralPath ./generated/host_gcc_O2.s, ./generated/cortex_a7_gcc_O2.s
```

PowerShell在某份输出不存在时会报告该路径缺失；只生成一个目标时，可只列它的路径。以上命令保留generated目录及其中其他手工保存的文件，避免把实验目录中的额外证据一起删除。
