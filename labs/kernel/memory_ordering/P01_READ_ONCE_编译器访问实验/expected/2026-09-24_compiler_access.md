---
id: labs.kernel.memory_ordering.compiler_access.result_2026_09_24
title: "2026-09-24 编译器访问与屏障观察"
kind: lab
status: evolving
domains: [linux, kernel, engineering]
topics: [memory_ordering, compiler]
---

# 第1章\_2026-09-24\_编译器访问与屏障观察

## 1.1\_环境和生成方式

本次使用[当前C材料](../src/access_once.c)和[Bash驱动](../run.sh)，只生成汇编，不链接或运行轮询函数。[2026-08-02记录](2026-08-02_windows_x86_64.md)原样保留，不能把新增加的屏障与写入案例归入旧结果。

| 工具 | 实际版本与目标 |
| --- | --- |
| GCC | gcc.exe (x86_64-posix-seh-rev0, Built by MinGW-Builds project) 14.2.0；x86_64-w64-mingw32 |
| Clang | clang version 18.1.8；x86_64-pc-windows-msvc |
| 启动环境 | Windows中的MSYS2 Bash |

两种编译器分别执行下列命令中的O0与O2版本，路径相对实验根目录；实际版本、目标和参数由驱动写入忽略目录generated中的gcc.txt和clang.txt。记录不包含机器绝对路径。

```bash
gcc -std=gnu11 -Wall -Wextra -Werror -O0 -S \
  -fno-asynchronous-unwind-tables -fno-ident src/access_once.c -o generated/gcc_O0.s
gcc -std=gnu11 -Wall -Wextra -Werror -O2 -S \
  -fno-asynchronous-unwind-tables -fno-ident src/access_once.c -o generated/gcc_O2.s
clang -std=gnu11 -Wall -Wextra -Werror -O0 -S \
  -fno-asynchronous-unwind-tables -fno-ident src/access_once.c -o generated/clang_O0.s
clang -std=gnu11 -Wall -Wextra -Werror -O2 -S \
  -fno-asynchronous-unwind-tables -fno-ident src/access_once.c -o generated/clang_O2.s
```

## 1.2\_读取和循环的观察

| 函数 | GCC O0 | Clang O0 | 两者O2 |
| --- | --- | --- | --- |
| plain_sum | 一次内存读，再寄存器加倍 | 两次内存读，其中一次是add内存操作数 | 一次内存读后加倍 |
| once_sum | 两次内存读 | 两次内存读 | 两次内存读；Clang第二次嵌在add中 |
| plain_poll | 回边重新读取 | 回边重新读取 | 循环前读一次，零值进入空转 |
| once_poll | 回边重新读取 | 回边重新读取 | 回边重新读取，退出后另读返回值 |
| barrier_poll | 回边重新读取 | 回边重新读取 | 回边重新读取 |

O0不意味着逐字照抄源码：GCC在O0已经合并plain_sum的两次读取，Clang此处没有合并。这也是不能把关闭优化当作并发协议的原因。

以GCC O2为例，普通循环的零值分支落到只跳回自身的标签；局部volatile轮询则返回读取位置：

```asm
plain_poll:
    movl shared(%rip), %eax
    testl %eax, %eax
    je .L6
    ret
.L6:
    jmp .L6

# once_poll的循环部分
.L8:
    movl (%rdx), %eax
    testl %eax, %eax
    je .L8
```

这里rdx在进入循环前由`leaq shared(%rip), %rdx`设置。Clang O2在once_poll中使用`cmpl $0, shared(%rip)`，比较指令本身就读取内存。

GCC的barrier_poll回边落回读取处：

```asm
.L13:
.L16:
    movl shared(%rip), %eax
    testl %eax, %eax
    je .L13
```

没有额外硬件屏障指令，仍不妨碍空asm的memory约束影响编译器。Clang在循环体保留`#APP/#NO_APP`标记并跳回读取标签，表达相同的编译器层效果。

## 1.3\_写入动作与结论边界

两种编译器O2的关键写入相同：

```asm
plain_stores:
    movl $2, shared(%rip)
    # 返回指令省略
once_stores:
    movl $1, shared(%rip)
    movl $2, shared(%rip)
    # 返回指令省略
```

普通第一个写被后一个覆盖；局部volatile形态保留两个写动作。它不保证远端观察者读到过1，不证明跨地址发布，也不构成通知队列。

本次没有执行Linux内核宏、ARM指令、中断、KCSAN或硬件弱序测试。固定ARM cpu_relax的屏障分支来自源码核对，不能把这里的x86-64汇编称为ARM运行证据。返回[实验入口](../README.md#1.5_观察方法)继续练习。
