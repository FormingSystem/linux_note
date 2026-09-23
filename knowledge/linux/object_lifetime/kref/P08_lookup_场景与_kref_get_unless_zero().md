---
id: knowledge.linux.object_lifetime.kref.p08_lookup_场景与_kref_get_unless_zero
title: "lookup 场景与 kref get unless zero()"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
---

# 第8章\_lookup\_场景与\_kref\_get\_unless\_zero()

## 8.1\_本章定位

上一章把已经拥有的一份引用从创建者交给了队列，再交给消费者。接下来换一个入口：请求只带对象编号，处理者需要到共享登记表中寻找对象。表里保存着地址，可处理者尚未拥有任何一份。**第一次取得自己的引用，该从哪里获得安全保证？**

先回想 P02 的单槽登记模块：登记槽拥有一份，查找在槽锁内追加一份，撤下入口以后旧读者仍可继续访问。本章从这个已成立的小模型出发，解释为何必须把“找到地址”和“取得引用”放在同一个受保护窗口内，再把槽扩展成链表和按编号查询的容器。读者应已理解 P03 的可见性、业务状态与存储寿命，以及 P07 的借用和转交；这里不会把它们重新合并成一个“活着”布尔值。

本章中的 lookup 指 **按入口或编号查找对象**。接口可能只借出临时地址，也可能返回调用者拥有的一份，语义由接口协议决定，不能从单词 lookup 推断。前半章先建立最容易审查的“容器持有一份”协议；只有当该保证不再成立、查找仍可能碰到零计数对象时，才引入条件取得。RCU 的读侧窗口放到后面与 P10 衔接，不把一个临界区函数当作自动回收保护。

## 8.2\_lookup\_的基本边界\_先分清指针\_对象和引用

考虑对象编号 7：创建者已把它放入登记槽，随后归还初始引用。现在计数为 1，这一份属于槽。查找者和撤销者可以在不同 CPU 上执行，查找者想把结果带回锁外处理。

### 8.2.1\_lookup\_为什么是\_kref\_最容易出错的场景

若查找接口只在内部加锁、读槽、解锁，返回后调用者才 get，锁确实保护了“读槽”这个瞬间，却没有保护下一次对对象成员的访问：

```mermaid
sequenceDiagram
    participant L as 查找者
    participant G as 登记槽与槽锁
    participant D as 撤销者
    participant O as 对象存储与计数
    L->>G: 加锁，读出地址，随后解锁
    G-->>L: 仅返回地址，没有新增份额
    D->>G: 加锁清空槽，再解锁
    D->>O: 归还槽的最后一份，1变0并回收
    L->>O: 根据旧地址访问ref，已经越过有效期限
```

这里甚至不需要两次计数更新同时发生。撤销者完全执行完以后，查找者才开始 get，仍然错误。换成条件 get 也必须读取相同的成员地址，不能先验证地址再决定要不要访问它。计数的原子性只规定 **对有效计数存储的操作** 怎样相互排序，不负责让已经释放的分配块重新属于原对象。

若分配器恰好尚未覆盖那块内存，程序可能暂时打印出旧编号；若内存被复用，同一地址又可能装着另一个对象。因此“不崩溃”“编号看起来对”“条件 get 返回成功”均不能替代地址期限的证明。不要为了演示这一点在正常实验里真的访问已释放存储，时序图已经给出了违反前提的确切位置。

### 8.2.2\_裸指针\_有效对象\_有效引用的区别

#### (1)\_裸指针

C 指针变量只保存地址，类型系统没有记下当前路径是否拥有一份 kref。同一个 `struct registry_object *` 可以表示创建者的一份、查找者的一份，也可以只是锁内借出的地址。“裸”在这里表示 **没有随这次返回交付独立引用**，不表示所有裸指针都不可访问：借用窗口内当然可以使用，关键是知道窗口何时结束。

#### (2)\_有效对象

不要把有效压缩成单一状态。查找准备访问 `obj->ref` 时，至少要证明这块存储仍属于该对象、计数字段仍可访问；若准备普通 get，还要证明当前计数为正。前一个条件不蕴含后一个：最后 put 可以已经归零，而清理者正在等待查找者持有的锁，尚不能摘链、回收。8.4 正是为这种窗口准备的。

在本章首先建立的拥有型槽中，这两个证明恰好来自同一协议：查找持槽锁时撤销者不能清槽；槽非空意味着槽的一份还没有被归还，所以对象存储仍在、计数仍正。**不是 mutex 自动保护任意对象，而是所有发布、撤下和归还路径共同赋予这把锁含义。**

对象的业务字段是否可修改是另一个问题。编号在发布前初始化、发布后不变，可以在取得引用后读取；若增加一个可变 `state`，仍须为其读写约定锁或其他同步，kref 不会替字段消除数据竞争。

#### (3)\_有效引用

查找在受保护窗口内成功追加一份以后，才把这份责任交给调用者。窗口可以结束，对象不能因为别的拥有者归还而提前回收；调用者仍须最终 put 或明确转交这份责任。引用没有永久保证“仍登记”“设备仍接受请求”或“字段没有变化”。

| 时刻 | 查找者拥有一份吗 | 为什么现在可访问 | 可以把地址带走吗 |
| --- | --- | --- | --- |
| 锁内刚读出槽 | 否 | 槽持有一份且撤下被同锁排斥 | 尚不能依本次查找带走 |
| 锁内 get 已完成 | 是 | 自己的一份已建立 | 可以按返回契约带走 |
| 锁外且槽已撤下 | 是 | 自己仍未归还 | 可以继续允许的操作，另查业务状态 |
| 归还自己的最后使用份额以后 | 本次责任已结束 | 本次查找不再提供任何保证 | 不得凭旧局部变量继续访问 |

### 8.2.3\_错误模型\_裸\_lookup\_后直接\_get

下面只是一段有意保留的错误接口组合，`find_raw` 表示“内部读槽后就解除保护”，不是可直接调用的内核 API：

```c
/* 错误：返回地址时，保护查找窗口的锁已经释放。 */
obj = find_raw(7);
if (obj)
    kref_get(&obj->ref);
```

修复点不在第二行换函数名，而在接口边界：由查找实现把 get 移到解锁之前，成功后返回带引用结果。另一种可行接口是要求调用者先持锁，`find_locked` 只借出锁内指针，由调用者在同一窗口内决定是否追加一份。两种设计都可以；不能让双方都以为对方负责保护间隙。

请先预测：如果把 get 提前到查找锁内，撤销者先取得锁与查找者先取得锁会分别得到什么？下一节用同一完整模块闭合这两个顺序。

## 8.3\_基础保护模型\_锁保护容器\_kref\_保护生命周期

固定版本接口由[源码总索引](../../../../research/source_reading/kref/navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)进入；普通取得和归零仍沿已有唯一实现核对。本节先保留只有一个槽的登记表，把链表遍历的细节暂时拿开。这样每次增加都能说清新份额属于谁，读者不必一边查链表宏一边猜回收协议。随后增加链表节点，寿命证明保持不变。

### 8.3.1\_正确模型一\_mutex/list\_lookup\_+\_kref\_get()

完整程序来自已引入的 [note_kref_registry.c](../../../../labs/kernel/object_lifetime/materials/note_kref_registry.c)，这里按“首次取得”的问题重新阅读。它没有导出并发入口，所有实验操作在模块初始化中执行；锁展示协议，不能把这次顺序运行称为真实竞争测试。

