---
id: knowledge.linux.synchronization.rcu.type_semantics_sparse_lockdep
title: "RCU 类型语义、Sparse 与 Lockdep"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
topics:
  - synchronization
  - rcu
---
# 第26章\_RCU\_类型语义\_Sparse与Lockdep


## 26.1\_RCU\_类型语义\_Sparse与\_Lockdep

前面的例子都依赖一条人工约定：共享入口必须用 RCU 发布/取得，取得动作必须位于正确保护条件中。本章用一个会编译、却可能 UAF 的错误驱动说明三层检查怎样分工：Sparse 检查指针类型，lockdep 检查运行时保护条件，业务代码仍须证明对象生命周期。

### 26.1.1\_具体问题\_普通赋值掩盖了哪两项错误

定义一个可替换的设备状态：

```c
struct dev_state {
	int status;
	struct rcu_head rcu;
};

static struct dev_state __rcu *gstate;
static DEFINE_MUTEX(state_lock);
```

下面的读取对普通 C 编译器看起来只是一次指针赋值：

```c
static int bad_read_status(void)
{
	struct dev_state *p = gstate; /* Sparse：不同 address space。 */

	return p->status;             /* 运行时：没有任何生命期保护。 */
}
```

同时发生更新时：

```c
old = rcu_replace_pointer(gstate, new,
			  lockdep_is_held(&state_lock));
kfree_rcu(old, rcu);
```

错误因果链是：

```text
读者普通加载gstate
    → 编译器/源码没有表达RCU取得契约
    → 读者也没有进入普通RCU读侧
    → 更新者取消发布并完成GP
    → 旧对象被释放
    → 读者随后解引用旧地址，发生UAF
```

Sparse 能发现第一项类型错误，却无法证明最后是否真的发生 UAF；lockdep 能发现某些“在错误保护上下文中取得”的问题，却也不知道业务所有权是否完整。二者是互补证据，不是内存安全证明器。

### 26.1.2\_rcu在Linux\_6.12中究竟是什么

`include/linux/compiler_types.h` 在 Sparse 定义 `__CHECKER__` 时声明：

```c
#define __rcu __attribute__((noderef, address_space(__rcu)))
```

含义是：

- `address_space(__rcu)` 把 RCU 保护的指针与普通内核指针区分；
- `noderef` 要求不能把这种受标记指针当普通指针直接解引用；
- `__force` 只能由清楚语义的底层宏显式跨越 address-space 类型边界。

普通编译器分支并不靠这些属性实现 GP 或内存序；Linux 6.12 可能保留 BTF type tag，但真正的 Sparse address-space 检查只在 `__CHECKER__` 路径生效。因此不能说“`__rcu` 属性会在运行时插入屏障”或“GCC 会禁止直接访问”。

```mermaid
flowchart LR
    S["共享入口<br/>struct obj __rcu *"] -->|"Sparse类型检查"| T["是否通过RCU宏<br/>跨越address space"]
    T -->|"rcu_dereference_check"| L["lockdep检查<br/>读侧或更新锁条件"]
    L --> M["READ_ONCE／依赖顺序<br/>或protected访问"]
    M --> U["调用者使用普通指针"]
    U -->|"仍由业务代码负责"| O["对象不逃逸<br/>GP/引用计数后回收"]
```

### 26.1.3\_正确的完整代码闭环

#### (1)\_发布者

```c
static int update_status(int value)
{
	struct dev_state *new;
	struct dev_state *old;

	new = kmalloc(sizeof(*new), GFP_KERNEL);
	if (!new)
		return -ENOMEM;
	new->status = value; /* 先完成对象初始化。 */

	mutex_lock(&state_lock);
	old = rcu_replace_pointer(gstate, new,
				  lockdep_is_held(&state_lock));
	mutex_unlock(&state_lock);

	if (old)
		kfree_rcu(old, rcu);
	return 0;
}
```

`rcu_replace_pointer()` 内部用 `rcu_dereference_protected()` 取得旧值，并用 `rcu_assign_pointer()` 发布新值。`__rcu` 使 Sparse 检查入口类型；更新 mutex 串行化多个发布者；`kfree_rcu()` 负责 GP 后回收。三者职责不同。

#### (2)\_普通读者

```c
static int read_status(void)
{
	struct dev_state *p;
	int value = -ENOENT;

	rcu_read_lock();
	p = rcu_dereference(gstate);
	if (p)
		value = READ_ONCE(p->status);
	rcu_read_unlock();

	return value;
}
```

`p` 是普通局部指针，仍直接指向 `gstate` 所发布的同一对象分配，并不是复制出的 `dev_state`。它只能在读侧内借用；若返回 `p` 或交给 workqueue，还要在读侧内取得 kref/refcount，具体所有权见 [RCU、kref 与复合对象生命周期](P04_RCU_kref与复合对象生命周期.md)。

#### (3)\_持有更新锁的读取

```c
static int read_status_while_updating(void)
{
	struct dev_state *p;
	int value = -ENOENT;

	mutex_lock(&state_lock);
	p = rcu_dereference_protected(gstate,
				      lockdep_is_held(&state_lock));
	if (p)
		value = p->status;
	mutex_unlock(&state_lock);
	return value;
}
```

`rcu_dereference_protected()` 只适用于调用者已经用更新侧同步阻止并发变化的路径。Linux 6.12 的底层实现甚至省略 `READ_ONCE()`；若实际没有持锁，偶发失败可能比普通错误加载更隐蔽。

### 26.1.4\_Sparse具体检查什么

