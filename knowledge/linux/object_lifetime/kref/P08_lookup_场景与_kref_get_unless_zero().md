---
id: knowledge.linux.object_lifetime.kref.p08_lookup_场景与_kref_get_unless_zero
title: "lookup 场景与 kref get unless zero()"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
---

# 第8章_lookup_场景与_kref_get_unless_zero()

## 8.1_本章定位

前面章节已经讲过：

```text
kref 保护对象内存生命周期；
有指针不等于有引用；
kref_get() 之前必须证明对象有效；
put 后不能继续访问对象；
handoff 要定义引用归属。
```

本章专门讨论另一个高风险场景：

```text
从某个全局结构、队列、list、hash、xarray、idr 中查找对象，然后安全获得引用。
```

这就是 lookup 场景。

**lookup 场景为什么容易出错**

lookup 场景容易出错，是因为它同时包含三件事：

```text
1. 从容器中找到对象指针；
2. 判断这个指针指向的对象是否仍然有效；
3. 给当前执行路径获得一份引用。
```

错误代码常见写法是：

```c
obj = find_obj(id);
kref_get(&obj->ref);
```

这不一定安全。

因为 `find_obj(id)` 得到的只是一个**裸指针**，它只表示“曾经从容器里读到了一个地址”，并不表示这个对象此刻仍然活着。

------

**kref 的边界**

`kref` 只解决一个问题：

```text
当当前路径已经能够安全访问对象时，
如何给对象增加一份生命周期引用。
```

它不解决：

```text
这个 obj 指针本身是否仍然有效；
这个 obj 指向的内存是否已经被释放；
这个对象是否已经从容器中删除；
lookup 到 get 之间是否存在并发释放窗口。
```

所以不能把 `kref_get()` 理解成“让一个裸指针变安全”。

更准确地说：

```text
kref 保护的是“拿到引用之后”的生命周期；
它不保护“从容器找到裸指针到成功拿引用之间”的窗口。
```

------

**裸指针和 kref 引用不是一回事**

```text
裸指针：
    只是一个地址。
    不表示对象还活着。
    不表示当前路径拥有生命周期所有权。

kref 引用：
    是对象生命周期上的一份所有权。
    只要持有这份引用，对象就不能被真正释放。
```

所以：

```c
obj = find_obj(id);
```

得到的只是裸指针。

而：

```c
kref_get(&obj->ref);
```

才是在增加引用。

但前提是：

```text
执行 kref_get() 时，obj 必须已经被证明是有效对象。
```

如果 `obj` 已经释放，那么 `kref_get(&obj->ref)` 只是对一块失效内存做原子加法，仍然是 UAF。

------

**atomic 不能解决对象失效问题**

`kref` 底层使用原子计数，但原子操作只能保证：

```text
对 refcount 数值的修改是原子的。
```

它不能保证：

```text
这个 refcount 所在的对象还活着。
```

也就是说，原子操作解决的是“计数竞争”，不是“对象存在性”。

如果对象已经被 `kfree()`，那么再执行：

```c
kref_get(&obj->ref);
```

即使这个加法本身是原子的，也没有意义。

因为此时访问的可能是：

```text
已经释放的内存；
已经被 slab 复用成其他对象的内存；
已经进入 release 路径的对象。
```

所以 lookup 的安全性不能只靠 atomic。

------

**lookup 正确性需要三层保证**

一个安全的 lookup 必须同时回答三个问题：

```text
1. 容器怎么保护？
   例如 mutex、spinlock、RCU、idr/xarray 锁。

2. obj 指针怎么保证稳定？
   从 find 到 get 期间，obj 不能被释放。

3. get 是否允许失败？
   如果对象可能正在释放，需要使用 kref_get_unless_zero()。
```

普通路径和 lookup 路径要区分：

```text
普通路径：
    当前路径已经持有对象引用；
    可以直接 kref_get()。

lookup 路径：
    当前路径只有裸指针，没有引用；
    不能无条件 kref_get()；
    必须在锁或 RCU 保护下完成 lookup 和 get。
```

------

**典型安全模型一：锁保护 lookup + get**

```c
spin_lock(&table_lock);

obj = find_obj_locked(id);
if (obj)
	kref_get(&obj->ref);

spin_unlock(&table_lock);
```

这个模型成立的前提是：

