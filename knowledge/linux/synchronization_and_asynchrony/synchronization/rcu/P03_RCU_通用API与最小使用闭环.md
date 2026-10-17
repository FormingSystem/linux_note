---
id: knowledge.linux.synchronization.rcu.api_reference
title: "RCU 通用 API 与最小使用闭环"
kind: interface
status: evolving
domains:
  - linux
  - kernel
topics:
  - synchronization
  - rcu
---
# 第3章\_RCU\_通用API与最小使用闭环


## 3.1\_先把四个动作还原成一条应用闭环

前两章已经推出 RCU 的四个动作：**构造新版本、发布新入口、等待边界前读者、回收旧版本**。本章沿服务配置的替换过程逐段解释接口，再把片段合成完整模块。读者将能指出：哪一行建立可见性，哪一行转交回收责任，退出时又要等待谁。

## 3.2\_场景\_高频查询服务配置\_低频整体替换

假设内核模块维护一份服务配置：网络收包和状态查询路径每秒读取几十万次，管理线程偶尔替换整份配置。读者允许继续使用旧配置完成当前操作，但不能看到只初始化一半的新配置，也不能访问已经释放的旧配置。

参与对象如下：

```mermaid
flowchart LR
    R0["CPU0 收包读者"] -->|"读取 active_cfg"| Entry["RCU 共享入口<br/>active_cfg"]
    R1["CPU1 查询读者"] -->|"读取 active_cfg"| Entry
    W["CPU2 管理线程"] -->|"构造并替换"| Entry
    Entry -->|"替换后指向"| New["新配置 new_cfg"]
    R0 -.->|"已取得地址暂存在本地"| Old["退休配置 old_cfg"]
    W -->|"请求 GP"| Core["普通 RCU"]
    Core -->|"GP完成后交付回收资格"| X["退休执行者<br/>同步writer或回调路径"]
    X -->|"执行本次唯一销毁动作"| Old
```

图中“退休执行者”是职责：同步时仍由管理线程承担，异步时由 RCU 回调路径承担，不表示总有一条新增的业务线程。其余约束如下：

1. 多个更新者必须由模块自己的锁串行化。
2. 新对象全部初始化后才能发布。
3. reader 取得的是共享对象的 **直接指针**，不是自动复制的副本。
4. reader 只能在 RCU 读侧临界区内借用这块内存。
5. 旧入口被替换后，旧对象仍须保留到 GP 结束。

## 3.3\_完整同步实现

下面按“定义共享入口 → reader 借用 → writer 替换 → 等待旧 reader → 释放旧对象”的顺序闭合一次同步更新。先观察每个接口替哪一段生命周期负责，再讨论接口名和配置分支。

### 3.3.1\_对象与共享入口

```c
struct service_cfg {
	struct rcu_head rcu;
	u32 generation;
	u32 timeout_ms;
	bool enabled;
};

static struct service_cfg __rcu *active_cfg;
static DEFINE_MUTEX(cfg_update_lock);
```

`active_cfg` 是唯一正式查找入口。`__rcu` 让 Sparse 能检查部分错误用法，但它不是运行时锁，也不会扫描系统中是否还有别的裸指针。

Sparse 是 C 静态检查工具，`__rcu` 是给它的指针标记。`rcu_head` 用于后面的异步回收登记，里面不保存“还有几个读者”；`generation` 是业务版本标记，32 位自然回绕，不承担 RCU 的安全证明。更新者持 `cfg_update_lock` 取得旧入口并发布新入口，读者仅借用已发布的不可变对象。

### 3.3.2\_读者\_直接借用当前对象

```c
static int service_fast_path(u32 *timeout_ms)
{
	struct service_cfg *cfg;
	u32 timeout;
	bool enabled;

	rcu_read_lock();
	cfg = rcu_dereference(active_cfg);
	if (!cfg) {
		rcu_read_unlock();
		return -ENOENT;
	}

	/* cfg 是共享对象本身的地址，不是副本。 */
	timeout = READ_ONCE(cfg->timeout_ms);
	enabled = READ_ONCE(cfg->enabled);
	rcu_read_unlock();

	/* 这里只使用已经复制到栈上的标量，不再解引用 cfg。 */
	*timeout_ms = enabled ? timeout : 0;
	return 0;
}
```

