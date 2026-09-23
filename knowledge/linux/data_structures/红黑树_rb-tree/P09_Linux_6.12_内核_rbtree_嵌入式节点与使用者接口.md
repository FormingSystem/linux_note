---
id: knowledge.linux.data_structures.红黑树_rb-tree.p09_linux_6.12_内核_rbtree_嵌入式节点与使用者接口
title: "Linux 6.12 内核 rbtree 嵌入式节点与使用者接口"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
---

# 第9章\_Linux\_6.12\_内核\_rbtree\_嵌入式节点与使用者接口

## 9.1\_章节内容说明

前一章能解释根与节点保存什么；这一章沿一个业务对象接入、使用、摘除的过程，补上这些存储关系由谁建立和保持。

### 9.1.1\_本章在\_Linux\_rbtree\_学习路线中的位置

[第 8 章](P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#8.2.7_把排序契约变成可观察结果)已经用到期任务区分排序索引与对象，又说明根槽、嵌入节点与父色编码。本章继续向使用者视角推进，重点回答一个问题：Linux rbtree 不是现成的泛型 map，使用者究竟要怎样把业务对象、排序规则、生命周期和 rbtree 底层接口组合起来。

### 9.1.2\_本章要解决的核心问题

本章重点讲清楚四件事：第一，为什么 `struct rb_node` 要嵌入业务结构体；第二，为什么 key、value、比较规则和对象释放都由调用者负责；第三，由[P37 完整框架](P37_构建rbtree调用者接口.md#37.16_运行完整的私有调用者框架)落实查找、插入与删除；第四，并发保护和节点生命周期为什么不能交给 rbtree 核心自动处理。

------

## 9.2\_嵌入式节点设计\_为什么内核不做泛型容器

仍用上一章的任务：同一个任务有 id 和到期时间，既希望按时间取下一个任务，也希望按 id 找回它。若为每个索引另分配包装节点，两个包装都要保存指向任务的地址，并分别处理分配失败和撤销关系。嵌入式设计把每个索引需要的链接字段直接放进任务中：索引可以独立，业务对象仍只有一个。下面先建立成员地址与对象地址的关系，再讨论比较、释放和并发责任。

先用一种外部包装设计作对照（这是教学模型，不概括所有用户态容器）：

```c
struct tree_node {
	int key;
	void *value;
	struct tree_node *left;
	struct tree_node *right;
	struct tree_node *parent;
};
```

也就是说，树节点本身保存 key、value、左右孩子、父节点，然后容器负责比较、插入、查找、删除。

但是 Linux 内核 rbtree 不是这样。

Linux 内核的 `struct rb_node` 只保存父色、右孩子与左孩子。固定版本从[源码总索引](../../../../research/source_reading/rbtree/navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界)进入，完整类型定义见[节点与对齐](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree_types.h.md#1.1_rb_node的三个字段与对齐)。这一节只使用其“嵌入成员”身份，不重复推导父色编码。

它不保存：

```text
key；
value；
compare 回调；
对象释放逻辑；
业务状态；
并发锁。
```

真正的业务对象需要自己定义，然后把 `struct rb_node` 嵌入进去：

```c
struct my_node {
	int key;
	int value;
	struct rb_node rb;
};
```

这就是 Linux 内核 rbtree 的基本使用模型：

```text
业务对象拥有 rb_node；
rbtree 只管理 rb_node；
调用者通过 rb_entry() 从 rb_node 找回业务对象。
```

整体关系如下：

```mermaid
graph TD
	rb_core["Linux rbtree 核心"]
	rb_node["struct rb_node"]
	rb_root["struct rb_root"]
	rb_fix["颜色修复 / 旋转 / 遍历"]

	user_obj["业务结构体"]
	user_key["业务 key"]
	user_value["业务 value"]
	user_cmp["比较规则"]
	user_lock["并发保护"]
	user_life["生命周期"]

	rb_core --> rb_node
	rb_core --> rb_root
	rb_core --> rb_fix

	user_obj --> user_key
	user_obj --> user_value
	user_obj --> rb_node
	user_obj --> user_cmp
	user_obj --> user_lock
	user_obj --> user_life

	classDef rb fill:#2f2f2f,stroke:#111,color:#fff,stroke-width:2px;
	classDef user fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;

	class rb_core,rb_node,rb_root,rb_fix rb;
	class user_obj,user_key,user_value,user_cmp,user_lock,user_life user;
```

这张图要表达的核心是：

```text
rbtree 核心不理解业务对象。
rbtree 核心只理解 rb_node。
```

所以 Linux rbtree 不是一个完整的“泛型 map 容器”，而是一个**可以嵌入业务对象内部的红黑树基础设施**。

------

### 9.2.1\_struct\_rb\_node\_为什么嵌入业务结构体

Linux 内核里，rbtree 的典型业务结构不是这样：

```c
struct rb_node {
	int key;
	void *value;
	struct rb_node *left;
	struct rb_node *right;
};
```

而是这样：

```c
struct my_node {
	int key;
	int value;
	struct rb_node rb;
};
```

也就是说：

```text
struct my_node 是业务对象；
key/value 是业务字段；
rb 是这个业务对象挂入红黑树所需的节点。
```

它的内存关系可以这样看：

```mermaid
graph TD
	my_node["struct my_node"]
	my_key["int key"]
	my_value["int value"]
	my_rb["struct rb_node rb"]

	rb_parent_color["__rb_parent_color"]
	rb_left["rb_left"]
	rb_right["rb_right"]

	my_node --> my_key
	my_node --> my_value
	my_node --> my_rb

	my_rb --> rb_parent_color
	my_rb --> rb_left
	my_rb --> rb_right

	classDef obj fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;
	classDef rb fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;

	class my_node,my_key,my_value obj;
	class my_rb,rb_parent_color,rb_left,rb_right rb;
```

这个模型里有一个关键方向：

```text
不是 rb_node 拥有 my_node；
而是 my_node 拥有 rb_node。
```

换句话说，红黑树节点只是业务对象里的一个成员。

这和 Linux 内核链表的设计完全一致。

链表通常这样用：

```c
struct my_obj {
	int id;
	struct list_head list;
};
```

红黑树通常这样用：

```c
struct my_obj {
	int key;
	struct rb_node rb;
};
```

哈希链表通常这样用：

```c
struct my_obj {
	int key;
	struct hlist_node hnode;
};
```

这是一种统一的内核设计风格：

```text
基础设施节点嵌入业务对象；
基础设施只维护节点关系；
业务代码通过节点反推出外层对象。
```

一个业务对象甚至可以同时嵌入多个基础设施节点：

```c
struct my_obj {
	int id;
	struct rb_node rb;
	struct list_head list;
	struct hlist_node hnode;
	struct kref ref;
};
```

图示如下：

```mermaid
graph TD
	biz_obj["struct my_obj"]
	key_field["业务字段 id/key"]
	rb_member["struct rb_node rb"]
	list_member["struct list_head list"]
	hash_member["struct hlist_node hnode"]
	ref_member["struct kref ref"]

	biz_obj --> key_field
	biz_obj --> rb_member
	biz_obj --> list_member
	biz_obj --> hash_member
	biz_obj --> ref_member

	classDef obj fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;
	classDef infra fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;

	class biz_obj,key_field obj;
	class rb_member,list_member,hash_member,ref_member infra;
```

这带来的直接好处是：一个对象可以根据不同需求，同时进入不同管理结构。

例如：

```text
按 key 挂入 rbtree；
按创建顺序挂入 list；
按 hash key 挂入 hash table；
通过 kref 管理引用计数。
```

这里的 kref 是引用计数设施，只有调用者在取得和放弃引用时执行相应协议，才参与寿命管理；把字段放进去不会自动保护任何一次树查找。同样，同一个 rb 成员只有一组父子链接，不能同时挂入两棵独立树。按时间和按 id 建立两个树索引，应各有一个 rb_node 成员，搜索哪棵树就按哪个成员还原对象。

对比两种设计。

普通泛型容器模型：

```mermaid
graph TD
	generic_node["泛型 tree_node"]
	data_ptr["void *data"]
	biz_obj["struct my_node"]
	key_field["key"]
	value_field["value"]

	generic_node --> data_ptr
	data_ptr --> biz_obj
	biz_obj --> key_field
	biz_obj --> value_field

	classDef generic fill:#ffebee,stroke:#c62828,color:#000,stroke-width:2px;
	classDef obj fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;

	class generic_node,data_ptr generic;
	class biz_obj,key_field,value_field obj;
```

Linux 内核嵌入式节点模型：

```mermaid
graph TD
	biz_obj["struct my_node"]
	key_field["key"]
	value_field["value"]
	rb_member["struct rb_node rb"]

	biz_obj --> key_field
	biz_obj --> value_field
	biz_obj --> rb_member

	classDef obj fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;
	classDef rb fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;

	class biz_obj,key_field,value_field obj;
	class rb_member rb;
```

两者的差异是：

```text
泛型容器：tree_node -> void *data -> 业务对象
内核 rbtree：业务对象内部直接包含 rb_node
```

在上述“另分配包装、再读 data”的设计中，嵌入节点省去包装分配和这一次 data 读取，调用者直接管理对象与索引成员。但代价也具体：业务类型必须预留字段，挂树期间对象地址必须稳定，加入另一个索引需要另一个成员。不能修改第三方对象布局、索引数量运行时变化，或希望索引有独立寿命时，外部包装仍有价值。是否减少缓存未命中还取决于实际布局与访问负载，不能由这个箭头图直接证明。

------

### 9.2.2\_container\_of()\_如何从\_rb\_node\_还原业务对象

既然 rbtree 里面挂的是 `struct rb_node`，那么遍历、查找、删除时，经常只能先拿到：

```c
struct rb_node *node;
```

但是业务代码真正需要的是：

```c
struct my_node *item;
```

这就需要从 `rb_node` 反推出它所在的业务结构体。

这件事由 `container_of()` 完成。

假设业务结构体如下：

```c
struct my_node {
	int key;
	int value;
	struct rb_node rb;
};
```

内存布局可以抽象为：

```mermaid
graph LR
	obj_start["struct my_node 起始地址"]
	key_mem["key"]
	value_mem["value"]
	rb_mem["rb"]
	obj_end["struct my_node 结束"]

	obj_start --> key_mem
	key_mem --> value_mem
	value_mem --> rb_mem
	rb_mem --> obj_end

	classDef obj fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;
	classDef rb fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;

	class obj_start,key_mem,value_mem,obj_end obj;
	class rb_mem rb;
```

如果现在只知道 `rb` 成员地址，那么可以通过成员偏移反推出整个结构体的起始地址。

公式是：

```text
外层结构体地址 = 成员地址 - 成员在结构体中的偏移量
```

对于上面的结构，就是：

```text
struct my_node 地址 = rb 成员地址 - offsetof(struct my_node, rb)
```

图示如下：

```mermaid
graph TD
	rb_addr["rb 成员地址"]
	offset_val["offsetof(struct my_node, rb)"]
	calc_node["rb 地址 - rb 成员偏移"]
	obj_addr["struct my_node 起始地址"]

	rb_addr --> calc_node
	offset_val --> calc_node
	calc_node --> obj_addr

	classDef calc fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;
	classDef obj fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;

	class rb_addr,offset_val,calc_node calc;
	class obj_addr obj;
```

这就是 `container_of()` 的本质。

它不是查表，不是搜索，也不是运行时反射。它就是一次地址计算。

可以把它理解成：

```text
已知房间地址；
已知房间距离大门的偏移；
反推出房子大门地址。
```

对应到结构体：

```text
已知 rb 成员地址；
已知 rb 在 struct my_node 中的偏移；
反推出 struct my_node 起始地址。
```

正向访问是：

```c
&item->rb
```

反向还原是：

```c
container_of(&item->rb, struct my_node, rb)
```

关系如下：

```mermaid
graph TD
	obj_addr["struct my_node *item"]
	member_addr["&item->rb"]
	forward_calc["item 起始地址 + rb 偏移"]
	backward_calc["rb 地址 - rb 偏移"]

	obj_addr --> forward_calc
	forward_calc --> member_addr

	member_addr --> backward_calc
	backward_calc --> obj_addr

	classDef forward fill:#e8f5e9,stroke:#2e7d32,color:#000,stroke-width:2px;
	classDef backward fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;

	class obj_addr,forward_calc,member_addr forward;
	class backward_calc backward;
```

`container_of()` 能工作，需要成员地址确实来自一个仍然存活的外层对象，并且类型与成员名匹配。它不接受任意整数地址，也不是接收 NULL 的查询接口。若查询可能失败，先判断节点非空，再还原对象。对象被释放或搬走后，旧地址不会因再次执行这个宏而有效。

编译器知道成员偏移，包括为对齐加入的空隙，因此不能按前几个字段大小手算。固定宏会做成员类型检查，但不同成员或不同外围类型可能拥有相同的成员类型；检查通过并不能证明所有权正确。类型写错时可能算出不同地址，也可能碰巧偏移相同、数值未变，后一种同样不能合法访问一个并不存在的目标类型对象。

例如，实际对象是：

```c
struct my_node {
	int key;
	struct rb_node rb;
};
```

但是你错误写成：

```c
struct other_node *bad;

bad = container_of(node, struct other_node, rb);
```

那么 `bad` 得到的就是错误对象。

因此调用者需要证明的是“这个地址确实是这个存活对象的这个成员”，而不只是“指针类型叫 rb_node”。固定 GNU C 实现、类型检查范围和 const 边界见[成员地址还原](../../../../research/source_reading/rbtree/source_explanations/include/linux/container_of.h.md#1.1_一次还原中的求值与类型检查)。下面用两个同类型成员亲自观察这个区别。

#### (1)\_两个嵌入成员还原同一个任务

本实验使用仓库保存的固定 container_of.h 与 rbtree_types.h。两个宿主适配头只把编译期断言、类型比较和 offsetof 接到 GCC；它们不是内核头文件的通用替代。完整程序保存为 [embedded_owner.c](../../../../labs/kernel/tree_basics/materials/embedded_owner.c)：

```c
/* GNU C 宿主实验：使用保存的固定宏；不调用树算法或父色指针编码。 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "../../../../research/source_reading/linux/include/linux/rbtree_types.h"
#include "../../../../research/source_reading/linux/include/linux/container_of.h"

struct job {
    unsigned int id;
    struct rb_node by_deadline;
    unsigned long deadline;
    struct rb_node by_id;
};

int main(void)
{
    struct job item = { .id = 7, .deadline = 40 };
    struct rb_node *deadline_node = &item.by_deadline;
    struct rb_node *id_node = &item.by_id;
    struct job *from_deadline = container_of(deadline_node, struct job, by_deadline);
    struct job *from_id = container_of(id_node, struct job, by_id);
    const struct rb_node *read_node = &item.by_id;
    const struct job *read_owner = container_of_const(read_node, struct job, by_id);
    struct job copy = item;

    /* 偏移由当前编译器布局决定，不能把某台机器的数值写死。 */
    size_t deadline_offset = offsetof(struct job, by_deadline);
    size_t id_offset = offsetof(struct job, by_id);
    assert(deadline_offset > 0 && id_offset > deadline_offset);
    assert(from_deadline == &item && from_id == &item && read_owner == &item);
    printf("offsets: deadline=%zu id=%zu\n", deadline_offset, id_offset);
    printf("same owner: %d; keys: %lu,%u\n",
           from_deadline == from_id, from_deadline->deadline, from_id->id);

    /* 成员类型相同不代表成员身份相同；这里只算整数，不构造错误指针。 */
    _Static_assert(__same_type(item.by_deadline, item.by_id), "same member type");
    printf("wrong member would shift origin by %zu bytes\n",
           id_offset - deadline_offset);

    /* 复制结构体不会让现有成员地址自动改指新对象。 */
    copy.id = 8;
    assert(container_of(id_node, struct job, by_id) == &item);
    assert(container_of(&copy.by_id, struct job, by_id) == &copy);
    printf("saved node owner id=%u; copied object id=%u\n", from_id->id, copy.id);

    /* 普通宏会丢掉 const；本例仅检查类型，不借此修改只读对象。 */
    _Static_assert(__same_type(container_of(read_node, struct job, by_id),
                              (struct job *)0), "ordinary result");
    _Static_assert(__same_type(container_of_const(read_node, struct job, by_id),
                              (const struct job *)0), "const result");
    puts("const owner preserved by container_of_const");
    return 0;
}
```

从仓库根目录在有 GCC 的 Bash 中执行；`-std=gnu11` 允许固定宏使用的 GNU C 扩展，`-I` 指定那两个实验适配头：

```bash
mkdir -p .cache/rb_owner
gcc -std=gnu11 -Wall -Wextra -Werror \
    -I labs/kernel/tree_basics/materials/hosted_include \
    labs/kernel/tree_basics/materials/embedded_owner.c \
    -o .cache/rb_owner/embedded_owner
.cache/rb_owner/embedded_owner
```

当前宿主的一次输出是：

```text
offsets: deadline=8 id=40
same owner: 1; keys: 40,7
wrong member would shift origin by 32 bytes
saved node owner id=7; copied object id=8
const owner preserved by container_of_const
```

先预测前两行。两个成员地址不同，但各自减去自己的偏移，都回到 item。偏移 8 和 40 只描述这次宿主布局；另一 ABI 可以不同，正确往返不依赖这两个常数。这里没有调用父色打包、旋转或树插入，不能把宿主程序当成 ARM 内核运行。

第三行只计算两个偏移的整数差：若拿 by_id 地址却减 by_deadline 的偏移，结果会落在对象首地址之后。程序刻意不构造或解引用这个错误对象指针。两个成员都是 rb_node，所以类型检查并不能发现成员名选错。

第四行说明结构体复制建立了另一个对象，却不改变先前保存的成员地址。示例的两个对象都活到 main 返回，因此可以安全比较；如果原对象先结束寿命，旧入口就失效，复制并不能修复它。正在树中的对象更不能直接 memcpy 后销毁原件，因为树中其他节点仍保存旧地址。

最后一行是 const 的边界。普通 container_of 明确丢失传入指针的 const 限定；`container_of_const` 依据指针类型保留它。只读类型不能代替寿命保护，反过来，普通宏给出了可写指针也不表示允许修改原本的只读对象。

#### (2)\_预测与修改

1. 在 id 前增加一个字符数组，再运行。哪些数值可能变化？偏移和对象大小可能变化，两个正确成员仍应回到同一个 item；不要预设没有填充。
2. 只把 from_id 那行的成员名改成 by_deadline，编译器必定拒绝吗？不会，两者类型相同。不要运行错误解引用，应先画出“传入偏移减所选偏移”的差，检查实际传入成员与成员名是否匹配。
3. 将 deadline_node 实参改为 `&item.id`，为什么应当编译失败？此时指向 unsigned int，却声明它是 rb_node，固定宏的静态断言能够检测这种类型不一致。
4. 能把 by_deadline 同时交给两棵树吗？不能，两棵树会改写同一组父子字段。应分别嵌入成员，再分别建立排序与同步协议。

到这里已经能找回对象，但还没有决定哪几个业务字段组成排序键。下一节的 rb_entry 只给这次还原命名，不替调用者回答排序问题。

------

### 9.2.3\_rb\_entry()\_的封装意义

在 rbtree 代码里，通常不会直接写：

```c
container_of(node, struct my_node, rb)
```

而是写：

```c
rb_entry(node, struct my_node, rb)
```

`rb_entry()` 本质上就是对 `container_of()` 的封装。

固定版本直接转发三个参数，唯一宏体见[rb_entry 实现](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.11_父地址与业务地址的两种还原)。它不补空指针检查，不取得引用，也不保留普通 container_of 丢失的 const。

它的意义不是新增能力，而是增强语义。

在红黑树上下文里：

```c
struct my_node *this;

this = rb_entry(parent, struct my_node, rb);
```

这句话读起来非常直接：

```text
parent 是一个 rb_node；
它嵌在 struct my_node 的 rb 成员里；
现在把它还原成 struct my_node。
```

图示如下：

```mermaid
graph TD
	rb_ptr["struct rb_node *node"]
	rb_entry_call["rb_entry(node,<br/> struct my_node, rb)"]
	my_obj["struct my_node *item"]
	key_access["item->key"]

	rb_ptr --> rb_entry_call
	rb_entry_call --> my_obj
	my_obj --> key_access

	classDef rb fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;
	classDef obj fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;

	class rb_ptr,rb_entry_call rb;
	class my_obj,key_access obj;
```

`rb_entry()` 和 `container_of()` 的关系可以这样看：

```mermaid
graph TD
	container_of_node["container_of"]
	rb_entry_node["rb_entry"]
	user_code["rbtree 使用者代码"]

	container_text["通用成员反推容器"]
	entry_text["rbtree 场景语义封装"]
	code_text["查找 / 插入 / 遍历 / 删除"]

	container_of_node --> rb_entry_node
	rb_entry_node --> user_code

	container_of_node --> container_text
	rb_entry_node --> entry_text
	user_code --> code_text

	classDef base fill:#eeeeee,stroke:#555,color:#000,stroke-width:2px;
	classDef rb fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;
	classDef user fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;

	class container_of_node,container_text base;
	class rb_entry_node,entry_text rb;
	class user_code,code_text user;
```

常见使用场景有三个。

查找时：

```c
while (node) {
	struct my_node *this;

	this = rb_entry(node, struct my_node, rb);

	if (key < this->key)
		node = node->rb_left;
	else if (key > this->key)
		node = node->rb_right;
	else
		return this;
}
```

遍历时：

```c
for (node = rb_first(root); node; node = rb_next(node)) {
	struct my_node *item;

	item = rb_entry(node, struct my_node, rb);
	pr_info("key=%d\n", item->key);
}
```

删除时，下面的两行只是“对象由 kmalloc 一类接口分配、已经撤掉其他索引、没有剩余使用者，且整个树修改受调用者保护”的简化情形；rb_erase 本身不能证明这些前提：

```c
rb_erase(&item->rb, root);
kfree(item);
```

这几个场景共同说明：

```text
rbtree API 操作 rb_node；
业务逻辑操作 my_node；
rb_entry() 是二者之间的转换桥梁。
```

------

### 9.2.4\_内核为什么不把\_key\_value\_放进\_struct\_rb\_node

很多人第一次看内核 rbtree 会疑惑：

```text
红黑树不是要比较 key 吗？
为什么 struct rb_node 里没有 key？
```

原因是：**内核里的 key 没有统一形态**。沿用任务例子，同一对象在到期索引中比较 deadline，在 id 索引中比较 id；若希望同一到期时间也有确定顺序，可以比较 `(deadline, id)`。两个树成员描述的是两种关系，不需要各复制一份业务数据。

不同子系统对“排序键”的定义完全不同。

例如：

```text
定时器：key 可能是 expires 到期时间；
调度器：key 可能是 vruntime；
区间索引：key 可能是地址起点；
输入输出调度：key 可能是磁盘扇区号；
驱动资源管理：key 可能是 id、地址、句柄；
区间管理：key 可能是 start/end 范围。
```

如果 `struct rb_node` 内置一个 key，会马上遇到问题：

```text
key 是 int 还是 unsigned long？
key 是 32 位还是 64 位？
key 是单字段还是多字段？
key 是普通标量还是区间？
key 相等时是否允许重复？
key 相等时是否还要比较第二关键字？
```

这些问题没有统一答案。

所以 Linux rbtree 的选择是：

```text
rb_node 不保存 key；
key 由业务对象自己保存；
比较规则由调用者自己实现。
```

简单 key 场景：

```c
struct my_node {
	int key;
	struct rb_node rb;
};
```

排序逻辑如下，`-EEXIST` 是“对象已存在”的错误返回，表示这个示例拒绝相同键。它只展示选孩子槽的片段，完整搜索与插入在[P37 调用者框架](P37_构建rbtree调用者接口.md#37.8_编写插入搜索函数)继续：

```c
if (item->key < this->key)
	link = &parent->rb_left;
else if (item->key > this->key)
	link = &parent->rb_right;
else
	return -EEXIST;
```

对应关系：

```mermaid
graph TD
	rb_20["key=20"]
	rb_10["key=10"]
	rb_30["key=30"]

	rb_20 -->|L| rb_10
	rb_20 -->|R| rb_30

	classDef black fill:#2f2f2f,stroke:#111,color:#fff,stroke-width:2px;
	class rb_20,rb_10,rb_30 black;
```

复合 key 场景：

```c
struct my_node {
	u32 major;
	u32 minor;
	struct rb_node rb;
};
```

排序规则可能是：

```text
先比较 major；
major 相等再比较 minor。
```

示意图如下：

```mermaid
graph TD
	rb_2_20["major=2 minor=20"]
	rb_1_50["major=1 minor=50"]
	rb_2_10["major=2 minor=10"]
	rb_3_01["major=3 minor=1"]

	rb_2_20 -->|L| rb_1_50
	rb_1_50 -->|R: larger major| rb_2_10
	rb_2_20 -->|R| rb_3_01

	classDef node fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;
	class rb_2_20,rb_1_50,rb_2_10,rb_3_01 node;
```

这是比较顺序允许的一种 BST 形状，不表达最终红黑颜色。`(2,10)` 小于根 `(2,20)`，但大于 `(1,50)`，所以它位于左子树中的右孩子槽；一个节点不能有两个左孩子。重点是：

```text
major/minor 合起来才是完整排序 key。
```

区间 key 场景：

```c
struct my_range {
	unsigned long start;
	unsigned long end;
	struct rb_node rb;
};
```

它可能按 `start` 排序：

```mermaid
graph TD
	range_3000["[3000, 3fff]"]
	range_1000["[1000, 1fff]"]
	range_5000["[5000, 5fff]"]

	range_3000 -->|L: start smaller| range_1000
	range_3000 -->|R: start larger| range_5000

	classDef range fill:#e8f5e9,stroke:#2e7d32,color:#000,stroke-width:2px;
	class range_3000,range_1000,range_5000 range;
```

但区间对象通常不只需要排序，还可能需要判断：

```text
某个地址是否落入区间；
两个区间是否重叠；
某个范围是否冲突；
子树内最大的 end 是多少。
```

按 start 排序只确定下行方向；“是否覆盖目标”还需检查 end，多区间重叠查询可能需要维护子树最大 end 等附加信息。排序索引不会自动知道这些聚合值，后面的增强树章节才负责解释其更新。

再回到两个任务索引：改 deadline 会破坏按时间的排序位置，却未必改变按 id 的位置。应先从受影响索引摘除，再修改键并重新接入；对象存在不等于所有索引都仍有效。具体重复键政策与更新路径将在后文继续展开。

所以内核 rbtree 不把 key/value 放进 `struct rb_node`，本质原因是：

```text
rb_node 是基础设施节点；
key/value 是业务语义；
内核不把业务语义塞进基础设施节点。
```

------

### 9.2.5\_比较规则与重复键由谁决定

上一节已经能从两个索引成员回到同一个任务，现在还差一条规则：到期时间相同的两个任务，是同一个键，还是仍按 id 区分？规则属于业务；写成回调还是展开在搜索循环里，是另一个选择。先看一种把比较函数指针 cmp 保存在容器里的设计：

```c
struct tree {
	struct rb_node *root;
	int (*cmp)(const void *a, const void *b);
};
```

然后插入时由容器内部调用：

```c
tree->cmp(new_obj, current_obj);
```

Linux rbtree 不要求把比较函数保存在根对象内；固定版本也允许通过参数传入比较函数。这里区分的是“根保存什么”和“调用时怎样比较”，不能据此说内核没有比较辅助接口。

它不会在 `struct rb_root` 里保存 `cmp`，也不会要求所有插入都走统一的：

```c
rb_insert(root, node, cmp);
```

根不绑定一种全局比较方式，让调用者可以在同一组对象上构造不同索引，也可以按自己的重复键契约选择帮助器或手写搜索。回调同样能表达业务语义；是否留下间接调用、是否内联，取决于编译器看到的目标与优化条件，下一节单独比较。

重复 key 策略就是最典型例子。

策略一：不允许重复 key。

```c
if (item->key < this->key)
	link = &parent->rb_left;
else if (item->key > this->key)
	link = &parent->rb_right;
else
	return -EEXIST;
```

策略二：允许重复 key，**本次插入搜索** 遇到相等键时继续向右。

```c
if (item->key < this->key)
	link = &parent->rb_left;
else
	link = &parent->rb_right;
```

这只是选空槽时的规则。三个相等键连续接到右侧，修复旋转后可以成为一个根和左右两个相等孩子；中序仍非递减。因此查找相等组不能假定相等对象永远只在右侧，应使用[最左匹配与后继](P10_Linux_6.12_内核_rbtree_查找与返回边界.md#10.2_rbtree_查找逻辑_手写_search_与内核辅助接口)的契约。

策略三：key 相同后继续比较稳定的业务 id，把 `(key, id)` 作为完整键。这里假定 id 在目标对象集合中唯一，且挂树期间不改变；若完整键也相同，就按本示例拒绝。

```c
if (item->key < this->key)
	link = &parent->rb_left;
else if (item->key > this->key)
	link = &parent->rb_right;
else if (item->id < this->id)
	link = &parent->rb_left;
else if (item->id > this->id)
	link = &parent->rb_right;
else
	return -EEXIST;
```

不要用两个独立分配对象指针的 `<` 关系直接代替业务 id：ISO C 对这类无共同数组/规定对象关系的指针不提供这里需要的通用顺序保证。即使某个平台约定可按地址整数排序，地址复用也未必满足业务身份的含义。稳定 id 明确了顺序来自哪里，但其唯一性仍须业务保证。

这三种键政策都可与红黑平衡共存，却给“相等”赋予不同含义。

```mermaid
graph TD
	cmp_rule["比较规则"]
	no_dup["不允许重复 key"]
	dup_right["重复 key 插右侧"]
	key_addr["key 相同再比较稳定 id"]

	no_dup_text["相等返回 -EEXIST"]
	dup_right_text["相等仍然插入"]
	key_addr_text["完整键相同则拒绝"]

	cmp_rule --> no_dup
	cmp_rule --> dup_right
	cmp_rule --> key_addr

	no_dup --> no_dup_text
	dup_right --> dup_right_text
	key_addr --> key_addr_text

	classDef root fill:#2f2f2f,stroke:#111,color:#fff,stroke-width:2px;
	classDef item fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;

	class cmp_rule root;
	class no_dup,dup_right,key_addr,no_dup_text,dup_right_text,key_addr_text item;
```

统一比较回调无法替调用者决定这些策略。

所以 Linux rbtree 的核心思路是：

```text
你自己写比较逻辑；
你自己决定重复 key 语义；
rbtree 只负责挂接后的颜色修复和结构维护。
```

当比较只服务一个短搜索循环时，就地写出方向和重复返回，容易把政策与控制流一起读清楚。

例如：

```c
while (*link) {
	parent = *link;
	this = rb_entry(parent, struct my_node, rb);

	if (item->key < this->key)
		link = &parent->rb_left;
	else if (item->key > this->key)
		link = &parent->rb_right;
	else
		return -EEXIST;
}
```

这段代码一眼就能看出：

```text
按照 item->key 排序；
小于走左边；
大于走右边；
相等认为重复。
```

若多个查询和插入都必须使用同一顺序，把规则集中成命名函数反而更便于保持一致。固定版本 [rb_find](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.1_rb_find的任意匹配)接收查询键与节点的比较器，[rb_find_add](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.7_查重后插入与RCU发布变体)接收节点间的三态比较器；[rb_add](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.6_不查重的rb_add)使用“是否小于”的布尔谓词。这些接口的签名和相等处理不同，不应只因为都叫比较就互换。

#### (1)\_让同一个比较规则走两种调用路径

任务到期时间相等时，id 是否参与比较，决定了它们是等价对象还是不同完整键。下面的完整 [job_compare.c](../../../../labs/kernel/tree_basics/materials/job_compare.c)只验证比较契约，不实现树，也不测速度：

```c
/* C11：比较规则与调用方式分开；不是 rbtree 实现或性能基准。 */
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

struct job_key {
    int deadline;
    unsigned int id;
};

static int compare_deadline(const struct job_key *a, const struct job_key *b)
{
    /* 先比较再相减布尔值，避免直接相减两个有符号键时溢出。 */
    return (a->deadline > b->deadline) - (a->deadline < b->deadline);
}

static int compare_job(const struct job_key *a, const struct job_key *b)
{
    int first = compare_deadline(a, b);
    if (first != 0)
        return first;
    return (a->id > b->id) - (a->id < b->id);
}

static int compare_through_callback(const struct job_key *a,
                                    const struct job_key *b,
                                    int (*compare)(const struct job_key *,
                                                   const struct job_key *))
{
    return compare(a, b);
}

int main(void)
{
    const struct job_key keys[] = {
        {INT_MIN, 0}, {40, 2}, {40, 4}, {INT_MAX, UINT_MAX}
    };
    size_t count = sizeof keys / sizeof keys[0];
    printf("same deadline: %d; full key: %d\n",
           compare_deadline(&keys[1], &keys[2]), compare_job(&keys[1], &keys[2]));
    printf("extreme signed keys: %d\n", compare_job(&keys[0], &keys[3]));
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < count; ++j) {
            int direct = compare_job(&keys[i], &keys[j]);
            int callback = compare_through_callback(&keys[i], &keys[j], compare_job);
            /* 表中完整键严格递增，数组下标给出独立的预期符号。 */
            int expected = (i > j) - (i < j);
            assert(direct == expected && callback == expected);
        }
    }
    puts("16 pairs: direct and callback agree; no timing claim");
    return 0;
}
```

从仓库根目录编译运行：

```bash
mkdir -p .cache/rb_compare
cc -std=c11 -Wall -Wextra -Werror -pedantic \
    labs/kernel/tree_basics/materials/job_compare.c \
    -o .cache/rb_compare/job_compare
.cache/rb_compare/job_compare
```

输出为：

```text
same deadline: 0; full key: -1
extreme signed keys: -1
16 pairs: direct and callback agree; no timing claim
```

第一行中 `(40,2)` 与 `(40,4)` 按时间比较相等，按完整键比较则前者小。比较函数的返回值只要求负、零、正的含义，不能随手写成两个 int 键相减；`INT_MIN` 和 `INT_MAX` 是 int 类型两端的可表示值，直接相减可能溢出。这里先做关系比较，再用取值为零或一的结果相减。

表中完整键已排序，所以两层循环用下标关系独立预测每次比较的符号。直接调用与经过函数指针参数的写法得到相同结果，只说明语义一致；优化器可能把后者也展开，运行输出不能证明机器码还存在间接调用。`UINT_MAX` 是 unsigned int 最大值，不是某个固定的十进制常数。

试着把 `(40,4)` 改成 `(40,2)`：现在两个不同数组元素的完整键相等，原来“下标严格递增对应严格键序”的断言会失败。先修正测试预期并决定重复政策，而不是偷偷让地址打破平局。再把 id 的比较方向反转，预测同时间任务的顺序；插入和查找必须一起采用新规则，不能只改其中一边。

------

### 9.2.6\_少一层函数指针间接调用带来的性能意义

内核 rbtree 不强制使用统一比较回调，手写路径可能消除函数指针间接调用。下文比较的是仍存在间接调用的代码形态；若编译器已内联或去虚拟化辅助接口的比较函数，这项差异可能消失，不能仅由 C 源码签名保证性能优势。

如果采用泛型插入模型，可能类似这样：

```c
int rb_insert_generic(struct rb_root *root,
		      struct rb_node *node,
		      int (*cmp)(struct rb_node *a, struct rb_node *b));
```

每走一层树，都要调用一次：

```c
cmp(node, current);
```

沿一条含 h 个实际节点的搜索路径，最多执行 h 次节点比较，可能在中途找到匹配就返回。红黑性质给出 h 的对数上界，并不是所有操作都恰好比较 log n 次；复杂字符串键的一次比较本身也不一定是常量成本。

如果每次比较都是函数指针间接调用，路径大概是：

```mermaid
graph TD
	search_start["查找 / 插入开始"]
	level_1["比较第 1 层"]
	callback_1["调用 cmp 回调"]
	level_2["比较第 2 层"]
	callback_2["调用 cmp 回调"]
	level_3["比较第 3 层"]
	callback_3["调用 cmp 回调"]
	search_end["找到节点或找到插入位置"]

	search_start --> level_1
	level_1 --> callback_1
	callback_1 --> level_2
	level_2 --> callback_2
	callback_2 --> level_3
	level_3 --> callback_3
	callback_3 --> search_end

	classDef path fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;
	classDef cost fill:#ffebee,stroke:#c62828,color:#000,stroke-width:2px;

	class search_start,level_1,level_2,level_3,search_end path;
	class callback_1,callback_2,callback_3 cost;
```

假设编译阶段无法确定 cmp 的目标，机器执行到某一层时先取得函数地址，再按调用约定交接参数与返回值，最后根据结果选孩子。处理器对间接分支的目标预测、调用约定的寄存器使用，以及跨调用优化受限，都可能增加成本；是否发生预测失败不能由源码断言。函数指针仍有声明的参数和结果类型，不是“用了指针就丢失 C 类型”。

同一层直接比较若被内联，编译器可以把取键、判断与选孩子一起优化。但若回调目标在此调用点已知，编译器也可能把它变成直接调用甚至内联。反过来，一个复杂比较即使写在循环里，也未必便宜；最终要观察实际编译配置下的机器码和目标负载。

而手写比较是这样：

```c
if (item->key < this->key)
	link = &parent->rb_left;
else if (item->key > this->key)
	link = &parent->rb_right;
else
	return -EEXIST;
```

它的路径更直接：

```mermaid
graph TD
	search_start["查找 / 插入开始"]
	level_1["读取 this->key"]
	direct_cmp_1["直接字段比较"]
	level_2["读取下一层 this->key"]
	direct_cmp_2["直接字段比较"]
	level_3["读取第三层 this->key"]
	direct_cmp_3["直接字段比较"]
	search_end["找到节点或找到插入位置"]

	search_start --> level_1
	level_1 --> direct_cmp_1
	direct_cmp_1 --> level_2
	level_2 --> direct_cmp_2
	direct_cmp_2 --> level_3
	level_3 --> direct_cmp_3
	direct_cmp_3 --> search_end

	classDef good fill:#e8f5e9,stroke:#2e7d32,color:#000,stroke-width:2px;
	class search_start,level_1,direct_cmp_1,level_2,direct_cmp_2,level_3,direct_cmp_3,search_end good;
```

两张图都画三次节点比较：调用方式本身不会让树少走一层。只有实际保留下来的指令路径不同，才有需要测量的调用成本差异；它不改变搜索的树高上界。

这并不是说函数指针比较一定不能用，而是说内核 rbtree 不把它作为强制模型。

总结一下：

- 先按所需返回值、重复政策和读写协议选择辅助接口；短规则需要跨多条路径一致时，集中比较函数有利于维护。
- 辅助接口无法表达特定搜索状态时，可以手写搜索，但仍应复用同一排序依据。
- 怀疑高频比较成为瓶颈时，在相同键序、树形、查询分布、编译配置和缓存条件下比较；先查实际机器码是否还保留间接调用，再测时间与事件。上面的语义程序不提供这个性能结论。

------

### 9.2.7\_节点嵌入式设计对缓存局部性的影响

上一节讨论执行比较的控制流，这一节追踪比较所读的数据。缓存局部性指近期或相邻访问能复用已进入缓存的数据；关键不是“属于同一个结构体”这句话，而是需要的字段实际落在哪些缓存行。缓存行是硬件以固定块维护的一段内存，具体大小和布局要按目标核对。

如果采用泛型容器设计，树节点和业务对象可能是两块内存。

```c
struct generic_rb_node {
	struct generic_rb_node *left;
	struct generic_rb_node *right;
	struct generic_rb_node *parent;
	void *data;
};

struct my_node {
	int key;
	int value;
};
```

访问 key 时路径是：

```text
先访问 generic_rb_node；
再通过 void *data 找到 my_node；
再访问 my_node->key。
```

图示如下：

```mermaid
graph TD
	generic_node["generic rb_node"]
	data_ptr["void *data"]
	my_obj["struct my_node"]
	key_field["key"]

	cache_a["缓存区域 A"]
	cache_b["缓存区域 B"]

	generic_node --> data_ptr
	data_ptr --> my_obj
	my_obj --> key_field

	generic_node --> cache_a
	my_obj --> cache_b

	classDef node fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;
	classDef cache fill:#ffebee,stroke:#c62828,color:#000,stroke-width:2px;

	class generic_node,data_ptr,my_obj,key_field node;
	class cache_a,cache_b cache;
```

而 Linux 内核嵌入式节点是：

```c
struct my_node {
	int key;
	int value;
	struct rb_node rb;
};
```

访问路径是：

```text
rb_node 在 my_node 内部；
通过 rb_entry() 回到 my_node；
key 和 rb 通常在同一个业务对象附近。
```

图示如下：

```mermaid
graph TD
	my_obj["struct my_node"]
	key_field["key"]
	value_field["value"]
	rb_field["struct rb_node rb"]
	cache_near["字段更可能位于相近缓存区域"]

	my_obj --> key_field
	my_obj --> value_field
	my_obj --> rb_field
	my_obj --> cache_near

	classDef obj fill:#e8f5e9,stroke:#2e7d32,color:#000,stroke-width:2px;
	classDef rb fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;

	class my_obj,key_field,value_field,cache_near obj;
	class rb_field rb;
```

这里不能绝对说 `key` 和 `rb` 一定在同一条 cache line 里，因为这取决于结构体布局、字段顺序、对象大小、分配器和对齐方式。

在上图指定的“包装和对象分别分配，键只在业务对象里”的方案中，程序必须先读取 data，取得业务地址后才能读取键。这条地址依赖会限制后一次读取能提前多少；如果对象所在缓存行未驻留，还需等待该行取得。嵌入方案从节点地址减去已知偏移，不需要先读 data 才知道键地址；若键和所需链接恰在同一条已驻留缓存行，读取键便能复用它。这里消除的是一次指针取值及其地址依赖，缓存命中仍有条件。

例如仅作布局模型，假定缓存行 64 字节、一个对象按 64 字节对齐、键位于偏移 0、此次需要的链接位于偏移 16，那么两处在同一行。把两者之间放进 128 字节冷数据，或改变对象起始对齐，结论就可能改变。64 是这个算例的参数，不是对所有 ARM 或其他处理器的统一声明。

反例也有用：若外部索引把比较需要的小键与链接一起紧凑保存，只有命中后才访问大业务对象，它可能少读许多冷字段；嵌入一个很大的对象反而可能降低索引密度。包装也可以成批分配，不能认定每个节点必然多做一次独立分配。因此选择时应先画实际地址与字段访问，再看目标测量，不能把“同一对象”当成命中率保证。

在内核中，这种设计很常见。

例如：

```c
struct list_head list;
struct hlist_node hnode;
struct rb_node rb;
struct work_struct work;
struct kref ref;
```

它们的共同点是：

```text
基础设施节点嵌入业务对象。
```

这些字段分别为链表、树、延迟工作或引用协议提供状态位置，但各自的加入、退出与寿命条件并不相同。布局解决了状态放在哪里，下一节继续解决谁能在什么时刻销毁这个对象。

------

### 9.2.8\_节点生命周期为什么由调用者管理

上一节解决了怎样比较以及数据从哪里读，现在设想取消 id=7 的任务：已经从按时间索引摘掉它，但按 id 索引仍能找到它，另一个使用者还保存着先前取得的指针。这时只有一条入口变化了，不能据此销毁对象。Linux rbtree 不负责分配或释放业务对象，原因首先是它不知道这些入口与持有者。

也就是说，rbtree 不会帮你做：

```c
kmalloc();
kfree();
```

它只负责红黑树结构本身：

```text
把已有的 rb_node 挂入树；
把已有的 rb_node 从树中摘除；
维护插入后的颜色和平衡；
维护删除后的颜色和平衡。
```

对象的 **树成员关系、外部入口、持有引用和存储寿命** 是几组正交状态，不是一个“在树/已释放”二态开关。删除只改变其中一部分。引用计数记录已经取得的持有权；RCU（Read-Copy Update，读侧保护与延迟回收机制）可让指定旧读侧继续访问被撤下的对象。这些都需要调用者完整接入，不能仅在删除末尾补一条函数调用。

所以必须先建立一个清晰边界：

```text
rb_link_node() / rb_insert_color() 只表示“节点进入树结构”；
rb_erase() 只表示“节点离开树结构”；
节点离开树结构，不等于业务对象可以立即释放。
```

换句话说：

```text
从 rbtree 摘除，只是对象生命周期中的一个阶段；
不是对象生命周期的终点。
```

生命周期关系应该这样理解：

```mermaid
graph TD
	alloc_obj["调用者分配业务对象"]
	init_obj["初始化业务字段"]
	clear_before["采用游离协议时<br/>初始化私有节点标记"]
	lock_insert["进入插入临界区"]
	search_pos["按统一比较规则搜索插入位置"]
	link_obj["rb_link_node 挂入 BST 位置"]
	fix_obj["rb_insert_color 修复红黑性质"]
	in_tree["对象处于 rbtree 中"]
	lock_delete["进入删除临界区"]
	erase_obj["rb_erase 从树中摘除"]
	clear_after["仅在旧路径不再需要字段时<br/>可选清游离标记"]
	check_refs["满足本协议全部回收条件<br/>其他入口、持有引用与旧读者"]
	free_obj["调用者释放业务对象"]

	alloc_obj --> init_obj
	init_obj --> clear_before
	clear_before --> lock_insert
	lock_insert --> search_pos
	search_pos --> link_obj
	link_obj --> fix_obj
	fix_obj --> in_tree
	in_tree --> lock_delete
	lock_delete --> erase_obj
	erase_obj --> clear_after
	clear_after --> check_refs
	check_refs --> free_obj

	classDef user fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;
	classDef rb fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;
	classDef state fill:#e8f5e9,stroke:#2e7d32,color:#000,stroke-width:2px;
	classDef danger fill:#ffebee,stroke:#c62828,color:#000,stroke-width:2px;

	class alloc_obj,init_obj,clear_before,lock_insert,search_pos,lock_delete,clear_after,check_refs,free_obj user;
	class link_obj,fix_obj,erase_obj rb;
	class in_tree state;
```

这张图里最重要的是中后段：

```text
rb_erase()
    ↓
在字段可修改时记录游离（并非必须）
    ↓
确认全部入口与持有者满足回收条件
    ↓
释放对象
```

不能把它简化成：

```c
rb_erase(&item->rb, root);
kfree(item);
```

然后默认认为这就是安全流程。

这段简化代码成立的关键是对象确由匹配 kfree 的分配方式创建，并且调用者已经排除了所有仍可能访问它的路径。下面列出一种足够条件，不是说所有实现都必须没有引用计数：

```text
没有并发读者；
没有 RCU 查找；
没有引用计数；
对象没有挂在其他结构中；
没有定时器、工作队列、中断、硬件直接内存访问或回调仍可能访问对象；
调用者已经持有正确的锁；
树外没有未交还的持有权。
```

如果这些条件不满足，`rb_erase()` 后立即 `kfree()` 就可能变成释放后使用（use-after-free）：旧指针数值未消失，指向的存储却已经归还分配器，甚至分给了另一个对象。

------

在非并发、无额外引用的简单场景中，删除流程可以写成：

```c
/*
 * 简单独占场景：
 *
 * 前提：
 *   - 当前路径持有必要的锁；
 *   - 没有并发读者；
 *   - 没有 RCU；
 *   - 没有引用计数；
 *   - 对象只由这棵 rbtree 管理；
 *   - 删除后不会再被其他路径访问。
 */
rb_erase(&item->rb, root);
RB_CLEAR_NODE(&item->rb);
kfree(item);
```

这里的 `RB_CLEAR_NODE()` 把父色字段写成自身地址，只是调用者约定的游离标记；它不搜索树，也不会自动摘除节点。若对象马上释放且没有后续标记检查，清标记没有必要。复用对象时是否执行它，要先确认旧路径不再读取该字段，再按同一成员协议初始化；不能把“多写一次总更安全”当作规则。具体语句见[游离标记实现](../../../../research/source_reading/rbtree/source_explanations/include/linux/rbtree.h.md#1.8_游离标记不等于成员搜索)。

------

拆开摘除和销毁，可以显式表达所有权交接，但拆成两个函数本身不增加安全性。下面是 **受同一锁保护、查找者不在解锁后保留裸指针、没有额外引用或延迟读者** 的片段；移除成功后把唯一持有权交给调用者。`-EINVAL` 表示参数无效，`-ENOENT` 表示没有该对象。

第一步，只负责从树中摘除对象：

```c
static int my_tree_remove(struct my_tree *tree, int key,
			  struct my_node **removed)
{
	struct my_node *item;

	if (!removed)
		return -EINVAL;

	*removed = NULL;

	spin_lock(&tree->lock);

	item = my_tree_search_locked(tree, key);
	if (!item) {
		spin_unlock(&tree->lock);
		return -ENOENT;
	}

	/*
	 * rb_erase() 只负责从 rbtree 中摘除节点。
	 * 它不会释放 struct my_node。
	 */
	rb_erase(&item->rb, &tree->root);

	/*
	 * 本例没有保留旧节点字段的读者，并采用游离标记协议。
	 * 其他并发协议不能照搬这个写入时机。
	 */
	RB_CLEAR_NODE(&item->rb);

	*removed = item;

	spin_unlock(&tree->lock);
	return 0;
}
```

第二步，由调用者根据对象所有权决定是否释放：

```c
struct my_node *item;
int ret;

ret = my_tree_remove(&tree, key, &item);
if (!ret)
	kfree(item);
```

这里的返回值不仅传出地址，还按上述约定移交持有权。如果真实对象还有引用或异步访问者，第二步就必须变为其 put/延迟回收操作，不能原样 kfree。两个动作分别是：

```text
从树中摘除；
释放业务对象。
```

这两个动作不是同一件事。

------

若读者在 RCU 读侧范围内保存对象地址，写者的锁只串行化更新者，不会自动等这些读者退出。已经持有旧地址的人仍需要原存储。RCU 宽限期用于确认与本次回收相关的旧读侧已结束，它不自动等待逃出读侧范围的裸指针，也不抵消额外引用。

下面只展示一个已经满足 rbtree 读写兼容、发布取得、写者串行化和所有引用不逃逸前提的 **回收片段**，不是任意树查找加上 rcu_read_lock 就能安全的完整实现。具体下行与漏查边界从[查找模块](../../../../research/source_reading/rbtree/navigation/P02_查找路径与返回边界导读.md#2.4_旋转期间沿什么路径继续)进入。

错误示例：

```c
spin_lock(&tree->lock);
rb_erase(&item->rb, &tree->root);
spin_unlock(&tree->lock);

kfree(item);	/* 错误：RCU 读者可能仍然持有 item */
```

在上述前提下，把最终回收排到旧读侧退出之后：

```c
spin_lock(&tree->lock);

rb_erase(&item->rb, &tree->root);
/* 此处不清旧节点字段；也不立即复用或重新挂入它。 */

spin_unlock(&tree->lock);

/*
 * 排队回收回调；call_rcu 本身不等待宽限期完成。
 * 此后当前写者也不再使用 item；本片段没有额外持有引用。
 */
call_rcu(&item->rcu, my_node_rcu_free);
```

其中释放函数类似：

```c
static void my_node_rcu_free(struct rcu_head *rcu)
{
	struct my_node *item;

	item = container_of(rcu, struct my_node, rcu);
	kfree(item);
}
```

RCU 场景的生命周期应该这样看：

```mermaid
graph TD
	in_tree["对象在 rbtree 中"]
	rcu_reader["RCU 读者可能查到对象"]
	writer_lock["写侧加锁"]
	erase_obj["rb_erase 摘除节点"]
	clear_obj["保留旧节点字段<br/>不清标记或复用"]
	unlock_writer["写侧解锁"]
	call_rcu_node["call_rcu 延迟释放"]
	grace_period["等待 RCU grace period"]
	rcu_free["RCU 回调中 kfree"]

	in_tree --> rcu_reader
	in_tree --> writer_lock
	writer_lock --> erase_obj
	erase_obj --> clear_obj
	clear_obj --> unlock_writer
	unlock_writer --> call_rcu_node
	call_rcu_node --> grace_period
	grace_period --> rcu_free

	classDef state fill:#e8f5e9,stroke:#2e7d32,color:#000,stroke-width:2px;
	classDef writer fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;
	classDef rcu fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;
	classDef danger fill:#ffebee,stroke:#c62828,color:#000,stroke-width:2px;

	class in_tree state;
	class writer_lock,erase_obj,clear_obj,unlock_writer writer;
	class rcu_reader,call_rcu_node,grace_period,rcu_free rcu;
```

这张图的核心是：

```text
摘除撤掉当前树中的成员入口，不会撤销旧读者已取得的地址；
已经在旧路径中的查询仍可能取得或使用旧对象；
其他索引和读侧外持有引用也必须按各自协议关闭。
```

所以 RCU 场景下，删除和释放必须分离。

------

如果对象使用引用计数，逻辑也类似。

从 rbtree 中摘除，只表示：

```text
这个对象不再能通过树查到。
```

在纯引用计数协议下，每个持有者取得引用后才可越过保护窗口继续使用；每次放弃只撤销自己的一份持有权。计数归零时调用匹配的释放操作。若另有 RCU 旧读者等未计入该计数的访问者，还要同时满足它们的退出条件。

示意流程如下：

```mermaid
graph TD
	in_tree["对象在 rbtree 中"]
	lookup_get["在入口保护范围内<br/>查找并获取引用"]
	other_user["已合法取得引用的其他路径"]
	remove_tree["rb_erase 从树中摘除"]
	not_find["后续不能再从树中查到"]
	put_ref["各持有者放弃自己的一份引用"]
	ref_zero{"引用计数为 0?"}
	keep_alive["对象继续存活"]
	free_obj["释放对象"]

	in_tree --> lookup_get
	in_tree --> other_user
	in_tree --> remove_tree
	remove_tree --> not_find
	not_find --> put_ref
	lookup_get --> put_ref
	other_user --> put_ref
	put_ref --> ref_zero
	ref_zero -->|否| keep_alive
	ref_zero -->|是| free_obj

	classDef state fill:#e8f5e9,stroke:#2e7d32,color:#000,stroke-width:2px;
	classDef ref fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;
	classDef free fill:#fff3e0,stroke:#ef6c00,color:#000,stroke-width:2px;

	class in_tree,not_find state;
	class lookup_get,other_user,put_ref,ref_zero,keep_alive ref;
	class remove_tree,free_obj free;
```

所以引用计数场景也不能简单写成：

```c
rb_erase(&item->rb, root);
kfree(item);
```

例如树成员关系拥有一份引用，并且旧字段已可修改时，摘除者放弃的就是这份引用：

```c
/* 已持有保护，采用纯引用计数与游离标记协议。 */
rb_erase(&item->rb, root);
RB_CLEAR_NODE(&item->rb);
my_node_put(item);
```

`my_node_put()` 放弃的引用必须有来源，不能因为刚调用过 rb_erase 就凭空 put。查找者若先解锁，再对刚才的裸指针加引用，中间可能已被另一个线程摘除并归还；因此“找到地址”和“取得引用”的窗口必须有保护。引用保护对象存在，不自动保护红黑树多字段更新。下面的单线程模型把这些持有权逐个记出来。

#### (1)\_两个入口关闭之后谁还在使用对象

仍是 id=7 的任务，两个索引各持有一份引用，读者在查到对象时再取得一份。这里采用入口关闭后才放弃对应引用的协议；为只观察寿命，索引简化为两个指针槽，没有树算法或真实并发。阶段与状态如下：

| 阶段 | 写入者与状态地址 | 谁随后读取，何时能继续 |
| --- | --- | --- |
| S0 私有准备 | 创建者分配 job，写 references=1、id=7 | 只有创建者访问，尚无共享入口 |
| S1 建立入口 | 创建者给 by_time、by_id 两槽赋地址，各增加一份引用，再交出自己的引用 | 两槽可供查找，计数为 2；真实版本须先初始化后按协议发布 |
| S2 借出 | 查找路径在保护窗口内读取 by_time，并对 job.references 加一 | reader 保存地址并持有引用，计数为 3 |
| S3 撤主入口 | 摘除者将 by_time 清空，放弃它的引用 | by_id 仍可达，reader 仍可用，计数为 2 |
| S4 关其余入口 | 摘除者将 by_id 清空，放弃它的引用 | 新查询为空，但 reader 的一份引用仍保持存储，计数为 1 |
| S5 最后退出 | reader 最后使用后 put，计数变 0，释放函数归还存储 | 不得再读旧对象；示例只读取对象外 destroyed 计数 |

```mermaid
flowchart LR
    creator[创建者] -->|S0 分配并初始化| obj[job：id 与 references]
    time[by_time 入口槽] -->|S1 持有一份引用并保存地址| obj
    ids[by_id 入口槽] -->|S1 持有另一份引用并保存地址| obj
    lookup[受保护的查找路径] -->|S2 读取入口并加引用| obj
    lookup -->|返回持有地址| reader[读者 reader]
    remover[摘除者] -->|S3 清槽再 put| time
    remover -->|S4 清槽再 put| ids
    reader -->|S5 最后 put| obj
    obj -->|仅最后引用归零| allocator[分配器回收存储]
```

```mermaid
sequenceDiagram
    participant W as 入口拥有者
    participant I as 两个索引槽
    participant O as job.references与载荷
    participant R as 持有引用的读者
    W->>O: S0 创建引用为1
    W->>I: S1 建立两入口，各拥有一份引用
    W->>O: S1 放弃创建引用，余2
    R->>I: S2 在保护窗口内查找
    R->>O: S2 加引用到3后才离开保护窗口
    W->>I: S3 关闭主入口
    W->>O: 放弃主入口引用，余2
    W->>I: S4 关闭其余入口
    W->>O: 放弃第二入口引用，余1
    alt 读者延迟退出
        R->>O: S4 仍可使用id，不能提前回收
    end
    R->>O: S5 最后put到0并回收
```

完整 [indexed_lifetime.c](../../../../labs/kernel/tree_basics/materials/indexed_lifetime.c) 如下。引用数使用普通整数，只适用于这个串行模型；真实并发不能把它直接替代内核引用计数设施。

```c
/* C11 单线程所有权模型：两个索引入口、一个借出者，不实现树或 RCU。 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct job {
    unsigned int id;
    unsigned int references;
};

static unsigned int destroyed;

static void job_get(struct job *item)
{
    assert(item && item->references > 0);
    ++item->references;
}

static void job_put(struct job *item)
{
    assert(item && item->references > 0);
    if (--item->references == 0) {
        ++destroyed;
        free(item);
    }
}

static struct job *lookup_get(struct job *index)
{
    /* 真实并发版本必须保护“找到入口至取得引用”的整个窗口。 */
    if (index)
        job_get(index);
    return index;
}

static void withdraw(struct job **index)
{
    struct job *old = *index;
    *index = NULL;                 /* 先撤入口，再放弃该入口持有的引用。 */
    if (old)
        job_put(old);
}

int main(void)
{
    struct job *item = malloc(sizeof *item);
    if (!item)
        return EXIT_FAILURE;
    *item = (struct job){ .id = 7, .references = 1 };
    puts("S0 private: creator owns 1 reference");

    job_get(item);
    struct job *by_time = item;
    job_get(item);
    struct job *by_id = item;
    job_put(item);                 /* 交出创建者引用，两个索引各保留一个。 */
    item = NULL;
    printf("S1 published: references=%u\n", by_time->references);

    struct job *reader = lookup_get(by_time);
    assert(reader && reader->id == 7);
    printf("S2 reader holds: references=%u\n", reader->references);
    withdraw(&by_time);
    assert(!by_time && by_id && destroyed == 0);
    printf("S3 first index closed: references=%u\n", reader->references);

    withdraw(&by_id);
    assert(!by_time && !by_id && lookup_get(by_time) == NULL);
    assert(reader->references == 1 && destroyed == 0);
    printf("S4 all entries closed: reader still uses id=%u\n", reader->id);
    job_put(reader);
    reader = NULL;                /* 最后放弃后不再读取已销毁对象。 */
    assert(destroyed == 1);
    printf("S5 final put: destroyed=%u\n", destroyed);
    return EXIT_SUCCESS;
}
```

从仓库根目录运行：

```bash
mkdir -p .cache/rb_lifetime
cc -std=c11 -Wall -Wextra -Werror -pedantic \
    labs/kernel/tree_basics/materials/indexed_lifetime.c \
    -o .cache/rb_lifetime/indexed_lifetime
.cache/rb_lifetime/indexed_lifetime
```

```text
S0 private: creator owns 1 reference
S1 published: references=2
S2 reader holds: references=3
S3 first index closed: references=2
S4 all entries closed: reader still uses id=7
S5 final put: destroyed=1
```

把 S4 与 S5 对照：在 S4 通过任意索引都找不到任务，却仍能合法读取 reader->id；S5 交还最后引用后，只能读对象外的统计，不能“为了确认释放”再访问 reader。分配失败时程序直接返回失败，没有已发布入口需要撤销。

试着交换两个 withdraw 的顺序，最终结果应相同；再让读者在关闭索引之前就 put，预测对象会在哪次 withdraw 中释放。两种顺序都必须保持每份引用恰好交还一次，不能用同一裸地址再伪造一个持有者。重复关闭已为 NULL 的入口应无动作，这与对仍然持有旧地址的对象重复 put 不同。

若改用不加引用的 RCU 读者，S2～S4 的计数表就不再适用：S4 还需要等待相关旧读侧退出，S5 才能销毁；若同时允许长期引用，则这两个条件都要满足。这个模型没有实现 RCU、锁或内存顺序，不能据其输出宣称并发安全。

------

为什么 rbtree 不自动释放对象？

因为 rbtree 根本不知道对象是怎么来的。

业务对象可能来自：

```text
kmalloc；
kzalloc；
slab cache；
静态全局对象；
percpu 对象；
引用计数对象；
RCU 延迟释放对象；
devm 管理资源；
其他子系统私有分配器。
```

如果 rbtree 试图自动释放对象，它必须知道：

```text
用 kfree 还是 kmem_cache_free；
是否还有引用计数；
是否需要 call_rcu 延迟释放；
是否还挂在别的链表里；
是否还被 timer 使用；
是否还被 workqueue 使用；
是否还被中断路径访问；
是否还有硬件 DMA 或回调路径可能访问。
```

这些都不是 rbtree 能知道的。

所以 Linux rbtree 的边界是：

```text
rbtree 负责树结构；
调用者负责对象生命。
```

可以用下面这张图总结责任边界：

```mermaid
graph TD
	rbtree_core["Linux rbtree"]
	tree_link["挂入树 rb_link_node"]
	tree_fix["插入修复 rb_insert_color"]
	tree_erase["摘除节点 rb_erase"]
	tree_walk["遍历辅助 rb_first/rb_next"]

	user_code["调用者"]
	obj_alloc["对象分配"]
	obj_init["业务初始化"]
	obj_cmp["比较规则"]
	obj_lock["结构保护、旧读者退出<br/>与合法引用持有分别设计"]
	obj_clear["节点状态清理"]
	obj_free["对象释放"]

	rbtree_core --> tree_link
	rbtree_core --> tree_fix
	rbtree_core --> tree_erase
	rbtree_core --> tree_walk

	user_code --> obj_alloc
	user_code --> obj_init
	user_code --> obj_cmp
	user_code --> obj_lock
	user_code --> obj_clear
	user_code --> obj_free

	classDef rb fill:#2f2f2f,stroke:#111,color:#fff,stroke-width:2px;
	classDef user fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;

	class rbtree_core,tree_link,tree_fix,tree_erase,tree_walk rb;
	class user_code,obj_alloc,obj_init,obj_cmp,obj_lock,obj_clear,obj_free user;
```

最后，还要特别强调 `RB_CLEAR_NODE()` 和 `RB_EMPTY_NODE()` 的边界。

`RB_CLEAR_NODE()` 的意义是：

```text
把 rb_node 标记成“当前不在树中”。
```

`RB_EMPTY_NODE()` 的意义是：

```text
检查父色字段是否等于自身地址这一约定标记。
```

但是它们不是同步机制。

也就是说，下面这种逻辑不能替代锁：

```c
if (!RB_EMPTY_NODE(&item->rb)) {
	rb_erase(&item->rb, root);
	RB_CLEAR_NODE(&item->rb);
}
```

只有对象始终存活、标记协议一直成立且 root 确为所属树时，这段代码才可据标记判断是否摘除；但在并发场景下，两个线程可能同时判断、同时删除，仍然会出问题。

所以必须记住：

```text
RB_EMPTY_NODE() 不是锁；
RB_CLEAR_NODE() 不是锁；
rb_erase() 不是锁；
rbtree 本身不提供并发保护。
```

并发设计要分别回答三件事：用符合执行上下文的锁等方式串行化树更新并保护所需观察范围；读者从入口取得对象时怎样确保地址与内容有效；最后一条可达或持有路径退出后怎样回收。自旋锁、互斥锁或读写锁能提供的等待/排他条件不同；RCU 和引用计数也不能互相替代，更不自动串行化更新。详细机制在后续同步与寿命专题展开，本节先把责任分开。

本小节的结论是：

```text
rb_erase() 只表示节点离开 rbtree；
节点离开 rbtree 不等于对象可以释放；
对象释放必须由调用者在确认没有任何访问者之后执行。
```

这也是 Linux rbtree 生命周期设计最重要的工程边界。

------

### 9.2.9\_本节小结

本节的核心是理解 Linux rbtree 的嵌入式节点设计。

与本节用来对照的外部包装设计相比：

```text
普通泛型容器：容器拥有节点，节点再指向业务对象；
Linux rbtree：业务对象拥有 rb_node，rbtree 只管理 rb_node。
```

最终责任边界如下：

```mermaid
graph TD
	rbtree["Linux rbtree"]
	rb_manage["维护 rb_node 树结构"]
	rb_color["维护颜色"]
	rb_rotate["执行旋转"]
	rb_traverse["提供遍历辅助"]

	user["调用者"]
	user_obj["定义业务对象"]
	user_key["定义 key"]
	user_cmp["定义比较规则"]
	user_dup["定义重复 key 策略"]
	user_lock["负责并发保护"]
	user_life["负责对象生命周期"]

	rbtree --> rb_manage
	rbtree --> rb_color
	rbtree --> rb_rotate
	rbtree --> rb_traverse

	user --> user_obj
	user --> user_key
	user --> user_cmp
	user --> user_dup
	user --> user_lock
	user --> user_life

	classDef rb fill:#2f2f2f,stroke:#111,color:#fff,stroke-width:2px;
	classDef user_class fill:#e3f2fd,stroke:#1565c0,color:#000,stroke-width:2px;

	class rbtree,rb_manage,rb_color,rb_rotate,rb_traverse rb;
	class user,user_obj,user_key,user_cmp,user_dup,user_lock,user_life user_class;
```

本节需要记住六个结论。

第一，`struct rb_node` 是嵌入业务结构体的，不是一个带 key/value 的完整容器节点。

```c
struct my_node {
	int key;
	int value;
	struct rb_node rb;
};
```

第二，rbtree API 操作的是 `struct rb_node`，业务代码需要通过 `rb_entry()` 找回外层对象。

```c
struct my_node *item;

item = rb_entry(node, struct my_node, rb);
```

第三，Linux rbtree 不把 key 放进 `struct rb_node`，因为内核对象的 key 形态没有统一标准。

第四，Linux rbtree 不强制一种比较调用方式；业务先定义键序与重复政策，再选择匹配的辅助接口或手写搜索。

第五，`rb_erase()` 只负责从树中摘除节点，不负责释放业务对象。S3 到 S5 可能隔着其他入口、引用与旧读者；只有相应条件全部满足才可交还存储。

第六，`RB_CLEAR_NODE()` 只是节点状态标记，不是并发保护机制。

用一句话总结本节：

```text
Linux rbtree 不是“替你管理对象的泛型容器”，而是“让你的业务对象具备红黑树挂载能力的内核基础设施”。
```

---

## 9.3\_从嵌入设计进入调用者框架

现在已经能解释成员地址如何回到对象、谁定义键序、为什么关入口不等于立即回收。接下来沿[P37 调用者框架](P37_构建rbtree调用者接口.md#37.1_使用者视角_如何在内核中使用_rbtree)把这些结论组合成一份完整程序：定义根与锁、按唯一键搜索、处理重复插入、复制查询结果、交还被摘除对象，并核对失败时谁负责清理。随后再进入 P10 的查询内部路径。