```text
删除路径也必须使用同一把锁。
```

例如：

```c
spin_lock(&table_lock);
remove_obj_locked(obj);
spin_unlock(&table_lock);

kref_put(&obj->ref, my_obj_release);
```

这样可以保证：

```text
find_obj_locked() 到 kref_get() 之间，
对象不会被并发删除并释放。
```

------

**典型安全模型二：RCU + kref_get_unless_zero()**

```c
rcu_read_lock();

obj = find_obj_rcu(id);
if (obj && kref_get_unless_zero(&obj->ref)) {
	rcu_read_unlock();
	return obj;
}

rcu_read_unlock();
return NULL;
```

这个模型里有两个保护点：

```text
RCU：
    保证读侧临界区内 obj 指向的内存不会立刻释放。

kref_get_unless_zero()：
    保证 refcount 已经为 0 的对象不会被重新复活。
```

两者缺一不可。

`kref_get_unless_zero()` 不能单独保证指针安全，它只是防止：

```text
对已经归零的引用计数重新加引用。
```

------

**异步场景的规则**

异步场景的核心规则是：

```text
谁异步使用对象，谁提前 get；
谁用完对象，谁 put。
```

例如：

```c
void submit_work(struct my_obj *obj)
{
	kref_get(&obj->ref);
	queue_work(wq, &obj->work);
}

void my_work_fn(struct work_struct *work)
{
	struct my_obj *obj = container_of(work, struct my_obj, work);

	/* 使用 obj */

	kref_put(&obj->ref, my_obj_release);
}
```

创建者不一定需要等待异步任务结束。

创建者只需要释放自己的引用：

```c
kref_put(&obj->ref, my_obj_release);
```

只要异步任务已经提前 `get`，对象就不会被提前释放。

错误写法是：

```c
queue_work(wq, &obj->work);
kref_put(&obj->ref, my_obj_release);
```

如果投递 work 前没有 `get`，那么创建者 `put` 后对象可能被释放，work 之后运行时就会访问已经释放的对象。

------

**最终结论**

`kref` 不是完整的对象安全机制。

完整安全性来自三件事配合：

```text
可发现性：
    对象是否还在容器中。

内存稳定性：
    obj 指针指向的内存是否还没有释放。

生命周期所有权：
    当前路径是否已经持有 kref 引用。
```

所以 lookup 场景的核心原则是：

```text
拿到地址，不等于拿到引用；
拿到引用之前，必须先证明地址有效；
拿到引用之后，kref 才能保护对象生命周期。
```

一句话总结：

```text
kref 只能延长一个“已经安全可访问对象”的生命周期，
不能把一个“不确定是否有效的裸指针”变成安全对象。
```

本章主线：

```text
lookup 的核心不是“找到指针”，而是“在对象仍然有效时获得引用”。
```

------

## 8.2_lookup_的基本边界_先分清指针_对象和引用

这一组内容先回答 lookup 为什么危险。

lookup 不是单纯“找到一个地址”，而是要完成一次转换：

```text
容器中的裸指针
    -> 在保护机制下证明对象仍然有效
        -> 成功获得 kref 引用
            -> 返回可在锁外使用的对象
```

如果这几个层次没有分清，后面的 `kref_get()`、`kref_get_unless_zero()`、RCU、锁都会被误用。

### 8.2.1_lookup_为什么是_kref_最容易出错的场景

普通持有路径比较简单。

例如：

```c
struct my_obj *obj = my_obj_alloc();

kref_init(&obj->ref);

/* 当前路径天然持有初始引用 */
```

这里当前路径从创建开始就拥有引用。

但是 lookup 不一样。

lookup 的对象已经存在于某个共享结构里：

```text
global list
hash table
xarray
idr
radix tree
rcu protected list
device registry
request table
```

其他线程可能同时做：

```text
删除对象；
从集合中 unlink；
put 最后一份引用；
触发 release；
释放对象内存。
```

所以 lookup 的危险窗口是：

```text
CPU0                                CPU1
--------------------------------    -------------------------------
obj = find_obj(id);

                                    remove_obj_from_table(obj);
                                    kref_put(&obj->ref, release);
                                    refcount 变成 0;
                                    release(obj);
                                    kfree(obj);

kref_get(&obj->ref);                // 对已经释放的对象加引用
```