`rcu_dereference(active_cfg)` 返回当前指针值。它没有调用 `kmemdup()`，也没有隐式增加引用计数。退出 `rcu_read_unlock()` 后，`cfg` 这个数值仍可能留在寄存器或栈上，但调用者已经失去继续解引用它的生命期保证。

返回值只表示成功或错误，超时值通过调用者提供的 `u32` 地址输出，避免把大于 `INT_MAX` 的合法超时误当成负错误码。输出地址必须有效且由调用者保证可写；没有配置时返回 `-ENOENT`，不修改输出值。`READ_ONCE()` 限制单次字段访问，但两个字段属于同一业务版本，是因为本例发布后不再原地修改它们，而不是因为用了两次 `READ_ONCE()` 就自动得到快照。

### 3.3.3\_同步更新者\_替换后等待并释放

```c
static int service_cfg_replace(u32 timeout_ms, bool enabled)
{
	struct service_cfg *new_cfg;
	struct service_cfg *old_cfg;

	new_cfg = kzalloc(sizeof(*new_cfg), GFP_KERNEL);
	if (!new_cfg)
		return -ENOMEM;

	/* 发布以前完成 reader 可见字段初始化。 */
	new_cfg->timeout_ms = timeout_ms;
	new_cfg->enabled = enabled;

	mutex_lock(&cfg_update_lock);
	old_cfg = rcu_dereference_protected(
		active_cfg, lockdep_is_held(&cfg_update_lock));
	new_cfg->generation = old_cfg ? old_cfg->generation + 1 : 1;
	rcu_assign_pointer(active_cfg, new_cfg);
	mutex_unlock(&cfg_update_lock);

	/* 这里允许睡眠，而且已经不持有更新锁。 */
	synchronize_rcu();
	kfree(old_cfg);
	return 0;
}
```

这段代码的安全性来自明确顺序：

```text
初始化 new_cfg
    -> 发布 new_cfg 并切断 old_cfg 的正式共享可达性
        -> 等待更新前可能取得 old_cfg 的读侧现场结束
            -> 释放 old_cfg
```

把 `synchronize_rcu()` 放在更新锁外不是 RCU 的硬性语法要求，而是常见的锁设计：等待 GP 可能很久，持锁等待会无谓阻塞其他更新者；同时还必须检查锁顺序，避免 GP 所等待的 reader 又需要这把锁才能退出。

### 3.3.4\_异步更新者\_区分私有析构与固定释放

不希望同步等待 GP 的更新路径可以把回收责任交给普通 RCU 的异步管线，但必须先区分两种不同需求：`call_rcu()` 接收调用者提供的 callback，可以在 GP 后执行对象私有析构；`kfree_rcu(ptr, rcu_member)` 不接收私有函数，只能在 GP 后通过内核通用释放管线销毁整个对象。二者共享“先取消发布，再等待边界前 reader”的生命期契约，回收动作和实现管线却不相同。

#### (1)\_需要私有析构时使用call\_rcu

本例先使用通用的 `call_rcu()`。下面仍在允许睡眠的进程上下文取得 mutex，所以整个更新函数不能放进硬中断或普通 RCU 读区；异步只移除了等待 GP 这一段。传入的 `new_cfg` 必须是调用者私有、尚未发布的非空独立分配，`timeout_ms` 与 `enabled` 已初始化，成功发布后所有权交给本模块，调用者不能再修改或释放它：

