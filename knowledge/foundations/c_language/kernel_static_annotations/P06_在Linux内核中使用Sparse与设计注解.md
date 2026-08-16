---
id: knowledge.foundations.c_language.kernel_static_annotations.p06
title: "在 Linux 内核中使用 Sparse 与设计注解"
kind: interface
status: evolving
domains:
  - foundations
  - c_language
  - linux
---

# 第6章\_在\_Linux\_内核中使用\_Sparse\_与设计注解

## 6.1\_谁来使用以及自己是否有资格

前五章已经区分每种注解的消费者。现在可以直接回答“谁来使用”：

- 内核和驱动作者在接口上维护地址域、整数域和上下文契约；
- 开发者在本机构建环境中运行 Sparse，修复自己改动触发的诊断；
- 子系统维护者和 CI 可以扩大到目录或整棵源码树；
- 评审者用宏展开、调用链和工具结果核对注解有没有掩盖真实问题。

普通开发者当然有资格使用 Sparse。它是构建阶段的静态分析工具，不要求修改正在运行的内核，也不要求在目标板上取得 root 权限。实际门槛是：

1. 能取得与改动一致的内核源码；
2. 准备好该源码需要的配置和生成头文件；
3. 在构建主机安装 Sparse；
4. 知道本次检查覆盖了哪些目录、配置分支和翻译单元。

若要加载实验内核模块或运行动态 Lockdep，才需要与目标机内核、模块签名和权限相匹配；那是另一层资格。

## 6.2\_安装并确认Sparse入口

Linux 发行版通常提供 `sparse` 软件包，也可以从 Sparse 官方源码构建。安装后先确认：

```bash
sparse --version
```

如果命令不存在，不要把普通编译器警告描述成 Sparse 结果。若版本差异影响某个注解，应在实验记录中同时保存 Sparse 版本、内核提交和构建命令。

## 6.3\_在内核构建系统中选择检查范围

内核构建系统提供两种常用入口：

```bash
# 只检查本次实际重新编译的C文件
make C=1

# 无论目标是否需要重编译，都对选中范围运行Sparse
make C=2
```

第一次参与不必直接扫描整棵树。优先缩小到改动目录：

```bash
make C=2 M=drivers/example
```

外部模块可以沿自己的内核构建入口运行：

```bash
make -C /path/to/kernel/build \
    M="$PWD" \
    C=2 \
    modules
```

需要额外 Sparse 选项时使用 `CF`：

```bash
make C=2 M=drivers/example CF="-Wcontext"
```

`C=1` 与 `C=2` 的差别是构建范围，不是检查严格程度。`M=` 决定目录范围，`CF` 才负责传递额外分析选项。

## 6.4\_先用预处理输出确认分析器看见什么

当诊断与预期不一致时，第一步不是添加 `__force`，而是确认条件分支与宏展开。对于本专题的独立实验，可以比较：

```bash
# 模拟Sparse条件分支，只观察预处理结果
gcc -E -P -D__CHECKER__ sparse_annotation_demo.c | less

# 观察普通编译分支
gcc -E -P sparse_annotation_demo.c | less
```

对真实内核文件，必须使用构建系统给出的完整 include 路径、自动生成配置和编译参数；脱离构建命令直接执行 `gcc -E file.c` 可能得到错误分支或缺少头文件。可先使用详细构建输出取得真实命令：

```bash
make V=1 M=drivers/example
```

## 6.5\_完成仓库中的最小实验