CPU0 表面上只是做了一个 `kref_get()`，但实际上它可能已经在访问释放后的内存。

这就是 lookup 场景的本质风险：

```text
裸指针可能已经悬挂；
悬挂指针上的 kref_get() 本身就是 UAF。
```

所以不能把规则写成：

```text
lookup 后 kref_get 就安全。
```

真正的规则是：

```text
lookup + get 必须在某种保护机制下完成。
```

这个保护机制可以是：

```text
mutex/spinlock
RCU
对象状态机
集合删除规则
引用非零检查
延迟释放机制
```

但一定不能是：

```text
我看到了一个指针，所以它应该还活着。
```

------

### 8.2.2_裸指针_有效对象_有效引用的区别

lookup 场景必须区分三个概念。

#### (1)_裸指针

裸指针只是一个地址值：

```c
struct my_obj *obj;
```

它只能说明：

```text
这里曾经有一个对象地址。
```

它不能说明：

```text
对象还活着；
对象还没有进入 release；
对象内存没有被释放；
当前路径有资格访问对象。
```

裸指针没有生命周期语义。

------

#### (2)_有效对象

有效对象表示：

```text
这块内存当前仍然属于这个对象；
对象还没有被 release/kfree；
对象结构体内部字段还能被访问。
```

但是“对象有效”本身也不等于当前路径持有引用。

例如在锁保护的 list 遍历里：

```c
mutex_lock(&global_lock);

list_for_each_entry(obj, &global_list, node) {
	/*
	 * 在 global_lock 保护下，obj 暂时不会从 list 中消失。
	 * 因此 obj 指针在这个临界区里是有效的。
	 */
}

mutex_unlock(&global_lock);
```

这里临界区内可以认为 `obj` 暂时有效。

但是一旦离开锁，如果没有 `kref_get()`，当前路径就没有长期访问资格。

------

#### (3)_有效引用

有效引用表示：

```text
当前路径已经拥有一份引用所有权；
对象内存至少活到当前路径 put；
当前路径使用结束必须 put。
```

示例：

```c
kref_get(&obj->ref);

/*
 * 从这里开始，当前路径持有引用。
 */

...

kref_put(&obj->ref, my_obj_release);
```

lookup 场景的目标就是：

```text
把“锁/RCU保护下看到的对象”转换成“当前路径持有的有效引用”。
```

也就是：

```text
裸指针
    -> 临界区证明对象有效
        -> kref_get 或 kref_get_unless_zero
            -> 当前路径拥有引用
```

------

### 8.2.3_错误模型_裸_lookup_后直接_get

错误示例：

```c
struct my_obj *my_obj_lookup_get_bad(int id)
{
	struct my_obj *obj;

	obj = my_obj_lookup_raw(id);
	if (!obj)
		return NULL;

	kref_get(&obj->ref);
	return obj;
}
```

这段代码看起来很合理：

```text
先找到对象；
再增加引用；
返回给调用者。
```

但它缺了关键前提：

```text
kref_get() 执行时，obj 是否仍然有效？
```

如果 `my_obj_lookup_raw()` 只是无保护地从全局结构中读出一个指针，那么可能出现：

```text
CPU0: obj = my_obj_lookup_raw(id)

CPU1: 从全局结构删除 obj
CPU1: kref_put(&obj->ref)
CPU1: release(obj)
CPU1: kfree(obj)

CPU0: kref_get(&obj->ref)
```

CPU0 的 `kref_get()` 已经在访问释放后的内存。

所以这个模式是错的。

错误点不是：

```text
没有使用 kref_get_unless_zero()
```

而是更基础：

```text
kref_get 前没有机制证明 obj 指针仍然指向有效对象。
```

------

## 8.3_基础保护模型_锁保护容器_kref_保护生命周期

这一组内容讲最基础、最容易审查的 lookup 模型：

```text
容器关系由锁保护；
对象挂入容器时，容器持有引用；
lookup 在锁内找到对象并 get；
remove 先 unlink，再 put 容器引用。
```

这个模型是后面 hash、xarray、idr、RCU 模型的参照物。

### 8.3.1_正确模型一_mutex/list_lookup_+_kref_get()

最基础的正确模型是：

```text
用锁保护集合；
在锁内 lookup；
在锁内 get；
释放锁后返回对象引用。
```

对象定义：

