---
id: knowledge.foundations.c_language.kernel_static_annotations.p03
title: "Sparse 地址空间与指针类型契约"
kind: concept
status: evolving
domains:
  - foundations
  - c_language
  - linux
---

# 第3章\_Sparse\_地址空间与指针类型契约

## 3.1\_相同机器表示为什么还要分类型

上一章已经能够把：

```c
char __user *src;
```

展开为 Sparse 可见的属性。仍然存在一个疑问：如果用户地址、MMIO 地址、per-CPU 地址和普通内核地址最终都可能装进机器字长的指针，为什么还要把它们区分成不同类型？

机器表示相同，只说明寄存器能够保存这些比特；不代表允许使用同一套访问协议：

- 用户地址可能缺页、失效或需要访问检查，应通过 `copy_from_user()`、`get_user()` 等接口；
- MMIO 访问需要设备 I/O 原语和相应顺序约束，不能用普通对象读写替代；
- per-CPU 地址需要先确定 CPU/抢占约束，再通过 per-CPU 接口取得正确实例；
- RCU 指针的发布、取得和生命期协议要求专用访问器。

Sparse 的目标是让“访问协议不同”在代码进入运行时以前就变成类型冲突。

## 3.2\_address\_space建立逻辑指针域

Linux 6.12 的 Sparse 分支定义：

```c
#define __kernel __attribute__((address_space(0)))
#define __user   __attribute__((noderef, address_space(__user)))
#define __iomem  __attribute__((noderef, address_space(__iomem)))
#define __percpu __attribute__((noderef, address_space(__percpu)))
#define __rcu    __attribute__((noderef, address_space(__rcu)))
```

`address_space(name)` 施加在指针目标类型上。Sparse 把不同名称的地址域视为不同类型，并对不受控的赋值、比较或转换发出诊断。

这里的地址域首先是 **静态类型域**，不是对硬件 MMU、页表或总线窗口的声明。`__user` 的真实安全访问仍然由架构访问检查、异常处理和 uaccess 实现完成；`__iomem` 的真实设备事务仍然由 I/O 访问器生成。Sparse 只检查调用者是否遵守了进入这些机制的类型边界。

`__kernel` 使用 `address_space(0)` 显式表示默认内核域。普通未标注指针通常落在这个默认语义中。

## 3.3\_noderef阻止错误的普通解引用

`noderef` 表示带此属性的指针不能通过普通左值解引用：

```c
int read_value(const int __user *src)
{
    return *src; /* Sparse 应报告直接解引用受限指针 */
}
```

它不是说这个地址永远不可访问，而是说当前表达式绕过了该地址域要求的访问接口。正确形式取决于具体域：

| 指针域 | 常见正确入口 | 裸解引用遗漏了什么 |
| --- | --- | --- |
| `__user` | `copy_from_user()`、`get_user()` | 访问检查、缺页/异常与失败返回 |
| `__iomem` | `readl()`、`writel()` 等 | 设备访问宽度、顺序和架构映射 |
| `__percpu` | `this_cpu_*()`、`per_cpu_ptr()` 等 | CPU 实例选择与抢占/迁移约束 |
| `__rcu` | `rcu_dereference()`、`rcu_assign_pointer()` 等 | 发布/取得语义、类型与动态条件检查 |

表中的接口不是互换关系。`noderef` 只统一表达“不要当普通 C 对象直接访问”，每种地址域仍有自己的功能协议。

## 3.4\_空函数怎样把实参送进类型检查

```c
static inline void
__chk_user_ptr(const volatile void __user *ptr)
{
}
```

函数体为空，但调用表达式仍需要完成参数类型匹配：

```c
__chk_user_ptr(candidate);
```

Sparse 会检查 `candidate` 能否作为 `const volatile void __user *` 传入。因此检查来自形参类型，不来自函数体，也不来自函数名。`__chk_io_ptr()` 对 `__iomem` 使用相同方法。

在普通编译分支中，这两个入口变成：

```c
#define __chk_user_ptr(x) (void)0
#define __chk_io_ptr(x)   (void)0
```

