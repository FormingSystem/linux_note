---
id: knowledge.linux.synchronization.lockdep.annotations_integration
title: "查询、断言、pin 与自定义原语接入"
kind: interface
status: evolving
domains:
  - linux
  - kernel
topics:
  - synchronization
  - locking
  - lockdep
---

# 第6章\_查询\_断言\_pin与自定义原语接入

## 6.1\_三类接口不能互相替代

前五章建立了可信的当前持锁账本和全局依赖图。业务代码接下来有三种不同需求：

| 需求 | 接口类型 | 是否改变 Lockdep 状态 |
| --- | --- | --- |
| 查询 current 是否持有指定锁 | `lockdep_is_held()` / `lockdep_is_held_type()` | 否，只读当前账本 |
| 声明函数必须在某种持锁条件下调用 | `lockdep_assert_*()` / pin | 断言本身不取得功能锁；pin 会给既有 held record 增加检查状态 |
| 让自定义同步域进入依赖分析 | `lock_acquire()` / `lock_release()` 或封装宏 | 是，必须与真实协议严格配对 |

查询不是断言，断言不是取得，事件上报更不是同步功能。

## 6.2\_lockdep\_is\_held查询的精确问题

```c
if (lockdep_is_held(&state_lock))
	do_locked_only_check();
```

它回答：

> 执行到当前代码位置时，current 的未释放持锁记录中是否存在 `state_lock.dep_map` 对应的实例？

它不回答：

- `state_lock` 是否被其他任务占用；
- 当前 CPU 上是否有任意任务持有它；
- 当前任务是否持有同一锁类的另一个实例；
- 共享数据是否没有竞争或对象是否仍存活。

因此 `mutex_is_locked(&state_lock)` 与 `lockdep_is_held(&state_lock)` 不可替换：前者关注功能锁是否处于被占用状态，后者关注 current 的影子持锁区间。