```c
struct my_obj {
	struct kref ref;
	struct list_head node;
	int id;
	int state;
};
```

全局集合：

```c
static LIST_HEAD(my_obj_list);
static DEFINE_MUTEX(my_obj_lock);
```

lookup 函数：

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

这个写法成立的前提是：

```text
所有对 my_obj_list 的 add/del 都必须持有 my_obj_lock；
对象从 list 删除和释放 list 引用也必须按规则执行；
只要 obj 还在 list 中，list 持有一份引用；
在 my_obj_lock 内，obj 不会被并发删除并释放。
```

也就是说，`my_obj_lock` 保护的是：

```text
obj 是否在 list 中；
lookup 期间 obj 指针是否稳定；
list 结构本身的一致性。
```

`kref_get()` 做的是：

```text
给当前路径增加一份长期引用。
```

两者配合后，返回给调用者的是：

```text
一个带引用的对象指针。
```

调用者使用完必须：

```c
kref_put(&obj->ref, my_obj_release);
```

------

### 8.3.2_list_持有引用的模型

上面的代码隐含一个重要设计：

```text
对象挂入 list 时，list 持有一份引用。
```

例如：

```c
int my_obj_publish(struct my_obj *obj)
{
	kref_get(&obj->ref);          /* 给全局 list 一份引用 */

	mutex_lock(&my_obj_lock);
	list_add_tail(&obj->node, &my_obj_list);
	mutex_unlock(&my_obj_lock);

	return 0;
}
```

撤销发布：

```c
void my_obj_unpublish(struct my_obj *obj)
{
	mutex_lock(&my_obj_lock);
	if (!list_empty(&obj->node))
		list_del_init(&obj->node);
	mutex_unlock(&my_obj_lock);

	kref_put(&obj->ref, my_obj_release);
}
```

这种模型下，引用归属是：

```text
创建者引用；
list 引用；
lookup 调用者引用；
其他异步路径引用。
```

只要对象还在 list 中，list 的引用就保证对象不会被释放。

因此 lookup 时，在锁内找到对象后，可以直接：

```c
kref_get(&obj->ref);
```

因为锁保证：

```text
此时 obj 还在 list 中；
list 引用还没有被 put；
对象 refcount 不可能已经为 0。
```

------

### 8.3.3_remove/unlink_与_lookup_的顺序

lookup 正确与否，和 remove 顺序强相关。

正确 remove 顺序通常是：

```text
先从集合中 unlink；
再 put 集合持有的引用。
```

示例：

```c
void my_obj_remove(struct my_obj *obj)
{
	mutex_lock(&my_obj_lock);

	if (!list_empty(&obj->node))
		list_del_init(&obj->node);

	mutex_unlock(&my_obj_lock);

	/*
	 * list 已经不再能 lookup 到 obj。
	 * 现在释放 list 持有的引用。
	 */
	kref_put(&obj->ref, my_obj_release);
}
```

为什么不能先 put 再 unlink？

错误示例：

```c
void my_obj_remove_bad(struct my_obj *obj)
{
	kref_put(&obj->ref, my_obj_release);

	mutex_lock(&my_obj_lock);
	list_del_init(&obj->node);
	mutex_unlock(&my_obj_lock);
}
```

如果 `kref_put()` 触发 release，对象可能已经被释放。

随后再访问：

```c
obj->node
```

就是 UAF。

更严重的是，如果 release 之后对象还留在 list 中，其他 lookup 可能读到悬挂指针。

所以规则是：

```text
从可查找结构中撤销对象，必须发生在释放该结构引用之前。
```

压缩成一句：

```text
unlink first, put later.
```

------

## 8.4_kref_get_unless_zero()_防复活_不防悬挂指针

这一组内容专门收束 `kref_get_unless_zero()`。

它只解决一个问题：

```text
refcount 已经是 0 时，不能再把对象重新加引用复活。
```

它不解决另一个更基础的问题：

```text
obj 指针本身是否仍然指向有效内存。
```

所以它必须和锁、RCU、延迟释放或其他内存稳定机制配合使用。

### 8.4.1_kref_get_unless_zero()_的定位

`kref_get_unless_zero()` 的语义是：

```text
如果引用计数不是 0，则尝试加 1；
如果引用计数已经是 0，则失败，不加引用。
```

接口形式：

```c
int kref_get_unless_zero(struct kref *kref);
```

