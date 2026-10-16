---
id: knowledge.foundations.c_language.gnu_extensions.c_language_extension
title: "Linux 内核开发中的 GNU C 扩展与内核特有语法"
kind: concept
status: evolving
domains:
  - foundations
  - c_language
  - linux
---

# 第1章\_Linux\_内核开发中的\_GNU\_C\_扩展与内核特有语法

已经会写C函数，为什么读内核时仍会被一行声明卡住？例如 `static int __init prepare_device(void)` 中的 `__init` 不是返回类型，`typeof(*p)` 看起来像解引用，`container_of(p, struct request, node)` 却能从一个成员回到完整对象。仅凭名字猜“优化宏”，无法判断它改变了布局、求值次数，还是运行时行为。

本篇面向掌握C指针、结构体、宏和编译链接的读者。沿“标准C还缺什么表达能力”逐项引出内联汇编、属性、类型表达式，再把这些工具放回内核宏；完成后应能区分编译器扩展与内核接口，并为每类写法选择正确的观察方法。GNU是编译工具家族的项目名，GNU C指这里使用的C扩展方言；Linux宏则是内核自己定义的名称，不会因为用了双下划线就变成语言关键字。

本篇语言依据采用GCC 14.2手册；内核实例按NXP官方linux-imx提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`、Linux 6.12.20核对。普通用户程序只用于观察语法和布局，不能据此执行特权指令或替代内核并发测试。

## 1.1\_GCC\_内联汇编\_Inline\_Assembly

标准C能表达加法和指针访问，却不能直接命名某个处理器控制寄存器。内联汇编允许把目标指令写进函数，同时把C值与汇编操作数连接起来。关键不是“往字符串里塞一条指令”，而是把输入、输出和其他破坏完整告诉编译器，否则编译器可能在寄存器分配或重排时作出与汇编实际行为不相容的安排。

### 1.1.1\_基本语法

```c
/* 语法骨架，不是可直接编译的具体指令。 */
__asm__ volatile ("汇编模板" : 输出操作数 : 输入操作数 : 其他破坏);
```

冒号按顺序划分三个接口：输出告诉编译器哪些C值由汇编产生，输入告诉它汇编需要哪些值，破坏列表说明额外改变的寄存器或状态。没有某类操作数时可以留空。模板中的 `%0`、`%1` 对应操作数编号；在x86的AT&T写法中，`%%cr0` 里的双百分号才表示输出一个字面的寄存器名称。

`volatile`限制某些删除和循环外提优化，**不等于整段代码禁止重排**。`"memory"`破坏项告诉编译器汇编可能触及未列出的内存，形成编译器层面的约束；它本身不生成CPU内存屏障。实际指令、操作数依赖、编译器约束和硬件顺序必须分别判断。[GCC 14.2 Extended Asm](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Extended-Asm.html)的Volatile、Operands与Clobbers分别规定这些职责。

### 1.1.2\_读取x86控制寄存器只能用于特权环境

下面保留读取CR0这个具体现场，但它只说明x86特权代码怎样接收寄存器值。CR0是x86控制寄存器，不属于ARM GIC的寄存器组；普通用户进程不能把该函数当成可运行实验。

```c
unsigned long read_cr0_demo(void)
{
    unsigned long value;

    /* x86 AT&T模板：只写输出；=r要求一个通用寄存器。 */
    __asm__ volatile ("mov %%cr0, %0" : "=r" (value));
    return value;
}
```

这里没有输入操作数，`value`由指令写入。CR0读取没有替C代码读写业务内存，所以不能从这个片段推导“前后的共享内存访问已全部排序”。真实内核应复用体系结构提供的访问接口；需要改变控制寄存器时，还必须遵守该体系结构的序列要求，不能照抄读函数改一个方向就提交。

| 本例寄存器 | 角色与访问方式 | 当前读写效果与边界 |
| --- | --- | --- |
| x86 CR0 | 处理器控制寄存器，通过特权MOV指令读取，不存在MMIO基址加偏移 | 本例只取得整体值，不修改控制位；按x86目标执行状态匹配输出寄存器宽度，不在此例解析具体控制字段 |

## 1.2\_属性怎样改变布局与链接

指令之外，驱动还会遇到另一种问题：同样的字段列表，编译器排出来的字节位置未必与设备协议相同；同样的函数，初始化完成以后又可能不再需要保留。`__attribute__((...))`是GNU属性的语法外壳，括号里的具体属性决定作用。它不是统一的“加速开关”，也不是所有工具都会解释的通用词典。

### 1.2.1\_packed减少填充但不建立硬件访问契约

设线上的一条记录要求第0字节是标签，第1至4字节是序号。普通结构体常把32位字段放到4字节边界，于是在标签后插入填充；接收端若把结构体整体当成五字节协议记录，就会读错位置。`packed`可以压紧成员布局，解决这个字节位置问题。

例如USB描述符前缀也使用8位和16位字段，但不能只看见u8后面有u16就断言中间必有空洞：两个连续u8之后的偏移已经是2，16位字段可能正好自然对齐。必须用具体字段顺序和目标ABI计算；ABI是编译器与运行环境约定的数据布局、调用方式等规则。1.3节实验同时测量一字节加32位字段和USB前缀这两种情况。

压紧布局有代价。成员地址可能不满足其自然对齐，生成的访问可能被拆分，某些目标还要求特殊处理；把packed成员地址传给期待自然对齐的指针参数尤其危险。packed也不转换端序、不保证原子性，更不能替代MMIO访问器。嵌套结构体内部的布局也不会仅因外层packed就自动全部压紧。依据：[GCC 14.2 Common Type Attributes，packed](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Common-Type-Attributes.html)。

### 1.2.2\_aligned指定起始对齐而非自动消除伪共享

若两个CPU分别频繁修改相邻对象，而它们恰好落在同一缓存行，缓存一致性维护可能使这一行反复转移；即使字段互不相同也会付出成本，这叫伪共享。`aligned(64)`可以要求类型或对象至少按64字节对齐，为布局隔离提供工具。

```c
struct counter_slot {
    unsigned long count;
} __attribute__((aligned(64)));
```

但64只是本例选择，不是所有处理器的缓存行大小。对齐一个含锁和计数器的结构体，也不会把它内部的锁与计数器自动拆到不同行；同一对象被真实共享时仍有同步与缓存成本。数组步长、分配器是否满足过度对齐、空间浪费也必须一起检查。内核常用基于目标配置的缓存行宏，而不是在可移植代码里到处写64。依据：[GCC aligned属性](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Common-Type-Attributes.html)及固定内核[include/linux/cache.h](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/include/linux/cache.h)。

### 1.2.3\_section提供归组标记而生命周期由内核落实

初始化代码完成任务后，长期占用内存没有意义。section属性允许把代码或数据放到命名的目标文件节中；链接脚本把这些节组织起来，内核再在合适时机回收初始化区。标记、布局和回收分别由不同参与者完成。

```c
/* 内核代码片段：该函数只在初始化阶段使用。 */
static int __init my_driver_init(void)
{
    return 0;
}
```

`__init`不自动调用函数，也不保证某函数只执行一次。内建代码通常在启动初始化路径使用它；可加载模块则可能在开机很久以后才加载，初始化区的寿命随模块加载过程决定。若把仍可能被再次调用的probe或运行期回调错误放入可回收区，保存着函数指针也无法阻止代码被回收。

| 内核标记或机制 | 当前职责 | 不提供的保证 |
| --- | --- | --- |
| `__init`、`__initdata`、`__initconst` | 分别标记初始化代码、数据、只读数据的节归属 | 不自动调用，也不修复回收后的引用 |
| `__exit`、`__exitdata` | 标记退出代码或数据；内建和模块的保留策略不同 | 不代表所有退出代码都在启动后立即释放 |
| `__exit_call` | 标记退出调用登记项的节和保留属性 | 不是一次运行时退出操作 |
| initcall登记或`module_init` | 把入口接入相应初始化流程 | 不等价于只把函数写成`__init` |

固定定义见[include/linux/init.h](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/include/linux/init.h)。本版本没有把`__init_call`作为与上述宏并列的通用入口；阅读时应核对真正的initcall登记宏，不根据对称拼写创造接口。

### 1.2.4\_weak在链接时选择默认实现

同一份上层代码调用平台操作，而不同构建需要不同实现。支持弱符号的目标文件格式允许提供一个默认定义；同一次链接中出现匹配的强定义时，强定义可以替代它。它解决的是链接选择，不是运行时切换回调。

```c
/* default_hook.c：默认实现放在自己的翻译单元。 */
int platform_action(void) __attribute__((weak));
int platform_action(void)
{
    return 1; /* 仅返回编号，避免伪造某平台复位寄存器。 */
}
```

```c
/* board_hook.c：需要时把这个强定义也加入链接。 */
int platform_action(void)
{
    return 2;
}
```

调用方通过声明调用同名函数；只有默认对象时选1，默认对象和强定义都参与支持该语义的链接时选2。不能把两段放进同一个C文件当成合法重定义。静态库成员是否被链接器抽取、符号可见性与目标文件格式仍会影响结果；这里只讨论显式参与链接的普通对象。内核的`__weak`是这种属性的封装，真实平台复位还需要硬件接口及错误处理，不能把默认空操作误当成复位成功。依据：[GCC Common Function Attributes，weak](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Common-Function-Attributes.html)。

## 1.3\_类型与关键字扩展

布局解决“数据在哪儿”，但宏还有“实参执行几次”的问题。以取较小值为例，普通三元表达式宏可能先比较a++，随后在选中分支再次执行a++。若该表达式实际是读硬件或取队列元素，重复求值就改变了行为，不能把它视为小小的性能差异。

### 1.3.1\_typeof\_获取表达式类型

`typeof(expr)`取得表达式的类型，使临时变量可以跟随实参；`({ ... })`是GNU语句表达式，它允许在内部声明局部变量并执行语句，最后一个表达式的值成为整个结构的值。两者合用，可以先保存实参，再比较保存值。**避免重复求值靠的是保存动作与后续复用，不是typeof单独具备“只执行一次”的魔法。**

下面的完整程序把布局和求值放进同一个小实验。DEMO_MIN是教学宏，实参限定为普通整数表达式；内部临时名不能与调用者实参中的标识冲突，不是对Linux当前min实现的复制。普通非可变修改类型的typeof操作数不执行运行时求值；变长数组等可变修改类型有例外，不应把这里的整数结论无限外推。

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define DEMO_MIN(x, y) ({                         \
    typeof(x) demo_min_x = (x);                   \
    typeof(y) demo_min_y = (y);                   \
    _Static_assert(__builtin_types_compatible_p(  \
        typeof(demo_min_x), typeof(demo_min_y)),  \
        "DEMO_MIN requires matching types");     \
    demo_min_x < demo_min_y ? demo_min_x : demo_min_y; \
})

struct natural_record { uint8_t tag; uint32_t sequence; };
struct packed_record {
    uint8_t tag;
    uint32_t sequence;
} __attribute__((packed));

/* 只模拟USB描述符前缀的宽度，不声明这是完整USB结构。 */
struct usb_prefix_demo {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t usb_version;
    uint8_t device_class;
} __attribute__((packed));

struct aligned_counter {
    unsigned long count;
} __attribute__((aligned(64)));

int main(void)
{
    int a = 10;
    int result = DEMO_MIN(a++, 20);

    printf("natural: offset=%zu size=%zu\n",
           offsetof(struct natural_record, sequence),
           sizeof(struct natural_record));
    printf("packed: offset=%zu size=%zu\n",
           offsetof(struct packed_record, sequence),
           sizeof(struct packed_record));
    printf("usb prefix: version_offset=%zu size=%zu\n",
           offsetof(struct usb_prefix_demo, usb_version),
           sizeof(struct usb_prefix_demo));
    printf("aligned: alignment=%zu size=%zu\n",
           _Alignof(struct aligned_counter), sizeof(struct aligned_counter));
    printf("minimum=%d next_a=%d\n", result, a);

    if (result != 10 || a != 11 ||
        offsetof(struct packed_record, sequence) != 1 ||
        sizeof(struct packed_record) != 5 ||
        offsetof(struct usb_prefix_demo, usb_version) != 2 ||
        sizeof(struct usb_prefix_demo) != 5 ||
        _Alignof(struct aligned_counter) < 64)
        return 1; /* 检查与执行都不放在可被关闭的断言中。 */
    return 0;
}
```

