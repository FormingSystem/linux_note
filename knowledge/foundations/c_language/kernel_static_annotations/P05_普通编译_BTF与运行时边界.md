---
id: knowledge.foundations.c_language.kernel_static_annotations.p05
title: "普通编译、BTF 与运行时边界"
kind: concept
status: evolving
domains:
  - foundations
  - c_language
  - linux
---

# 第5章\_普通编译\_BTF\_与运行时边界

## 5.1\_Sparse分支结束后还剩下什么问题

前两章已经解释了 Sparse 怎样建立地址空间类型和 context 账本。现在切换到普通 GCC/Clang 构建：既然 `__CHECKER__` 没有定义，为什么 `__user`、`__percpu` 和 `__rcu` 没有一律写成空宏，而是可能变成 `BTF_TYPE_TAG()`？

原因是“不给 Sparse 使用”不等于“没有其他工具消费者”。Linux 需要让同一个友好记号按构建能力服务不同目的，同时保证不支持该能力时仍可退化。

## 5.2\_BTF\_TYPE\_TAG的启用门槛

Linux 6.12 的逻辑可以概括为：

```c
#if defined(CONFIG_DEBUG_INFO_BTF) && \
    defined(CONFIG_PAHOLE_HAS_BTF_TAG) && \
    __has_attribute(btf_type_tag) && \
    !defined(__BINDGEN__)
# define BTF_TYPE_TAG(value) \
    __attribute__((btf_type_tag(#value)))
#else
# define BTF_TYPE_TAG(value)
#endif
```

必须同时满足：

1. 内核配置要求生成 BTF 调试信息；
2. `pahole` 工具链支持 BTF type tag；
3. 当前编译器识别 `btf_type_tag` 属性；
4. 当前不是需要避开该属性的 bindgen 处理路径。

任何一个条件不满足，宏就为空。这是能力探测和可退化构建，不是运行时开关。

## 5.3\_普通分支中的地址域宏矩阵

| 宏 | 普通编译分支 | 主要消费者 | 结果类别 |
| --- | --- | --- | --- |
| `__kernel` | 空 | 无 | 普通 C 类型 |
| `__user` | `STRUCTLEAK_PLUGIN` 属性，或者 `BTF_TYPE_TAG(user)` | GCC 插件或 BTF 工具 | 插件输入或类型元数据 |
| `__iomem` | 空 | 无 | 普通 C 类型 |
| `__percpu` | `BTF_TYPE_TAG(percpu)` | BTF 工具 | 类型元数据 |
| `__rcu` | `BTF_TYPE_TAG(rcu)` | BTF 工具 | 类型元数据 |

这张表只描述本专题对应的 Linux 6.12 公共头文件分支。Sparse 路径中的 `address_space` 和 `noderef` 约束不能从普通分支反推；普通构建成功也不能替代 Sparse 检查。

## 5.4\_BTF标签保存信息但不执行协议

BTF type tag 可以把 `user`、`percpu` 或 `rcu` 之类的字符串关联到类型信息，供调试、BPF 和其他类型消费者观察。它不具备下面这些能力：

- 在赋值时拒绝两个 Sparse 地址域混用；
- 在运行时拦截用户指针裸解引用；
- 自动执行 `rcu_dereference()`；
- 维护锁或 context 计数；
- 保证对象生命期和内存顺序。

可以用一条最小映射理解：

```text
源码中的__rcu
  ├─ Sparse分支 → 受限指针类型 → 静态诊断
  └─ 普通分支 → btf_type_tag("rcu")或空 → 类型元数据或无额外产物
```

两个分支保留的是同一个设计意图，但证明能力完全不同。

## 5.5\_STRUCTLEAK插件分支是第三种消费者

```c
#ifdef STRUCTLEAK_PLUGIN
# define __user __attribute__((user))
#else
# define __user BTF_TYPE_TAG(user)
#endif
```

这里的 `user` 属性是交给对应 GCC 插件的输入，不是 Sparse 的 `address_space(__user)`。同名语义意图可以映射到不同工具的属性，但必须继续沿当前分支确认哪个工具真正启用。

因此不能写出“`__user` 就是某一个固定属性”的绝对结论。稳定结论是：它表达用户指针边界；具体展开由分析器、插件、调试配置和工具能力决定。

## 5.6\_上下文宏在普通构建中怎样退化

```c
#define __must_hold(x)
#define __acquires(x)
#define __releases(x)
#define __acquire(x) (void)0
#define __release(x) (void)0
#define __cond_lock(x, c) (c)
```

这里有两类退化：

- 声明属性直接消失，不改变函数 ABI；
- 表达式宏保留真实条件 `c`，删除分析事件。

尤其是 `__cond_lock(x, c)` 不能退化成常量真或假，否则会改变 trylock 的真实控制流。它必须保留 `c` 的求值次数和返回语义。

`__chk_user_ptr(x)` 退化成 `(void)0`，意味着 `x` 不求值。设计新的静态检查辅助宏时，也应明确普通分支是否允许求值参数；不能无意中让调试/分析开关改变业务副作用。

## 5.7\_builtin\_warning回退只提供表达式形状

普通分支定义：

```c
#define __builtin_warning(x, y...) (1)
```

这个宏接受一个固定参数和任意附加参数，最终返回 `1`。它不调用编译器诊断接口，也不会在运行时打印信息。只有读到具体调用表达式，才能判断常量 `1` 会选择哪个分支或帮助什么代码完成编译。

这是阅读内核宏的通用纪律：宏名只是作者提供的入口名称，证明语义必须来自当前配置下的展开结果与调用位置。

## 5.8\_从源码到结果的四层核对表

| 想确认的问题 | 应观察什么 | 推荐工具 | 不能用什么替代 |
| --- | --- | --- | --- |
| 条件分支和宏怎样展开 | 预处理输出 | `gcc -E`、构建详细命令 | 运行日志 |
| 类型/上下文契约是否冲突 | Sparse 诊断 | `make C=1/C=2`、`sparse` | 普通 `make` 成功 |
| 生成了什么指令 | 目标文件反汇编 | `objdump`、`llvm-objdump` | BTF 标签 |
| BTF 中保留什么类型标签 | `.BTF` 与类型转储 | `pahole`、`bpftool btf dump`、`readelf` | Sparse 无告警 |
| 真实锁/RCU路径怎样运行 | trace、告警、测试和状态 | Lockdep、KCSAN、ftrace、专项实验 | 静态类型正确 |

观察工具必须与问题层次匹配。下一章把这些工具组织成可操作流程，并明确普通开发者怎样在不修改运行内核的情况下参与 Sparse 检查。

上一篇：[Sparse 上下文与控制流记账](P04_Sparse上下文与控制流记账.md)。

下一篇：[在 Linux 内核中使用 Sparse 与设计注解](P06_在Linux内核中使用Sparse与设计注解.md)。