返回值通常按布尔语义使用：

```text
返回非 0：成功获得引用；
返回 0：引用计数已经是 0，没有获得引用。
```

典型使用：

```c
if (!kref_get_unless_zero(&obj->ref))
	return NULL;
```

它解决的问题是：

```text
避免对已经归零的引用计数重新加引用。
```

也就是避免这种错误：

```text
refcount 已经到 0；
release 已经开始或即将开始；
另一个路径又把 refcount 从 0 加回 1；
对象被“复活”。
```

`kref_get_unless_zero()` 的核心价值是：

```text
只允许从非 0 引用计数上获得新引用；
不允许从 0 重新复活对象。
```

------

### 8.4.2_kref_get_unless_zero()_不解决什么

必须强调：

```text
kref_get_unless_zero() 不解决 obj 指针本身是否有效的问题。
```

错误理解：

```text
普通 kref_get 不安全；
换成 kref_get_unless_zero 就安全。
```

这是错的。

如果 `obj` 指针已经悬挂，那么：

```c
kref_get_unless_zero(&obj->ref);
```

仍然是在访问释放后的内存。

也就是说，它必须先访问：

```c
obj->ref
```

而访问 `obj->ref` 的前提是：

```text
obj 指针仍然指向有效内存。
```

所以 `kref_get_unless_zero()` 只解决：

```text
refcount 不是 0 才加引用。
```

它不解决：

```text
obj 指针是不是悬挂；
obj 内存是不是已经 kfree；
lookup 过程有没有并发删除；
集合结构是不是一致；
对象状态是否允许使用。
```

本章最重要的一句话：

```text
kref_get_unless_zero() 不是裸 lookup 的护身符。
```

------

### 8.4.3_kref_get_unless_zero()_仍然需要锁或_RCU

正确使用 `kref_get_unless_zero()` 时，仍然需要一种机制保证：

```text
在执行 kref_get_unless_zero(&obj->ref) 时，
obj 所在内存还没有被释放。
```

这个机制通常来自：

```text
mutex/spinlock 保护集合；
RCU 保护读侧访问；
延迟释放；
对象内存由更外层结构保证；
释放路径和 lookup 路径有明确序列化。
```

典型错误：

```c
obj = my_obj_lookup_raw(id);
if (!obj)
	return NULL;

if (!kref_get_unless_zero(&obj->ref))
	return NULL;

return obj;
```

如果 `my_obj_lookup_raw()` 没有任何保护，这仍然是错的。

正确形式应该是：

```c
mutex_lock(&my_obj_lock);

obj = my_obj_lookup_locked(id);
if (obj && !kref_get_unless_zero(&obj->ref))
	obj = NULL;

mutex_unlock(&my_obj_lock);

return obj;
```

或者在 RCU 场景中：

```c
rcu_read_lock();

obj = my_obj_lookup_rcu(id);
if (obj && !kref_get_unless_zero(&obj->ref))
	obj = NULL;

rcu_read_unlock();

return obj;
```

但 RCU 版本还要求：

```text
对象内存释放必须延迟到 RCU grace period 之后；
release 不能立即 kfree 掉 RCU 读侧可能看到的对象内存。
```

这部分第 10 章会专门展开。

------

### 8.4.4_什么时候用_kref_get()_什么时候用_kref_get_unless_zero()

#### (1)_可以确认_refcount_一定非_0_用_kref_get()

如果锁保护下可以证明：

```text
对象还在集合中；
集合持有引用；
对象不可能正在释放；
refcount 不可能为 0。
```

那么可以直接：

```c
kref_get(&obj->ref);
```

例如：

```c
mutex_lock(&my_obj_lock);

obj = my_obj_find_locked(id);
if (obj)
	kref_get(&obj->ref);

mutex_unlock(&my_obj_lock);
```

这个模式依赖：

```text
只要 obj 在 list 中，list 就持有一份引用。
```

因此 refcount 不可能是 0。

------

#### (2)_可能看到正在退出的对象_用_kref_get_unless_zero()

如果 lookup 可能看到一个正在退出、正在撤销、refcount 可能接近 0 的对象，就适合用：

```c
kref_get_unless_zero()
```

例如某些场景下，对象可能仍被 RCU 读侧看到，但已经从正常生命周期中退出。

此时不能无条件：