保存为gnu_syntax_probe.c，或取用[同源材料](../../../../labs/foundations/c_language/materials/gnu_syntax_probe.c)。在该文件所在实验目录，用支持这些GNU扩展的GCC或Clang编译：

```bash
# 当前目录：gnu_syntax_probe.c 所在目录。
cc -std=gnu11 -Wall -Wextra -Werror -O2 gnu_syntax_probe.c -o gnu_syntax_probe
```

这里显式选择gnu11，不以严格ISO C检查模式运行GNU语法。编译成功后执行：

```bash
./gnu_syntax_probe
```

Windows原生工具链生成.exe时，在PowerShell中使用 `./gnu_syntax_probe.exe`。常见x86-64 ABI下，前两行分别是offset=4、size=8和offset=1、size=5；普通结构体数字由目标ABI决定，程序故意没有把它们当跨平台断言。USB前缀version_offset=2，aligned的对齐至少64；最后一行应为minimum=10、next_a=11，并返回0。

练习一，把第二实参20改成20L，预测会发生什么。它与int实参类型不同，本教学宏的静态检查应拒绝编译，不能在运行时再等待报错。练习二，只把DEMO_MIN换成 `#define DEMO_MIN(x, y) ((x) < (y) ? (x) : (y))`，预测a变成12；先比较得到旧值10，再选中a++分支得到11，因此结果也改变。练习三，去掉packed_record的packed属性再观察偏移和size：这是目标布局观察，不代表每个目标都必然插入相同空洞。后两个变体故意破坏原程序的检查前提，在本例x86-64工具链上应返回1，不应保留原成功预期。