```c
static void service_cfg_free_rcu(struct rcu_head *head)
{
	struct service_cfg *cfg;

	cfg = container_of(head, struct service_cfg, rcu);
	kfree(cfg);
}

static int service_cfg_replace_async(struct service_cfg *new_cfg)
{
	struct service_cfg *old_cfg;

	mutex_lock(&cfg_update_lock);
	old_cfg = rcu_dereference_protected(
		active_cfg, lockdep_is_held(&cfg_update_lock));
	new_cfg->generation = old_cfg ? old_cfg->generation + 1 : 1;
	rcu_assign_pointer(active_cfg, new_cfg);
	mutex_unlock(&cfg_update_lock);

	if (old_cfg)
		call_rcu(&old_cfg->rcu, service_cfg_free_rcu);
	return 0;
}
```

`call_rcu()` 只取得内嵌 `rcu_head` 的地址和 callback 函数地址。callback 以后收到的也是 `struct rcu_head *`，所以 `service_cfg_free_rcu()` 必须用 `container_of()` 找回 `service_cfg` 基地址，再执行私有清理。若对象还要归还引用、释放嵌套资源或调用专用 allocator，这些动作都应由该 callback 明确完成，并遵守普通 RCU callback 的执行上下文约束。

这里的唯一回收出口是 `service_cfg_free_rcu()`。本次交给回调的对象不能再由更新者直接 `kfree()`。同步和异步函数可以在同一模块中用于不同次更新，但每个从入口摘下的对象只能沿被选中的一条销毁路径走完。

#### (2)\_只需释放整个对象时使用kfree\_rcu

如果 `service_cfg_free_rcu()` 唯一要做的事就是 `kfree(cfg)`，可以改用 Linux 普通 RCU 的公共便利宏。下列片段展示另一种退休选择，假定 `new_cfg` 连业务代次也已按更新锁协议初始化；不能与上一段对同一个 `old_cfg` 重复执行：

```c
mutex_lock(&cfg_update_lock);
old_cfg = rcu_replace_pointer(
	active_cfg, new_cfg,
	lockdep_is_held(&cfg_update_lock));
mutex_unlock(&cfg_update_lock);

if (old_cfg)
	kfree_rcu(old_cfg, rcu);
```

这不是驱动私有接口，也不是适用于所有 RCU flavor 的抽象标准。它是声明在 Linux 公共内核头文件中的普通 RCU 固定动作回收接口，但仍属于内核内部源码 API，不是用户态稳定 ABI 或跨操作系统标准。真实签名是 `kfree_rcu(ptr, rcu_member)`：

- `ptr` 是待释放对象的基地址，本例为 `old_cfg`；
- `rcu_member` 是该对象内嵌 `struct rcu_head` 的 **成员名**，本例为 `rcu`，不是函数指针；
- `ptr` 必须是能够由通用 `kfree()` / `kvfree()` 语义释放的完整分配对象，不能是某个更大分配中的子对象，也不能依赖私有析构协议；
- 调用者仍须先从 `active_cfg` 等全部正式入口摘除对象；该宏不会寻找或取消发布入口；
- 该宏不会调用 `service_cfg_free_rcu()` 或其他私有 release，最终动作固定为内核通用内存释放。需要私有析构时必须回到 `call_rcu()`。

`kfree_rcu(old_cfg, rcu)` 不需要业务侧 `container_of()`：调用宏时已经同时具有对象基地址和成员名，可以直接得到 `old_cfg` 与 `&old_cfg->rcu`。这两个参数怎样进入 Linux 6.12 的通用释放管线，见 3.5.1 节。

## 3.4\_用同步路径时序固定GP边界

