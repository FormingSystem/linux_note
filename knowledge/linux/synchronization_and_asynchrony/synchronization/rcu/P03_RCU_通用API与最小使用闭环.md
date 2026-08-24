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


## 3.1\_RCU\_通用\_API与最小使用闭环

前两章已经推出 RCU 的四个动作：**构造新版本、发布新入口、等待边界前读者、回收旧版本**。本章先把它们组成一段能运行的代码，再解释接口。这样后文看到 `gp_seq`、`qsmask` 和被抢占任务链表时，读者始终知道这些状态在替哪一行应用代码工作。

### 3.1.1\_场景\_高频查询服务配置\_低频整体替换

假设内核模块维护一份服务配置：网络收包和状态查询路径每秒读取几十万次，管理线程偶尔替换整份配置。读者允许继续使用旧配置完成当前操作，但不能看到只初始化一半的新配置，也不能访问已经释放的旧配置。

参与对象如下：

```mermaid
flowchart LR
    R0["CPU0 收包读者"] -->|"读取 active_cfg"| Entry["RCU 共享入口<br/>active_cfg"]
    R1["CPU1 查询读者"] -->|"读取 active_cfg"| Entry
    W["CPU2 管理线程"] -->|"构造并替换"| Entry
    Entry --> Old["旧配置 old_cfg"]
    Entry -.-> New["新配置 new_cfg"]
    W -->|"请求 GP"| Core["Tree RCU"]
    Core -->|"GP 完成后允许回收"| Old
```

问题约束是：

1. 多个更新者必须由模块自己的锁串行化。
2. 新对象全部初始化后才能发布。
3. reader 取得的是共享对象的 **直接指针**，不是自动复制的副本。
4. reader 只能在 RCU 读侧临界区内借用这块内存。
5. 旧入口被替换后，旧对象仍须保留到 GP 结束。

### 3.1.2\_完整同步实现

#### (1)\_对象与共享入口

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

#### (2)\_读者\_直接借用当前对象

```c
static int service_fast_path(void)
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
	return enabled ? timeout : 0;
}
```

`rcu_dereference(active_cfg)` 返回当前指针值。它没有调用 `kmemdup()`，也没有隐式增加引用计数。退出 `rcu_read_unlock()` 后，`cfg` 这个数值仍可能留在寄存器或栈上，但调用者已经失去继续解引用它的生命期保证。

#### (3)\_同步更新者\_替换后等待并释放

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

#### (4)\_异步更新者\_把回收交给回调

不允许阻塞的更新路径可以改用 `call_rcu()`：

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
	old_cfg = rcu_replace_pointer(
		active_cfg, new_cfg,
		lockdep_is_held(&cfg_update_lock));
	mutex_unlock(&cfg_update_lock);

	if (old_cfg)
		call_rcu(&old_cfg->rcu, service_cfg_free_rcu);
	return 0;
}
```

这里的唯一回收出口是 `service_cfg_free_rcu()`。同一对象不能有时直接 `kfree()`，有时又登记 RCU 回调，否则调用者无法证明哪条路径拥有最终销毁权。

### 3.1.3\_用一条时序解释每个接口

```mermaid
sequenceDiagram
    autonumber
    participant R0 as CPU0旧读者
    participant E as active_cfg
    participant W as CPU2更新者
    participant GP as Tree RCU
    participant R1 as CPU1晚到读者

    R0->>R0: rcu_read_lock()
    R0->>E: rcu_dereference(active_cfg)
    E-->>R0: 返回old_cfg直接指针
    R0->>R0: 使用old_cfg

    W->>W: 分配并初始化new_cfg
    W->>E: rcu_assign_pointer(active_cfg, new_cfg)
    Note over E: old_cfg从正式入口不可达
    W->>GP: synchronize_rcu()

    R1->>R1: 此时才真正开始运行
    R1->>E: rcu_dereference(active_cfg)
    E-->>R1: 返回new_cfg
    Note over R1: 任务过去已经创建或排队<br/>不等于过去已经取得old_cfg

    R0->>R0: rcu_read_unlock()
    GP-->>W: 边界前潜在旧读者已经结束
    W->>W: kfree(old_cfg)