语言依据：[GCC Typeof](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Typeof.html)、[Statement Expressions](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Statement-Exprs.html)。Linux当前min宏还要处理常量、符号性与类型规则，不能把这个教学实现直接覆盖内核接口。

### 1.3.2\_container\_of\_通过成员地址反推结构体地址

通用链表只需要知道前后节点地址，业务代码却需要学生编号、请求状态等完整对象。如果每次遍历再维护一张“节点到对象”的查找表，会增加一套同步和寿命责任。内核常把通用节点嵌入业务对象：成员与外层对象有固定布局关系，拿到真实成员地址后即可用偏移还原对象地址。

```mermaid
flowchart LR
    B["业务对象首地址B"] -->|"加成员偏移k"| P["嵌入成员地址P"]
    T["实际结构体类型与成员名"] -->|"offsetof计算布局偏移"| K["偏移k，含编译器填充"]
    P -->|"按字节减去k"| R["还原业务对象地址B"]
    K -->|"提供偏移，不查询运行时登记表"| R
```

关系是 `P = B + k`，所以反向计算为 `B = P - k`，这里按字节计数。`offsetof(type, member)`提供偏移，不能拿前面字段大小简单相加，因为ABI可能插入填充。不要为理解它而真的解引用NULL；某些实现里的 `((type *)0)->member`只参与不求值的类型表达式，不是给运行时空指针访问开绿灯。