```c
kref_get(&obj->ref);
```

因为这可能把一个已经走向销毁的对象重新拉回来。

应该：

```c
if (!kref_get_unless_zero(&obj->ref))
	obj = NULL;
```

意思是：

```text
只有对象仍然有活跃引用时，当前路径才加入持有者集合；
如果引用已经归零，就不要复活它。
```

------

#### (3)_判断表

| 场景                      | 是否能用 `kref_get()` | 是否适合 `kref_get_unless_zero()` | 说明                    |
| ------------------------- | --------------------- | --------------------------------- | ----------------------- |
| 当前路径本来就持有引用    | 可以                  | 通常不需要                        | 已经证明对象有效        |
| 锁内 lookup，集合持有引用 | 可以                  | 可用但通常多余                    | refcount 必然非 0       |
| 无保护裸 lookup           | 不可以                | 也不可以                          | 指针本身可能悬挂        |
| RCU lookup，内存延迟释放  | 不应无条件用          | 常用                              | 必须防止复活 0 引用对象 |
| 对象可能正在退出          | 不应无条件用          | 常用                              | 失败表示不能获得引用    |
| refcount 可能已为 0       | 不可以                | 可以尝试                          | 前提是 obj 内存仍有效   |

一句话：

```text
kref_get() 要求你已经证明对象活着；
kref_get_unless_zero() 只允许你在对象尚未归零时加入引用者。
```

但两者共同前提都是：

```text
obj 指针本身必须有效。
```

------

### 8.4.5_mutex_+_list_+_kref_get_unless_zero()_模板

虽然在“list 持有引用”的模型里通常直接用 `kref_get()` 就够了，但也可以写成 `kref_get_unless_zero()` 模板。

```c
struct my_obj *my_obj_lookup_get(int id)
{
	struct my_obj *obj, *found = NULL;

	mutex_lock(&my_obj_lock);

	list_for_each_entry(obj, &my_obj_list, node) {
		if (obj->id != id)
			continue;

		if (kref_get_unless_zero(&obj->ref))
			found = obj;

		break;
	}

	mutex_unlock(&my_obj_lock);

	return found;
}
```

这个模板表达的是：

```text
在锁内找到对象；
确认引用计数未归零；
成功则当前路径获得引用；
失败则返回 NULL。
```

如果你的设计能保证：

```text
只要 obj 在 list 中，refcount 必然非 0。
```

那么 `kref_get_unless_zero()` 失败理论上不应该发生。

这时可以加调试检查：

```c
if (WARN_ON(!kref_get_unless_zero(&obj->ref)))
	found = NULL;
else
	found = obj;
```

但更常见的写法仍然是：

```c
kref_get(&obj->ref);
```

因为锁和 list 引用已经证明 refcount 非 0。

------

## 8.5_常见容器_lookup_模板

这一组内容按容器类型归类。

不要把这些模板理解成新的生命周期规则，它们只是把同一条规则套到不同容器上：

```text
容器查找必须被对应同步机制保护；
返回给锁外调用者之前必须获得引用；
容器删除路径必须和 lookup 路径配套。
```

### 8.5.1_hash_table_lookup_的引用规则

hash table 和 list 本质一样：

```text
hash bucket 是可查找结构；
bucket lock 保护链表结构；
hash 表通常持有对象引用；
lookup 成功后给调用者新引用。
```

示例：

```c
struct my_obj {
	struct kref ref;
	struct hlist_node hnode;
	u32 id;
};
```

全局 hash：

```c
static DEFINE_HASHTABLE(my_obj_ht, 8);
static DEFINE_SPINLOCK(my_obj_ht_lock);
```

插入：

```c
int my_obj_hash_add(struct my_obj *obj)
{
	kref_get(&obj->ref);     /* hash 表持有引用 */

	spin_lock(&my_obj_ht_lock);
	hash_add(my_obj_ht, &obj->hnode, obj->id);
	spin_unlock(&my_obj_ht_lock);

	return 0;
}
```

lookup：

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

删除：

```c
void my_obj_hash_remove(struct my_obj *obj)
{
	spin_lock(&my_obj_ht_lock);
	hash_del(&obj->hnode);
	spin_unlock(&my_obj_ht_lock);

	kref_put(&obj->ref, my_obj_release);
}
```

这个模型仍然是：