```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

struct registry_object {
    int id;
    struct kref ref;
};

static DEFINE_MUTEX(registry_lock);
static struct registry_object *registry_entry;
static unsigned int release_calls;

static void registry_release(struct kref *ref)
{
    struct registry_object *obj = container_of(ref, struct registry_object, ref);
    ++release_calls; /* 本实验没有并发回调，记录在对象之外。 */
    kfree(obj);
}

static void registry_put(struct registry_object *obj)
{
    if (obj)
        kref_put(&obj->ref, registry_release);
}

static struct registry_object *registry_create(int id)
{
    struct registry_object *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    obj->id = id;
    kref_init(&obj->ref);
    return obj;
}

/* 调用者持有一份；只允许向空槽发布，成功后槽拥有新增的一份。 */
static int registry_publish(struct registry_object *obj)
{
    int result = 0;
    kref_get(&obj->ref);
    mutex_lock(&registry_lock);
    if (registry_entry)
        result = -EEXIST;
    else
        registry_entry = obj;
    mutex_unlock(&registry_lock);
    if (result)
        registry_put(obj); /* 拒绝后归还预留，调用者原份额不变。 */
    return result;
}

static struct registry_object *registry_lookup(void)
{
    struct registry_object *obj;
    mutex_lock(&registry_lock);
    obj = registry_entry;
    if (obj)
        kref_get(&obj->ref); /* 锁内槽仍持一份，普通 get 有正引用保证。 */
    mutex_unlock(&registry_lock);
    return obj;
}

static void registry_remove(void)
{
    struct registry_object *obj;
    mutex_lock(&registry_lock);
    obj = registry_entry;
    registry_entry = NULL;
    mutex_unlock(&registry_lock);
    registry_put(obj); /* 撤下入口后归还槽那份；release 不再取槽锁。 */
}

static int __init note_registry_init(void)
{
    struct registry_object *creator = registry_create(7);
    struct registry_object *reader;
    int result;
    if (!creator)
        return -ENOMEM;
    result = registry_publish(creator);
    registry_put(creator);
    if (result)
        return result;

    reader = registry_lookup();
    registry_remove();
    if (!reader)
        return -ENOENT;
    pr_info("note_registry: detached reader id=%d\n", reader->id);
    registry_put(reader);
    return 0;
}

static void __exit note_registry_exit(void)
{
    /* 所有操作在 init 中同步完成，没有导出入口或外部使用者。 */
    pr_info("note_registry: release_calls=%u\n", release_calls);
}

module_init(note_registry_init);
module_exit(note_registry_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("容器持有引用与锁内取得实验");
```

仍按统一的 S0～S5 周期查看它。表中计数表示本实验无其他拥有者时的正常数值，不能拿生产系统中的快照数代替责任记录。

| 阶段 | 触发者与写入地址 | 改变前后 | 谁随后读取，何时退出 |
| --- | --- | --- | --- |
| S0 创建 | 创建者写 obj.id 与 obj.ref | 私有对象，初始份额为 1 | 发布前字段已初始化 |
| S1 发布 | 创建者预留一份；持 registry_lock 写 registry_entry | 成功计数 2、槽可见；槽占用则退回预留 | 创建者归还自己的份额后，槽独持 1 |
| S2 取得 | 查找者持同锁读槽，写对象计数 | 非空时 1→2，建立查找者份额 | 解锁后把这一份交给 reader |
| S3 撤下 | 撤销者持锁把槽值移入局部 obj 并清槽 | 可见→不可见；槽的一份暂由撤销者负责 | 新查找读到 NULL；旧 reader 仍持有 |
| S4 归还 | 撤销者解锁后 put 槽的一份 | 2→1；随后 reader 用完再 put | 谁最后归还，谁同步进入回调 |
| S5 回收 | release 更新对象外记录并 kfree | 1→0 后存储退出 | 只读外部记录，不再访问 obj |

```mermaid
flowchart LR
    C[创建者] -->|S1预留并发布| E[registry_entry槽]
    E -->|非空槽拥有一份| R[obj.ref计数]
    L[查找者] -->|S2同锁读槽后get| E
    L -->|取得独立份额，之后put| R
    D[撤销者] -->|S3同锁清槽| E
    D -->|S4在锁外归还槽份额| R
    R -->|最后归还同步调用| F[release回收对象]
```

锁保护槽地址的读写和交付窗口；原子计数保存共享总数。它没有拥有者数组，谁负责哪一份仍由接口协议规定。锁的释放/取得建立槽与初始化数据的同步关系，计数增减不会另给每个读者发一条“对象还活着”的通知。

```mermaid
sequenceDiagram
    participant L as 查找者
    participant G as 槽与registry_lock
    participant D as 撤销者
    participant O as obj.ref
    alt 查找者先获锁
        L->>G: S2持锁读非空槽
        L->>O: get，1变2
        L->>G: 解锁，带自己的份额返回
        D->>G: S3持锁清槽，再解锁
        D->>O: S4归还槽份额，2变1
        L->>O: 使用完put，1变0，进入S5
    else 撤销者先获锁
        D->>G: S3清槽并解锁
        L->>G: S2读到空槽，不访问旧obj
        D->>O: S4归还槽份额，可进入S5
        G-->>L: 返回NULL，无须put
    end
```

第二分支的 put 可以在查找空槽之前或之后发生，两者都安全：查找者已经没有取得旧地址的入口。第一分支则允许对象在“不可再查找”时继续存活，这正是引用与可见性分开的价值。

在配好同版本目标内核构建环境后，从材料目录运行 `make -C "$KDIR" M="$PWD" modules`，其中 KDIR 指向已经配置并准备好目标头文件的构建树，不是任意版本源码目录。将生成模块放到匹配的 Linux 实验环境后执行 `sudo insmod note_kref_registry.ko`，读取日志，再用 `sudo rmmod note_kref_registry` 卸载。正常初始化日志含 `detached reader id=7`，卸载记录 `release_calls=1`。即使登记已撤下，reader 仍可打印编号，因为 S2 已建立自己的份额。

既有验证包括 ARM 前端和显式顺序替身下的成功、空槽、重复发布、重复撤下及分配失败分支；本节复用原程序，不新增另一份实现。目标构建链接、装卸及真实并发尚未执行，不能把上面的预期日志当成本次设备实测。

### 8.3.2\_list\_持有引用的模型

现在把单槽扩展成链表：对象增加一个 `struct list_head node`，表头保存多个节点。节点只是链接，不会自动增加 kref。应用程序仍须在 **每次成功挂入一个拥有型集合时交付一份**，在成功摘下那次成员关系时取回同一份。

与槽不同，链表发布还须拒绝同一节点重复插入、按接口约定处理重复编号；创建时先 `INIT_LIST_HEAD`，不能把全零节点当作合法的自链接空节点。按编号遍历也必须比较 id，不能把“遍历到了一个节点”当作命中。

下面是从完整槽协议迁移到链表时的核心片段，不是一份省去创建和退出的可加载模块。前提是调用者有独立引用、节点只用于这一张表、全部成员变化受同一锁保护、没有未说明的重新发布者：

```c
/* found 初始为 NULL；锁保护链表及其拥有的一份。 */
mutex_lock(&table_lock);
list_for_each_entry(obj, &object_list, node) {
    if (obj->id != id)
        continue;
    kref_get(&obj->ref); /* 找到时集合仍拥有一份，所以计数为正。 */
    found = obj;
    break;
}
mutex_unlock(&table_lock);
/* 非空 found 是本次取得的一份，调用者最终归还或转交。 */
```

普通链表、哈希桶、整数索引都可以采用拥有型协议，但数据结构不会替你实施它。集合维护的是可发现关系，应用维护的是成员引用。换容器以后，锁的覆盖范围、插入失败和重复删除都必须重新核对，不能只替换查找函数名。

### 8.3.3\_remove/unlink\_与\_lookup\_的顺序

拥有型集合的退出顺序是：同锁撤下入口，再归还成员那一份。两个动作之间计数暂时偏高并不危险，因为撤销者接管着待归还责任；反过来先 put 可能直接回收节点，再摘链就要访问失效存储。

还有一个比顺序更隐蔽的错误：`if (!list_empty(...)) list_del_init(...)` 后无条件 put。第一次调用摘链并归还，第二次虽然没有摘链，却仍执行 put，消耗的就可能是调用者或旧读者的一份。**是否归还成员引用必须由这次是否实际移除成员决定。**

```c
/* 片段：调用者持有独立引用；node 属于唯一的 object_list。 */
bool removed = false;
mutex_lock(&table_lock);
if (!list_empty(&obj->node)) {
    list_del_init(&obj->node);
    removed = true;
}
mutex_unlock(&table_lock);
if (removed)
    object_put(obj); /* 只归还本次成功撤下的成员份额。 */
```

该片段的独立调用者引用保证两次 remove 调用时参数都有效；不能第一次已经释放对象，第二次再拿悬挂指针来检验“幂等”。自链接判断也只在本例唯一集合协议下表示成员状态，不提供一般的容器身份验证。若允许再次发布，每次成功发布都要另交一份，每次摘下只归还它自己的那次责任。

完整槽模块通过“锁内取旧槽值并清空”自然实现同样效果：第二次只拿到 NULL，`registry_put(NULL)` 不做事。请做三项练习：先把 lookup 移到 remove 之后，预测为何不再打印编号；再连续 remove 两次，解释为何没有第二次归还；最后仅在纸上把 get 移到解锁之后，补出图中导致失效访问的交错，勿运行错误版本。前两项需同步调整 init 的返回处理，不能把原先刻意检查成功的分支保留成新的预期结果。

到这里证明了“拥有型容器 + 同锁取得”。但有些索引只提供发现关系，不拥有引用，清理者会在最后 put 之后才取锁摘链。此时锁内地址可以仍有效、计数却已经为零。下一节用这一个新约束说明条件取得为什么有必要。