```

这条时序给出后续所有实现章节的观察目标：Tree RCU 不需要知道 R0 取得了哪个对象，也不等待 R1 将来执行一次。它只需证明：**在入口切换和 GP 边界以前可能已经进入的读侧执行现场，不能继续越过 GP 完成点使用旧对象。**

### 3.1.4\_接口放回生命周期链

| 阶段 | 常用接口 | 提供的保证 | 不提供的保证 |
| --- | --- | --- | --- |
| 进入读区 | `rcu_read_lock()` | 建立当前 RCU 类型的读侧执行约束 | 不登记对象地址，不取得长期所有权 |
| 取得指针 | `rcu_dereference()` | 单次读取、依赖/取得顺序与检查语义 | 不复制对象，不增加 kref |
| 发布 | `rcu_assign_pointer()` | release 发布已初始化对象 | 不串行化多个写者 |
| 更新侧替换 | `rcu_replace_pointer()` | 在指定更新保护条件下取旧值并发布新值 | 不等待 GP |
| 同步等待 | `synchronize_rcu()` | 返回时已跨过一个满足调用边界的 GP | 不保证所有已排队回调已经执行 |
| 异步等待 | `call_rcu()` | 目标 GP 后调用回调 | 调用者不能立即释放承载 `rcu_head` 的内存 |
| 延迟释放 | `kfree_rcu()` | 目标 GP 后释放对象 | 不替调用者取消发布入口 |
| 等待回调 | `rcu_barrier()` | 等待调用前排队的普通 RCU 回调执行完成 | 不是普通对象删除的 GP 替代品 |

发布/取得的通用内存顺序见[Linux 内存顺序专题](../memory_ordering/大纲.md)。本专题后文只解释这些原语怎样与 GP 和对象生命期组合，不重复维护体系结构屏障教程。

### 3.1.5\_同步与异步回收怎样选择

#### (1)\_使用\_synchronize\_rcu()

适合以下情况：

- 当前上下文允许睡眠。
- GP 后还要同步释放多个资源或执行允许睡眠的收尾。
- 更新频率低，调用者愿意承担等待延迟。

禁止在普通 RCU 读侧临界区内调用；源码中的 lockdep 检查会诊断这种自等待风险。

#### (2)\_使用\_call\_rcu()或\_kfree\_rcu()

适合更新路径不能等待，或者大量对象可以共享 GP 并批量推进的情况。代价是旧对象和回调会在一段时间内积压，模块退出还要确认回调代码不会在卸载后执行。

#### (3)\_模块卸载为什么还需要\_rcu\_barrier()

`synchronize_rcu()` 只保证一个 GP 完成，不保证此前排队的每个 callback 函数体都已经运行。若 callback 位于即将卸载的模块文本中，退出路径通常要先停止新回调来源，再用 `rcu_barrier()` 等待已有回调执行完毕。

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

### 3.1.6\_两个必须在基础章就识别的错误

#### (1)\_把旧指针带出读区

```c
static struct service_cfg *saved;

rcu_read_lock();
saved = rcu_dereference(active_cfg);
rcu_read_unlock();

/* 错误：这里已经没有任何机制保证 saved 指向的内存仍存在。 */
queue_work(system_wq, &work_using_saved);
```

GP 只覆盖受协议约束的读侧区间。跨区间长期保存对象要取得独立引用、转移所有权或改用能覆盖完整使用期的锁。

#### (2)\_仍有另一个入口指向旧对象

如果模块更新了 `active_cfg`，却还允许某个无锁缓存、全局数组或 work item 用裸指针取得 `old_cfg`，Tree RCU 不会扫描这些地址。调用者必须把全部正式入口纳入同一取消发布和生命期协议。

### 3.1.7\_本章留下的问题

现在已经会正确调用接口，但还不知道 `synchronize_rcu()` 如何完成证明：

1. 写者不知道实际有哪些 reader，怎样决定等待谁？
2. 一个任务已经排队但尚未运行，为什么不需要等待？
3. `rcu_read_lock()` 是否登记 reader？
4. `rcu_read_unlock()` 是否直接清 `qsmask`？
5. 非抢占式和抢占式构建为何需要两种不同的状态承载方式？

下一章先处理 RCU 与 kref 的对象生命期组合，随后以这五个问题分别推导非抢占式和抢占式 Tree RCU。

上一篇：[RCU 抽象机制推演](P02_RCU_抽象机制推演.md)。

下一篇：[RCU、kref 与复合对象生命周期](P04_RCU_kref与复合对象生命周期.md)。
