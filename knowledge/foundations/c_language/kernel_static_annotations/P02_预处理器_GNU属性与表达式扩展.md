---
id: knowledge.foundations.c_language.kernel_static_annotations.p02
title: "预处理器、GNU 属性与表达式扩展"
kind: concept
status: evolving
domains:
  - foundations
  - c_language
  - linux
---

# 第2章\_预处理器\_GNU\_属性与表达式扩展

## 2.1\_从处理链进入语法层

上一章已经区分了源码契约、静态分析状态、编译元数据和运行时状态。现在的问题变成：为什么 Sparse 可以在一份仍然像 C 的代码中表达额外类型和路径约束？

答案不是 Sparse 搜索固定宏名，而是 Linux 先利用预处理器把友好宏名改写成一组解析器能够理解的属性、类型和表达式。要读懂这类代码，需要掌握五种语法角色。

## 2.2\_预处理条件与宏替换只处理记号

```c
#ifdef __CHECKER__
# define __acquire(x) __context__(x, 1)
#else
# define __acquire(x) (void)0
#endif
```

预处理器的工作是选择记号和替换记号。它不知道 `x` 是锁，也不知道 `__context__()` 会怎样改变 Sparse 状态。

一次手工展开可以写成：

```text
原始源码：__acquire(lock);

定义__CHECKER__：
  → __context__(lock, 1);

未定义__CHECKER__：
  → (void)0;
```

只有完成这一步，才轮到 Sparse 或编译器解释剩余语法。

## 2.3\_GNU属性是语法容器而不是统一功能

GNU 属性的基本外形是：

```c
__attribute__((attribute_list))
```

双层括号属于语法本身。属性可以附着在函数、变量、声明或类型的不同位置，其意义由 **具体属性名称和消费工具** 决定。例如：

```c
void stop_now(void) __attribute__((noreturn));
int __attribute__((address_space(1))) *pointer;
```

`noreturn` 可以影响普通编译器的控制流判断和优化；`address_space` 在这里主要由 Sparse 建立逻辑指针域。仅仅看到相同的 `__attribute__((...))` 外形，不能推出二者由同一工具执行，也不能推出都会产生机器指令。

Linux 宏经常给属性名也加双下划线，例如 `__noreturn__`。这样可以降低它与用户宏或普通标识符冲突的概率；它仍是同一种 GNU 属性语法。

## 2.4\_typeof构造依赖实参的类型

`typeof(expr)` 取得表达式的类型，并且可以出现在通常允许类型名的位置。RCU 的类型桥接使用了这种能力：

```c
#define rcu_check_sparse(p, space) \
    ((void)(((typeof(*p) space *)p) == p))
```

按层拆解：

1. `*p` 表示 `p` 指向的对象表达式；
2. `typeof(*p)` 取得对象类型，不读取对象值；
3. `typeof(*p) space *` 构造“指向同类对象、但目标带指定 Sparse 地址域”的指针类型；
4. `(该类型)p` 迫使 Sparse 检查转换；
5. `== p` 继续要求两边类型可比较；
6. 最外层 `(void)` 丢弃没有业务意义的比较结果。

工具识别的是展开后的类型构造，不是 `rcu_check_sparse` 这个名字。

关于 `typeof`、`container_of()` 等通用 GNU C 用法，继续参考 [GNU C 扩展](../gnu_extensions/C_language_extension.md#1.3.1_typeof_获取表达式类型)。本专题只讨论它怎样承载分析契约。

## 2.5\_语句表达式把路径事件放进一个值表达式

GNU C 允许：

```c
({
    statement_1;
    statement_2;
    final_expression;
})
```

整个语句表达式的值来自最后一个表达式。Linux 的条件记账宏使用：

```c
#define __cond_lock(x, c) ((c) ? ({ __acquire(x); 1; }) : 0)
```

如果 `c` 为真，Sparse 在该控制流分支上看到 `__acquire(x)`，同时整个表达式返回 `1`；假分支不登记取得并返回 `0`。它之所以能嵌进 `if (...)`，靠的就是语句表达式把“分析事件”和“条件值”组合成一个 C 表达式。

正常编译分支把它定义成：

```c
#define __cond_lock(x, c) (c)
```

因此不会为了静态记账多执行一次真实加锁。

## 2.6\_typeof与强制转换怎样构造受控逃生口

```c
#define ACCESS_PRIVATE(p, member) \
    (*((typeof((p)->member) __force *)&(p)->member))
```

可以按从内到外的顺序阅读：

```text
(p)->member
  → typeof(...)取得成员类型
  → &(p)->member取得成员地址
  → 强制转换为“成员类型 __force *”
  → 最外层*恢复为可读写的成员左值
```

C 本身没有因此产生真正的私有成员；`__force` 是 Sparse 的显式越权标记。正常编译分支直接展开为 `(p)->member`。这个宏的价值在于把“只有实现所有者可以绕过 `__private`”集中到一个可搜索入口，而不是让调用者到处散落裸强转。

## 2.7\_GNU可变参数宏与占位回退

```c
#define __builtin_warning(x, y...) (1)
```

`y...` 是 GNU 风格的命名可变参数，近似于：

```c
#define __builtin_warning(x, ...) (1)
```

在这个普通编译分支里，无论附加多少参数，宏都返回常量 `1`。它本身不会打印警告；具体调用方怎样利用返回值，需要继续读调用表达式。这里再次说明：名字中带 `warning` 或 `builtin`，都不能替代宏展开后的真实语义。

## 2.8\_一套可复用的手工展开顺序

以后遇到复杂宏，可以固定采用下面的顺序：

1. 先确定当前配置定义了哪些条件宏；
2. 删除不会进入翻译单元的条件分支；
3. 从最外层宏逐层替换参数；
4. 标出剩余的 GNU C 扩展和工具专用记号；
5. 判断属性究竟附着在哪个声明或类型上；
6. 判断表达式是否会求值，是否只用于形成类型约束；
7. 最后才讨论诊断、元数据或运行时代码。

完成语法层后，下一章把 `address_space`、`noderef` 和类型桥接函数放进同一套指针契约中，解释它们怎样发现用户指针、MMIO、per-CPU 与 RCU 指针的混用。

上一篇：[同一份源码怎样面对多个消费者](P01_同一份源码怎样面对多个消费者.md)。

下一篇：[Sparse 地址空间与指针类型契约](P03_Sparse地址空间与指针类型契约.md)。