```mermaid
sequenceDiagram
    autonumber
    participant R0 as CPU0旧读者
    participant E as active_cfg
    participant W as CPU2更新者
    participant GP as 普通RCU
    participant R1 as CPU1晚到读者

    R0->>R0: rcu_read_lock()
    R0->>E: rcu_dereference(active_cfg)
    E-->>R0: 返回old_cfg直接指针
    R0->>R0: 使用old_cfg

    W->>W: 分配并初始化new_cfg
    W->>E: rcu_assign_pointer(active_cfg, new_cfg)
    Note over E: old_cfg从正式入口不可达
    W->>GP: synchronize_rcu()

    R1->>R1: 进入读侧临界区
    R1->>E: rcu_dereference(active_cfg)
    E-->>R1: 本条交错中观察到new_cfg
    Note over R1: 任务创建不是进入读区<br/>并发读取不保证立即得到最新版本
    R1->>R1: 使用复制出的标量前退出读区

    R0->>R0: rcu_read_unlock()
    GP-->>W: 边界前潜在旧读者已经结束
    W->>W: kfree(old_cfg)
```

图中选择了一条 R1 读到新版本的合法交错，没有声明发布后所有并发读者立即看到新值。它给出后续实现章节的观察目标：普通 RCU 不需要知道 R0 取得了哪个对象，也不要求一个尚未进入读区的任务将来必须执行一次。与等待并发进入的读者可能被保守覆盖；不能从图上推导绝不等待 R1。它只需证明：**在入口切换和 GP 边界以前可能已经进入的读侧执行现场，不能继续越过 GP 完成点使用旧对象。**

## 3.5\_接口放回生命周期链

| 阶段 | 常用接口 | 提供的保证 | 不提供的保证 |
| --- | --- | --- | --- |
| 进入读区 | `rcu_read_lock()` | 建立当前 RCU 类型的读侧执行约束 | 不登记对象地址，不取得长期所有权 |
| 取得指针 | `rcu_dereference()` | 单次读取、依赖/取得顺序与检查语义 | 不复制对象，不增加 kref |
| 发布 | `rcu_assign_pointer()` | release 发布已初始化对象 | 不串行化多个写者 |
| 更新侧替换 | `rcu_replace_pointer()` | 在指定更新保护条件下取旧值并发布新值 | 不等待 GP |
| 同步等待 | `synchronize_rcu()` | 返回时已跨过一个满足调用边界的 GP | 不保证所有已排队回调已经执行 |
| 异步等待 | `call_rcu(head, callback)` | 普通 RCU 的目标 GP 后调用私有 callback | 调用者不能立即释放承载 `rcu_head` 的内存 |
| 固定动作延迟释放 | `kfree_rcu(ptr, rcu_member)` | 通过普通 RCU 的通用管线在目标 GP 后释放整个对象 | 不调用私有 release，不替调用者取消发布入口 |
| 等待回调 | `rcu_barrier()` | 等待调用前排队的普通 RCU 回调执行完成 | 不是普通对象删除的 GP 替代品 |

### 3.5.1\_kfree\_rcu参数怎样进入通用释放管线

在本专题的 Linux 6.12.20 源码基线中，代入本例参数后的关键宏展开等价于：

```c
typeof(old_cfg) ___p = old_cfg;
if (___p) {
	BUILD_BUG_ON(offsetof(typeof(*old_cfg), rcu) >= 4096);
	kvfree_call_rcu(&___p->rcu, (void *)___p);
}
```

`BUILD_BUG_ON()` 要求 `rcu_head` 成员位于对象起始地址后的前 4096 字节内；`kvfree_call_rcu()` 同时取得成员地址和对象基地址，所以不需要从成员反推业务对象，也没有私有 release 可以调用。

Linux 6.12 的常见路径把对象基地址直接记录进 per-CPU 批次，取得 GP 快照，随后由 `rcu_work` 在 GP 后进入 workqueue，并使用 `kfree_bulk()` 或 `vfree()` 批量释放。批量记录不可用时，回退路径把基地址暂存在 `rcu_head.func`，GP 后再取出并调用 `kvfree()`。源码中的另一个 `container_of()` 只用于从内部 `rcu_work` 找回 `kfree_rcu_cpu_work`，与业务对象无关。