## 8.4\_kref\_get\_unless\_zero()\_防复活\_不防悬挂指针

上一节查找锁内能普通 get，是因为集合的一份尚在。现在考虑另一种设计：链表只是索引，**它不拥有引用**；对象最后一份由外部使用者归还，release 再拿索引锁摘链并回收。这样索引不必另有一条显式归还成员引用的路径，但查找必须面对一个新窗口：最后 put 已经把计数归零，release 正等着索引锁，节点仍在表里。

不要把这两种设计拼起来。拥有型集合“先摘链再归还成员那份”，非拥有索引“最后归零后由回调摘链”；两者的安全依据和 API 选择不同。

### 8.4.1\_kref\_get\_unless\_zero()\_的定位

查找持有索引锁时，回调不能越过这把锁去回收，因此计数地址仍可访问。可最后 put 在锁外执行，查找并不能阻止计数从 1 变成 0。它需要一个不可拆开的决定：**只有当前计数仍非零，才追加自己的一份；否则失败，绝不撤销已开始的清理。** 这就是条件取得的用途。

在正常计数范围内，`kref_get_unless_zero(&obj->ref)` 非零返回表示取得一份，零返回表示没有取得。本接口返回 int，调用者按布尔语义判断；失败后既不能因这次调用而 put，也不能把未拥有的地址带出保护窗口继续访问。

尝试增加不是“先 read 非零，再普通 get”。两步之间可能有最后一次减少。真实条件取得在比较和更新之间不能被插入一次成功的冲突修改；若观察值过时，必须重新判断。P05 已经给出这个机制，下面复用完整 [conditional_take.c](../../../../labs/kernel/object_lifetime/materials/conditional_take.c)，让四条分支在本章问题中重新变得可预测。

```c
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>

enum interference { NONE, DROP_LAST, ADD_OWNER };

/* refs 本身始终在有效存储中；这里只模拟零/正数，不模拟内核饱和。 */
static bool try_take(atomic_uint *refs, enum interference event,
                     unsigned int *attempts)
{
    unsigned int old = atomic_load_explicit(refs, memory_order_relaxed);
    *attempts = 0;
    while (old != 0) {
        assert(old < UINT_MAX);
        if (*attempts == 0 && event != NONE) {
            /* 在第一次观察与比较之间，显式安排另一条路径先改变计数。 */
            atomic_store_explicit(refs, event == DROP_LAST ? 0u : 2u,
                                  memory_order_relaxed);
        }
        ++*attempts;
        if (atomic_compare_exchange_strong_explicit(refs, &old, old + 1,
                memory_order_relaxed, memory_order_relaxed))
            return true;
        /* 比较失败已把 old 更新为当前值，下一轮必须重新检查它是否为零。 */
    }
    return false;
}

int main(void)
{
    const unsigned int initial[] = {0, 1, 1, 1};
    const enum interference events[] = {NONE, NONE, DROP_LAST, ADD_OWNER};
    const unsigned int expected_count[] = {0, 2, 0, 3};
    const unsigned int expected_attempts[] = {0, 1, 1, 2};
    const bool expected_result[] = {false, true, false, true};
    for (unsigned int path = 0; path < 4; ++path) {
        atomic_uint refs;
        atomic_init(&refs, initial[path]);
        unsigned int attempts;
        bool taken = try_take(&refs, events[path], &attempts);
        unsigned int count = atomic_load_explicit(&refs, memory_order_relaxed);
        assert(taken == expected_result[path]);
        assert(count == expected_count[path]);
        assert(attempts == expected_attempts[path]);
        printf("path=%u taken=%u count=%u attempts=%u\n",
               path, taken ? 1u : 0u, count, attempts);
    }
    return 0;
}
```

这是独立 C11 教学模型。`refs` 始终是仍有效的自动变量；模型不分配/释放对象，不实现 Linux 饱和标记，也不启动其他线程。`event` 在观察和比较之间顺序插入一个状态变化，模拟需要处理的交错。`atomic_compare_exchange_strong_explicit` 比较失败会把当前值写回 old，这正是下一轮判断所需的信息。

在材料目录运行：

```bash
cc -std=c11 -Wall -Wextra -Werror -O2 conditional_take.c -o conditional_take
./conditional_take
```

输出应为：

```text
path=0 taken=0 count=0 attempts=0
path=1 taken=1 count=2 attempts=1
path=2 taken=0 count=0 attempts=1
path=3 taken=1 count=3 attempts=2
```

先只比较路径 1 和 2：同样先读到 1，路径 2 在首次比较前被改成 0，所以比较失败、old 更新为 0，循环结束，绝不重试成 1。路径 3 则把计数改成 2，第一次比较同样失败，但下一次可以把 2 改成 3。**比较失败不等于取得最终失败**；只有重新看到零，才走本模型的拒绝分支。

模型中的 assert 检查预期，编译时不要加 NDEBUG。运行结果不证明任意地址有效，也不证明 ARM 上的全部排序行为；这里观察的只是比较、重试与零值退出。

### 8.4.2\_kref\_get\_unless\_zero()\_不解决什么

条件操作的第一步仍然要读取计数。若返回的 obj 已释放，`&obj->ref` 指向的存储就没有本次协议保证，函数根本没有机会先安全地询问“这个地址还能不能读”。改成条件取得，或者先读一次编号、标志位，均不能修复这一点。

即使地址没有失效，成功也只增加寿命责任。对象可能已停止接单；业务字段可能由另一个锁保护；地址复用以后编号可能代表另一代对象。这些分别需要业务状态、字段同步和身份协议。特别是内存已被复用成新对象时，某个非零计数不能证明它还是你最初寻找的那个对象。

