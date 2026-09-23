---
id: labs.kernel.memory_ordering.read_once_compiler_access
title: "READ ONCE 编译器访问实验"
kind: lab
status: evolving
domains:
  - linux
  - kernel
  - engineering
topics:
  - memory_ordering
  - compiler
---

# 第1章\_READ\_ONCE\_编译器访问实验

## 1.1\_实验目标

本实验只验证编译器层的三个结论：

1. 两次普通读取可能在优化后合并成一次；
2. ONCE 形式要求保留两个访问实例；
3. 普通轮询可能把共享值只读一次，ONCE 轮询会在循环中重新读取。

它不验证 CPU 间可见性、硬件不撕裂或 acquire/release。对应理论见[编译器共享访问与 READ/WRITE_ONCE](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/memory_ordering/P02_编译器共享访问与READ_WRITE_ONCE.md)。

## 1.2\_为什么使用实验版宏

`src/access_once.c` 使用：

```c
#define LAB_READ_ONCE(x) (*(volatile __typeof__(x) *)&(x))
```

它只复现 Linux 6.12 `__READ_ONCE()` 的核心编译器访问形态，使文件能在用户态工具链独立编译。它没有复制 Linux 的类型检查、KASAN/KCSAN 集成和体系结构契约，不能在产品代码中替代真正 `READ_ONCE()`。

## 1.3\_环境

- Bash（Windows使用MSYS2 Bash）；
- GCC 和/或 Clang；
- 支持 GNU `__typeof__` 扩展；
- Windows、Linux 均可运行，结果需记录目标三元组。

本轮结果见[2026-09-24编译器访问与屏障观察](expected/2026-09-24_compiler_access.md)；[2026-08-02历史结果](expected/2026-08-02_windows_x86_64.md)保持原样。两个目标三元组不同，不能用相同宿主操作系统代替目标身份。

## 1.4\_运行步骤

```bash
cd labs/kernel/memory_ordering/P01_READ_ONCE_编译器访问实验
bash run.sh --clean
bash run.sh
```

脚本会为可用的 GCC/Clang 分别生成 `-O0` 和 `-O2` 汇编：

```text
generated/
├── gcc_O0.s
├── gcc_O2.s
├── clang_O0.s
└── clang_O2.s
```

默认跳过缺少的编译器并明确打印，两个都没有则失败；显式指定的编译器缺失也失败。gcc.txt/clang.txt保存完整版本、目标和各次参数。可只运行一个编译器：

```bash
bash run.sh --compiler gcc
bash run.sh --compiler clang
```

## 1.5\_观察方法

依次定位七个函数：

| 函数 | 重点观察 |
| --- | --- |
| `plain_sum()` | `-O2` 是否只读取一次 `shared` 再加倍 |
| `once_sum()` | `-O2` 是否保留两次读取 `shared` |
| `plain_poll()` | 循环中是否还会重新读取 `shared` |
| `once_poll()` | 循环回边是否重新读取 `shared` |
| `barrier_poll()` | 空asm的memory约束是否让普通读取留在循环内 |
| `plain_stores()` | 第一个普通写是否被删除 |
| `once_stores()` | 是否保留两次写入动作 |

不要只数源码行或mov指令；Clang可以把第二次读取放在add的内存操作数中。沿跳转标签检查循环回边，区分“循环内重新读”与“退出后为了返回值再读”。barrier_poll用于说明循环体的编译器约束会改变外提条件：固定ARM cpu_relax本来就包含这种约束，不能把它当纯空循环。完整C与逐步练习在[正文](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/memory_ordering/P02_编译器共享访问与READ_WRITE_ONCE.md#2.3.1_完整编译材料)。

## 1.6\_预期结果

- `plain_sum()` 在常见 `-O2` GCC/Clang 中会合并读取；
- `once_sum()` 保留两个读取；
- `plain_poll()` 可能在循环外读取一次，值为 0 时进入不再访问内存的循环；
- `once_poll()` 在循环内持续读取。

具体寄存器、助记符和标签因编译器/目标而异。若结果不同，应保存版本和完整汇编，而不是把本页预期当成编译器规范。

## 1.7\_失败现象与排查

| 现象 | 排查 |
| --- | --- |
| 找不到编译器 | 使用 `--compiler` 或把 GCC/Clang 加入 PATH |
| 编译器拒绝 `__typeof__` | 确认使用 GCC/Clang GNU 扩展模式 |
| 普通版本没有合并 | 检查是否为 `-O2`、编译器版本和目标选项 |
| ONCE 版本也只有一次读取 | 检查汇编函数边界和源文件是否被改动 |
| 汇编包含额外安全检查 | 记录工具链默认选项，使用脚本给出的完整命令复现 |

## 1.8\_实际结果记录规则

每次正式记录必须包含：编译器完整版本、目标三元组、命令、七个函数的关键汇编和结论边界。实验只证明生成访问形态，不得写成“ONCE 已让其他 CPU 看见最新值”。

## 1.9\_清理

```bash
bash run.sh --clean
```

只删除本实验generated目录下两组已知汇编和工具链记录；目录为空才删除目录，其他手工文件保留。驱动拒绝符号链接输出目录，不递归删除；源码和历史记录保留。