```text
hash 表持有引用；
lookup 在锁内找到对象；
lookup 在锁内 get；
remove 先 hash_del，再 put hash 引用。
```

注意：

```text
spinlock 保护 hash 结构；
kref 保护对象生命周期；
两者不能互相替代。
```

------

### 8.5.2_xarray_lookup_的引用规则

xarray 常用于通过整数 ID 查找对象。

典型模型：

```text
xarray 保存对象指针；
xarray 持有对象引用；
lookup 时在 xa_lock 下查找并 get；
erase 时先从 xarray 删除，再 put xarray 引用。
```

对象：

```c
struct my_obj {
	struct kref ref;
	u32 id;
};
```

xarray：

```c
static DEFINE_XARRAY(my_obj_xa);
```

插入：

```c
int my_obj_xa_insert(struct my_obj *obj)
{
	int ret;

	kref_get(&obj->ref);       /* xarray 持有引用 */

	xa_lock(&my_obj_xa);
	ret = __xa_insert(&my_obj_xa, obj->id, obj, GFP_KERNEL);
	xa_unlock(&my_obj_xa);

	if (ret)
		kref_put(&obj->ref, my_obj_release);

	return ret;
}
```

lookup：

```c
struct my_obj *my_obj_xa_lookup_get(u32 id)
{
	struct my_obj *obj;

	xa_lock(&my_obj_xa);

	obj = xa_load(&my_obj_xa, id);
	if (obj)
		kref_get(&obj->ref);

	xa_unlock(&my_obj_xa);

	return obj;
}
```

删除：

```c
void my_obj_xa_remove(u32 id)
{
	struct my_obj *obj;

	xa_lock(&my_obj_xa);
	obj = xa_erase(&my_obj_xa, id);
	xa_unlock(&my_obj_xa);

	if (obj)
		kref_put(&obj->ref, my_obj_release);
}
```

这里要注意：

```text
xa_load() 返回的只是指针；
只有在锁内 get 成功后，调用者才真正持有引用。
```

不要写成：

```c
obj = xa_load(&my_obj_xa, id);
if (obj)
	kref_get(&obj->ref);
```

除非你明确知道当前 xarray 使用方式允许无锁 RCU 查找，并且对象释放路径也配套 RCU 延迟释放。

否则裸 `xa_load()` 后再 `kref_get()` 仍然可能踩悬挂指针。

------

### 8.5.3_idr_lookup_的引用规则

idr 也是常见的 ID 到对象指针映射结构。

模型和 xarray 类似：

```text
idr 保存对象指针；
idr 持有对象引用；
lookup 在锁内完成；
remove 先删除映射，再 put。
```

定义：

```c
static DEFINE_IDR(my_obj_idr);
static DEFINE_MUTEX(my_obj_idr_lock);
```

插入：

```c
int my_obj_idr_alloc(struct my_obj *obj)
{
	int id;

	kref_get(&obj->ref);      /* idr 持有引用 */

	mutex_lock(&my_obj_idr_lock);
	id = idr_alloc(&my_obj_idr, obj, 0, 0, GFP_KERNEL);
	mutex_unlock(&my_obj_idr_lock);

	if (id < 0) {
		kref_put(&obj->ref, my_obj_release);
		return id;
	}

	obj->id = id;
	return 0;
}
```

lookup：

```c
struct my_obj *my_obj_idr_lookup_get(int id)
{
	struct my_obj *obj;

	mutex_lock(&my_obj_idr_lock);

	obj = idr_find(&my_obj_idr, id);
	if (obj)
		kref_get(&obj->ref);

	mutex_unlock(&my_obj_idr_lock);

	return obj;
}
```

remove：

```c
void my_obj_idr_remove(int id)
{
	struct my_obj *obj;

	mutex_lock(&my_obj_idr_lock);
	obj = idr_remove(&my_obj_idr, id);
	mutex_unlock(&my_obj_idr_lock);

	if (obj)
		kref_put(&obj->ref, my_obj_release);
}
```

核心仍然是：

```text
idr_find() 返回裸指针；
锁内 kref_get() 才把裸指针变成当前路径引用。
```

------

### 8.5.4_lookup_成功_失败_正在释放的状态表

lookup 不是只有“找到”和“没找到”两种状态。

更完整的状态表如下：

