---
id: research.source_reading.memory_ordering.rwonce_impl
title: "rwonce.h 单次访问源码实现"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
---

# 第1章\_rwonce.h单次访问源码实现

## 1.1\_沿读取现场核对实现

[模块导读](../../../navigation/P02_单次访问与类型边界导读.md#2.3_一条访问怎样闭合)已经把 `READ_ONCE(slot->value)` 分成R0～R3：选定对象、检查尺寸、生成并执行访问、交回结果。本章逐段解释这条路径的实现原理，再比较写入、特殊机器字读取与按字访问。这里没有锁状态机，不能从宏名称推导排他性。

源码身份为NXP官方linux-imx、标签lf-6.12.20-2.0.0、提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20；[总索引](../../../navigation/P01_Linux_6.12_LKMM_源码与模型导读.md#1.1_版本和研究边界)负责整体阅读。主体位置为 `include/asm-generic/rwonce.h`，类型辅助定义位于 `include/linux/compiler_types.h`。以下中文Doxygen说明、行内注释均为 **仓库补充阅读说明**，不是上游注释；代码保留原始条件及求值形态，仅裁剪文件头、包含语句和非本单元定义。

原始证据：[rwonce.h](../../../../linux/include/asm-generic/rwonce.h)、[compiler_types.h](../../../../linux/include/linux/compiler_types.h)。不通过本页代码替换内核头文件。

## 1.2\_类型工具与尺寸门槛

R1要判断的是类型，不是读取一个运行中的值。`sizeof`给出表达式类型的大小；`typeof`是GNU C类型扩展；C11的_Generic按控制表达式类型选择分支，控制表达式本身不执行。这里的普通标量场景不会为了判断类型先读取一次目标对象。

```c
/**
 * __unqual_scalar_typeof - 为受约束读取形成结果类型（仓库阅读说明）
 * @x: 调用者指定的表达式；本组工具只参与编译期类型选择
 * 常用整数类型选择无限定的零值表达式，其他类型进入default。
 */
#define __scalar_type_to_expr_cases(type) \
        unsigned type: (unsigned type)0, \
        signed type: (signed type)0

#define __unqual_scalar_typeof(x) typeof( \
        _Generic((x), \
            char: (char)0, \
            __scalar_type_to_expr_cases(char), \
            __scalar_type_to_expr_cases(short), \
            __scalar_type_to_expr_cases(int), \
            __scalar_type_to_expr_cases(long), \
            __scalar_type_to_expr_cases(long long), \
            default: (x)))

/* 这里只按尺寸判断；不是判断变量是否位于某个原子存储区。 */
#define __native_word(t) \
    (sizeof(t) == sizeof(char) || sizeof(t) == sizeof(short) || \
     sizeof(t) == sizeof(int) || sizeof(t) == sizeof(long))

/**
 * compiletime_assert_rwonce_type - 公共ONCE入口的尺寸门槛
 * @t: 待访问表达式
 * 接受本机常用整数尺寸，另接受long long尺寸；不验证对齐或寿命。
 */
#define compiletime_assert_rwonce_type(t) \
    compiletime_assert(__native_word(t) || sizeof(t) == sizeof(long long), \
        "Unsupported access size for {READ,WRITE}_ONCE().")
```

单独的char分支有理由：C的char、signed char与unsigned char并非三个可随意合并的兼容类型。其余整数类型由辅助宏成对列出，default使指针等其他表达式继续拥有其相应类型。不要把这个工具解释为“无论输入什么，都转换成unsigned long”。

compiletime_assert是内核的编译期断言设施，这里只讲调用条件，不复制它在其他编译配置中的诊断实现。尺寸相等只说明公共ONCE允许生成此类访问。固定源码特意指出32位机器也接纳64位大小，某些组合可原子访问，另一些可能拆开。删去这个例外会改变接口接纳范围；保留例外又不能替调用者证明不撕裂。

## 1.3\_READ\_ONCE与内部读取

R1通过后，R2并非调用一个有运行时账本的函数，而是把左值地址用于受约束读取。

```c
/**
 * __READ_ONCE - 通用内部读取形式，不承担公共尺寸断言
 * @x: 有效、可取地址的对象左值
 * 返回通过const volatile指针读到的值；架构可以预先提供自己的定义。
 */
#ifndef __READ_ONCE
#define __READ_ONCE(x) (*(const volatile __unqual_scalar_typeof(x) *)&(x))
#endif

/**
 * READ_ONCE - 先通过尺寸门槛，再返回受约束读取的结果
 * @x: 读取目标；对象有效性仍由调用方保证
 */
#define READ_ONCE(x) \
({ \
    compiletime_assert_rwonce_type(x); \
    __READ_ONCE(x); \
})
```

外层 `({ ... })` 是GNU语句表达式，末尾内部读取表达式的值成为整个宏的结果。`&(x)`取得原对象地址，指针转换没有复制对象。const表示该读取表达式不用于写入，volatile使编译器按受约束访问对待这次加载；二者都不分配锁，也不延长对象生命期。

对普通整数 `array[index++]`，类型和尺寸工具不执行一次额外自增，实际取地址时才推进index。这个观察不能扩大为“带任意可变长度类型或任意副作用的表达式都适合宏”；最清楚的共享协议仍应传入稳定对象左值，让求值与同步责任可见。

直接调用__READ_ONCE会绕开外层尺寸检查，源码也警告可能撕裂。内部读取可被架构覆盖，因此这里核对的是通用实现，不把这行volatile转换等同于每一种架构的最终指令。它同样没有隐含完整acquire；若后续字段需要发布取得保证，必须核对相应原语或合法依赖链。

## 1.4\_WRITE\_ONCE与内部写入

公共写入宏WRITE_ONCE需要保留值赋给原对象的动作，内部宏__WRITE_ONCE承担当次存储，不能把读—改—写的中间步骤藏进“单次”二字。

```c
/**
 * __WRITE_ONCE - 通过volatile左值执行内部写入
 * @x: 目标对象左值
 * @val: 待赋值表达式；转换仍遵守该赋值的类型规则
 */
#define __WRITE_ONCE(x, val) \
do { \
    *(volatile typeof(x) *)&(x) = (val); \
} while (0)

/**
 * WRITE_ONCE - 通过尺寸门槛后执行受约束写入
 * @x: 目标对象
 * @val: 待写值；不是比较条件
 */
#define WRITE_ONCE(x, val) \
do { \
    compiletime_assert_rwonce_type(x); \
    __WRITE_ONCE(x, val); \
} while (0)
```

`do ... while (0)`把展开体组成一个语句，方便用于条件分支；它不表示反复轮询。val在赋值处求值，没有先比对内存旧值。若调用者先READ_ONCE计数再把加一结果WRITE_ONCE回去，两名更新者仍可覆盖彼此的修改。

指针转换也不会给const对象合法的写权限。写入目标、对齐、活期、与其他参与者的同步都是调用方前提；宏不返回交换出的旧值，不获得排他资格，不是通用release发布。修改宏时若添加运行时读取以“检查当前值”，便已经改变访问事件和竞态窗口，不能当作无害排版。

## 1.5\_机器字读取与检测边界

栈回溯等特殊路径可能需要读取一个机器字，又不能按普通路径进行全部检测插桩。固定头文件提供宏READ_ONCE_NOCHECK，并进一步收紧大小；名称后缀NOCHECK表示有意抑制特定检测，不表示取消所有检查。

```c
/**
 * __read_once_word_nocheck - 以机器字读取指定地址
 * @addr: 调用方保证可用于该特殊读取的地址
 * 属性依编译配置选择检测抑制或内联；本函数不探测地址是否安全。
 */
static __no_sanitize_or_inline
unsigned long __read_once_word_nocheck(const void *addr)
{
    return __READ_ONCE(*(unsigned long *)addr);
}

/**
 * READ_ONCE_NOCHECK - 限定为unsigned long大小的特殊读取
 * @x: 待读取左值，结果转回其表达式类型
 */
#define READ_ONCE_NOCHECK(x) \
({ \
    compiletime_assert(sizeof(x) == sizeof(unsigned long), \
        "Unsupported access size for READ_ONCE_NOCHECK()."); \
    (typeof(x))__read_once_word_nocheck(&(x)); \
})
```

路径是“检查大小→传原地址→以unsigned long受约束读取→转换结果类型”。这里的NOCHECK并非去掉所有检查：尺寸断言仍在。变化集中在承担读取的辅助函数属性，不是凭空获得故障恢复、对象引用或额外顺序。

`__no_sanitize_or_inline`在compiler_types.h里按编译器插桩标记分支选择：地址检测分支禁用相应检查，线程检测分支使用更严格的插桩抑制属性，内存初始化检测分支也有专门处理；没有相应标记时退为强制内联。仅看到函数名，既不能判定当前构建启用了哪些检测，也不能把“没有告警”当作这次读取有效的证明。本批宿主检查不启用这些内核检测器。

## 1.6\_read\_word\_at\_a\_time的检查尺寸

另一种特殊读取也返回unsigned long，却有不同的访问与检测契约。KASAN（Kernel Address Sanitizer）是内核地址访问检测器，下面需要区分它显式检查的范围与函数实际读取的范围。

```c
/**
 * read_word_at_a_time - 显式检查起始字节后读取一个机器字
 * @addr: 由特定调用场景保证满足该读取条件的地址
 * 显式检查长度为1，真实读取大小为sizeof(unsigned long)。
 */
static __no_kasan_or_inline
unsigned long read_word_at_a_time(const void *addr)
{
    kasan_check_read(addr, 1); /* 起始字节的显式地址检测。 */
    return *(unsigned long *)addr; /* 普通机器字读取，不是ONCE。 */
}
```

KASAN是内核地址访问检测器；这里的属性在相关构建中限制自动地址插桩，再由函数显式检查一个字节。关闭KASAN时，这个显式检查也不能成为实际安全证据。重要的差别有两个：一字节检查不证明随后每个字节都在合法对象内；返回表达式不是volatile读取，不能沿用ONCE的编译器访问约束。

因此修改检查长度、去掉属性或换成READ_ONCE，都不是保持原语义的整理。应回到具体调用方的按字处理算法、对齐和地址边界，确定它为什么允许这样读取；本页没有授予一般调用者越界读取的许可。

## 1.7\_从实现返回协议

本页覆盖rwonce.h的公共与内部读写、尺寸断言、机器字NOCHECK辅助函数及按字读取，并交代所依赖的类型工具。回看R0～R3：编译期决定可接纳形态，运行期访问原对象，结果交给调用者，期间没有任何引用、唤醒或远端确认。

保持实现语义时，至少不能无理由改变左值求值次数、volatile访问形态、尺寸接纳范围和检测属性。并发应用还要单独证明访问顺序、修改资格与对象寿命。继续[模块边界练习](../../../navigation/P02_单次访问与类型边界导读.md#2.6_带着边界继续读屏障)，再进入[总索引的屏障入口](../../../navigation/P01_Linux_6.12_LKMM_源码与模型导读.md#1.3.2_通用屏障)。