这段版本化实现可以从 [Linux 6.12 RCU 源码总阅读索引](../../../../../research/source_reading/rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.1_版本边界与总索引职责)进入核对；公共宏定义位于 [`include/linux/rcupdate.h`](../../../../../research/source_reading/linux/include/linux/rcupdate.h)，底层 `kvfree_call_rcu()` 在 [`kernel/rcu/tree.c`](../../../../../research/source_reading/linux/kernel/rcu/tree.c) 中通过 `EXPORT_SYMBOL_GPL` 导出，批处理、GP 快照和最终释放也位于该文件。这个声明与导出边界证明它不是某个驱动的私有封装；应用代码可以依赖“普通 RCU GP 后释放”的契约，不应依赖当前批次数量、延迟定时器或 workqueue 布局。

发布/取得的通用内存顺序见[Linux 内存顺序专题](../memory_ordering/大纲.md)。本专题后文只解释这些原语怎样与 GP 和对象生命期组合，不重复维护体系结构屏障教程。

## 3.6\_同步与异步回收怎样选择

同步与异步两类路径共享发布、旧 reader 边界和对象摘除契约，差别只在 **由当前调用者等待并回收，还是把回收责任交给普通 RCU 的异步 callback 或批量释放管线**。选择前同时核对调用上下文、可接受延迟、积压预算和模块退出方式。

### 3.6.1\_使用\_synchronize\_rcu()

适合以下情况：

- 当前上下文允许睡眠。
- GP 后还要同步释放多个资源或执行允许睡眠的收尾。
- 更新频率低，调用者愿意承担等待延迟。

禁止在普通 RCU 读侧临界区内调用；源码中的 lockdep 检查会诊断这种自等待风险。

### 3.6.2\_使用\_call\_rcu()或\_kfree\_rcu()

两者都适合更新路径不愿同步等待 GP 的场景，但不能只因为“都是异步回收”就互换：

- GP 后需要对象私有析构时，使用 `call_rcu(head, callback)`；callback 用 `container_of()` 找回对象并完成唯一销毁协议。
- GP 后只需通用释放整个对象时，使用 `kfree_rcu(ptr, rcu_member)`；它不登记模块私有函数地址，Linux 6.12 还会把多个请求放进独立批处理管线。

共同代价是旧对象会在一段时间内积压。`kfree_rcu()` 的批处理还可能让实际释放晚于“刚好跨过一个 GP”的最早时刻，因此内存峰值和释放时延必须按更新速率与回收积压共同评估。

### 3.6.3\_模块卸载时到底等待哪条异步管线

`synchronize_rcu()` 只保证一个 GP 完成，不保证此前排队的每个 callback 函数体都已经运行。若使用 `call_rcu()`，而 callback 位于即将卸载的模块文本中，退出路径通常要先停止新回调来源，再用 `rcu_barrier()` 等待已有普通 RCU callback 执行完毕。

沿用本章配置对象，模块退出的最小闭环可以写成：

```c
static void service_cfg_shutdown(void)
{
	struct service_cfg *old_cfg;

	/* 调用者此前必须停止所有查询入口和配置更新来源。 */
	mutex_lock(&cfg_update_lock);
	old_cfg = rcu_replace_pointer(
		active_cfg, NULL,
		lockdep_is_held(&cfg_update_lock));
	mutex_unlock(&cfg_update_lock);

	/* 保护刚从入口摘除、尚未交给回调的最后一个对象。 */
	synchronize_rcu();
	kfree(old_cfg);

	/* 等待更早的异步更新所登记的回调函数体执行完毕。 */
	rcu_barrier();
}
```

这里有两个不同等待条件：`synchronize_rcu()` 让最后一个 `old_cfg` 跨过读者边界；`rcu_barrier()` 防止更早登记的 `service_cfg_free_rcu()` 在模块文本卸载后才执行。前提“已经停止所有新更新来源”不可省略，否则 `rcu_barrier()` 返回后仍可能有人登记新回调。