本章的正常返回契约也不覆盖引用计数损坏。固定 6.12.20 的条件链遇到异常值或溢出会走饱和诊断，可能仍返回非零；它不是一个“true 代表对象完全健康”的检查器。普通 get 同样有零值/溢出诊断，不能以“有诊断”推导允许从零取得。具体语句沿[源码总索引](../../../../research/source_reading/kref/navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)进入[条件取得实现](../../../../research/source_reading/kref/source_explanations/include/linux/refcount.h.md#1.5_条件增加与失败重试)和[普通增加的异常分支](../../../../research/source_reading/kref/source_explanations/include/linux/refcount.h.md#1.2_普通增加与异常检测)。

### 8.4.3\_kref\_get\_unless\_zero()\_仍然需要锁或\_RCU

把本节非拥有索引的一轮过程接回 S0～S5：S0 创建初始引用，S1 只建立索引关系、不追加索引份额；S2 查找持锁并条件取得；S4 最后归还可能在锁外发生；S5 回调必须先取相同索引锁，摘链后才回收。这里 S2 与 S4/S5 可以交叠，所以它不是“单一 LIVE 状态”能完整描述的对象。

```mermaid
flowchart LR
    L[查找者] -->|S2持有| M[索引mutex与链表关系]
    L -->|读与条件比较更新| C[obj.ref计数]
    P[外部引用持有者] -->|S4锁外最后put| C
    C -->|归零同步进入| R[release]
    R -->|S5必须取得同锁才能摘链| M
    R -->|摘链并解锁后| F[回收对象存储]
```

```mermaid
sequenceDiagram
    participant L as 查找者
    participant P as 最后外部持有者
    participant M as 索引mutex
    participant C as 对象计数
    L->>M: S2持锁找到节点
    alt 条件增加先完成
        L->>C: 1变2，取得自己的份额
        P->>C: S4归还原份额，2变1，不进回调
        L->>M: 解锁并带引用返回
    else 最后归零先完成
        P->>C: S4将1变0，进入release
        P->>M: S5请求同锁，必须等待
        L->>C: 读到零或比较失败后见零，不取得
        L->>M: 解锁，返回NULL
        P->>M: 获锁并摘链，然后解锁
        P->>P: 回收存储
    end
```

图中查找者没有通知计数“等待我”，也没有给每个拥有者发消息。保护地址的成本由 mutex 的互斥与等待承担；计数竞争由共享原子比较承担。release 中拿这把可睡眠的锁还约束了最后 put 的上下文，不能把这套示例直接搬到中断回调。调用普通 put 时也不能已经持有同一 mutex，否则最后一份触发的回调会递归等待自己。

RCU 可以提供另一种读侧窗口，但必须先有匹配的发布/摘除和延迟回收协议，且窗口覆盖条件取得；仅写 `rcu_read_lock()` 不会让任意对象免于被 kfree。P10 再比较具体回收排序，本节不把两种协议混成可互换的锁函数。

### 8.4.4\_什么时候用\_kref\_get()\_什么时候用\_kref\_get\_unless\_zero()

#### (1)\_可以确认\_refcount\_一定非\_0\_用\_kref\_get()

已有独立引用，或者查找锁内仍有容器拥有的一份，就已经获得正计数保证。此时普通 get 表达“在已有责任基础上增加一份”。P05 的另一种非拥有索引通过 `kref_put_mutex` 把最后减少也纳入同锁，锁内找到成员同样可以证明正数；不要仅按“容器是否拥有”这一个标签选 API。

#### (2)\_可能看到正在退出的对象\_用\_kref\_get\_unless\_zero()

准确条件是 **计数存储仍有效，但最后归零不被当前保护窗口排除**。本节锁外最后减少、回调内取锁摘链就是实例。“正在退出”单独不够精确：一个已停止业务却仍有旧用户份额的对象，计数可以明确为正；选条件 get 也不会自动拒绝其业务。

#### (3)\_判断表

| 已经证明的条件 | 取得选择 | 仍须另外证明什么 |
| --- | --- | --- |
| 调用者已有一份 | 普通 get | 追加用途及归还责任 |
| 拥有型容器的成员在同锁内 | 普通 get | 插入交付一份、摘下才归还 |
| 非拥有索引，最后减少也与查找同锁串行 | 普通 get 可成立 | 所有可能最后归还的路径都遵守该协议 |
| 非拥有索引，最后减少在锁外，回调拿查找锁才回收 | 条件 get | 地址窗口、失败不 put、回调上下文 |
| 正确延迟回收的 RCU 窗口，可能见到零 | 条件 get | 发布可见性、身份及回收排序 |
| 无法证明地址期限 | 两种都不能用 | 先修复外层协议 |

条件 get 可以在拥有型容器里使用，但正常失败本不应发生。若它真的失败，不能安慰自己“已安全处理不存在”：那可能说明本该由容器持有的份额已经被错误消耗，需要调查协议。不额外增加诊断分支也可以，普通 get 的前提本来就由程序设计证明。

### 8.4.5\_mutex\_+\_list\_+\_kref\_get\_unless\_zero()\_模板

下面只展示配对的关键函数；创建、初始引用交付和节点发布须按本节非拥有索引协议完成。这是接口片段，不是另一份完整模块。所有节点变化使用 `index_lock`，发布前完成初始化；索引从不持有一份；全部普通 put 在未持此锁且可睡眠的上下文执行。不要把上一节拥有型 remove 再接到这里，两个退出者会争夺同一个节点。

```c
/* 借助同锁阻止回调摘链回收，但允许计数在锁外归零。 */
static struct indexed_object *lookup_get(int id)
{
    struct indexed_object *obj, *found = NULL;
    mutex_lock(&index_lock);
    list_for_each_entry(obj, &object_index, node) {
        if (obj->id != id)
            continue;
        if (kref_get_unless_zero(&obj->ref))
            found = obj;
        break; /* 本例要求编号唯一，零计数候选不再交付。 */
    }
    mutex_unlock(&index_lock);
    return found;
}

/* 仓库示意回调：最后普通put同步调用；调用者此时不得持index_lock。 */
static void indexed_release(struct kref *ref)
{
    struct indexed_object *obj = container_of(ref, struct indexed_object, ref);
    mutex_lock(&index_lock);
    list_del(&obj->node); /* 唯一摘链者；不归还所谓的索引引用。 */
    mutex_unlock(&index_lock);
    kfree(obj);
}
```

固定 NXP 文档中的该协议由[条件取得模块](../../../../research/source_reading/kref/navigation/P03_条件取得与查找窗口导读.md#3.2_从观察到自己持有)组织，具体 kref 包装只在[唯一实现](../../../../research/source_reading/kref/source_explanations/include/linux/kref.h.md#1.7_有效地址上的条件取得)展开。上面的应用函数名是本章示意，不冒充上游同名函数。

做两个推理练习：如果把 kfree 移到回调拿锁之前，图中哪一步首先失效？如果保持原回调，却让查找失败后也 put，会消耗哪一份？答案分别是查找锁不再保障计数地址，以及失败没有交付任何份额可供归还。最后回看四条 C 输出：能重试成功只说明尚有正计数，并没有给你一个新的业务许可。

下一节保留已经讲清的拥有型协议，把单槽换成哈希桶和整数映射；重点将变成具体容器接口怎样维持同一取得窗口。

## 8.5\_常见容器\_lookup\_模板

查找协议已经成立，为什么还要逐个看容器？因为“锁内查找并 get”只说出了应用需要的窗口，具体函数是否自行加锁、是否暂时解锁分配、何时发布字段，都要按接口兑现。本节继续使用 **容器拥有一份** 的设计，不把 8.4 的非拥有回调摘链协议混进来。

链表要逐个比较编号；哈希表先按编号分桶，再在候选中比较键；XArray 用整数索引定位条目；IDR 在整数范围内分配可用编号并关联指针。它们解决查找或编号问题，均不会自动知道对象里有没有 kref。以下仅讨论普通对象指针、单一登记关系和进程上下文，不把值编码条目、IRQ 使用方式或多索引关系隐含加入示例。

### 8.5.1\_hash\_table\_lookup\_的引用规则

哈希表的桶可能有多个候选，所以命中桶以后仍要比较 id。沿前面单槽协议，把全局入口改成 `DEFINE_HASHTABLE(object_table, 8)`，对象增加初始化好的 `struct hlist_node hnode`；这里 8 是桶索引位数，即 256 个桶。用一把 `table_lock` 保护所有桶，便于先审查整个协议；这不是每桶锁的并发扩展实现。

下面三个函数构成配对片段。`hash_object` 具有 id、hnode、ref，创建时初始化 hnode，编号发布后不变；`hash_put` 归还一份，release 只清理已经摘下的对象。调用 publish/remove 的路径都必须另持独立引用。plain spinlock 只适用于本例没有中断侧操作同一表的约定。

```c
/* 成功让表持有新的一份；拒绝重复节点或编号时退回预留。 */
static int hash_publish(struct hash_object *obj)
{
    struct hash_object *candidate;
    int result = -EEXIST;
    kref_get(&obj->ref);
    spin_lock(&table_lock);
    if (!hlist_unhashed(&obj->hnode))
        goto out_unlock;
    hash_for_each_possible(object_table, candidate, hnode, obj->id) {
        if (candidate->id == obj->id)
            goto out_unlock;
    }
    hash_add(object_table, &obj->hnode, obj->id);
    result = 0;
out_unlock:
    spin_unlock(&table_lock);
    if (result)
        hash_put(obj);
    return result;
}

static struct hash_object *hash_lookup_get(u32 id)
{
    struct hash_object *obj, *found = NULL;
    spin_lock(&table_lock);
    hash_for_each_possible(object_table, obj, hnode, id) {
        if (obj->id == id) {
            kref_get(&obj->ref);
            found = obj;
            break;
        }
    }
    spin_unlock(&table_lock);
    return found;
}

static void hash_remove(struct hash_object *obj)
{
    bool removed = false;
    spin_lock(&table_lock);
    if (!hlist_unhashed(&obj->hnode)) {
        hash_del(&obj->hnode);
        removed = true;
    }
    spin_unlock(&table_lock);
    if (removed)
        hash_put(obj); /* 仅归还本次实际撤下的成员份额。 */
}
```

同一节点只能属于本例这一张表；unhashed 不是跨表身份检查。这里的删除会恢复未挂接状态，因此重复 remove 不再消耗表引用；但参数有效性仍由调用者的独立份额保证。插入拒绝也不能在持 spinlock 时随意执行复杂回调，示例把预留归还放到解锁后。

桶和节点机制沿[哈希表专题](../../data_structures/哈希表_Hash_Table/大纲.md)继续阅读。本节新增的是成员引用和查找窗口，而不是另一套哈希算法。全局锁的代价是所有桶的增删查找都串行；只有实测表明该锁成为瓶颈、且能维护相同寿命和编号规则时，才进一步考虑按桶拆锁。

### 8.5.2\_xarray\_lookup\_的引用规则

XArray 可以按整数索引保存指针。它有内部锁，但不同 API 的锁覆盖范围不一样：`xa_insert` 和 `xa_erase` 自己取得/释放 xa_lock；`xa_load` 的内部读侧窗口在返回前结束，并没有交付对象引用。因此本例 **查找必须在外层 xa_lock 内把 load 和 get 连起来，删除却直接调用自行加锁的 xa_erase**。

下面是完整 [note_kref_xarray.c](../../../../labs/kernel/object_lifetime/materials/note_kref_xarray.c)。它把上一节单槽实验换成整数索引 7，并故意撤下两次。XArray 中只放非空普通对象地址，不放编码值或保留条目；所有对象字段在发布前完成初始化。

```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/xarray.h>

struct indexed_object {
    unsigned long id; /* 发布前写入，之后不再改变。 */
    struct kref ref;
};
static DEFINE_XARRAY(object_index);
static unsigned int release_calls; /* 本实验初始化内顺序完成全部操作。 */

static void indexed_release(struct kref *ref)
{
    struct indexed_object *obj = container_of(ref, struct indexed_object, ref);
    ++release_calls;
    kfree(obj);
}

static void indexed_put(struct indexed_object *obj)
{
    if (obj)
        kref_put(&obj->ref, indexed_release);
}

static struct indexed_object *indexed_create(unsigned long id)
{
    struct indexed_object *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    obj->id = id;
    kref_init(&obj->ref);
    return obj;
}

/* 调用者持有一份；成功让映射拥有新增份额，失败退回预留。 */
static int indexed_publish(struct indexed_object *obj)
{
    int result;
    kref_get(&obj->ref);
    result = xa_insert(&object_index, obj->id, obj, GFP_KERNEL);
    if (result)
        indexed_put(obj);
    return result;
}

static struct indexed_object *indexed_lookup(unsigned long id)
{
    struct indexed_object *obj;
    xa_lock(&object_index);
    obj = xa_load(&object_index, id);
    if (obj)
        kref_get(&obj->ref); /* 映射尚在，同一 xa_lock 排斥删除。 */
    xa_unlock(&object_index);
    return obj;
}

static void indexed_remove(unsigned long id)
{
    /* xa_erase 自行加锁；返回被摘下条目的份额，不再套一层 xa_lock。 */
    struct indexed_object *obj = xa_erase(&object_index, id);
    indexed_put(obj); /* 解锁以后归还；空映射不产生第二次归还。 */
}

static int __init note_index_init(void)
{
    struct indexed_object *creator = indexed_create(7), *reader;
    int result;
    if (!creator)
        return -ENOMEM;
    result = indexed_publish(creator);
    indexed_put(creator);
    if (result) {
        xa_destroy(&object_index); /* 清理索引内部节点，不代替对象 put。 */
        return result;
    }
    reader = indexed_lookup(7);
    indexed_remove(7);
    indexed_remove(7); /* 第二次查无条目，不再消耗引用。 */
    xa_destroy(&object_index); /* 本例映射已空，且没有外部入口。 */
    if (!reader)
        return -ENOENT;
    pr_info("note_index: detached reader id=%lu\n", reader->id);
    indexed_put(reader);
    return 0;
}

static void __exit note_index_exit(void)
{
    pr_info("note_index: release=%u\n", release_calls);
}

module_init(note_index_init);
module_exit(note_index_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("XArray拥有型查找与重复撤下实验");
```

沿 S0～S5 数一次责任：创建为 1；发布前预留后为 2；成功发布让新增一份归映射所有；创建者归还后为 1；查找锁内 get 后为 2；第一次删除交回成员份额，锁外 put 后为 1；第二次返回 NULL，没有份额可归还；reader 最后 put 才回收。`xa_destroy` 只清理索引内部资源，不会替应用逐个调用 indexed_put，所以调用它前已经把本例唯一映射摘下。

为什么不能照旧写 `xa_lock; xa_erase; xa_unlock`？外层已经持锁，xa_erase 又尝试取得同一锁，会造成重复加锁。若调用者确实需要一个更大的临界区，固定版本另有要求已持锁的 `__xa_erase`；不能靠函数名前有下划线就猜所有 API 的行为，必须核对契约。

插入可能因相同索引已占用返回 -EBUSY，也可能因内部节点分配失败返回 -ENOMEM。两种失败都退回预留、保留调用者原份额。允许分配的底层插入可暂时释放再取得 xa_lock，这不是随意修改已发布对象字段的窗口；本例发布前字段就已固定，不依赖整个分配过程始终持锁来掩护半成品。

从材料目录按 8.3 的 KDIR 构建模块，在匹配目标加载 `note_kref_xarray.ko`，正常初始化预期打印 `detached reader id=7`，卸载预期 `release=1`。本轮 ARM 前端及宿主六组路径已通过；宿主执行实际应用和固定 xa_insert/xa_load/xa_erase 包装，存储节点、锁、RCU、分配及原子是显式顺序替身。目标构建链接、装卸、真实 XArray 节点分配与并发未执行，预期日志不作为设备实测。

源码阅读先从[总索引](../../../../research/source_reading/kref/navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)进入[整数索引模块](../../../../research/source_reading/kref/navigation/P06_整数索引与拥有型查找导读.md#6.2_把容器动作接到引用周期)，再分别核对[插入包装](../../../../research/source_reading/kref/source_explanations/include/linux/xarray.h.md#1.1_插入包装自行管理锁)、[查询窗口](../../../../research/source_reading/kref/source_explanations/lib/xarray.c.md#1.1_查询内部读侧窗口在返回前结束)与[删除包装](../../../../research/source_reading/kref/source_explanations/lib/xarray.c.md#1.2_删除包装与已持锁入口)。这些结论限于记录的固定 NXP Linux 6.12.20。

### 8.5.3\_idr\_lookup\_的引用规则

IDR 除了关联指针，还能从指定范围分配一个未使用的编号。这多出一个初始化问题：只有分配成功才知道编号，但把对象挂入索引以后其他路径就可能查到它。本例选择一把外层 mutex 保护所有分配、查找和移除，**在分配成功后、解除这把锁之前写完 obj->id**。

以下是从完整登记协议迁移的配对片段。`id_object` 包含 int id 和 kref ref，创建时其他字段初始化完成；`id_put` 归还引用，release 回收已经撤下的对象。`DEFINE_IDR(object_ids)` 与 `DEFINE_MUTEX(id_lock)` 为唯一索引和锁。publish 只接受尚未发布的私有对象且每个对象仅成功发布一次；所有读写方都遵守该 mutex，不混用无锁 RCU 读者。

```c
/* 返回新编号或负错误；调用者原份额始终保留。 */
static int id_publish(struct id_object *obj)
{
    int id;
    kref_get(&obj->ref);
    mutex_lock(&id_lock);
    id = idr_alloc(&object_ids, obj, 0, 0, GFP_KERNEL);
    if (id >= 0)
        obj->id = id; /* 必须在解锁以前完成，查找者才不会先看到半成品。 */
    mutex_unlock(&id_lock);
    if (id < 0)
        id_put(obj);
    return id;
}

static struct id_object *id_lookup_get(int id)
{
    struct id_object *obj;
    mutex_lock(&id_lock);
    obj = idr_find(&object_ids, id);
    if (obj)
        kref_get(&obj->ref);
    mutex_unlock(&id_lock);
    return obj;
}

static void id_remove(int id)
{
    struct id_object *obj;
    mutex_lock(&id_lock);
    obj = idr_remove(&object_ids, id);
    mutex_unlock(&id_lock);
    if (obj)
        id_put(obj); /* 只有返回了旧映射才有成员份额。 */
}
```

idr_alloc 的范围下界包含 0，上界参数 0 在该接口中表示可用至 INT_MAX；不是“只能分配编号零”。0 是成功编号，判断必须用 `id < 0`，不能写成 `if (id)`。内存不足和编号耗尽分别可能返回 -ENOMEM、-ENOSPC。

固定实现的 idr_alloc 将新编号写进自己的局部变量，并不会找到应用的 obj->id 字段。`idr_alloc_u32` 则允许传入一个 u32 编号地址，并在发布指针前写它；若使用该接口须重新约定类型与范围。两种做法都可建立完整协议，不能把其中一个的保证套在另一个函数上。实现见[编号发布](../../../../research/source_reading/kref/source_explanations/lib/idr.c.md#1.1_返回编号与对象字段初始化)及[查询/移除](../../../../research/source_reading/kref/source_explanations/lib/idr.c.md#1.2_查询与移除不管理对象引用)。

编号也会复用。如果另一个对象后来得到同一 id，按 id 删除会作用于当时的映射，不保证还是最早那个对象；需要防止旧请求误操作新对象时，须增加代际或验证映射身份。该问题与普通 get、条件 get 的选择不同。退出时先阻止新操作、撤下并归还所有成员份额，再销毁索引内部资源；不能用 idr_destroy 代替对象回收。本节 IDR 片段已作源码契约核对，不声称运行过完整 IDR 模块。

### 8.5.4\_lookup\_成功\_失败\_正在释放的状态表

先确定本次接口承诺返回一份，再看返回结果。下表的成功指 lookup_get 的正常成功，不适用于只借出指针的 find_locked。

| 场景 | 可见性与计数依据 | 本次结果和责任 |
| --- | --- | --- |
| 拥有型映射仍在，同锁查找 | 条目持有一份，地址有效且计数正 | 追加一份，返回者最终归还 |
| 没有该编号 | 未得到对象地址 | 返回 NULL，不 put |
| 已摘下但旧用户仍持有 | 新查找无入口，计数可以仍正 | 新查找失败，旧用户继续按其协议使用 |
| 非拥有索引仍在，回调等查找锁 | 地址有效，计数可能已零 | 条件取得成功才带走；失败没有份额 |
| RCU 读者保留了被摘除的旧指针 | 依赖完整延迟回收协议，计数可能为零 | 依协议条件取得；不能把逻辑摘除等同旧指针立刻消失 |
| 容器损坏或寿命协议已被破坏 | 地址与引用均无可信保证 | 不把某次返回值当作修复或验证手段 |

请对照完整 XArray 程序预测三件事：把 lookup 移到第一次 remove 后，会得到 NULL；保留原 lookup，再移除两次，reader 的一份仍在；让第二个对象使用相同编号发布，失败者只退预留，不影响表里原对象。前两者检验窗口，第三者检验失败责任，不需要靠制造 UAF 来证明它们。

如果接口只想在锁内读一个不可变编号，也可以直接返回编号副本而不追加引用。若要把对象带到锁外、保存或交给异步路径，才需要明确的一份或另一种完整借用协议。下一节把这些区别写进函数契约，让调用者无需猜测返回值的责任。

## 8.6\_lookup\_API\_契约\_返回裸指针还是返回引用

这一组内容把 lookup 的接口语义收住。

lookup API 最怕名字含糊：

```text
返回的是裸指针，还是带引用对象？
调用者能否在锁外保存？
调用者是否必须 put？
失败是没找到，还是对象正在退出？
```

因此建议把 `find_locked()`、`lookup_get()`、`tryget()` 这几类接口明确拆开。

### 8.6.1\_lookup\_raw()\_和\_lookup\_get()\_必须分开

建议工程里明确区分两类函数。

#### (1)\_lookup\_raw()\_只返回临时裸指针

```c
static struct my_obj *my_obj_lookup_raw_locked(int id)
{
	struct my_obj *obj;

	lockdep_assert_held(&my_obj_lock);

	list_for_each_entry(obj, &my_obj_list, node) {
		if (obj->id == id)
			return obj;
	}

	return NULL;
}
```

这个函数的语义是：

```text
只能在 my_obj_lock 持有期间调用；
返回值不能逃出临界区；
调用者不能保存；
调用者不能 put；
调用者不能异步传递。
```

适合命名：

```c
my_obj_lookup_raw_locked()
my_obj_find_locked()
my_obj_peek_locked()
```

------

#### (2)\_lookup\_get()\_返回带引用对象

```c
struct my_obj *my_obj_lookup_get(int id)
{
	struct my_obj *obj;

	mutex_lock(&my_obj_lock);

	obj = my_obj_lookup_raw_locked(id);
	if (obj)
		kref_get(&obj->ref);

	mutex_unlock(&my_obj_lock);

	return obj;
}
```

这个函数的语义是：

```text
返回 NULL：没有获得引用；
返回非 NULL：调用者获得引用，必须 put。
```

适合命名：

```c
my_obj_lookup_get()
my_obj_find_get()
my_obj_get_by_id()
```

这类函数必须在注释里写清楚：

```text
Return object with a reference held.
Caller must drop it with my_obj_put().
```

中文就是：

```text
返回成功时，调用者持有对象引用；
使用结束必须 put。
```

------

### 8.6.2\_lookup\_get()\_的标准注释

建议写成：

```c
/**
 * my_obj_lookup_get - find object by id and take a reference
 * @id: object id
 *
 * Returns the object with a reference held on success.
 * The caller must drop the reference with my_obj_put().
 *
 * Returns NULL if no live object is found.
 */
struct my_obj *my_obj_lookup_get(int id);
```

如果使用 `kref_get_unless_zero()`，可以写得更明确：

```c
/**
 * my_obj_lookup_get - find live object by id and take a reference
 * @id: object id
 *
 * The lookup is protected by my_obj_lock.
 * If a matching object is found and its reference count is non-zero,
 * this function returns it with a reference held.
 *
 * Returns NULL if the object is not found or is no longer live.
 */
struct my_obj *my_obj_lookup_get(int id);
```

中文说明：

```text
查找成功并不只是找到指针；
查找成功表示当前路径已经获得一份引用。
```

------

### 8.6.3\_lookup\_后的调用者规则

如果函数名是：

```c
obj = my_obj_lookup_get(id);
```

调用者必须按“持有引用”处理：

```c
obj = my_obj_lookup_get(id);
if (!obj)
	return -ENOENT;

ret = do_something(obj);

kref_put(&obj->ref, my_obj_release);
return ret;
```

如果中间有多个错误路径，要保证每条路径都 put：

```c
obj = my_obj_lookup_get(id);
if (!obj)
	return -ENOENT;

ret = prepare(obj);
if (ret)
	goto out_put;

ret = run(obj);
if (ret)
	goto out_put;

out_put:
	kref_put(&obj->ref, my_obj_release);
	return ret;
```

如果要 handoff 给异步路径，有两种选择。

#### (1)\_给异步路径新引用

```c
obj = my_obj_lookup_get(id);
if (!obj)
	return -ENOENT;

ret = my_obj_schedule_work_ref(obj);

kref_put(&obj->ref, my_obj_release);
return ret;
```

这里：

```text
lookup_get 给当前路径一份引用；
schedule_work_ref 给 work 一份引用；
当前路径最后 put 自己的引用。
```

------

#### (2)\_把\_lookup\_得到的引用直接转移给异步路径

```c
obj = my_obj_lookup_get(id);
if (!obj)
	return -ENOENT;

ret = my_obj_schedule_work_take(obj);
if (ret) {
	kref_put(&obj->ref, my_obj_release);
	return ret;
}

/*
 * 成功后 work 接管 lookup 得到的引用。
 * 当前路径不能再访问 obj。
 */
return 0;
```

这里：

```text
lookup_get 得到的引用没有在当前路径 put；
而是成功转移给 work；
失败时当前路径仍负责 put。
```

这就是第 7 章 handoff 模型和本章 lookup 模型的组合。

------

### 8.6.4\_lookup\_函数不要返回\_可能要\_put\_的对象

最差的接口是这种：

```c
struct my_obj *my_obj_lookup(int id);
```

但它没有说明：

```text
返回对象是否带引用？
调用者是否要 put？
调用者能否保存？
是否只能持锁使用？
失败路径怎么处理？
```

这种接口很容易导致两类 bug。

第一类：调用者以为返回带引用，结果其实没有。

```c
obj = my_obj_lookup(id);
queue_work_with_obj(obj);     /* 可能 UAF */
```

第二类：调用者以为需要 put，结果其实只是借用。

```c
obj = my_obj_lookup(id);
kref_put(&obj->ref, my_obj_release);   /* 可能提前释放 */
```

所以建议把接口拆开：

```c
my_obj_find_locked();     /* 裸指针，只能锁内用 */
my_obj_lookup_get();      /* 返回带引用对象 */
my_obj_put();             /* 释放引用 */
```

不要写一个语义含糊的 `lookup()` 让调用者猜。

------

## 8.7\_退出\_状态和\_RCU\_边界

这一组内容处理 lookup 和对象退出阶段的关系。

对象“还没释放”和“允许新用户进入”不是一回事：

```text
对象可能仍有旧引用，但已经不允许新的 lookup 成功；
对象可能仍被 RCU 读侧看到，但不能被重新复活；
对象从容器撤销可见性，必须和最后 put、延迟释放配套。
```

### 8.7.1\_释放路径必须和\_lookup\_路径配套

lookup 正确性不是 lookup 函数自己能单独保证的。

它还依赖释放路径是否配套。

#### (1)\_正确释放路径

```c
void my_obj_destroy(struct my_obj *obj)
{
	mutex_lock(&my_obj_lock);

	if (!list_empty(&obj->node))
		list_del_init(&obj->node);

	mutex_unlock(&my_obj_lock);

	kref_put(&obj->ref, my_obj_release);
}
```

lookup：

```c
struct my_obj *my_obj_lookup_get(int id)
{
	struct my_obj *obj;

	mutex_lock(&my_obj_lock);

	obj = my_obj_find_locked(id);
	if (obj)
		kref_get(&obj->ref);

	mutex_unlock(&my_obj_lock);

	return obj;
}
```

这两个路径配套的原因是：

```text
lookup 和 unlink 都被同一把锁序列化；
lookup 在锁内看到 obj 时，unlink 不可能同时完成；
obj 仍然有集合引用；
所以 kref_get 安全。
```

------

#### (2)\_错误释放路径

```c
void my_obj_destroy_bad(struct my_obj *obj)
{
	kref_put(&obj->ref, my_obj_release);

	mutex_lock(&my_obj_lock);
	list_del_init(&obj->node);
	mutex_unlock(&my_obj_lock);
}
```

这个释放路径会破坏 lookup 假设。

因为 lookup 可能在 list 中看到一个已经 release 的对象。

所以 lookup 的正确性要问：

```text
所有删除路径是否都先 unlink，再 put？
所有 lookup 是否都在同一保护机制下完成？
所有能释放对象的路径是否都遵守这个顺序？
```

只要有一条路径破坏规则，lookup 就不安全。

------

### 8.7.2\_对象状态和\_lookup\_的关系

有时候对象虽然还没释放，但已经不应该被新的 lookup 获得。

例如状态机：

```c
enum my_obj_state {
	OBJ_LIVE,
	OBJ_DYING,
	OBJ_DEAD,
};
```

这时 lookup 不只要判断：

```text
对象是否在集合中；
refcount 是否非 0。
```

还要判断：

```text
对象状态是否允许新用户进入。
```

示例：

```c
struct my_obj *my_obj_lookup_get_live(int id)
{
	struct my_obj *obj = NULL;

	mutex_lock(&my_obj_lock);

	obj = my_obj_find_locked(id);
	if (!obj)
		goto out;

	if (obj->state != OBJ_LIVE) {
		obj = NULL;
		goto out;
	}

	kref_get(&obj->ref);

out:
	mutex_unlock(&my_obj_lock);
	return obj;
}
```

这里状态检查必须和 lookup 保护在同一个临界区里。

否则可能出现：

```text
CPU0: 查到 obj 状态是 LIVE
CPU1: 把 obj 改成 DYING 并删除
CPU0: get
```

如果状态和集合都由 `my_obj_lock` 保护，则可以保证状态判断和 get 是一致的。

规则：

```text
如果 lookup 需要检查状态，那么状态检查、集合查找、get 必须被同一套机制保护。
```

------

### 8.7.3\_DYING\_状态通常拒绝新的\_lookup\_引用

有些对象进入 `DYING` 后，仍然允许已有路径继续使用，但不允许新 lookup 进入。

这很常见。

语义可以定义为：

```text
LIVE：
    新 lookup 可以成功；
    已有引用可以继续使用。

DYING：
    新 lookup 失败；
    已有引用可以继续收尾；
    最后一个 put 后 release。

DEAD：
    不可 lookup；
    不可使用；
    内存即将或已经释放。
```

lookup 函数：

```c
struct my_obj *my_obj_lookup_get_live(int id)
{
	struct my_obj *obj;

	mutex_lock(&my_obj_lock);

	obj = my_obj_find_locked(id);
	if (!obj)
		goto out_null;

	if (obj->state != OBJ_LIVE)
		goto out_null;

	kref_get(&obj->ref);
	mutex_unlock(&my_obj_lock);
	return obj;

out_null:
	mutex_unlock(&my_obj_lock);
	return NULL;
}
```

remove 路径：

```c
void my_obj_mark_dying_and_remove(struct my_obj *obj)
{
	mutex_lock(&my_obj_lock);

	obj->state = OBJ_DYING;

	if (!list_empty(&obj->node))
		list_del_init(&obj->node);

	mutex_unlock(&my_obj_lock);

	kref_put(&obj->ref, my_obj_release);
}
```

这样做的结果是：

```text
新的 lookup 找不到或因为 DYING 失败；
已有引用不受影响，可以继续 put 收敛；
最后一个 put 后 release。
```

这体现了第 3 章的结论：

```text
撤销发布不等于立即销毁对象。
```

------

### 8.7.4\_kref\_get\_unless\_zero()\_和对象状态不能互相替代

`kref_get_unless_zero()` 只能判断：

```text
引用计数是否非 0。
```

它不能判断：

```text
对象是否还允许新用户进入；
对象是否处于 DYING；
对象是否已经从业务上关闭；
设备是否可用；
请求是否已经完成。
```

所以不能写成：

```c
if (!kref_get_unless_zero(&obj->ref))
	return NULL;

return obj;
```

然后认为这就表示对象可用。

更完整的 lookup 可能需要：

```c
mutex_lock(&my_obj_lock);

obj = my_obj_find_locked(id);
if (!obj)
	goto out_null;

if (obj->state != OBJ_LIVE)
	goto out_null;

if (!kref_get_unless_zero(&obj->ref))
	goto out_null;

mutex_unlock(&my_obj_lock);
return obj;

out_null:
	mutex_unlock(&my_obj_lock);
	return NULL;
```

这里三件事各自负责不同问题：

```text
find_locked：
    找对象。

state == OBJ_LIVE：
    判断业务状态是否允许新用户进入。

kref_get_unless_zero：
    判断生命周期引用是否还能加入。
```

不要把其中任何一个当成全部安全模型。

------

### 8.7.5\_RCU\_lookup\_的提前预告

RCU lookup 是更复杂的 lookup 场景，本章只先给出核心边界。

RCU 保护的是：

```text
读侧遍历期间，指针指向的内存不会立刻释放。
```

但 RCU 不自动给你对象引用。

所以 RCU lookup 常见结构是：

```c
rcu_read_lock();

obj = my_obj_lookup_rcu(id);
if (obj && kref_get_unless_zero(&obj->ref)) {
	rcu_read_unlock();
	return obj;
}

rcu_read_unlock();
return NULL;
```

这个模型成立必须满足：

```text
对象从 RCU 可见结构删除后，不能立即 kfree；
对象内存必须撑过 RCU grace period；
release 要用 kfree_rcu() 或 call_rcu() 之类的延迟释放方式；
或者释放路径用 synchronize_rcu() 等方式等待读侧结束。
```

否则即使用了 `kref_get_unless_zero()`，也可能访问已经释放的 `obj->ref`。

所以 RCU 场景的规则是：

```text
RCU 保证 get_unless_zero 时 obj 内存还在；
get_unless_zero 成功后，kref 保证离开 RCU 后对象继续活着。
```

第 10 章会专门展开。

------

### 8.7.6\_lookup\_与对象\_复活\_问题

对象复活指的是：

```text
refcount 已经归零；
release 已经开始；
另一个路径又把 refcount 加回去。
```

错误模型：

```text
CPU0                                CPU1
--------------------------------    -------------------------------
kref_put(&obj->ref, release);
refcount 变成 0;
进入 release(obj);

                                    obj = find_obj(id);
                                    kref_get(&obj->ref);
                                    // 以为对象又活了

release(obj) 继续执行;
kfree(obj);

                                    使用 obj;
                                    // UAF：使用了已经释放或正在释放的对象
```

这就是复活。

普通 `kref_get()` 不会检查“原来是不是 0”。

所以在可能遇到 0 的场景，要用：

```c
kref_get_unless_zero()
```

它的语义是：

```text
0 就不加；
非 0 才加。
```

也就是说：

```text
已经走到最后释放点的对象，不能被重新拉回生命周期。
```

但是再次强调：

```text
防复活不等于防悬挂指针。
```

防悬挂指针靠：

```text
锁；
RCU；
延迟释放；
集合引用；
严格 remove 顺序。
```

防复活靠：

```text
kref_get_unless_zero()。
```

这两个问题不能混。

------

## 8.8\_错误清单\_模板和检查项

这一组内容作为本章最后的落地部分。

先看错误清单，再看模板，最后用检查清单收尾：

```text
错误清单：识别常见 bug 形态。
设计模板：把正确写法固定下来。
检查清单：写代码或 review 时逐项确认。
```

### 8.8.1\_lookup\_的错误清单

#### (1)\_无保护\_lookup\_后\_get

错误：

```c
obj = my_obj_lookup_raw(id);
if (obj)
	kref_get(&obj->ref);
```

问题：

```text
obj 可能已经被删除和释放。
```

正确：

```c
mutex_lock(&my_obj_lock);
obj = my_obj_find_locked(id);
if (obj)
	kref_get(&obj->ref);
mutex_unlock(&my_obj_lock);
```

------

#### (2)\_以为\_kref\_get\_unless\_zero()\_可以替代锁

错误：

```c
obj = my_obj_lookup_raw(id);
if (obj && kref_get_unless_zero(&obj->ref))
	return obj;
```

问题：

```text
obj 指针本身可能已经悬挂。
```

正确：

```c
mutex_lock(&my_obj_lock);
obj = my_obj_find_locked(id);
if (obj && !kref_get_unless_zero(&obj->ref))
	obj = NULL;
mutex_unlock(&my_obj_lock);
return obj;
```

或者 RCU 配套延迟释放。

------

#### (3)\_lookup\_返回裸指针给锁外使用

错误：

```c
mutex_lock(&my_obj_lock);
obj = my_obj_find_locked(id);
mutex_unlock(&my_obj_lock);

obj->state = OBJ_BUSY;      /* 错误 */
```

问题：

```text
离开锁后，没有引用保护；
obj 可能已经被释放。
```

正确：

```c
mutex_lock(&my_obj_lock);
obj = my_obj_find_locked(id);
if (obj)
	kref_get(&obj->ref);
mutex_unlock(&my_obj_lock);

if (!obj)
	return -ENOENT;

obj->state = OBJ_BUSY;

kref_put(&obj->ref, my_obj_release);
```

------

#### (4)\_remove\_时先\_put\_后\_unlink

错误：

```c
kref_put(&obj->ref, my_obj_release);

mutex_lock(&my_obj_lock);
list_del_init(&obj->node);
mutex_unlock(&my_obj_lock);
```

问题：

```text
put 可能释放 obj；
后续 list_del 访问释放内存；
其他 lookup 可能看到悬挂节点。
```

正确：

```c
mutex_lock(&my_obj_lock);
list_del_init(&obj->node);
mutex_unlock(&my_obj_lock);

kref_put(&obj->ref, my_obj_release);
```

------

#### (5)\_lookup\_成功后忘记\_put

错误：

```c
obj = my_obj_lookup_get(id);
if (!obj)
	return -ENOENT;

do_something(obj);
return 0;
```

问题：

```text
lookup_get 返回带引用对象；
调用者忘记 put；
对象泄漏。
```

正确：

```c
obj = my_obj_lookup_get(id);
if (!obj)
	return -ENOENT;

do_something(obj);

kref_put(&obj->ref, my_obj_release);
return 0;
```

------

#### (6)\_lookup\_得到引用后\_handoff\_失败忘记回滚

错误：

```c
obj = my_obj_lookup_get(id);
if (!obj)
	return -ENOENT;

ret = my_obj_schedule_work_take(obj);
if (ret)
	return ret;     /* 错误：lookup 引用泄漏 */
```

正确：

```c
obj = my_obj_lookup_get(id);
if (!obj)
	return -ENOENT;

ret = my_obj_schedule_work_take(obj);
if (ret) {
	kref_put(&obj->ref, my_obj_release);
	return ret;
}

return 0;
```

------

### 8.8.2\_lookup\_函数设计模板

#### (1)\_list\_+\_mutex\_模板

```c
struct my_obj *my_obj_lookup_get(int id)
{
	struct my_obj *obj;

	mutex_lock(&my_obj_lock);

	list_for_each_entry(obj, &my_obj_list, node) {
		if (obj->id == id) {
			kref_get(&obj->ref);
			mutex_unlock(&my_obj_lock);
			return obj;
		}
	}

	mutex_unlock(&my_obj_lock);
	return NULL;
}
```

------

#### (2)\_list\_+\_mutex\_+\_state\_模板

```c
struct my_obj *my_obj_lookup_get_live(int id)
{
	struct my_obj *obj;

	mutex_lock(&my_obj_lock);

	list_for_each_entry(obj, &my_obj_list, node) {
		if (obj->id != id)
			continue;

		if (obj->state != OBJ_LIVE)
			break;

		kref_get(&obj->ref);
		mutex_unlock(&my_obj_lock);
		return obj;
	}

	mutex_unlock(&my_obj_lock);
	return NULL;
}
```

------

#### (3)\_hash\_+\_spinlock\_模板

```c
struct my_obj *my_obj_hash_lookup_get(u32 id)
{
	struct my_obj *obj;

	spin_lock(&my_obj_ht_lock);

	hash_for_each_possible(my_obj_ht, obj, hnode, id) {
		if (obj->id == id) {
			kref_get(&obj->ref);
			spin_unlock(&my_obj_ht_lock);
			return obj;
		}
	}

	spin_unlock(&my_obj_ht_lock);
	return NULL;
}
```

------

#### (4)\_xarray\_+\_lock\_模板

```c
struct my_obj *my_obj_xa_lookup_get(unsigned long index)
{
	struct my_obj *obj;

	xa_lock(&my_obj_xa);

	obj = xa_load(&my_obj_xa, index);
	if (obj)
		kref_get(&obj->ref);

	xa_unlock(&my_obj_xa);

	return obj;
}
```

------

#### (5)\_RCU\_+\_kref\_get\_unless\_zero()\_模板预告

```c
struct my_obj *my_obj_lookup_get_rcu(int id)
{
	struct my_obj *obj;

	rcu_read_lock();

	obj = my_obj_lookup_rcu(id);
	if (obj && !kref_get_unless_zero(&obj->ref))
		obj = NULL;

	rcu_read_unlock();

	return obj;
}
```

这个模板不能单独复制使用。

它依赖释放路径：

```text
删除时先从 RCU 可见结构 unlink；
对象内存延迟释放到 grace period 之后；
release 不能直接 kfree 给 RCU 读侧制造悬挂指针。
```

第 10 章再详细展开。

------

### 8.8.3\_lookup\_API\_的命名建议

推荐命名：

```c
my_obj_find_locked()
```

语义：

```text
调用者必须持锁；
返回裸指针；
不能在锁外使用；
不增加引用。
```

推荐命名：

```c
my_obj_lookup_get()
```

语义：

```text
内部完成 lookup + get；
返回成功时调用者持有引用；
调用者必须 put。
```

推荐命名：

```c
my_obj_get_by_id()
```

语义：

```text
按 ID 查找对象；
成功返回带引用对象。
```

推荐命名：

```c
my_obj_tryget()
```

语义：

```text
尝试从已有对象指针获得引用；
通常基于 kref_get_unless_zero；
但调用者仍要保证 obj 指针内存有效。
```

不推荐含糊命名：

```c
my_obj_lookup()
my_obj_find()
my_obj_get()
my_obj_search()
```

除非注释明确说明：

```text
是否返回引用；
调用者是否要 put；
是否只能锁内使用；
失败时是否可能是对象正在退出。
```

------

### 8.8.4\_本章检查清单

写 lookup 代码时，逐项检查：

```text
1. lookup 返回的是裸指针，还是带引用对象？
2. 如果是裸指针，是否只在锁内使用？
3. 如果要锁外使用，是否在锁内完成 kref_get？
4. kref_get 前由什么机制证明 obj 有效？
5. 集合是否持有对象引用？
6. 对象插入集合时是否 get？
7. 对象从集合删除时是否先 unlink 再 put？
8. 所有 add/del/lookup 是否使用同一把锁或同一套同步机制？
9. lookup 是否需要检查对象状态？
10. 状态检查和 get 是否在同一临界区完成？
11. 是否错误地用 kref_get_unless_zero 替代锁？
12. RCU lookup 是否配套延迟释放？
13. lookup_get 成功后调用者是否所有路径都 put？
14. lookup 后 handoff 失败路径是否回滚 put？
15. 函数名是否说明 find_locked / lookup_get / tryget 语义？
```

最关键的是这几个问题：

```text
我拿到的是指针，还是引用？
get 前对象由谁保护？
返回后谁负责 put？
对象从哪里撤销可见性？
最后释放是否可能和 lookup 并发？
```

------

## 8.9\_本章小结

lookup 是 kref 使用里最容易误判的场景。

因为 lookup 不是简单地从容器中拿一个地址，而是要完成下面的转换：

```text
共享结构中的裸指针
    -> 保护机制证明对象仍然有效
        -> kref_get / kref_get_unless_zero
            -> 当前路径持有有效引用
```

本章核心结论：

```text
1. 有指针不等于有引用。
2. lookup 后不能无保护 kref_get。
3. kref_get 前必须证明对象有效。
4. kref_get_unless_zero 只防止从 0 复活，不防止悬挂指针。
5. 锁保护集合关系，kref 保护对象生命周期。
6. RCU 保护读侧内存可见性，kref 保护拿到引用后的生命周期。
7. remove 路径必须先 unlink，再 put。
8. lookup_get 成功返回时，调用者必须 put。
```

再压缩成一句话：

```text
lookup 的目标不是找到对象，而是在对象仍然活着的时候拿到一份引用。
```

不要把代码写成：

```text
find pointer
then maybe get
```

而要写成：

```text
protected lookup
then get
then return referenced object
```

这才是 kref lookup 场景的正确工程模型。

------

专题导航：[kref 引用计数机制章节大纲](大纲.md)。

上一篇：[handoff 所有权转移模型](P07_handoff_所有权转移模型.md#7.8_本章小结)。

下一篇：[kref 与锁的组合](P09_kref_与锁的组合.md)。