| 状态                 | 容器中是否可见 | refcount 是否非 0 | lookup 结果            | 当前路径是否获得引用 |
| -------------------- | -------------- | ----------------- | ---------------------- | -------------------- |
| 对象正常存在         | 是             | 是                | 成功                   | 是                   |
| 对象不存在           | 否             | 无                | 失败                   | 否                   |
| 对象已经 unlink      | 否             | 可能非 0          | 失败                   | 否                   |
| 对象正在释放         | 不应再可见     | 可能为 0          | 失败                   | 否                   |
| RCU 读侧仍可见旧指针 | 逻辑上已删除   | 可能为 0          | 取决于 get_unless_zero | 成功才有             |
| 数据结构损坏         | 不确定         | 不确定            | 不可信                 | 不可信               |

这个表想说明：

```text
lookup 成功不只是“容器里有指针”；
lookup 成功应该意味着“当前路径已经获得引用”。
```

所以函数名最好写成：

```c
my_obj_lookup_get()
```

而不是：

```c
my_obj_lookup()
```

如果函数只是返回裸指针，要明确限制：

```text
只能在持锁期间使用；
不能跨越锁；
不能保存；
不能异步传递；
不能 put；
不能在锁外访问。
```

------

## 8.6_lookup_API_契约_返回裸指针还是返回引用

这一组内容把 lookup 的接口语义收住。

lookup API 最怕名字含糊：

```text
返回的是裸指针，还是带引用对象？
调用者能否在锁外保存？
调用者是否必须 put？
失败是没找到，还是对象正在退出？
```

因此建议把 `find_locked()`、`lookup_get()`、`tryget()` 这几类接口明确拆开。

### 8.6.1_lookup_raw()_和_lookup_get()_必须分开

建议工程里明确区分两类函数。

#### (1)_lookup_raw()_只返回临时裸指针

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

#### (2)_lookup_get()_返回带引用对象

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

### 8.6.2_lookup_get()_的标准注释

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

### 8.6.3_lookup_后的调用者规则

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

#### (1)_给异步路径新引用

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

#### (2)_把_lookup_得到的引用直接转移给异步路径

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

### 8.6.4_lookup_函数不要返回_可能要_put_的对象

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

## 8.7_退出_状态和_RCU_边界

这一组内容处理 lookup 和对象退出阶段的关系。

对象“还没释放”和“允许新用户进入”不是一回事：

```text
对象可能仍有旧引用，但已经不允许新的 lookup 成功；
对象可能仍被 RCU 读侧看到，但不能被重新复活；
对象从容器撤销可见性，必须和最后 put、延迟释放配套。
```

### 8.7.1_释放路径必须和_lookup_路径配套

lookup 正确性不是 lookup 函数自己能单独保证的。

它还依赖释放路径是否配套。

#### (1)_正确释放路径

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

#### (2)_错误释放路径

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

### 8.7.2_对象状态和_lookup_的关系

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

### 8.7.3_DYING_状态通常拒绝新的_lookup_引用

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

### 8.7.4_kref_get_unless_zero()_和对象状态不能互相替代

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

### 8.7.5_RCU_lookup_的提前预告

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

### 8.7.6_lookup_与对象_复活_问题

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

## 8.8_错误清单_模板和检查项

这一组内容作为本章最后的落地部分。

先看错误清单，再看模板，最后用检查清单收尾：

```text
错误清单：识别常见 bug 形态。
设计模板：把正确写法固定下来。
检查清单：写代码或 review 时逐项确认。
```

### 8.8.1_lookup_的错误清单

#### (1)_无保护_lookup_后_get

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

#### (2)_以为_kref_get_unless_zero()_可以替代锁

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

#### (3)_lookup_返回裸指针给锁外使用

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

#### (4)_remove_时先_put_后_unlink

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

#### (5)_lookup_成功后忘记_put

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

#### (6)_lookup_得到引用后_handoff_失败忘记回滚

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

### 8.8.2_lookup_函数设计模板

#### (1)_list_+_mutex_模板

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

#### (2)_list_+_mutex_+_state_模板

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

#### (3)_hash_+_spinlock_模板

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

#### (4)_xarray_+_lock_模板

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

#### (5)_RCU_+_kref_get_unless_zero()_模板预告

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

### 8.8.3_lookup_API_的命名建议

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

### 8.8.4_本章检查清单

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

## 8.9_本章小结

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