若异步更新全部使用 `kfree_rcu()`，请求中没有模块私有 callback 函数地址，因此“防止模块文本被卸载”这一理由不再要求 `rcu_barrier()`。但如果模块退出时还要销毁对象来源的私有 slab cache，或者必须确认通用批处理释放已经完成，Linux 6.12 Tree RCU 应等待 `kvfree_rcu_barrier()` 所覆盖的释放管线；不能假定 `rcu_barrier()` 会替它排空 `kfree_rcu()` 的独立批次。

## 3.7\_运行一次配置发布到退出的完整闭环

现在把前面的对象、查询、两种更新和退出函数放到同一个可构建文件中。实验只在加载线程内顺序执行三次更新：先发布启用的 100 ms 配置，再发布启用的最大 32 位值，最后发布禁用配置。每次查询只带出标量；卸载时回收最后一份对象并排空此前私有回调。两次替换分别选择同步和异步退休，能够观察不同请求共存时最终销毁权仍然清楚。

本实验没有注册设备、启动并发查询线程或模拟网络流量，所以退出函数要求的“停止新来源”在这里由模块结构直接满足。它验证接口组合与资源收尾，不验证多核速度或抢占读者；后者在第 6 章的专门实验中建立。

### 3.7.1\_完整模块与失败收尾

在专用实验目录保存下面完整文件为 `service_config.c`，或取用仓库中的 [同源 C 文件](../../../../../labs/kernel/rcu/materials/service_config/service_config.c)。该名称由本实验选择，后面的构建目标必须与它一致。