参数不求值，也没有运行时调用。若调用者把有副作用的表达式传给这类只用于静态检查的宏，必须先确认普通分支是否会丢弃求值；不能假定检查宏等同于普通函数。

## 3.5\_rcu\_check\_sparse不是关键字扫描

RCU 使用：

```c
#define rcu_check_sparse(p, space) \
    ((void)(((typeof(*p) space *)p) == p))
```

它完成的是三步类型构造：

```text
p的目标类型
  → 给目标类型附加期望的space
  → 与p的实际指针类型比较
```

如果把宏改名而保持展开结果，Sparse 仍然能够检查；如果保留宏名却删除 `typeof(*p) space *` 的类型约束，检查就会消失。因此“RCU 静态工具的关键字是 `rcu_check_sparse`”这个模型不成立。

该实现的 Linux 6.12 版本化讲解见 [`rcu_check_sparse()` 静态类型桥接](../../../../research/source_reading/rcu/source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.3.3_rcu_check_sparse静态类型桥接)。

## 3.6\_force等逃生口分别放宽什么

| 注解 | Sparse 语义 | 适合什么情况 | 不能替代什么 |
| --- | --- | --- | --- |
| `__force` | 在显式转换处抑制额外限定导致的诊断 | 实现层已经独立证明转换有效 | 拷贝、范围检查、对象生命期和硬件访问 |
| `__nocast` | 对转换给出较弱约束 | 防止宽整数被轻易截断等 | `__bitwise` 的严格独立整数类型 |
| `__safe` | 声明指针不会为 NULL 或陷阱值，并对条件测试给出诊断 | 已由更强不变量保证的指针 | 运行时空指针检查本身 |
| `__private` | 借助 `noderef` 限制普通成员访问 | 让对象所有者集中管理内部字段 | C 语言真正的访问控制 |

`__force` 最容易被误用。下面的转换只表示“作者要求 Sparse 接受它”：

```c
void *ptr = (__force void *)user_pointer;
```

它没有把用户地址复制到内核内存，也没有建立安全访问窗口。若后续直接解引用，真实风险仍然存在。

## 3.7\_ACCESS\_PRIVATE集中管理有意越权

`__private` 使普通成员访问触发 `noderef` 约束；实现所有者可以通过：

```c
ACCESS_PRIVATE(object, member)
```

集中执行一次带 `__force` 的受控转换。这样做的工程价值不是创造 C 的私有字段，而是形成三个可审查事实：

1. 外部调用者不能在无意中直接访问；
2. 有意越权点具有统一名字，可以用 `rg` 审查；
3. 普通编译分支仍是直接成员访问，不增加运行成本。

Linux 6.12 中 RCU 节点私有锁和 VMA 私有标志都能看到这类用法。先从 [Linux 6.12 compiler types 注解模块概念导读](../../../../research/source_reading/compiler_annotations/navigation/P02_Linux_6.12_compiler_types注解模块概念导读.md#2.4_地址空间模块链)确认实现所有权，再进入 [compiler types 注解宏源码实现](../../../../research/source_reading/compiler_annotations/source_explanations/P01_Linux_6.12_compiler_types注解宏源码实现.md#1.7_其他限定与_ACCESS_PRIVATE逃生口)核对具体宏体。

## 3.8\_地址空间检查的证明边界

Sparse 报告类型兼容，只能形成以下有限结论：在本次翻译单元、当前配置分支和已经建立的类型传播中，没有观察到相应地址域冲突。它不能单独证明：

- 用户地址在运行时一定有效；
- MMIO 设备仍在电源开启状态；
- per-CPU 指针使用期间任务没有迁移；
- RCU 目标对象尚未回收；
- 所有配置分支和其他翻译单元都已经检查。

下一章加入另一条正交状态轴：同一个指针类型正确的函数，仍可能在错误的锁上下文中调用。Sparse 使用 `context()` 与 `__context__()` 沿控制流维护这本账。

上一篇：[预处理器、GNU 属性与表达式扩展](P02_预处理器_GNU属性与表达式扩展.md)。

下一篇：[Sparse 上下文与控制流记账](P04_Sparse上下文与控制流记账.md)。
