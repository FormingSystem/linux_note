---
id: knowledge.linux.data_structures.红黑树_rb-tree.p12_linux_6.12_内核_rbtree_工程扩展_并发与验证
title: "Linux 6.12 内核 rbtree 工程扩展 并发与验证"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
---

# 第12章\_Linux\_6.12\_内核\_rbtree\_工程扩展\_并发与验证

## 12.1\_章节内容说明

[P37 调用者框架](P37_构建rbtree调用者接口.md#37.16_运行完整的私有调用者框架)已把对象、计数和失败清理接成程序，后续查询、插入、删除、遍历和同键替换又逐项建立了底层契约。现在回到一组按到期时间排序的任务：若业务反复问“下一件事是什么”，每次都从根重新向左查找是否值得？

### 12.1.1\_本章在\_Linux\_rbtree\_学习路线中的位置

默认路线 P08/P09/P37 → P10/P26 → P11/P27/P28 → P29 已建立类型、业务持有权、更新与返回边界。P28 留下额外缓存与摘要的维护责任；[P29 完成边界](P29_普通旋转与Linux修复的完成边界.md#29.5_回顾与练习)说明回调中间态不等于整轮完成。本章据此讨论三类不同问题：保存一个可直接取得的首节点入口、随子树变化维护摘要，以及在真实读写协议下验证结果。它们不互相替代。

先在 12.2 解释缓存根的正常与故障过程，再进入增强信息、并发、接口和验证。取首常量时间不等于删除或整个调度操作常量时间；增加一个指针也不表示它自动具有新的并发保证。

### 12.1.2\_本章参照的源码文件

固定版本从[NXP Linux 6.12.20 源码总索引](../../../../research/source_reading/rbtree/navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界)进入，不再引用旧的 kernel_source 路径。正文解释为何需要这些状态；实现语句按上游位置在源码层单独展开：

| 上游位置 | 本章的阅读任务 |
| --- | --- |
| include/linux/rbtree_types.h | rb_root_cached 的普通根和最左入口 |
| include/linux/rbtree.h | 缓存取首、插入、删除、替换包装与比较辅助 |
| include/linux/rbtree_augmented.h | 增强回调、旋转时摘要与删除传播 |
| lib/rbtree.c | 通用修复、遍历、替换及无锁下行的说明边界 |

先沿[缓存模块导读](../../../../research/source_reading/rbtree/navigation/P08_最左缓存与结构更新导读.md#8.2_沿接入与摘除跟踪C0到C6)观察状态地址，再按需要进入具体实现；不能把当前本地配置或实验分支头当作固定发布证据。

## 12.2\_cached\_rbtree\_struct\_rb\_root\_cached

cached rbtree 是在普通树之外维护一个中序首地址的包装。没有改变红黑修复算法，而是增加了一项必须与树结构共同维护的不变量：稳定观察时，缓存地址应与沿左链找到的首节点 **完全相同**。有等价键时，“也是一个最小键”还不足以证明对象身份正确。

### 12.2.1\_为什么要缓存最左节点

普通根只保存拓扑根地址。`rb_first(root)` 从根逐次读取左孩子，直到没有更左的节点；实际读取几层取决于当前形状，红黑高度给出对数上界，空树则直接返回空。一次查询已经很便宜，但频繁重复会访问同一段路径。

设某个稳定树形取得首节点需要读 L 个节点，业务在下一次更新前询问一千次。普通方式仍做一千次左链查找；若在更新时保存首地址，一千次询问就各读一次缓存槽。这是操作数量的推导，不是缓存未命中或时间的实测，也没有假定每次左链都落到主存。

`struct rb_root_cached` 包含普通 `rb_root` 与 `rb_leftmost` 两个入口，完整类型见[缓存根实现](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree_types.h.md#1.3_rb_root_cached增加一个最左入口)。`rb_first_cached()` 只读取后者，因此取得地址是 O(1)，但它不查树、不加锁、不取得引用，更不保证返回对象仍活着。宏体见[直接取首](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.12_缓存取首只读取入口)。

最早到期任务或下一个时间线事件是自然场景；若业务还需按位置、资格等额外条件挑选，最左节点未必就是最终候选。例如不能仅凭“调度器有虚拟运行时间”就推导所有版本的下一任务只读最左缓存，具体场景在本章后面分别核对。

### 12.2.2\_为什么只缓存最左节点\_不缓存最右节点

缓存不只占一枚指针，还在插入、删除和替换时增加判断与写入。两端都缓存，则每个采用该结构的根都需额外存储，并让两端的更新协议保持一致。固定头文件选择只提供最左入口；这是这份接口的工程取舍，不是最右端点在算法上难以缓存。

只有偶尔取最小、树很小或主要按任意键查询时，继续用普通根可以省掉额外不变量。反复取首且更新路径可统一维护缓存时，cached 根才更有吸引力。若需要频繁取最大或两端，业务可以另行设计对应入口，但要明确所有修改如何维护它们；不能只给结构体增加一个字段。

已有缓存根不因“也需要最大值”就一定不适用：仍可沿右链偶尔查询最大值，或在确有收益时增加另一端缓存。选择依据是观察与更新负载、字段成本和维护复杂度，最终性能需要同一场景测量。

### 12.2.3\_rb\_insert\_color\_cached()\_的使用方式

沿用普通插入的空槽搜索。新增局部布尔量 leftmost，初始 true：若一路向左，新节点将排在所有现存节点之前；只要有一次向右，就已有一个节点位于它之前，此后再向左也无法抹掉那个祖先。因此第一次向右时置 false，之后不恢复。

这里跟踪的是 **实际插入路径**。若等价键按“不小于则向右”处理，新对象并不是已有等价组中的首对象；不能只判断新键是否等于当前最小键就传 true。`rb_add_cached()` 已把搜索、标志计算、挂接和 cached 修复组合起来，它不做唯一键拒绝；需要拒绝重复的业务仍须使用相应搜索契约。

手写路径先 rb_link_node 接入空槽，再传入正确 leftmost 给 rb_insert_color_cached。包装先按标志写缓存，再调用普通修复。此时不是两份独立可随意观察的结构：C2 挂接后缓存可能仍旧，C3 写缓存后红黑修复可能尚未完成，调用者必须保护整个操作。

| 阶段 | 状态实际在哪里，谁修改 | 稳定性与后续动作 |
| --- | --- | --- |
| C0 初始化 | 调用者写普通根与 rb_leftmost 为 NULL | 空树与空缓存同时成立 |
| C1 搜索 | 更新者的局部 parent、link、leftmost | 遇右置 false，直到得到空槽 |
| C2 挂接 | rb_link_node 写节点字段和根/孩子槽 | 尚未完成缓存及红黑修复 |
| C3 缓存与修复 | cached 包装按标志写 rb_leftmost，再调用普通插入修复 | 返回后才向受保护观察者承诺一致 |
| C4 取首 | 读者按协议读 rb_leftmost | 只省查找路径，不替代对象寿命保护 |
| C5 摘除 | 更新者必要时先用旧拓扑求后继并改缓存，再结构删除 | 返回时重新满足缓存等于当前中序首 |
| C6 退出 | 调用者处理其他入口和持有权 | 与树中是否还保存旧地址是不同问题 |

```mermaid
flowchart LR
    writer[受保护的更新者] -->|C2及C5写根与孩子槽| topology[普通树拓扑]
    writer -->|C3及C5写首地址| cache[rb_leftmost槽]
    topology -->|C4沿左链读到| first[当前中序首对象]
    cache -->|C4直接指向同一地址| first
    owner[业务持有权协议] -->|C6保持或回收| first
```

```mermaid
sequenceDiagram
    participant W as 更新者
    participant P as 局部搜索状态
    participant T as 根与节点字段
    participant C as 缓存槽
    W->>P: C1 查空槽，初始leftmost=true
    opt 曾向右
        P->>P: leftmost=false
    end
    W->>T: C2 接入新节点
    opt leftmost为true
        W->>C: C3 保存新首地址
    end
    W->>T: C3 普通红黑修复
    C-->>W: C4 现在可在保护下取得首对象
    alt C5 删中首对象
        W->>T: 删除前求rb_next
        W->>C: 改为后继或NULL
    else 删除其他对象
        W->>W: 缓存不改
    end
    W->>T: C5 结构删除和修复
    W->>W: C6 另行处理对象寿命
```

源码对应[搜索产生标志](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.15_辅助插入如何产生最左标志)与[缓存先写、修复随后](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.13_缓存写入先于插入修复)。rb_add_cached 返回新节点仅表示它新成首节点；返回 NULL 时节点也已经插入，不能按“失败”释放它。

### 12.2.4\_rb\_erase\_cached()\_如何更新最左缓存

若删除对象不是当前 rb_leftmost，首对象不变；若是，新的首对象是它的中序后继。必须在旧对象仍处于原拓扑时计算 rb_next，再修改缓存并调用 rb_erase。删除后旧节点不再是有效遍历起点，即使它的字段没有被清零；旋转和回接也可能改变其他节点的链接。

最后一个对象没有后继，缓存变为 NULL，结构删除后普通根也为空。返回值却要更仔细地读：固定 rb_erase_cached 的局部返回值初始 NULL，仅在删中缓存时赋为后继。因此下面两种删除都返回 NULL：删除非首对象而缓存未变；删除最后对象而后继为空。不能由这个返回值直接判断当前树是否有首节点。完整函数见[缓存删除](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.14_缓存删除先取后继)。

同键替换也属于维护入口的操作：若旧对象正是缓存对象，即使键值不变，也必须改成新对象地址。[P28 缓存替换](P28_Linux同键替换与旧对象退出.md)及[唯一包装实现](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.10_替换时的最左缓存入口)已区分这个地址身份变化。更新缓存没有撤销已有读者保存的旧地址，C6 仍须按持有权协议完成。

### 12.2.5\_cached\_rbtree\_的使用边界

使用 cached 根时，插入、删除、替换都要维持额外入口。只调用普通 rb_erase 可能留下“所有红黑性质都对，首指针却错”的状态。若旧对象被释放，继续相信缓存就会使用失效地址；若它尚存活，错误也不能因为暂时不崩溃而被忽略。

普通根和缓存不是一次原子写入，更没有因宏只读一个指针就自动支持无锁读者。当前标准包装需调用者完整保护更新；若需要另一种读写协议，应单独证明缓存发布、节点拓扑和回收条件，而不是把普通替换换成带 RCU 后缀的函数后就宣布完成。

#### (1)\_运行缓存一致性实验

下面用五个私有任务观察两种返回值与一次故障。按 `(deadline, id)` 排序，这组数据没有重复完整键；数组中的 active 是实验核对台账，不是 Linux 自动维护字段。它让我们独立扫描仍在树中的对象预测最小地址，再与普通左链和缓存比较。完整材料为 [note_rbtree_cached.c](../../../../labs/kernel/tree_basics/materials/note_rbtree_cached.c)：

```c
// SPDX-License-Identifier: GPL-2.0
/* 私有自动对象：缓存故障演示不会释放对象，也不发布并发入口。 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/errno.h>

struct cached_job {
    int deadline;
    unsigned int id;
    bool active;
    struct rb_node rb;
};

static bool job_less(struct rb_node *a, const struct rb_node *b)
{
    struct cached_job *first = rb_entry(a, struct cached_job, rb);
    const struct cached_job *second = rb_entry(b, struct cached_job, rb);
    if (first->deadline != second->deadline)
        return first->deadline < second->deadline;
    return first->id < second->id;
}

/* 数组扫描独立预测最小对象，不从缓存或树的左链生成预期值。 */
static bool cache_matches(struct rb_root_cached *root, struct cached_job *jobs,
                          unsigned int count)
{
    struct cached_job *expected = NULL;
    unsigned int i;
    for (i = 0; i < count; ++i) {
        if (jobs[i].active && (!expected || job_less(&jobs[i].rb, &expected->rb)))
            expected = &jobs[i];
    }
    return rb_first_cached(root) == (expected ? &expected->rb : NULL) &&
           rb_first(&root->rb_root) == (expected ? &expected->rb : NULL);
}

static int __init note_cached_init(void)
{
    struct cached_job jobs[] = {
        {.deadline = 40, .id = 0}, {.deadline = 10, .id = 1},
        {.deadline = 40, .id = 2}, {.deadline = 25, .id = 3},
        {.deadline = 70, .id = 4}
    };
    struct rb_root_cached root = RB_ROOT_CACHED;
    struct rb_node *result;
    unsigned int i;

    if (!cache_matches(&root, jobs, ARRAY_SIZE(jobs)))
        return -EINVAL;
    for (i = 0; i < ARRAY_SIZE(jobs); ++i) {
        result = rb_add_cached(&jobs[i].rb, &root, job_less);
        jobs[i].active = true;
        if (result != (i < 2 ? &jobs[i].rb : NULL) ||
            !cache_matches(&root, jobs, ARRAY_SIZE(jobs)))
            return -EINVAL;
    }
    pr_info("note_cached: five inserts, first=10:1\n");

    result = rb_erase_cached(&jobs[4].rb, &root); /* 删除非最小的 70。 */
    jobs[4].active = false;
    if (result || !cache_matches(&root, jobs, ARRAY_SIZE(jobs)))
        return -EINVAL;
    pr_info("note_cached: erase non-first returns NULL, first still exists\n");

    result = rb_erase_cached(&jobs[1].rb, &root); /* 最小从 10 变为 25。 */
    jobs[1].active = false;
    if (result != &jobs[3].rb || !cache_matches(&root, jobs, ARRAY_SIZE(jobs)))
        return -EINVAL;
    pr_info("note_cached: erase first returns 25:3\n");

    /* 故意混用普通删除：树结构更新，但额外入口仍指向已摘除的25。 */
    rb_erase(&jobs[3].rb, &root.rb_root);
    jobs[3].active = false;
    if (cache_matches(&root, jobs, ARRAY_SIZE(jobs)))
        return -EINVAL;
    pr_info("note_cached: ordinary erase leaves a stale cache\n");
    /* 故障演示中对象仍活着且完全独占，重建缓存后继续观察。 */
    root.rb_leftmost = rb_first(&root.rb_root);
    if (!cache_matches(&root, jobs, ARRAY_SIZE(jobs)))
        return -EINVAL;

    while ((result = rb_first_cached(&root)) != NULL) {
        struct cached_job *item = rb_entry(result, struct cached_job, rb);
        rb_erase_cached(result, &root);
        item->active = false;
        if (!cache_matches(&root, jobs, ARRAY_SIZE(jobs)))
            return -EINVAL;
    }
    pr_info("note_cached: empty tree and empty cache agree\n");
    return 0;
}

static void __exit note_cached_exit(void)
{
    /* 根与节点均为初始化期间私有自动对象，没有外部保存其地址。 */
}
module_init(note_cached_init);
module_exit(note_cached_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Private cached rbtree invariant exercise");
```

先预测：插入 40:0 时它是首对象；插入 10:1 后首对象改变；后面三次返回 NULL，但都已插入。删除 70:4 返回 NULL 且树不空；删除 10:1 返回后继 25:3。故意用普通删除摘掉 25:3 时，树会修复，缓存仍保存那个旧成员地址，数组核对就会发现不一致。

故障对象是仍然存活的局部数组元素，程序不会释放它，也没有并发入口。因此这里能安全比较地址并在独占条件下重新计算缓存，以便继续后面的观察；这不是建议生产代码对任意悬空缓存进行解引用后再修补。真正的修正是让所有合法修改路径维持不变量。

所有对象与根都仅在初始化函数里存活，既无动态申请也无外部发布，任何检查失败返回都没有堆对象或共享入口需要回收。成功路径逐个 cached 摘除到空。程序没有用 RB_EMPTY_NODE 作为台账，因此不要求额外清游离标记；若业务采用该协议，应按自己的安全时机维护。

已有 [Makefile](../../../../labs/kernel/tree_basics/materials/Makefile) 登记该模块。Linux 构建环境先设置匹配目标内核的 KDIR，并按工具链设置 ARCH/CROSS_COMPILE；在仓库根目录构建：

```bash
: "${KDIR:?先设置匹配目标内核的构建目录}"
make -C "$KDIR" M="$PWD/labs/kernel/tree_basics/materials" modules
```

把 note_rbtree_cached.ko 放到匹配目标系统后，从它所在目录执行：

```bash
sudo insmod ./note_rbtree_cached.ko
sudo dmesg | tail -n 30
sudo rmmod note_rbtree_cached
```

成功时预期五条核心消息如下；初始化失败时先检查 insmod 错误和日志，不假定模块已经加载：

```text
note_cached: five inserts, first=10:1
note_cached: erase non-first returns NULL, first still exists
note_cached: erase first returns 25:3
note_cached: ordinary erase leaves a stale cache
note_cached: empty tree and empty cache agree
```

本轮 ARMv7 前端与宿主显式适配检查已执行，目标 Kbuild、MODPOST、装卸及这组目标日志未执行。宿主父色位宽和访问宏有适配，只检查串行算法与接口返回，不证明内核 ABI、并发可见性或实际缓存性能。

### 12.2.6\_本节小结

缓存把反复查找首节点的工作移到更新者维护的额外入口，但不改变树操作和寿命契约。按 C0～C6 核对的是两个槽、树结构与对象持有三组不同状态；稳定时首地址一致，中间态需被保护。三个练习检查这个认识：

1. 将 25 的输入改为 5，哪次插入会新成首节点？它会排在 10 之前，rb_add_cached 那次返回该节点；测试预期也须随输入契约改变，不能继续硬编码“只有前两次返回节点”。
2. 删除非首对象时返回 NULL，能据此清空 root.rb_leftmost 吗？不能，NULL 在这个分支只是没有新的首地址要报告，原缓存仍正确。
3. 只比较缓存键值与最小键值，能发现所有错误吗？不能，两个等价键对象地址不同；还需核对当前中序首对象身份以及它是否仍为有效成员。

下一节不再缓存一个端点，而是给每个子树增加摘要。旋转仍保持中序关系，却会改变哪些节点属于某个子树，因此仅保存最左地址的办法不能直接维护区间最大值等增强信息。


## 12.3\_augmented\_rbtree\_增强红黑树

缓存一个最左地址之后，再看另一种附加状态：每个节点为整棵子树保存一份摘要。本节沿区间查询建立摘要的含义，再追踪它在接入、旋转和摘除时如何保持有效。

### 12.3.1\_什么是\_augmented\_rbtree

最左缓存回答了“当前第一个对象在哪”，却不能替我们回答另一类问题：一批有效地址范围按起点排好了，地址 72 落在哪个范围里？设每个对象保存闭区间 `[first,last]`，两个端点都包含在范围内。只看起点，`[30,80]` 能覆盖 72，`[35,40]` 却不能。按起点比较一次后，不能像查找一个确定键那样简单地丢弃另一半区间。

最直接的方法是按序检查每个范围。这种方法容易验证，数据很少、查询不频繁时也完全值得保留。缺口出现在范围多而点查询频繁时：大量范围早已在 72 之前结束，查询却仍须逐个读取对象才能知道这一点。我们想为整棵子树留一份摘要，提前回答“这里有没有结束位置足够大的区间”。

给每个业务对象增加 `subtree_last`，表示 **以自己的 rb 成员为根的整棵子树中最大的 last**。于是节点的摘要满足：

```text
subtree_last = max(自己的 last,
                   左子树的 subtree_last（如果有左孩子）,
                   右子树的 subtree_last（如果有右孩子）)
```

如果某棵子树的摘要小于 72，其中每个区间都已在 72 之前结束，可以整体跳过。这就是增强红黑树的起点：普通树继续负责排序与平衡，业务对象多存一份可沿树形组合的子树信息。子树计数、最小值、最大值和区间上界都可能承担这样的职责；具体摘要应由查询问题推出，不能因为宏现成就随手塞一个统计字段。

这里同时维护三组状态：父子关系与颜色、排序载荷 `first/last`、摘要 `subtree_last`。树依旧平衡并不证明摘要正确。一个新增区间改变了祖先覆盖的对象集合，旋转改变了局部根所覆盖的对象集合，删除又从集合中拿走一个对象；普通修复代码不知道我们的 last 存在哪，因而需要业务回调在这些事件上同步摘要。

```mermaid
flowchart LR
    caller[独占更新者] -->|接入或摘除| tree[rb 根槽与父子颜色字段]
    caller -->|修改原始端点| payload[range_item.first / last]
    payload -->|本节点标量| compute[业务计算函数]
    child[孩子的 subtree_last] -->|已经有效的子树结果| compute
    compute -->|写入新摘要| summary[本节点 subtree_last]
    summary -->|父节点重算时读取| ancestor[祖先摘要]
    tree -->|结构变化时调用| callback[propagate / copy / rotate]
    callback -->|选择重算或移交位置| compute
    summary -->|查询据此选择或排除子树| reader[点查询者]
```

图中的通信是同一更新调用内对对象字段的读写，没有自动的跨 CPU 通知协议。示例使用私有对象；若放到共享树，保护范围必须同时覆盖拓扑、载荷和摘要。只锁旋转或者只把摘要改成原子变量，不能让查询看到同一时刻的一组状态。

### 12.3.2\_struct\_rb\_augment\_callbacks\_的三个回调

先从[固定版本阅读索引](../../../../research/source_reading/rbtree/navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界)进入源码，再看[增强模块的 A0～A5](../../../../research/source_reading/rbtree/navigation/P09_子树摘要与增强回调导读.md#9.2_沿A0到A5维护同一份摘要)。本节使用 NXP 官方固定 Linux 6.12.20 的接口；宏和字段的唯一实现放在[回调结构](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree_augmented.h.md#1.7_三个回调的结构契约)，正文先建立它们为什么分成三类。

| 变化 | 回调 | 它交付什么 |
| --- | --- | --- |
| 一条祖先路径覆盖的对象集合改变 | `propagate(node, stop)` | 从 node 向父方向重算，stop 不包含在重算范围内；模板发现结果未变可提前结束 |
| 删除中后继接替旧节点的位置 | `copy(old, new)` | 把旧局部根的摘要交给新局部根，作为接替过程的起点；不是复制整个业务对象 |
| 旋转改变两个局部根的子树范围 | `rotate(old, new)` | 让提升后的 new 接住旧整体摘要，再重算覆盖范围缩小的 old |

这里的 old/new 指特定结构动作的两个节点，不是“申请新对象再释放旧对象”。copy 后也不意味着删除完成：旧摘要可能仍把将被删除的范围计算在内，之后必须重新传播。判断完成要看整次更新的稳定观察点，不能把某个回调返回当作全部不变量已恢复。

### 12.3.3\_增强信息为什么需要随旋转更新

保留此前的左旋模型，把没画出的外侧子树记成 L 和 R：

```text
        old                         new
       /   \                       /   \
      L    new        -->        old    R
          /   \                 /   \
         T     R               L     T
```

旋转前后的中序顺序始终是 `L, old, T, new, R`。提升后的 new 覆盖的对象集合，与旋转前 old 覆盖的集合完全相同。因此，对“集合内所有 last 的最大值”来说，new 可以直接继承 old 原有的摘要。随后 old 只覆盖 L、自己和 T，必须根据自己的新孩子重算；若仍沿用旧摘要，它可能继续宣称自己包含 R 中最大的端点。

这解释了模板 rotate 的顺序：先把 old 的旧摘要写给 new，再强制重算 old。先重算 old 再复制，会把较小的新集合摘要错交给 new。把 new 的当前孩子简单合并也不一定安全，因为其中 old 的摘要正在等待修正。

这套移交有两个前提。第一，旋转前 old 的摘要已经正确；所以不能靠旋转顺便修复所有此前漏掉的传播。第二，摘要只取决于这批对象的聚合内容，不取决于当前树形。最大值、正确类型与运算约束下的计数或求和可以满足这种旋转不变性；子树高度和依赖树形的哈希通常不能直接套用这个复制模板。后者需要另行设计回调，并证明每个修复事件上的更新顺序。

### 12.3.4\_RB\_DECLARE\_CALLBACKS()\_与\_RB\_DECLARE\_CALLBACKS\_MAX()

现在再看生成器就不只是记参数名了。`RB_DECLARE_CALLBACKS` 接受可见性、回调组名称、业务结构体、嵌入 rb 成员、摘要字段，以及重算函数。它生成同组的 propagate、copy、rotate 和回调表。重算函数接收业务节点与 `exit` 布尔参数：`exit=true` 时若结果未变，返回 true 允许传播停止；否则写入结果并返回 false。旋转回调传 false，要求 old 真正完成一次重算，不允许把提前退出作为省略更新的理由。

提前结束不是碰运气。假设节点的两个孩子摘要已经正确，本节点由旧值重算得到相同值，而祖先只通过这个摘要感知这棵子树，那么祖先输入没有变化，自然无须继续。若祖先还读取其他未纳入摘要的变化量，或者孩子摘要已经过时，这条推理便不成立。

`RB_DECLARE_CALLBACKS_MAX` 在通用模板外再生成一层最大值计算：调用者只需提供返回 **单个节点原始标量** 的函数，生成器读取存在的孩子摘要、取最大值、比较旧值，再交给通用回调。示例的 `item_last()` 返回 last，不是返回 subtree_last。把缓存值再次当成原始值会使删除最大值后无法降下来。

具体参数与宏体分别在[通用回调模板](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree_augmented.h.md#1.8_通用模板的停止与移交)和[最大值生成器](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree_augmented.h.md#1.9_从本节点标量生成最大值)。宏只减少重复代码，不能替调用者证明摘要的数学含义，也不会替新叶子初始化业务字段。

### 12.3.5\_rb\_insert\_augmented()\_的插入流程

选择一个容易验证的插入顺序：先完成查重，再初始化新叶子的摘要为自己的 last，接入空槽，从 parent 向上传播，最后调用增强插入修复。查重阶段没有修改祖先，所以重复起点失败不会留下“树没插入、摘要却变大”的半次操作。本例故意拒绝重复起点；它是示例的业务政策，不是红黑树接口的普遍要求。

```mermaid
sequenceDiagram
    participant U as 独占调用者
    participant N as 新对象及其 rb
    participant P as parent 到根的摘要
    participant R as 插入修复
    U->>U: A0 搜索空槽并拒绝重复起点
    U->>N: A1 subtree_last=last，rb_link_node
    U->>P: A2 propagate(parent,NULL)
    P->>P: 重算，结果未变则提前停止
    U->>R: A3 rb_insert_augmented
    alt 发生旋转
        R->>N: 改变局部孩子关系
        R->>P: rotate：new 继承 old，重算 old
    else 没有旋转
        R->>R: 只按需要调整颜色
    end
    R-->>U: A4 返回后拓扑与摘要共同有效
    U->>U: 查询；以后修改载荷或 A5 摘除
```

这个先接入再向上传播的顺序，与固定版本的 [rb_add_augmented_cached](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree_augmented.h.md#1.11_缓存增强插入的挂接与传播)一致。该辅助函数还维护最左缓存，但不会替使用者初始化叶子摘要或拒绝重复业务键。它从 parent 开始传播，若叶子缓存没初始化，错误会由孩子传到祖先。

另一种设计可在搜索下降时更新路径上的最大值，避免之后重走父链；但它必须保证不会在后续查重、分配或其他失败中留下未发生的插入。应先解决失败语义，再讨论少走一次路径的收益。固定辅助函数的 `suboptimal` 注释不等于正确性缺陷，也不证明任意改成边搜边写都会更合适。

尤其要区分“增强插入接口”与“完整业务插入”。[rb_insert_augmented](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree_augmented.h.md#1.10_增强插入只接入旋转回调)把 rotate 交给平衡修复，**并不自动从新节点执行 propagate**。若本次无旋转，缺少 A2 的错误更容易藏住：键序、颜色和父链全对，祖先摘要仍是旧值。

### 12.3.6\_rb\_erase\_augmented()\_的删除流程

插入时最大值常常只增不减，删除却必须处理最大值消失。设一棵子树同时包含 `[30,80]` 和 `[35,40]`，删去前者后，摘要应降到剩余对象的最大终点，不能保留 80。保留偏大的值不总是“只是多查几步”：查询怎样使用摘要决定了错误后果，本节后面的单路径查找就会据此选错方向。

增强删除仍沿已有[对象摘除与缺黑修复 D0～D4](../../../../research/source_reading/rbtree/navigation/P04_对象摘除与缺黑修复导读.md#4.2_从对象到缺黑父槽)前进。新的职责是给这些结构事件配上摘要操作：

| 结构事件 | 摘要的变化与接下来的读者 |
| --- | --- |
| 直接移走零/单孩子节点 | 从结构变化所在的祖先开始传播，让上层读到剩余子树的摘要 |
| 右孩子直接作为后继 | copy 将旧位置摘要暂交后继；后继完成接管后，再从它向上重算 |
| 后继来自右子树深处 | copy 之后先从后继的旧 parent 传播到 successor 之前，修复移走后继留下的旧路径 |
| 后继接管左右子树并换入旧位置 | 从已接管的 successor 向根传播，扣除真正被删对象的贡献 |
| 还需颜色修复并发生旋转 | rotate 更新旋转的局部摘要；颜色变化本身不改变最大值 |

深层后继需要两段传播，是因为它既离开一个位置，又接管另一个位置。第一段的 stop 是 successor，**不重算 stop 本身**；此时它尚处在接替过程之中。最后一次传播才使用接管完成后的左右孩子重算。copy 提供旧位置的比较基准，不是断言新位置最终仍等于旧值。只保留 copy 而删掉末次传播，会把被删最大值继续算进结果。

唯一结构代码仍在[结构摘除与缺黑父槽](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree_augmented.h.md#1.3_结构摘除与缺黑父槽)，公共收尾见[增强删除与旋转回调](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree_augmented.h.md#1.12_增强删除的两段收尾)。不要把 copy 回调当成 `rb_replace_node` 的通用增强包装；换入业务对象时，其载荷和祖先摘要也必须符合自己的业务协议。

还有一种不改变树形的更新：保持 first 不动，将 `[30,80]` 的 last 改为 33。此时应先改原始 last，再从这个节点调用 propagate，保留旧 subtree_last 供计算器比较。若先把 subtree_last 改成 33，再从自己传播，模板可能发现“算出的 33 与当前 33 一样”而立即停下，祖先仍保留 80。排序键 first 改变则不同，应按受保护的摘除与重新插入处理，不能仅传播摘要。

### 12.3.7\_增强树为什么容易让代码体积膨胀

固定版本的 `Documentation/core-api/rbtree.rst` 提醒：增强删除的传播与复制回调可能被内联进删除骨架，使编译结果较大；每个增强树使用者宜集中一个增强删除调用点。原文讨论的是 **编译器可能内联造成的代码体积**，不是“每个编译单元只能拥有一棵增强树”的接口限制。

将删除集中在一个业务封装里，可以同时集中成员关系、同步和对象退出的约束，避免多处展开相同组合。最终机器码是否重复、重复多少，还受编译器、优化和链接影响；没有反汇编或体积对照时，不能把宏数量直接换算成性能或字节数。本例没有进行这种测量。

### 12.3.8\_本节小结

#### (1)\_运行完整区间摘要实验

下面的完整模块把六个闭区间插入私有树，查找点 72，再缩短其中唯一覆盖 72 的区间，最后逐个删除。材料是 [note_rbtree_augmented.c](../../../../labs/kernel/tree_basics/materials/note_rbtree_augmented.c)，构建入口仍是前节使用的 [Makefile](../../../../labs/kernel/tree_basics/materials/Makefile)。这里没有向其他任务发布任何节点，自动对象一直活到初始化返回；模块退出函数无待回收对象。

`inspect_summary()` 递归读取原始 last 独立计算期望值，逐个核对缓存，而不是拿缓存验证缓存。空子树返回 0 只因本例端点为非负无符号数，不能原封不动推广到允许负数的最大值问题。

`insert_range()` 的 `-EEXIST` 表示相同起点已存在，`-EINVAL` 表示无效范围或实验检查失败。返回失败时没有发布新对象，也没有改动已有摘要；它不提供在树节点上重复插入同一成员的通用防护，调用者仍须保证每个待插成员尚未挂接。

```c
// SPDX-License-Identifier: GPL-2.0
/* 闭区间摘要：私有自动对象，不发布并发入口。 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rbtree_augmented.h>
#include <linux/errno.h>

struct range_item {
    unsigned long first;
    unsigned long last;
    unsigned long subtree_last;
    struct rb_node rb;
};

static unsigned long item_last(struct range_item *item)
{
    return item->last;
}

RB_DECLARE_CALLBACKS_MAX(static, range_callbacks, struct range_item, rb,
                        unsigned long, subtree_last, item_last)

static int insert_range(struct rb_root *root, struct range_item *item)
{
    struct rb_node **link = &root->rb_node;
    struct rb_node *parent = NULL;
    if (item->first > item->last)
        return -EINVAL;
    while (*link) {
        struct range_item *entry = rb_entry(*link, struct range_item, rb);
        parent = *link;
        if (item->first < entry->first)
            link = &parent->rb_left;
        else if (item->first > entry->first)
            link = &parent->rb_right;
        else
            return -EEXIST; /* 重复起点失败时尚未改任何祖先摘要。 */
    }
    item->subtree_last = item->last;
    rb_link_node(&item->rb, parent, link);
    range_callbacks.propagate(parent, NULL);
    rb_insert_augmented(&item->rb, root, &range_callbacks);
    return 0;
}

/* 独立递归读取原始 last，不拿缓存字段计算期望最大值。 */
static unsigned long inspect_summary(struct rb_node *node, bool *valid)
{
    struct range_item *item;
    unsigned long result, child;
    if (!node)
        return 0;
    item = rb_entry(node, struct range_item, rb);
    result = item->last;
    child = inspect_summary(node->rb_left, valid);
    if (child > result)
        result = child;
    child = inspect_summary(node->rb_right, valid);
    if (child > result)
        result = child;
    if (item->subtree_last != result)
        *valid = false;
    return result;
}

/* 返回任意覆盖 point 的闭区间；只能在本例独占且摘要正确时使用。 */
static struct range_item *find_point(struct rb_root *root, unsigned long point)
{
    struct rb_node *node = root->rb_node;
    while (node) {
        struct range_item *item = rb_entry(node, struct range_item, rb);
        if (node->rb_left) {
            struct range_item *left = rb_entry(node->rb_left, struct range_item, rb);
            if (left->subtree_last >= point) {
                node = node->rb_left;
                continue;
            }
        }
        if (item->first > point)
            return NULL;
        if (item->last >= point)
            return item;
        node = node->rb_right;
    }
    return NULL;
}

static int __init note_augmented_init(void)
{
    struct range_item items[] = {
        {.first=20,.last=21}, {.first=10,.last=12}, {.first=30,.last=80},
        {.first=25,.last=29}, {.first=35,.last=40}, {.first=5,.last=6}
    };
    struct rb_root root = RB_ROOT;
    struct range_item *found;
    bool valid = true;
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(items); ++i) {
        if (insert_range(&root, &items[i]))
            return -EINVAL;
        inspect_summary(root.rb_node, &valid);
        if (!valid)
            return -EINVAL;
    }
    found = find_point(&root, 72);
    if (found != &items[2])
        return -EINVAL;
    pr_info("note_augmented: max=80, point72 finds [30,80]\n");

    /* 排序起点不变，只改载荷；从该节点重算，不预先覆盖旧摘要。 */
    items[2].last = 33;
    range_callbacks.propagate(&items[2].rb, NULL);
    if (inspect_summary(root.rb_node, &valid) != 40 || !valid ||
        find_point(&root, 72))
        return -EINVAL;
    pr_info("note_augmented: payload shrinks, max=40, point72 absent\n");

    for (i = 0; i < ARRAY_SIZE(items); ++i) {
        rb_erase_augmented(&items[i].rb, &root, &range_callbacks);
        inspect_summary(root.rb_node, &valid);
        if (!valid)
            return -EINVAL;
    }
    if (root.rb_node)
        return -EINVAL;
    pr_info("note_augmented: all removals preserve summaries\n");
    return 0;
}

static void __exit note_augmented_exit(void)
{
    /* 自动存储对象已在初始化返回前结束，未向外部发布地址。 */
}
module_init(note_augmented_init);
module_exit(note_augmented_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Private augmented rbtree interval summary exercise");
```

程序中的 `find_point()` 每层只选一条路，返回任意一个覆盖区间，不枚举所有重叠项。为什么左摘要足够大时可以直接向左？若当前起点大于查询点，当前与右子树都不能命中，只需继续在左边找；即使左边最后也没有答案，也没有丢失右边的候选。若当前起点不大于查询点，左子树所有起点都更小；左摘要至少等于查询点，便保证左侧确实存在一个足够长的区间。沿这套选择向下走，遇到当前起点已大于点即可退出；在排除左边且自己未覆盖后，才进入右边。这依赖起点有序与摘要精确共同成立，单独平衡没有用。

每步下降一层，所以树有效时这次点查找走 O(log n) 高度；程序中的递归审查仍是 O(n)，用于教学检查而非热点查询。先预测再在与目标内核匹配、已配置并准备的构建树上运行：

```bash
# 在仓库根目录，使用与目标内核匹配的构建树。
make -C "$KERNEL_BUILD" M="$PWD/labs/kernel/tree_basics/materials" modules
# 只在上述构建对应的目标内核上装入这个模块。
sudo insmod labs/kernel/tree_basics/materials/note_rbtree_augmented.ko
sudo dmesg | tail -n 20
sudo rmmod note_rbtree_augmented
```

三个业务日志的预期内容为：

```text
note_augmented: max=80, point72 finds [30,80]
note_augmented: payload shrinks, max=40, point72 absent
note_augmented: all removals preserve summaries
```

本轮实际执行的是宿主固定算法的明确位宽适配夹具与 ARMv7 前端语法检查，并未执行目标 Kbuild、MODPOST 或装卸。宿主覆盖 720 种插入次序各配 12 种移除次序，包含端点增减、重复/非法范围失败、120960 个稳定状态和 11007360 次逐点线性对照；这验证有限样本下的字段维护与查询语义，不是目标运行日志或真实并发证据。

#### (2)\_改变摘要之前先预测查询

1. 用根 `[20,21]`、左孩子 `[10,80]`、右孩子 `[30,40]` 手算摘要。将左摘要错误改成 12，查询 72 会走哪里？随后恢复，确认正确答案位于左侧。
2. 把左区间改为 `[10,12]` 并正确传播，再把左摘要错误写成 100，查询 35。它会被引向左树并返回空，错过右边 `[30,40]`。因此对这个不回溯算法，摘要偏大同样可能漏查；只有具体查询算法允许回溯等额外保证时，才可能把偏大限定为效率问题。
3. 保持树形，用前文“先改 last，再提前覆盖本节点摘要”的错误顺序，解释根为什么停留在 80。使用独立递归检查定位错误，恢复正确摘要后再继续下一项；不要在生产树注入这些错误。
4. 给区间增加相同起点，决定是拒绝、合并，还是添加稳定 id 作为第二排序键。修改比较与线性对照后重新检验；不能只删除 EEXIST 就默认所有查询契约没有变化。

回看三种回调，它们维护的是路径重算、位置接替和局部旋转三个不同事件。选择增强树的收益是让查询从精确子树信息中排除工作，代价是每个对象的存储、每次变更的维护和更强的同步不变量。数据小或更新远多于查询时，应保留直接扫描这一简单基线。现在我们能说明独占更新后哪些字段共同有效，尚未说明并发读者怎样获得这样的观察点；下一节继续讨论外部同步。

------


## 12.4\_rbtree\_与并发控制

### 12.4.1\_为什么\_rbtree\_核心不内置锁

Linux rbtree 不保存锁。

原因是不同使用场景的并发模型不同：

```text
有的树只在单线程初始化阶段使用；
有的树由 spinlock 保护；
有的树由 mutex 保护；
有的读侧走 RCU；
有的对象还有引用计数；
有的树嵌在更大的对象锁之下。
```

如果 rbtree 核心内置锁，会带来问题：

```text
锁类型无法统一；
锁粒度无法统一；
中断上下文和进程上下文需求不同；
可能和调用者已有锁重复；
无法处理对象生命周期。
```

所以 Linux rbtree 只提供结构操作。

并发保护由调用者决定。

------

### 12.4.2\_使用者需要保护哪些操作

至少需要保护：

```text
查找和插入之间的竞争；
两个插入之间的竞争；
插入和删除之间的竞争；
两个删除之间的竞争；
遍历和删除之间的竞争；
删除和对象释放之间的竞争；
替换和读者访问之间的竞争。
```

一个简单模型是：

```c
spin_lock(&tree->lock);
/* search / insert / erase / replace */
spin_unlock(&tree->lock);
```

如果查找结果要在解锁后使用，还需要：

```text
引用计数；
RCU；
对象生命周期保证；
或者复制数据而不是返回裸指针。
```

否则容易出现：

```text
查找到 item；
释放锁；
另一个 CPU 删除并释放 item；
当前 CPU 继续使用 item；
use-after-free。
```

------

### 12.4.3\_WRITE\_ONCE()\_在\_rbtree\_实现中的意义

`lib/rbtree.c` 开头有一段 lockless lookup 注释。

本节按[固定版本索引](../../../../research/source_reading/rbtree/navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界)核对；[P10 完整 C 实验](P10_Linux_6.12_内核_rbtree_查找与返回边界.md#10.2.10_用完整C程序观察相等节点和旧路径)已经展示旧根漏查和错误写序形成的环，这里把它落实为调用者的并发边界。

它强调：

```text
所有对 rb_left 和 rb_right 的树结构写入必须使用 WRITE_ONCE()。
同时，写入顺序不能在程序顺序中构造临时环。
```

目的不是提供完整无锁正确性。

在比较字段和对象寿命有效、并满足上述改边约束时，注释讨论的向下查询具有以下有限保证：

```text
读者不会因为临时结构看到循环而卡死；
遍历会最终结束；
如果读者返回某个元素，这个元素是正确的。
```

它不保证：

```text
读者一定能看到所有节点；
读者一定不会漏掉并发旋转影响的子树；
查找返回 NULL 就代表节点不存在；
对象生命周期自动安全。
```

`WRITE_ONCE()` 约束单次访问，不能替代第二条改边顺序要求；先建立反向边、后撤旧边仍会出现环。注释还明确不涵盖父指针的循环检查，所以 `rb_next()` 或 `rb_for_each()` 沿父链前进时，不能借用这项向下查询保证。整个论证也不自动授予对象的返回后寿命。

------

### 12.4.4\_RCU\_查找与普通修改路径的区别

RCU 相关接口包括：

```text
rb_link_node_rcu()
rb_find_rcu()
rb_replace_node_rcu()
```

它们分别解决不同问题。

`rb_link_node_rcu()`：

```text
用 rcu_assign_pointer() 发布新节点链接；
保证读者看到链接时，新节点基本字段已经初始化。
```

`rb_find_rcu()`：

```text
读侧用 rcu_dereference_raw() 读取左右孩子；
允许 RCU 读路径下降查找。
```

固定版的首次取根仍是普通 `tree->rb_node` 表达式，函数内没有建立读侧临界区、重扫或取得引用。真实语句见[查找实现](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.4_rb_find_rcu的孩子读取与缺失边界)；入口发布、读侧保护与结果处置仍由调用者证明。

`rb_replace_node_rcu()`：

```text
先准备 replacement；
最后用 RCU 方式更新父节点孩子槽或根槽；
相容读者从发布入口取得 new 时，读取发布前准备的载荷和前向结构。
```

孩子的 parent 在最后发布之前已经改向 new，因此这不保证父链遍历的原子快照。cached 包装又先更新最左缓存，不能直接当成 RCU 组合接口。完整交接与回收条件见[同键替换周期](P28_Linux同键替换与旧对象退出.md#28.2.4_rb_replace_node_rcu%28%29_与_RCU_读侧安全)及[缓存入口](P28_Linux同键替换与旧对象退出.md#28.2.5_rb_replace_node_cached%28%29_如何维护最左缓存)。

但是写侧修改仍然需要同步。

RCU 不是多个写者同时旋转、插入、删除的许可证。

采用对应 RCU 读侧寿命方案时，撤下对象后应按该方案等待旧读者；如果还有引用持有者，也必须满足它们的释放条件。典型流程是：

```text
rb_erase()
call_rcu()
对应宽限期结束，且其他持有权已满足释放条件后回收
```

------

### 12.4.5\_lockless\_lookup\_能保证什么\_不能保证什么

可以把 lockless lookup 的保证写成两列。以下仍以正确发布、寿命受保护、比较字段稳定、孩子写入及其顺序符合上一节约束为前提，仅讨论向下查找。

能保证：

```text
不会因为临时环导致无限循环；
遍历到的节点是有效结构节点；
如果找到了匹配元素，它是正确的。
```

不能保证：

```text
一定找到并发存在的节点；
一定看到完整树结构；
不需要锁或 RCU 生命周期；
删除对象后可以立即释放；
多个写者可以无锁并发修改。
```

所以工程上不能把 rbtree 当成自动无锁容器。

更准确的理解是：

```text
rbtree 的指针写入方式尽量不给无锁读者制造灾难；
但正确并发语义仍然由调用者设计。
```

------

### 12.4.6\_本节小结

并发部分的结论：

```text
第一，rbtree 不内置锁，因为锁模型属于使用场景。

第二，调用者必须保护 search/insert/erase/replace 的并发关系。

第三，返回业务对象指针时必须处理生命周期。

第四，孩子访问约束与不成环的改边顺序共同支持有限的向下路径，但不保证查找完整性或父链遍历。

第五，RCU 接口只处理读侧访问和发布顺序，不替代写侧同步。
```

------

## 12.5\_Linux\_内核\_rbtree\_示例代码

### 12.5.1\_示例目标与约束

下面构造一个最小示例：

```text
按 int key 管理 demo_rb_item；
不允许重复 key；
使用 spinlock 保护树；
插入、查找、删除都使用同一套比较规则；
删除后返回对象，由调用者释放。
```

这不是完整内核模块，只是展示 rbtree 使用骨架。

------

### 12.5.2\_定义业务结构体与树对象

```c
struct demo_rb_item {
	int key;
	int value;
	struct rb_node rb;
};

struct demo_rb_tree {
	struct rb_root root;
	spinlock_t lock;
	unsigned int count;
};
```

初始化：

```c
static void demo_tree_init(struct demo_rb_tree *tree)
{
	tree->root = RB_ROOT;
	spin_lock_init(&tree->lock);
	tree->count = 0;
}
```

这里 `struct rb_root` 只保存根节点。

锁和计数都是业务层自己加的。

------

### 12.5.3\_实现统一比较函数

```c
static int demo_cmp_key(int key, const struct demo_rb_item *item)
{
	if (key < item->key)
		return -1;
	if (key > item->key)
		return 1;
	return 0;
}

static int demo_cmp_item(const struct demo_rb_item *a,
			 const struct demo_rb_item *b)
{
	return demo_cmp_key(a->key, b);
}
```

这样查找和插入都能使用同一套 key 规则。

这比到处手写 `<`、`>` 更不容易写偏。

------

### 12.5.4\_实现查找

```c
static struct demo_rb_item *
demo_search_locked(struct demo_rb_tree *tree, int key)
{
	struct rb_node *node = tree->root.rb_node;

	while (node) {
		struct demo_rb_item *item;
		int cmp;

		item = rb_entry(node, struct demo_rb_item, rb);
		cmp = demo_cmp_key(key, item);

		if (cmp < 0)
			node = node->rb_left;
		else if (cmp > 0)
			node = node->rb_right;
		else
			return item;
	}

	return NULL;
}
```

函数名里带 `_locked`，表示调用者必须已经持有 `tree->lock`。

这是内核代码中常见的命名习惯：

```text
把锁语义写进函数名，避免误用。
```

------

### 12.5.5\_实现插入

```c
static int demo_insert(struct demo_rb_tree *tree,
		       struct demo_rb_item *item)
{
	struct rb_node **link = &tree->root.rb_node;
	struct rb_node *parent = NULL;

	spin_lock(&tree->lock);

	while (*link) {
		struct demo_rb_item *this;
		int cmp;

		parent = *link;
		this = rb_entry(parent, struct demo_rb_item, rb);
		cmp = demo_cmp_item(item, this);

		if (cmp < 0)
			link = &parent->rb_left;
		else if (cmp > 0)
			link = &parent->rb_right;
		else {
			spin_unlock(&tree->lock);
			return -EEXIST;
		}
	}

	rb_link_node(&item->rb, parent, link);
	rb_insert_color(&item->rb, &tree->root);
	tree->count++;

	spin_unlock(&tree->lock);
	return 0;
}
```

这里有几个关键点：

```text
发现重复 key 时，不调用 rb_link_node()；
只有成功找到空 link 后才挂接；
rb_link_node() 后立刻 rb_insert_color()；
count 在修复完成后增加；
整个修改路径在锁内完成。
```

------

### 12.5.6\_实现删除

```c
static int demo_remove(struct demo_rb_tree *tree, int key,
		       struct demo_rb_item **removed)
{
	struct demo_rb_item *item;

	if (!removed)
		return -EINVAL;

	*removed = NULL;

	spin_lock(&tree->lock);

	item = demo_search_locked(tree, key);
	if (!item) {
		spin_unlock(&tree->lock);
		return -ENOENT;
	}

	rb_erase(&item->rb, &tree->root);
	RB_CLEAR_NODE(&item->rb);
	tree->count--;
	*removed = item;

	spin_unlock(&tree->lock);
	return 0;
}
```

这个函数只从树中摘除节点，不释放对象。

调用者可以根据生命周期选择：

```c
kfree(item);
demo_item_put(item);
call_rcu(&item->rcu, demo_item_free_rcu);
```

这正是 Linux rbtree 的对象生命周期边界。

------

### 12.5.7\_实现中序遍历

```c
static void demo_dump_locked(struct demo_rb_tree *tree)
{
	struct rb_node *node;

	for (node = rb_first(&tree->root); node; node = rb_next(node)) {
		struct demo_rb_item *item;

		item = rb_entry(node, struct demo_rb_item, rb);
		pr_info("key=%d value=%d\n", item->key, item->value);
	}
}
```

中序遍历输出顺序就是 key 从小到大。

如果遍历期间可能有并发修改，应按协议稳定遍历依赖的拓扑并保护对象寿命。仅延迟释放不能稳定父链；中序循环的条件见[有序推进](P27_Linux有序遍历与整树销毁.md#27.2.8_为什么一个保存的地址仍会漏访)。

------

### 12.5.8\_实现整棵树清理

一种简单清理方式是反复取最小节点：

```c
static void demo_clear(struct demo_rb_tree *tree)
{
	struct rb_node *node;

	spin_lock(&tree->lock);
	while ((node = rb_first(&tree->root))) {
		struct demo_rb_item *item;

		item = rb_entry(node, struct demo_rb_item, rb);
		rb_erase(node, &tree->root);
		RB_CLEAR_NODE(node);
		tree->count--;
		spin_unlock(&tree->lock);

		kfree(item);

		spin_lock(&tree->lock);
	}
	spin_unlock(&tree->lock);
}
```

也可以使用后序遍历做销毁，但要注意第 11 章讲过的限制：

```text
postorder safe 不适合循环体中随意 rb_erase() 导致重平衡后继续依赖原遍历关系。
```

最保守的写法是：

```text
每次 rb_first()；
每次 rb_erase()；
直到树空。
```

每次重新取最左可以避开上一次游标因旋转失效的问题，但上面每轮释放锁，整个清理不是原子事务。要保证最终清空，还须停止或约束其他写者继续插入；kfree 也要求没有外部持有者。后序整树回收需要更强的排他销毁前提，完整对照见[遍历与销毁模块](P27_Linux有序遍历与整树销毁.md#27.2.9_运行完整遍历与销毁模块)。

------

### 12.5.9\_示例代码的边界

这个示例没有覆盖：

```text
RCU 读侧；
引用计数；
重复 key；
cached rbtree；
augmented rbtree；
错误注入；
模块参数；
调试断言。
```

但它覆盖了普通 rbtree 使用的核心闭环：

```text
定义对象；
嵌入 rb_node；
统一比较；
查找；
插入；
删除；
遍历；
生命周期交给调用者。
```

------

## 12.6\_Linux\_rbtree\_调试与验证

### 12.6.1\_如何验证\_BST\_有序性

最直接的方法是中序遍历。

遍历时记录上一个 key：

```text
prev_key <= current_key
```

如果不允许重复：

```text
prev_key < current_key
```

一旦出现逆序，说明：

```text
插入比较规则错误；
替换节点 key 错误；
手写 search / insert 不一致；
或者某处错误修改了 rb_left / rb_right。
```

红黑修复不会主动检查业务 key。

所以 BST 有序性验证必须由业务层或调试工具完成。

------

### 12.6.2\_如何验证父指针正确性

递归或栈遍历整棵树，对每个节点检查：

```text
如果 node->rb_left 存在：
	rb_parent(node->rb_left) == node

如果 node->rb_right 存在：
	rb_parent(node->rb_right) == node
```

根节点检查：

```text
rb_parent(root->rb_node) == NULL
```

父指针错误常见来源：

```text
手写旋转错误；
错误使用 rb_replace_node()；
破坏 __rb_parent_color；
把节点重复插入不同树；
删除后继续把旧节点当树中节点使用。
```

------

### 12.6.3\_如何验证红节点没有红孩子

遍历每个节点：

```text
如果 node 是红色：
	left 必须是 NULL 或黑色；
	right 必须是 NULL 或黑色。
```

Linux 中 NULL 叶子按黑色理解。

所以检查逻辑是：

```text
NULL 不算红；
非 NULL 才需要 rb_is_red()。
```

如果出现红红冲突，重点排查：

```text
插入后是否忘记 rb_insert_color()；
删除修复是否被跳过；
是否手动改过颜色；
是否误用 rb_replace_node() 替换了不等价节点。
```

------

### 12.6.4\_如何验证所有路径黑高一致

黑高验证可以递归实现。

对每个节点：

```text
左子树黑高；
右子树黑高；
二者必须相等；
当前节点是黑色则返回子树黑高 + 1；
当前节点是红色则返回子树黑高。
```

NULL 叶子按黑色叶子处理时，要统一计数规则。

可以约定：

```text
NULL 返回 1；
黑色实体节点在子树黑高基础上 +1；
红色实体节点不增加。
```

也可以约定：

```text
NULL 返回 0；
只统计实体黑节点。
```

关键是整棵检查使用同一套规则。

黑高不一致通常说明：

```text
删除黑色节点后没有正确修复；
Case 2 向上推进处理错；
Case 4 染色错；
父指针或旋转导致子树接错。
```

------

### 12.6.5\_如何验证\_cached\_rbtree\_的\_rb\_leftmost

cached 验证很简单：

```text
rb_first(&root->rb_root) == root->rb_leftmost
```

如果不相等，说明 cached 信息失效。

常见原因：

```text
插入时 leftmost 参数算错；
对 cached tree 调用了普通 rb_insert_color()；
删除时调用了普通 rb_erase()；
替换最左节点时调用了普通 rb_replace_node()；
手动移动节点但没有维护 rb_leftmost。
```

------

### 12.6.6\_如何验证\_augmented\_rbtree\_的增强信息

增强树验证要按业务字段重算。

例如增强字段是子树最大 end：

```text
expected = node->end;
if (left)
	expected = max(expected, left->subtree_max);
if (right)
	expected = max(expected, right->subtree_max);
node->subtree_max 必须等于 expected。
```

可以整树递归重新计算一遍，并与节点保存值比较。

如果错误，重点排查：

```text
插入搜索路径上是否更新了增强信息；
rotate 回调是否正确；
copy 回调是否正确；
删除 successor 原路径是否 propagate；
是否混用了普通 rb_insert_color() / rb_erase()。
```

------

### 12.6.7\_如何构造插入修复测试序列

可以构造三类插入序列。

父红叔红：

```text
插入形成 4-node 分裂。
例如先让祖父有两个红孩子，再向其中一个红孩子下插入。
```

内侧结构：

```text
LR：插入 30, 10, 20
RL：插入 10, 30, 20
```

外侧结构：

```text
LL：插入 30, 20, 10
RR：插入 10, 20, 30
```

这些序列能触发：

```text
Case 1 染色；
Case 2 预旋转；
Case 3 最终旋转。
```

------

### 12.6.8\_如何构造删除修复测试序列

删除修复测试更适合从目标形态反推。

要覆盖：

```text
兄弟红；
兄弟黑双侄黑；
兄弟黑近侄红；
兄弟黑远侄红。
```

测试思路：

```text
先构造一棵合法红黑树；
选择删除一个黑色叶子或黑色单子树位置；
观察 rebalance parent、sibling、near nephew、far nephew。
```

不要只看最终中序结果。

删除测试应该同时验证：

```text
BST 有序性；
父指针；
红红冲突；
黑高一致；
root 为黑；
遍历前驱后继；
cached / augmented 信息。
```

------

### 12.6.9\_本节小结

调试验证要分层：

```text
BST 层：
	中序顺序。

结构层：
	父指针、root、左右孩子。

红黑层：
	根黑、红节点无红孩子、黑高一致。

工程扩展层：
	cached leftmost、augmented 字段。

生命周期层：
	删除后不再通过树访问、对象释放安全。
```

只验证中序遍历不够。

一棵树可能中序顺序正确，但红黑性质已经坏了，后续复杂插入删除迟早出问题。

------

## 12.7\_Linux\_rbtree\_常见误区

### 12.7.1\_误以为内核\_rbtree\_会自动比较\_key

不会。

`struct rb_node` 不保存 key。

比较逻辑必须由调用者提供。

------

### 12.7.2\_误以为\_rb\_link\_node()\_已完成红黑修复

没有。

`rb_link_node()` 只做 BST 挂接。

挂接后必须调用：

```c
rb_insert_color()
```

或增强树版本：

```c
rb_insert_augmented()
```

------

### 12.7.3\_误以为\_rb\_erase()\_会释放业务对象

不会。

`rb_erase()` 只摘除 `rb_node`。

业务对象释放由调用者决定。

------

### 12.7.4\_误以为\_rb\_replace\_node()\_可以替换任意\_key

不能。

`rb_replace_node()` 不重新比较。

replacement 必须保持相同排序位置。

------

### 12.7.5\_误以为遍历时可以任意删除节点

不能。

`rb_erase()` 可能旋转，破坏遍历过程中预期的结构关系。

删除遍历要专门设计。

------

### 12.7.6\_误以为\_rbtree\_自带并发保护

没有。

锁、RCU、引用计数都属于调用者责任。

------

### 12.7.7\_误以为\_RCU\_接口让所有修改路径都无锁安全

不会。

RCU 接口主要处理读侧访问和发布顺序。

多个写者之间仍然需要同步。

------

### 12.7.8\_误以为\_cached\_/\_augmented\_会自动维护业务字段

不会。

cached 需要正确维护 `leftmost`。

augmented 需要正确提供并调用回调。

------

## 12.8\_Linux\_rbtree\_在内核中的典型使用场景

### 12.8.1\_高精度定时器为什么适合缓存最小节点

高精度定时器关心：

```text
下一个最早到期的定时器是谁？
```

这就是最小 key 查询。

如果 key 是到期时间，最早到期就是最左节点。

因此 cached rbtree 非常适合这种场景：

```text
插入删除保持有序；
rb_first_cached() O(1) 获取最早到期事件。
```

------

### 12.8.2\_调度实体按虚拟运行时间排序的思路

调度器需要在可运行实体中选择合适对象。

如果按虚拟运行时间排序：

```text
vruntime 小的实体更靠左；
最左节点代表当前最应该运行的实体之一。
```

rbtree 能提供：

```text
动态插入；
动态删除；
按 vruntime 排序；
快速找到最小 vruntime。
```

这种场景同样能从 leftmost 缓存受益。

------

### 12.8.3\_I/O\_调度与按位置排序

I/O 请求可能按扇区、偏移或设备位置排序。

有序结构可以支持：

```text
找到相邻请求；
合并相邻区间；
按位置选择下一个请求；
减少随机跳转成本。
```

哈希表适合等值查找，但不适合前驱后继和范围邻近关系。

rbtree 的中序关系正适合这类需求。

------

### 12.8.4\_epoll\_等对象集合为什么可能需要有序管理

某些对象集合不仅需要保存对象，还需要：

```text
按 fd、地址、时间或其他 key 管理；
快速查找；
有序遍历；
插入删除稳定。
```

rbtree 可以作为底层有序集合。

但是否使用 rbtree，要看具体内核版本和具体子系统实现。

不要把“某场景历史上用过 rbtree”理解成“永远必须用 rbtree”。

------

### 12.8.5\_VMA\_历史上使用\_rbtree\_与后来转向\_Maple\_Tree\_的原因

VMA 是虚拟内存区域。

它天然是范围结构：

```text
[start, end)
```

历史上可以用 rbtree 按起始地址组织 VMA。

这样能支持：

```text
按地址查找所在 VMA；
查找前驱后继；
插入删除区间。
```

但 VMA 管理不是单纯的点 key 有序集合。

它更偏向：

```text
范围查找；
范围更新；
减少锁竞争；
更适合缓存和批量遍历的数据结构。
```

Maple Tree 为这类非重叠范围提供容器接口。完整取舍与 G/H 更新实例见[P14](P14_Maple_Tree_与_VMA_管理.md#14.1_一个地址为什么需要三种查询)，不能仅因实现更新就认定所有负载更快。

更准确地说，新内核的 VMA 管理已经从传统：

```text
mm_struct
	-> mmap        // VMA 链表
	-> mm_rb       // VMA 红黑树
```

转向：

```text
mm_struct
	-> mm_mt       // maple_tree
```

也就是：

```c
struct maple_tree mm_mt;
```

VMA 查找、遍历、插入、删除更多走 `maple_tree` / `vma_iterator` 这一套。

Maple Tree 官方文档把它描述为一种 B-Tree 数据类型，优化用于保存非重叠范围，支持范围迭代、cache-efficient 的 previous / next 访问和 RCU-safe 模式，并明确说它最重要的用途是跟踪 VMA。([Linux Kernel Documentation](https://docs.kernel.org/core-api/maple_tree.html))

Maple Tree 引入补丁系列也明确提到，它替换了 VMA 管理里的 augmented rbtree、VMA cache 和 VMA linked list；补丁组织中还包含从 `mm_struct` 移除 rbtree、引入 VMA iterator 等修改。([LKML](https://lkml.iu.edu/2202.1/09876.html))

但这句话不能扩大成：

```text
新内核已经不用 rbtree；
任务管理也改成 Maple Tree；
所有有序集合都应该换成 Maple Tree。
```

更稳的边界是：

| 子系统 | 新内核主要结构或方向 |
| --- | --- |
| 虚拟内存 VMA 管理 | Maple Tree |
| 页缓存 / 一些整数 ID 索引 | XArray / radix tree 演进 |
| 普通内核有序集合 | rbtree 仍然大量存在 |
| 公平调度任务选择 | 从 CFS 语义走向 EEVDF，不是 Maple Tree |

任务调度容易和这里混淆。

老 CFS 经典讲法里，可运行实体按 `vruntime` 组织在 `tasks_timeline` 红黑树上。

新内核公平调度的核心语义转向 EEVDF，关注的是 lag 和 virtual deadline，选择 eligible 且虚拟截止时间更早的任务。([Linux Kernel Documentation](https://docs.kernel.org/scheduler/sched-eevdf.html))

这属于调度算法语义变化，不是“调度任务也改用 Maple Tree”。

所以这里的学习重点是：

```text
rbtree 适合有序对象集合；
Maple Tree 更适合某些范围映射和 VMA 场景；
数据结构选择要看访问模式，而不是只看复杂度公式。
```

一句话总结：

```text
Maple Tree 主要替代的是内存管理里的 VMA rbtree / linked list 模型；
它不是泛泛替代整个内核中的 rbtree，也不是任务调度 EEVDF 的同义词。
```

------

### 12.8.6\_interval\_tree\_与\_augmented\_rbtree\_的关系

区间树是 augmented rbtree 的典型应用。

每个节点保存：

```text
区间起点；
区间终点；
子树最大终点。
```

按起点排序形成 BST。

查询某个区间是否重叠时，根据子树最大终点剪枝：

```text
如果左子树最大终点小于查询起点；
左子树不可能有重叠区间；
可以跳过。
```

这就是增强信息的价值：

```text
不改变 rbtree 的排序结构；
额外保存子树摘要；
让查询可以剪枝。
```

------

### 12.8.7\_从使用场景反推结构选择

可以用下面的方式选择结构：

| 需求 | 更可能适合的结构 |
| --- | --- |
| 小规模简单遍历 | 链表 |
| 等值查找 | 哈希表 |
| 整数索引到对象 | XArray / radix 类结构 |
| 有序 key、前驱后继、最小最大 | rbtree |
| 频繁取最小 key | cached rbtree |
| 需要子树聚合信息 | augmented rbtree |
| 范围映射和区间管理 | Maple Tree / interval tree 等 |

不要把 rbtree 当万能结构。

它解决的是：

```text
动态有序集合。
```

------

## 12.9\_本章小结

本章把 Linux rbtree 的工程扩展收束起来。

cached rbtree 的核心是：

```text
用 rb_leftmost 把 rb_first() 优化成 O(1)；
代价是插入、删除、替换时必须维护缓存。
```

augmented rbtree 的核心是：

```text
在普通红黑树结构之外维护子树增强信息；
通过 propagate、copy、rotate 三个回调覆盖路径传播、节点替换和旋转更新。
```

并发控制的核心是：

```text
rbtree 不内置锁；
WRITE_ONCE() 需要配合不成环的改边顺序，且向下查找保证不涵盖父链遍历；
RCU 接口处理发布和读侧访问；
完整并发语义仍然属于调用者。
```

调试验证的核心是：

```text
不能只验证中序顺序；
还要验证父指针、红黑性质、黑高、cached leftmost、augmented 字段和对象生命周期。
```

到这里，Linux 6.12 rbtree 的工程实现主线已经完整：

```text
基础结构
	↓
嵌入式节点与使用者接口
	↓
查找、插入与旋转修复
	↓
删除、遍历与替换
	↓
cached / augmented / 并发 / 验证 / 场景
```

接着先读[P34 页级索引与访问成本](P34_从多路节点到页级索引.md#34.1_高度相同不等于访问成本相同)，区分页请求、缓存和存储定位，再沿 P13 继续多路页布局和范围查询。