```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>

struct service_cfg {
	struct rcu_head rcu;
	u32 generation;
	u32 timeout_ms;
	bool enabled;
};

static struct service_cfg __rcu *active_cfg;
static DEFINE_MUTEX(cfg_update_lock);

static int service_fast_path(u32 *timeout_ms)
{
	struct service_cfg *cfg;
	u32 timeout;
	bool enabled;

	rcu_read_lock();
	cfg = rcu_dereference(active_cfg);
	if (!cfg) {
		rcu_read_unlock();
		return -ENOENT;
	}

	/* cfg 是共享对象本身的地址，不是副本。 */
	timeout = READ_ONCE(cfg->timeout_ms);
	enabled = READ_ONCE(cfg->enabled);
	rcu_read_unlock();

	/* 这里只使用已经复制到栈上的标量，不再解引用 cfg。 */
	*timeout_ms = enabled ? timeout : 0;
	return 0;
}

static int service_cfg_replace(u32 timeout_ms, bool enabled)
{
	struct service_cfg *new_cfg;
	struct service_cfg *old_cfg;

	new_cfg = kzalloc(sizeof(*new_cfg), GFP_KERNEL);
	if (!new_cfg)
		return -ENOMEM;

	/* 发布以前完成 reader 可见字段初始化。 */
	new_cfg->timeout_ms = timeout_ms;
	new_cfg->enabled = enabled;

	mutex_lock(&cfg_update_lock);
	old_cfg = rcu_dereference_protected(
		active_cfg, lockdep_is_held(&cfg_update_lock));
	new_cfg->generation = old_cfg ? old_cfg->generation + 1 : 1;
	rcu_assign_pointer(active_cfg, new_cfg);
	mutex_unlock(&cfg_update_lock);

	/* 这里允许睡眠，而且已经不持有更新锁。 */
	synchronize_rcu();
	kfree(old_cfg);
	return 0;
}

static void service_cfg_free_rcu(struct rcu_head *head)
{
	struct service_cfg *cfg;

	cfg = container_of(head, struct service_cfg, rcu);
	kfree(cfg);
}

static int service_cfg_replace_async(struct service_cfg *new_cfg)
{
	struct service_cfg *old_cfg;

	mutex_lock(&cfg_update_lock);
	old_cfg = rcu_dereference_protected(
		active_cfg, lockdep_is_held(&cfg_update_lock));
	new_cfg->generation = old_cfg ? old_cfg->generation + 1 : 1;
	rcu_assign_pointer(active_cfg, new_cfg);
	mutex_unlock(&cfg_update_lock);

	if (old_cfg)
		call_rcu(&old_cfg->rcu, service_cfg_free_rcu);
	return 0;
}

static void service_cfg_shutdown(void)
{
	struct service_cfg *old_cfg;

	/* 调用者此前必须停止所有查询入口和配置更新来源。 */
	mutex_lock(&cfg_update_lock);
	old_cfg = rcu_replace_pointer(
		active_cfg, NULL,
		lockdep_is_held(&cfg_update_lock));
	mutex_unlock(&cfg_update_lock);

	/* 保护刚从入口摘除、尚未交给回调的最后一个对象。 */
	synchronize_rcu();
	kfree(old_cfg);

	/* 等待更早的异步更新所登记的回调函数体执行完毕。 */
	rcu_barrier();
}

/* 只比较读区内复制出的值，不保存共享对象地址。 */
static int service_expect(u32 expected)
{
	u32 timeout_ms = 0;
	int ret = service_fast_path(&timeout_ms);

	if (ret)
		return ret;
	pr_info("service_config: observed=%u expected=%u\n", timeout_ms, expected);
	return timeout_ms == expected ? 0 : -EINVAL;
}

static int __init service_cfg_init(void)
{
	struct service_cfg *candidate;
	u32 untouched = 7;
	int ret;

	/* 尚未发布时应返回缺失，且不修改输出。 */
	ret = service_fast_path(&untouched);
	if (ret != -ENOENT || untouched != 7)
		return -EINVAL;

	ret = service_cfg_replace(100, true);
	if (ret)
		goto fail;
	ret = service_expect(100);
	if (ret)
		goto fail;

	/* 候选在发布前始终私有；本函数成功交接后不再使用它。 */
	candidate = kzalloc(sizeof(*candidate), GFP_KERNEL);
	if (!candidate) {
		ret = -ENOMEM;
		goto fail;
	}
	candidate->timeout_ms = 0xffffffffU;
	candidate->enabled = true;
	service_cfg_replace_async(candidate);
	ret = service_expect(0xffffffffU);
	if (ret)
		goto fail;

	ret = service_cfg_replace(300, false);
	if (ret)
		goto fail;
	ret = service_expect(0);
	if (ret)
		goto fail;

	pr_info("service_config: lifecycle checks passed\n");
	return 0;

fail:
	/* 初始化失败不会进入正常module_exit，必须在此排空。 */
	service_cfg_shutdown();
	return ret;
}

static void __exit service_cfg_exit(void)
{
	service_cfg_shutdown();
	pr_info("service_config: shutdown complete\n");
}

module_init(service_cfg_init);
module_exit(service_cfg_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RCU service configuration lifecycle example");
```

`service_expect()` 检查本次复制的值而不保留对象指针。中间使用 `0xffffffffU`，是为了检查“业务数值与错误返回值分离”这一契约。失败分支也调用同一关闭函数，因为初始化返回错误时不会再自动调用模块退出函数。只有成功发布的对象交给入口/退休协议，分配失败没有产生新对象，也不能借此跳过已有对象的清理。

### 3.7.2\_构建观察与恢复

以下操作在用于实验的 Linux 系统执行，需要与目标运行内核一致的已准备构建树、模块构建能力和加载权限。Windows 宿主的 C 检查不能代替这些条件。先建立独立目录，把上一节 C 文件放进去：

```bash
# Linux实验机：在自己的实验父目录中执行。
mkdir -p rcu_service_config
cd rcu_service_config
```

这里的目录名没有内核含义，只是隔离构建产物。当前目录应已有 `service_config.c`，然后新建 `Makefile`，完整内容为：

```makefile
obj-m += service_config.o
```

`obj-m` 要求 Kbuild 从同名 C 文件生成可加载模块；文件名应为 `Makefile`，不是带文本扩展名的文件。先定位运行内核配套的构建入口：

```bash
# Linux实验机：仍在rcu_service_config目录。
kernel_build="/lib/modules/$(uname -r)/build"
test -f "$kernel_build/Makefile"
```