独立实验位于：[Sparse 地址空间与上下文记账](../../../../labs/foundations/c_language/P01_Sparse地址空间与上下文记账/README.md#1.1_实验目标)。建议按下面顺序执行：

```bash
cd labs/foundations/c_language/P01_Sparse地址空间与上下文记账

make check-good
make check-address-space
make check-context
make check-all
```

四轮实验分别回答：

1. 正确路径是否能够建立干净基线；
2. `address_space + noderef` 能否发现指针域混用和裸解引用；
3. `context + __context__` 能否发现未持有调用和不配对退出；
4. 两类错误同时出现时，能否按类型状态与路径状态分组解释报告。

本仓库当前 Windows 会话没有 Sparse 可执行环境，因此实验文件经过静态审查，但没有把未运行结果写成已验证观测。读者应在 Linux 构建环境中保存自己的版本与原始输出。

## 6.6\_怎样解释一条诊断

不要只记录警告文本。每条诊断至少还原五项事实：

| 项目 | 要回答的问题 |
| --- | --- |
| 原始对象 | 哪个变量、成员、锁或地址域参与冲突？ |
| 展开结果 | 当前配置下宏变成什么类型或 context 事件？ |
| 控制流 | 诊断发生在哪个成功、失败或返回分支？ |
| 契约来源 | 形参类型、函数属性还是语句标记建立了要求？ |
| 修复位置 | 应修功能访问路径、接口类型还是注解本身？ |

若真实访问路径错误，应改用正确的 uaccess、I/O、per-CPU、RCU 或锁接口；若功能正确但包装层没有向 Sparse 传播契约，才补注解。用 `__force` 压掉所有警告会把两类情况混在一起。

## 6.7\_为自定义接口接入上下文契约

假设一个 helper 要求调用者持有对象锁：

```c
void update_object(struct object *object)
    __must_hold(&object->lock);
```

如果另一个包装函数真正取得并返回时保持该锁，可以声明：

```c
void object_lock(struct object *object)
    __acquires(&object->lock);
```

相应释放函数声明为：

```c
void object_unlock(struct object *object)
    __releases(&object->lock);
```

接入前必须核对：

- 功能动作成功后才登记取得；
- 失败和回滚路径不会留下静态计数；
- 所有正常返回都满足声明的退出计数；
- trylock 的成功条件通过 `__cond_lock()` 等可见路径传递；
- 普通编译分支不改变实参求值、ABI 和真实控制流；
- 正确示例与故意错误示例都纳入检查。

不应为每个业务布尔状态滥用 context。只有它确实表示跨函数边界需要配对或保持的控制流契约，且 Sparse 能稳定识别相关表达式时，才适合接入。

## 6.8\_与哪些工具搭配

| 问题 | 首选证据 | 相邻工具 |
| --- | --- | --- |
| 宏到底展开成什么 | 预处理输出 | `gcc -E`、Clang `-E` |
| 地址域和整数域混用 | Sparse 类型系统 | 编译器类型警告、Smatch |
| 函数上下文是否配对 | Sparse `-Wcontext` | Lockdep 动态路径、人工协议审查 |
| 大规模接口模式迁移 | 语义规则 | Coccinelle |
| 实际锁顺序和 IRQ 依赖 | 运行路径事件 | Lockdep、KUnit/kselftest |
| 数据竞争 | 运行时访问冲突 | KCSAN、专项压力测试 |
| 越界和释放后访问 | 内存访问插桩 | KASAN、对象生命期审查 |
| 指令与屏障是否生成 | 目标文件 | `objdump`、`llvm-objdump` |
| BTF标签是否保留 | BTF转储 | `pahole`、`bpftool btf dump`、`readelf` |

这些工具不是“多跑几个就自动证明正确”。应先指出待证明的问题，再选择能观察相应状态的工具。

## 6.9\_把无告警写成条件命题

一个合格结论应写成：

> 在记录的内核配置、Sparse 版本、构建范围和选项下，目标翻译单元已经进入 Sparse；当前可见地址域转换与 context 路径没有产生相应诊断。

不能扩写成：

> 这个驱动已经没有用户指针、锁、RCU、对象生命期或并发问题。

静态分析依赖注解完整性和配置覆盖，动态分析依赖实际路径与检查器生命状态，功能正确性还依赖硬件、内存序和对象所有权。把结论限定在证据真正覆盖的层次，才是使用工具而不是迷信工具。

## 6.10\_迁移练习

看到下面的新宏时，不查答案，先按专题模型推导：

```c
#ifdef __CHECKER__
#define __device_handle \
    __attribute__((noderef, address_space(__device_handle)))
#else
#define __device_handle
#endif
```

至少回答：

1. 它给哪个类型位置增加约束？
2. Sparse 能发现哪些混用，不能发现哪些设备生命期问题？
3. 哪个 API 负责从受限句柄执行真实设备操作？
4. 是否真的需要新地址域，还是已有 `__iomem` 或不透明类型已经足够？
5. 普通编译分支为空时，怎样避免调用者绕过接口？
6. 应准备哪些正确/错误样例和运行时验证？

能够独立回答这些问题，说明读者已经可以把本专题模型迁移到新的内核注解，而不是只记住 `__user` 和 `__rcu` 的固定定义。

上一篇：[普通编译、BTF 与运行时边界](P05_普通编译_BTF与运行时边界.md)。

返回：[专题大纲](大纲.md#1.3_阅读依赖图)。

## 6.11\_参考资料

- [Linux 内核 Sparse 文档](https://docs.kernel.org/dev-tools/sparse.html)
- [Sparse 注解语义](https://sparse.docs.kernel.org/en/latest/annotations.html)
- [GCC GNU 属性语法](https://gcc.gnu.org/onlinedocs/gcc/Attribute-Syntax.html)
- [GCC typeof 语法](https://gcc.gnu.org/onlinedocs/gcc/Typeof.html)
- [Clang btf type tag 属性](https://clang.llvm.org/docs/AttributeReference.html#btf-type-tag)
