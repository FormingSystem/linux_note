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

对同一份 C 源码分别生成 host 和 Cortex-A7 目标汇编，观察：

1. 自然对齐 32 位读取映射为怎样的访问；
2. 64 位读取是否使用一条多寄存器指令或多次访问；
3. packed 结构体中的未对齐 64 位字段怎样被拆分；
4. 为什么“源码只有一次读取”不足以证明硬件不撕裂。

理论前置：[访问粒度、对齐与撕裂](../../../../../knowledge/foundations/computer_architecture/memory_ordering/P02_访问粒度_对齐与撕裂.md)。

## 1.2\_环境

- Python 3.10 或更新版本；
- host GCC（可选）；
- `arm-none-eabi-gcc`（推荐）；
- ARM 目标固定为 `-mcpu=cortex-a7 -marm -O2`，对应 i.MX6ULL 的具体观察载体。

本实验的 ARM 输出只是编译器证据。它不代替 ARMv7 架构手册对原子性、对齐和内存类型的正式保证。

## 1.3\_运行

```bash
cd labs/foundations/computer_architecture/memory_ordering/P01_访问宽度_对齐与ARM反汇编
python run.py --clean
python run.py
```

只生成 ARM 输出：

```bash
python run.py --target arm
```

## 1.4\_观察函数

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

实际工具链记录在 `expected/2026-08-02_compiler_observation.md`。

## 1.6\_失败现象与排查

| 现象 | 排查 |
| --- | --- |
| 找不到 ARM 编译器 | 安装 GNU Arm Embedded Toolchain 或只运行 `--target host` |
| packed 访问仍像单条指令 | 核对目标选项、汇编地址偏移和编译器版本 |
| 编译失败 | 保存完整命令，确认编译器支持 Cortex-A7 |
| 输出与记录不同 | 这是允许的研究结果，记录版本后按架构手册重新解释 |

## 1.7\_清理

```bash
python run.py --clean
```

只删除本实验的 `generated/`。
