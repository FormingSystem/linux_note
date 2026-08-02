---
id: labs.foundations.computer_architecture.memory_ordering.arm_access_width.result_2026_08_02
title: "2026-08-02 访问宽度与 ARM 编译器观察"
kind: lab
status: maintained
domains:
  - foundations
  - engineering
topics:
  - computer_architecture
  - memory_ordering
  - arm
---

# 第1章\_2026-08-02\_访问宽度编译器观察

## 1.1\_环境

| 目标 | 编译器 | 选项 |
| --- | --- | --- |
| Windows x86-64 | GCC 14.2.0 MinGW | `-O2 -S` |
| ARMv7-A Cortex-A7 | GNU Arm Embedded 10.3.1 | `-O2 -mcpu=cortex-a7 -marm -S` |

## 1.2\_Cortex\_A7\_目标

自然对齐 u32 使用一条字读取：

```asm
load_aligned_u32:
    ldr r0, [r0]
    bx lr
```

自然对齐 u64 生成 `ldrd`：

```asm
load_aligned_u64:
    ldrd r0, [r0]
    bx lr
```

packed u64 位于偏移 1，编译器拆成两次带未对齐标记的 32 位读取：

```asm
load_packed_u64:
    mov r3, r0
    ldr r0, [r0, #1]    @ unaligned
    ldr r1, [r3, #5]    @ unaligned
    bx lr
```

写入也拆成两个 32 位 Store：

```asm
store_packed_u64:
    str r2, [r0, #1]    @ unaligned
    str r3, [r0, #5]    @ unaligned
    bx lr
```

这直接证明 packed u64 的一次 C 赋值在该构建中不是一个单独的编译器访存事件。

## 1.3\_host\_x86\_64\_目标

同一 packed 访问由一条未对齐 64 位 `movq` 表达：

```asm
load_packed_u64:
    movq 1(%rcx), %rax

store_packed_u64:
    movq %rdx, 1(%rcx)
```

这说明编译器映射随目标架构变化；不能从 x86-64 输出推断 Cortex-A7，也不能从 C 类型宽度推断指令数量。

## 1.4\_结论边界

反汇编只证明编译器生成了 `ldr/ldrd/str/movq` 等指令。它没有单独证明：

- `ldrd` 在所有地址和并发场景中具备 64 位单复制原子性；
- 一条 x86-64 未对齐 `movq` 在跨缓存行/页边界时拥有何种原子契约；
- 普通内存结论适用于设备内存；
- 指令之间具备跨 CPU 顺序。

这些结论必须继续核对相应体系结构手册、内存类型和 Linux API 契约。