`uname -r` 给出运行内核发布名，末条命令成功才说明该约定路径下有构建入口。若不存在，应先安装或准备匹配的构建树；不能用任意另一个版本的头文件继续。构建时：

```bash
make -C "$kernel_build" M="$PWD" modules
```

`-C` 进入内核构建树，`M` 指向当前外部模块目录。成功后本目录生成 `service_config.ko`；编译器错误、MODPOST 未解析符号或版本不匹配都表示此步骤未完成。不要用强制加载参数绕过版本检查。

```bash
sudo insmod ./service_config.ko
```

加载会运行三轮顺序检查。返回失败时先看内核日志，不能因旧日志中存在一次成功就认为这次成功。读取本次加载附近的消息：

```bash
sudo dmesg --color=never | tail -n 30
```

预期依次出现 `observed=100 expected=100`、`observed=4294967295 expected=4294967295`、`observed=0 expected=0`，随后是 `service_config: lifecycle checks passed`。这说明当前初始化路径读到了三个预期值，不说明 GP 固定耗时，也不能单靠这一行证明私有回调都已执行。随后卸载：

```bash
sudo rmmod service_config
```

卸载返回以后再读取日志，应出现 `service_config: shutdown complete`。它位于 `synchronize_rcu()` 与 `rcu_barrier()` 都返回之后。若卸载失败，保留错误并检查状态，不应直接删除构建文件来假装已经清理运行内核中的模块。成功卸载以后才清理本地构建产物：

```bash
make -C "$kernel_build" M="$PWD" clean
```

再做两个不破坏内核的变体。先把第二轮候选值和对应预期同时改为 200，三次查询应变成 100、200、0；再只把第二轮预期改成 201，加载应返回错误，失败路径仍必须排空旧回调并回收当前对象。不要通过删掉 GP 或提前 free 来在真实内核上制造 UAF。若只想检验错误推理，使用前两章的纸面交错即可。

## 3.8\_两个必须在基础章就识别的错误

前面的最小闭环只覆盖从 `active_cfg` 进入、在读区内借用、从同一入口摘除的对象。下面两个反例分别破坏 **使用期边界** 和 **入口完整性**；任何一个存在，等待 GP 都不足以推出可以释放。

### 3.8.1\_把旧指针带出读区

```c
static struct service_cfg *saved;

rcu_read_lock();
saved = rcu_dereference(active_cfg);
rcu_read_unlock();

/* 错误：这里已经没有任何机制保证 saved 指向的内存仍存在。 */
queue_work(system_wq, &work_using_saved);
```

GP 只覆盖受协议约束的读侧区间。跨区间长期保存对象要取得独立引用、转移所有权或改用能覆盖完整使用期的锁。

### 3.8.2\_仍有另一个入口指向旧对象

如果模块更新了 `active_cfg`，却还允许某个无锁缓存、全局数组或 work item 用裸指针取得 `old_cfg`，Tree RCU 不会扫描这些地址。调用者必须把全部正式入口纳入同一取消发布和生命期协议。

## 3.9\_本章留下的问题

现在已经把借用、发布、等待和最终释放配对起来，但还不知道 `synchronize_rcu()` 如何完成证明：

1. 写者不知道实际有哪些 reader，怎样决定等待谁？
2. 一个任务已经排队但尚未运行，为什么不需要等待？
3. `rcu_read_lock()` 是否登记 reader？
4. `rcu_read_unlock()` 是否直接完成全局证明？
5. 非抢占式和抢占式构建为何需要两种不同的状态承载方式？

下一章先区分保护域、实现家族与运行策略，再用公共周期组织这些问题；读者能否被抢占的差异在同一模块中比较。RCU 与 kref 的长期所有权组合留到第 21 章。

上一篇：[RCU 抽象机制推演](P02_RCU_抽象机制推演.md)。

下一篇：[RCU 分类坐标与内核配置](P04_RCU_分类坐标与内核配置.md)。