Linux 6.12 的 `rcupdate.h` 在 `__CHECKER__` 下用：

```c
#define rcu_check_sparse(p, space) \
	((void)(((typeof(*p) space *)p) == p))
```

RCU 宏通过 `rcu_check_sparse(p, __rcu)` 验证传入入口是否带正确 address space，再在内部用 `__force` 生成可用的普通指针。于是 Sparse 可以报告：

- 把 `__rcu` 指针直接赋给普通指针；
- 把普通指针错误写入 `__rcu` 入口；
- 对受标记指针直接解引用；
- 给期待 `__rcu` 指针的 RCU 宏传入不匹配类型。

在内核源码树中检查本模块：

```bash
# 检查正常构建会重新编译的文件。
make C=1 M=drivers/example

# 强制对目标目录的C文件运行检查，适合专门审计。
make C=2 M=drivers/example
```

典型输出包含：

```text
warning: incorrect type in initializer (different address spaces)
```

`C=1`/`C=2` 是否可用还取决于构建机安装了 Sparse，并不等价于普通 `make` 自动执行全部静态检查。

### 26.1.5\_Lockdep检查的是哪一个运行时条件

`rcu_dereference(p)` 展开到：

```text
rcu_dereference_check(p, 0)
    → 检查 rcu_read_lock_held()
    → READ_ONCE(p)
    → Sparse类型检查
```

`rcu_dereference_check(p, c)` 则接受两条合法路径的析取：

```c
static struct dev_state *lookup_under_either_lock(void)
{
	return rcu_dereference_check(gstate,
				     lockdep_is_held(&state_lock));
}
```

它表达：调用者要么在普通 RCU 读侧，要么持有 `state_lock`。Linux 6.12 的底层 `__rcu_dereference_check()` 在 `CONFIG_PROVE_RCU` 生效时用 `RCU_LOCKDEP_WARN(!(c), ...)` 报告可疑调用。

RCU 链表宏也使用同一思路。例如带额外条件的 `list_for_each_entry_rcu()` 能表达遍历由指定 SRCU 域或更新锁保护；条件为假且没有任何认可的 RCU 读锁时，`CONFIG_PROVE_RCU_LIST` 路径会报告。

### 26.1.6\_三类检查不能互相替代

| 检查层 | 能发现 | 不能证明 |
| --- | --- | --- |
| Sparse / `__rcu` | 错误 address-space 赋值、直接解引用、宏参数类型不匹配 | 当前线程真的持有读锁；对象是否已释放 |
| lockdep / `*_check()` | 当前运行路径是否满足声明的读侧或锁条件 | 指针是否来自正确对象；引用是否被带出临界区 |
| KASAN、业务压力测试 | 已实际触发的越界/UAF 等动态错误 | 未覆盖交错一定安全 |
| 所有权与 GP 证明 | 取消发布、旧读者、最终释放的完整先后关系 | 需要人工设计并由测试辅助验证 |

一个调用点可以 Sparse 与 lockdep 全部通过，却仍然错误：

```c
static struct dev_state *bad_escape(void)
{
	struct dev_state *p;

	rcu_read_lock();
	p = rcu_dereference(gstate); /* 类型和保护条件都正确。 */
	rcu_read_unlock();
	return p;                    /* 生命周期错误：裸指针逃逸。 */
}
```

### 26.1.7\_容易写错的绝对化结论

| 错误说法 | 准确边界 |
| --- | --- |
| `__rcu` 会插入屏障 | `__rcu` 主要表达类型；发布/取得宏实现内存序 |
| `__rcu` 会阻止错误代码编译 | Sparse 通常给出诊断；普通编译与构建策略可能不运行它 |
| 所有含 RCU 的结构字段都要标 `__rcu` | 标记共享 RCU **指针入口**；普通 payload 字段和嵌入的 `rcu_head` 不是同一概念 |
| 链表头必须写成 `struct list_head __rcu *` | 常见模式是普通 `struct list_head` 头配合 `list_*_rcu()` 宏；受保护的是链接更新和节点生命期协议 |
| lockdep 通过就不会 UAF | lockdep 不跟踪业务对象所有权和临界区外裸指针 |

### 26.1.8\_交付核对表

| 检查项 | 要回答的问题 |
| --- | --- |
| RCU 入口类型 | 共享指针是否标为 `__rcu`，而非给所有相关字段滥加标记？ |
| 发布 | 新对象是否先初始化，再经 `rcu_assign_pointer()` 或 `rcu_replace_pointer()` 发布？ |
| 取得 | 普通读者、更新锁持有者、SRCU 读者是否使用了与其域匹配的取得宏？ |
| 生命周期 | 局部普通指针是否只在保护区内借用；逃逸前是否安全取得长期引用？ |
| 回收 | 对象是否先取消发布，再由相同保护域的 GP/回调后释放？ |
| 静态检查 | 是否针对修改目录运行可用的 Sparse `C=1` 或 `C=2` 检查？ |
| 动态检查 | 是否启用适用的 `CONFIG_PROVE_RCU`、KASAN、debug objects 和并发测试？ |

源码证据集中在 [`compiler_types.h`](../../../../research/source_reading/linux/include/linux/compiler_types.h) 与 [`rcupdate.h`](../../../../research/source_reading/linux/include/linux/rcupdate.h)。

上一篇：[RCU 驱动与子系统应用模式](P25_RCU_驱动与子系统应用模式.md)。

下一篇：[RCU 调试、验证与集成误用](P27_RCU_调试验证与集成误用.md)。