查询、注解与诊断的版本化入口见 [Lockdep 查询适配与诊断模块导读](../../../../research/source_reading/lockdep/navigation/P04_Linux_6.12_Lockdep查询适配与诊断模块导读.md#4.1_模块问题)。Linux 6.12.20 的实例匹配、读写类型和 UNKNOWN 返回见 [`lock_is_held_type()` 当前持锁查询](../../../../research/source_reading/lockdep/source_explanations/P08_Linux_6.12_Lockdep查询注解与配置源码实现.md#8.2_lock_is_held_type当前持锁查询)。

## 6.3\_为什么查询存在UNKNOWN

当 Lockdep 已编译但此刻不可用或已经停检时，查询若直接返回“未持有”，会让 `lockdep_assert_held()` 产生假阳性告警。实现因此可以返回三态：

```text
HELD       当前账本明确找到
NOT_HELD   检查器可用且明确未找到
UNKNOWN    检查器无法可靠回答
```

持锁断言通常只在结果明确为 `NOT_HELD` 时报警；不持锁断言则只在明确为 `HELD` 时报警。UNKNOWN 保护的是诊断可信度，不表示业务前置条件自动成立。

`CONFIG_LOCKDEP=n` 时，大多数断言和事件宏为空操作或由编译器消除。业务代码不能把查询结果用于改变功能正确性，例如不能写成“只有 Lockdep 认为持锁才真正修改数据”。

## 6.4\_把注释升级为可执行断言

与其只写：

```c
/* 调用者必须持有 state->lock。 */
static void update_payload(struct device_state *state)
{
	state->value++;
}
```

更可验证的写法是：

```c
static void update_payload(struct device_state *state)
{
	lockdep_assert_held(&state->lock);
	state->value++;
}
```

断言把接口契约放在被保护操作附近。测试实际覆盖到错误调用路径时，它能打印当前持锁状态和调用栈；关闭检查时，调用者义务仍然存在。

接口速览：

| 接口或检查点 | 本场景作用 | 调用上下文 | 省略或误用后果 | 版本化源码讲解 |
| --- | --- | --- | --- | --- |
| `lockdep_assert_held()` | 声明 current 必须持指定锁 | 被调函数入口或关键状态修改前 | 错误调用只剩注释，动态测试无法直接定位 | [`lockdep_assert_held` 断言展开](../../../../research/source_reading/lockdep/source_explanations/P08_Linux_6.12_Lockdep查询注解与配置源码实现.md#8.3_lockdep_assert_held断言展开) |
| `lockdep_assert_not_held()` | 声明当前路径不得持该锁 | 可能递归回调或外部调用前 | 容易隐藏反向调用和重入 | [`lockdep_assert_held` 断言展开](../../../../research/source_reading/lockdep/source_explanations/P08_Linux_6.12_Lockdep查询注解与配置源码实现.md#8.3_lockdep_assert_held断言展开) |
| `lockdep_assert_held_read/write()` | 区分共享读与独占写要求 | 读写锁保护的数据路径 | 把只读保护误当成可写保护 | [`lockdep_assert_held` 断言展开](../../../../research/source_reading/lockdep/source_explanations/P08_Linux_6.12_Lockdep查询注解与配置源码实现.md#8.3_lockdep_assert_held断言展开) |

## 6.5\_pin检查的是锁没有被中途偷换

有时上层不仅要求“此刻持锁”，还要求一个回调期间没有人把锁释放再重新取得。仅在回调前后各做一次 `lockdep_assert_held()` 无法区分：

```text
进入回调前持锁
  → 下层偷偷unlock
  → 运行一段无保护代码
  → 下层重新lock
  → 返回时再次看起来持锁
```

`lockdep_pin_lock()` 在现有 held record 上增加 pin 状态，`lockdep_unpin_lock()` 用 cookie 核对。被 pin 期间 release 会报警。pin 仍不让锁变得“不可解锁”，也不提供额外硬件互斥；它只检查约定。

具体 `pin_count` 和 cookie 更新见 [`lockdep_pin_lock()` 锁保持注解](../../../../research/source_reading/lockdep/source_explanations/P08_Linux_6.12_Lockdep查询注解与配置源码实现.md#8.4_lockdep_pin_lock锁保持注解)。

## 6.6\_might\_lock表达潜在取得关系

某个函数并非每次都取得内部锁，但调用者若已经持有与之反向的锁，仍可能在特定分支死锁。`might_lock()` 一类注解通过一次配对的模拟 acquire/release 让 Lockdep 检查潜在顺序，同时不建立真实临界区。

它适合表达“这个接口可能取得 L”的调用契约，不适合：

- 代替真正的 `mutex_lock()`；
- 给所有函数无差别添加，制造不存在的依赖；
- 隐藏本应由被调函数真实锁路径上报的事件。

## 6.7\_自定义同步原语怎样接入

一个自定义原语至少需要：

```c
struct my_gate {
	/* 真正决定进入／退出的功能状态。 */
	atomic_t state;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/* 只用于调试验证的检查身份。 */
	struct lockdep_map dep_map;
#endif
};
```

完整接入顺序是：

```text
初始化功能状态
  + 初始化dep_map与持久key

真实进入路径
  + 按真实阻塞/读写/nested语义上报acquire
  + 失败取得必须撤销或不能提交持锁记录

真实退出路径
  + 上报release
  + 再按原语要求释放功能状态

销毁
  + 先确保无人持有、无人等待
  + 再处理动态key生命周期
```

最重要的核对项不是“是否调用了 `lock_acquire()`”，而是上报模型是否忠实：

- 会等待的路径不能标成 trylock；
- 递归读不能标成普通读，反之亦然；
- 失败路径不能遗留 held record；
- 真实允许同类嵌套时需要可证明的层级，而不是随意 subclass；
- `dep_map` 和 key 的生命期不能短于检查器可能引用它们的时间。

标准原语已经正确接入时，业务模块不应在外面重复上报一遍，否则 current 账本会出现伪造的双重取得。

## 6.8\_本章结论

`lockdep_is_held()` 只读 current 的实例持锁记录；`lockdep_assert_*()` 把前置条件变成动态断言；pin 检查持锁连续性；acquire/release 注解才会改变影子状态。所有这些接口都只验证声明，不提供真正同步。下一章将用 RCU 展示一个子系统怎样建立虚拟 lockdep map，并把自己的保护条件接到通用查询和告警路径。

上一篇：[递归、依赖环、IRQ 与读写规则](P05_递归_依赖环_IRQ与读写规则.md)。

下一篇：[RCU 与子系统检查适配](P07_RCU与子系统检查适配.md)。