```c
/* 内核链表使用片段：假设student_list已正确初始化并受到访问保护。 */
struct student {
    int id;
    char name[20];
    struct list_head list;
};
struct list_head *pos;
struct student *entry;

list_for_each(pos, &student_list) {
    entry = container_of(pos, struct student, list);
    pr_info("ID: %d\n", entry->id);
}
```

list_for_each给出通用节点地址，container_of用实际学生类型和list成员还原业务对象。name仍是普通字段，需要打印时必须保证初始化与字符串终止，成员还原本身不负责验证内容。传入NULL、已释放对象的成员地址或另一个同类型成员都可能违反前提；编译期类型检查不能证明对象还活着，也不能证明同类型成员名没有选错。普通container_of还可能丢失const，固定版本另有container_of_const用于保留只读限定。

这里保留调用者原理，不重复维护版本宏体。固定Linux 6.12.20的定义在include/linux/container_of.h；先从[rbtree源码总阅读索引](../../../../research/source_reading/rbtree/大纲.md#1.1_从查询承诺进入实现)进入版本化证据，再对照[唯一实现中的求值、类型与const边界](../../../../research/source_reading/rbtree/source_explanations/include/linux/container_of.h.md#1.1_一次还原中的求值与类型检查)。

## 1.4\_内核性能提示必须由实际负载验证

前面的工具改变可表达的程序或布局；性能宏则多了一项前提：作者对高频路径的判断必须符合实际负载。写了一个“常见”名字，不会让CPU自动把所有访存都变快。

### 1.4.1\_likely与unlikely提供概率提示

编译器要安排分支和代码布局，但有时缺乏真实输入分布。likely/unlikely向它表达预期真假概率，常见非跟踪分支通过 `__builtin_expect(!!x, expected)`实现；`!!`先把表达式转换成布尔值。这是编译器提示，不是给CPU预测器下达必须命中的命令，也不改变条件真假。

```c
/* 内核片段：错误必须处理，提示只表达已经有依据的频率判断。 */
ptr = kmalloc(size, GFP_KERNEL);
if (unlikely(!ptr))
    return -ENOMEM;
use_object(ptr);
```

低内存或故障注入时，失败可能不再少见；返回错误仍必须正确。布局变化可能减少热路径跳转，也可能因预测判断错误而没有收益；不能承诺“最大化指令缓存命中”。固定内核开启分支跟踪配置后还可能加入统计状态，不能断言这类宏永远没有额外运行成本。依据：[GCC Other Builtins 的 __builtin_expect](https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/Other-Builtins.html)及[固定compiler.h](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/include/linux/compiler.h)。

### 1.4.2\_read\_mostly让布局表达读多写少

一个初始化后很少变动、热路径频繁读取的函数指针，若与高频写计数器放在同一缓存行，读者也可能受到无关写入的一致性影响。`__read_mostly`向支持它的体系结构表达读多写少的布局意图，使这类变量有机会集中放置并避开频繁写区域。

```c
/* 内核片段：初始化后很少修改；真实并发更新仍须同步协议。 */
static void (*irq_handler)(void) __read_mostly;
```

它不意味着只读保护，也不会把一个共享指针更新自动变成安全发布。固定cache.h允许体系结构定义该宏，未定义时回退为空；是否形成专门节、最终怎样排列都要核对目标构建。过度标记还会增大热数据工作集，因此先用负载证据判断，再检查布局，最后测性能；不能仅凭变量名字“读得多”就承诺收益。

## 1.5\_内存访问与静态契约各负责什么

现在再看两个常被归为“编译器优化”的对象：__user描述指针应怎样使用，READ_ONCE约束一次共享访问。它们使用前面建立的宏和类型工具，却承担不同责任，不能相互替代。

### 1.5.1\_地址空间检查\_Sparse\_工具与\_user

内核收到用户缓冲地址时，该地址可能无效、访问可能触发异常，也不属于可以直接随意解引用的内核对象。`__user`是内核注解宏，Sparse这个静态分析器可据此区分指针域和禁止的直接访问；普通编译路径是否保留元数据另受配置影响。它不是硬件权限开关，也不是标准C的const；把标签去掉不会让用户地址变得安全。

下面只演示边界检查和拷贝契约，是进程上下文中的内核辅助函数，不是凭函数名就注册好的系统调用：

```c
static ssize_t copy_request(const char __user *buf, size_t count)
{
    char kernel_buf[100];

    if (count > sizeof(kernel_buf))
        return -EINVAL; /* 先保护目标容量，不能把任意长度交给拷贝。 */
    if (copy_from_user(kernel_buf, buf, count))
        return -EFAULT; /* 返回的是未复制字节数，非零表示没有全部成功。 */

    /* 本例不解析内容，也不把kernel_buf当作已终止的字符串。 */
    return count;
}
```

合法拷贝路径本身也可能处理缺页，所以不能把“发生page fault”一概当成错误；危险的是绕开规定接口和上下文要求。copy_from_user可能睡眠，不应在持有普通自旋锁或不允许睡眠的上下文中照搬。即使返回成功，也只说明这次字节复制完成，不证明内容合法或多个用户字段来自同一个不可变快照。

指针注解会参与分析器的类型传播，错误的赋值和传参可暴露域混用；受控转换仍需真实接口兑现。进一步的消费者、BTF与Sparse分支进入[静态注解专题](../kernel_static_annotations/大纲.md)，实际拷贝返回语义见固定[include/linux/uaccess.h](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/include/linux/uaccess.h)。

### 1.5.2\_READ\_ONCE限制单次访问而不完成同步协议

如果编译器看不到一个普通变量在循环中发生变化，就可能复用先前读值。READ_ONCE让调用者明确表达一次受约束的读取，WRITE_ONCE表达相应写入；它们不是互斥锁，也不因名字里有ONCE就保证任意尺寸、对齐和平台上的硬件原子性。

```c
/* 内核片段：flag对象有效，写方与完整同步协议由调用者建立。 */
while (READ_ONCE(flag) == 0)
    cpu_relax();
```

这段循环每轮请求重新采样，但不承诺生产者一定运行、没有超时，也没有直接唤醒机制。若去掉READ_ONCE且循环体没有阻止优化的作用，普通读取可能被提出循环；**不能把这个退化机械套到所有保留cpu_relax的版本**，因为某些架构的cpu_relax本身带编译器约束。访问次数、发布顺序、等待进展是三项不同保证。

ACCESS_ONCE属于旧接口背景，不作为本版本的新代码选择。单次访问的类型边界和实际汇编对照由[READ/WRITE_ONCE专题](../../../linux/synchronization_and_asynchrony/synchronization/memory_ordering/P02_编译器共享访问与READ_WRITE_ONCE.md#2.1_问题发生在机器指令出现之前)独立讲解；本节只建立它与语法扩展的关系。

## 1.6\_诊断宏不替错误恢复负责

编译期类型断言拒绝生成不合要求的程序，运行时BUG_ON/WARN_ON则检查已经执行到的条件。两者所在阶段不同：后者可能在持锁、设备活动或对象正在使用时触发，因而不能把“输出一条诊断”当成已经完成收尾。

BUG_ON满足条件时进入严重错误路径，可能造成Oops或panic，具体受架构和配置影响。固定版本的弃用说明要求避免新增BUG/BUG_ON，用能够恢复的错误路径处理尽可能多的情况；不能把它推荐成普通输入检查工具。

WARN_ON适合报告设计上不应到达的状态，并返回条件真假；正常策略下通常继续，但panic_on_warn等策略可能使告警触发panic，所以不能承诺系统必定继续。可预期的无效输入和资源失败应返回错误，必要时使用普通日志，而不是滥用WARN作为用户可触发的断言。

```c
/* 内核片段：不变量已由同一把锁保护；此处只演示检查与恢复分工。 */
if (WARN_ON_ONCE(state->completed > state->submitted)) {
    ret = -EIO;
    goto out_unlock; /* 调用方仍须解锁和完成本函数的错误收尾。 */
}
```

字段含义是已完成数不应超过已提交数；如果读取本身不受正确同步保护，先修同步，不能用告警替锁。类似地，`kfree(NULL)`本来就是合法空操作，不应额外加WARN把它宣称为内核错误。业务接口若规定参数非空，应该说明那是该接口自己的契约。

依据为固定版本[deprecated.rst 的 BUG() and BUG_ON()](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/Documentation/process/deprecated.rst)、[kernel sysctl的panic_on_warn](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/Documentation/admin-guide/sysctl/kernel.rst)和[slub.c的kfree空值分支](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/mm/slub.c)。

## 1.7\_按改变的对象选择验证方法

回到开头的陌生声明，现在可以逐层判断：先展开宏，辨认它交给编译器的属性或表达式；再判断改变的是值计算、目标布局、链接选择、分析器类型还是运行时路径。不能用“它是个宏”代替这些不同问题的答案。

| 要核对的结论 | 合适的观察 | 不能据此证明 |
| --- | --- | --- |
| 整数宏只求值一次 | 本章副作用计数实验及展开结果 | 所有VLA、嵌套宏或所有内核min实现都相同 |
| packed/aligned布局 | offsetof、sizeof、_Alignof及目标产物 | 端序正确、访问原子或没有伪共享 |
| 特权寄存器访问 | 目标汇编和体系结构接口契约 | 普通用户程序可运行或共享内存已同步 |
| section/weak | 目标文件节、符号与最终链接结果 | 代码仍存活或某平台复位已成功 |
| __user/READ_ONCE/WARN | 各自的静态检查、访问协议与错误路径 | 仅凭无告警就保证运行时正确 |

最后做一次判断练习：给共享指针加aligned和__read_mostly，再在读取前加WARN_ON，是否已经得到安全发布？没有。前三者分别处理布局意图与异常诊断，仍未建立写入者和读取者之间的数据发布顺序及对象寿命。若需要解释“同一宏为什么给不同工具看不同内容”，下一步进入静态注解专题；若需要解释共享访问的顺序，则进入内存顺序专题。
