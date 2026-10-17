---
id: research.source_reading.rcu.linux_6_12_lockdep_adapter_navigation
title: "Linux 6.12 RCU Lockdep适配模块源码概念导读"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
topics:
  - synchronization
  - rcu
  - lockdep
  - debugging
source_project: linux
source_version: "6.12.20"
---

# 第12章\_Linux\_6.12\_RCU\_Lockdep适配模块源码概念导读

## 12.1\_模块问题与实现所有权

本模块回答一个交叉问题：RCU 没有实体 mutex，却为什么会出现 `rcu_lock_map`、`rcu_bh_lock_map`、`rcu_sched_lock_map` 和 `rcu_callback_map`，这些对象怎样把 RCU 调用条件交给 Lockdep 检查？

例如一段代码调用 `rcu_dereference()`，却忘了进入读侧。GP 机制仍按真正的读侧状态工作，不会因为访问器名字正确就替它保住对象。检查器要报告这种误用，先得知道当前任务声明了哪一种逻辑范围。四个 map 提供范围身份，进入/退出注解把身份写入检查器账本，访问器再查询该账本；因此需要这些对象，但不需要为每次读取再取得一把互斥锁。

本章核对 [源码基线](../../linux/SOURCE_BASELINE.md) 的 NXP Linux 6.12.20、提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。配置矩阵按固定源码展开，本次没有重新验证目标内核是否编译和启用这些检查。

实现所有权分成两层：

| 层次 | 负责回答 | 权威入口 |
| --- | --- | --- |
| Lockdep 通用框架 | map 怎样映射到 lock class，acquire/release 怎样维护 `current->held_locks[]`，查询怎样匹配实例 | [Linux 6.12 Lockdep 源码导读](../../lockdep/navigation/P01_Linux_6.12_Lockdep源码导读.md#1.1_基线与阅读目标) |
| RCU 适配层 | 四个 map 为什么分开、参数怎样选择、在哪些 RCU 路径登记、由哪些 RCU 条件消费 | [RCU Lockdep适配层源码实现](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.1_实现所有权与读者目标) |

前缀 `lockdep` 表明使用的框架，不等于全部内容都归 Lockdep 专题。通用算法只在 Lockdep 展开一次，RCU 实例和接入时序只在 RCU 展开一次。

## 12.2\_先区分功能状态与检查影子状态

普通、BH 和 sched 三种读侧入口都要区分两条因果链；下面按入口列出其功能动作，不是说同一次普通 `rcu_read_lock()` 会依次执行全部动作：

```text
功能路径
    → 普通入口：__rcu_read_lock()
    → sched入口：preempt_disable()；BH入口：local_bh_disable()
    → 建立当前RCU实现真正依赖的读侧约束

检查路径
    → rcu_lock_acquire(&对应map)
    → Lockdep在current账本登记逻辑范围
    → 后续held查询、断言或RCU_LOCKDEP_WARN消费
```

功能路径决定 RCU 正确性；检查路径只验证已经执行到的调用协议。map 不是锁字，不会排斥其他 CPU，也不参与 GP、QS、回调成熟或对象释放。

公共 inline 中的 `__acquire(RCU)`、`__release(RCU)` 还属于 Sparse 的静态 context 注解，不是运行时 acquire/release 的另一次调用。静态分析账本、Lockdep 当前影子账本和 RCU 功能状态是三个不同层次。

callback 形成第三个阶段：它既不是 reader 进入，也不是 GP 协调。GP 已完成并且 callback 真正开始执行时，`rcu_callback_map` 才把“当前正处于 RCU 延迟动作范围”登记给 Lockdep。

## 12.3\_参与者\_状态地址与通信方向

| 参与者 | 状态位置 | 写入或登记动作 | 读取或消费动作 |
| --- | --- | --- | --- |
| RCU 公共 inline API | `include/linux/rcupdate.h` | 在功能动作内侧调用 acquire/release | 不保存共享功能状态 |
| 四个全局 map | `kernel/rcu/update.c` | 静态 key、名称和 wait type 建立稳定身份 | 地址传给 Lockdep 和查询调用点 |
| 当前任务 | `current->held_locks[]` 与 `lockdep_depth` | Lockdep acquire 登记、release 撤销对应层次 | `lock_is_held()` 等查询当前上下文中的匹配记录 |
| RCU held 查询 | `kernel/rcu/update.c` | 不写状态 | 组合 map、抢占/BH、watching 和 online 条件 |
| RCU 告警与断言 | `rcupdate.h`、`tree.c` 等 | 命中时写告警一次性状态 | 消费 held 谓词或业务锁条件 |
| callback 执行者 | `rcu_do_batch()`、批量释放路径 | callback 前 acquire、返回后 release | callback 内部受保护条件查询 |
| 框架历史与身份 | Lockdep 全局锁类及历史结构 | 框架按事件类别登记或检查 | 后续框架检查；release 不撤销锁类身份与既有历史 |
| 检查器有效性 | `debug_locks`、`current->lockdep_recursion`、`rcu_scheduler_active` | 框架启停、递归与启动路径 | `debug_lockdep_rcu_enabled()` 决定本次诊断是否可依赖 |

map 的全局地址提供身份，真正随进入/退出变化的当前影子状态位于任务的 held stack。多个任务可以同时记录指向同一个 RCU map 的 held record，不发生互斥。

RCU 的 acquire 包装传入 `read=2` 和 `check=0`，按递归读范围登记，不把它当作普通阻塞锁取得事件来推导所有依赖边。全局历史、当前 held 记录和检查器是否还有效不能合成一个“已上锁”标志；退出本次范围也不表示把全局检查历史清空。

## 12.4\_三条调用链怎样闭环

适配层并不只在 `rcu_read_lock()` 处记一个标志。它要完成读侧 acquire/release 登记、访问器或断言查询告警，以及同步等待入口的自等待检查；三条链共享 lockdep 基础设施，但各自观察不同事件。

### 12.4.1\_读侧登记链

```mermaid
flowchart LR
    A["RCU读侧入口"] -->|"V1/V6建立和撤销"| B["RCU后端功能状态<br/>嵌套或执行约束"]
    A -->|"V2 acquire / V5 release"| C["Lockdep事件包装<br/>四个全局map选择身份"]
    C -->|"登记或撤销当前层次"| D["current held_locks<br/>及lockdep_depth"]
    E["访问器或断言"] -->|"V3 held查询"| D
    F["debug_locks / 递归 / 启动状态"] -->|"决定诊断有效性"| E
```

普通、BH 和 sched 三种 API 使用不同 map，因为精确断言必须区分调用者经过了哪一种入口。嵌套范围需要 acquire/release 逐层配对，不能压成一个布尔位。

正常运行时沿用 [正文 V1～V6](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P23_RCU_类型语义_Sparse与Lockdep.md#23.6.6_用一次读取周期对齐功能状态与检查状态)：先建立功能保护，再登记影子，随后取指针并诊断、在保护内使用数据，最后先撤销影子再退出功能保护。V0 是构建时静态检查，不在运行时序内。这样 release 诊断发生时，功能约束仍然存在。

具体参数和源代码顺序见 [`rcu_lock_acquire()` 与 `rcu_lock_release()`](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.4.1_rcu_lock_acquire和rcu_lock_release包装参数)。

### 12.4.2\_查询与告警链

```text
以rcu_dereference_check为例
    → 先READ_ONCE加载共享指针
    → 诊断有效时检查调用者条件或held谓词
    → 条件不满足且告警点尚未报告，则输出诊断
    → 返回加载结果；诊断不自动阻止后续使用
```

`rcu_read_lock_held()`、`rcu_read_lock_bh_held()` 和 `rcu_read_lock_sched_held()` 不使用完全相同的最终条件。检查框架可依赖时，公共判断先排除 RCU 不 watching 或 RCU 视角离线的 CPU，然后才进入各自的最终条件：

| 查询 | 本版本最终判断 | 不能据此声称 |
| --- | --- | --- |
| `rcu_read_lock_held()` | `rcu_lock_map` 的 held 记录 | 对象一定已被正确发布、裸指针可以逃逸 |
| `rcu_read_lock_bh_held()` | `in_softirq()` 或 IRQ 禁用 | 一定显式调用过 `rcu_read_lock_bh()`；精确入口断言另查 map |
| `rcu_read_lock_sched_held()` | sched map 已登记或 `!preemptible()` | 所有保护都来自 sched API 名称 |

若 `debug_lockdep_rcu_enabled()` 本身为假，上述公共路径会保守返回真以避免误报，不能拿这个值授权实际访问。`RCU_LOCKDEP_WARN()` 在条件求值前后两次检查有效性，并使用每个宏展开点的 `__warned` 限制重复报告；没有再次告警可能是曾经报告过、路径没执行、递归抑制或检查器已停用。

```mermaid
sequenceDiagram
    autonumber
    participant T as 当前任务
    participant R as RCU功能后端
    participant L as Lockdep当前账本
    participant P as 共享入口
    participant D as RCU诊断
    T->>R: V1 __rcu_read_lock
    T->>L: V2 rcu_lock_acquire普通map
    T->>P: V3 访问器先READ_ONCE取指针
    T->>D: 检查有效性，再计算保护条件
    D->>L: held查询或调用者业务锁谓词
    alt 有效且条件不满足，尚未报告
        D-->>T: 输出诊断，仍不替代功能保护
    else 条件成立或检查不可依赖
        D-->>T: 不报告，不能据此扩展保护范围
    end
    T->>P: V4 合法路径在保护内读取对象数据
    T->>L: V5 rcu_lock_release
    T->>R: V6 __rcu_read_unlock
```

第 3 步先于第 4～6 步，说明诊断不是读指针前的许可闸门。图中的 V4 只描述协议正确的路径：若已经发现缺少保护，应修正调用者，不能因为宏仍返回指针就继续把结果当作安全对象。

具体分支见 [held查询怎样消费三种读侧map](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.6_held查询怎样消费三种读侧map)，公共告警宏见 [`RCU_LOCKDEP_WARN()` 检查适配层](../source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.6_RCU_LOCKDEP_WARN检查适配层)。

### 12.4.3\_callback上下文链

```mermaid
sequenceDiagram
    autonumber

    participant G as "GP与回调列表"
    participant X as "rcu_do_batch或批量释放"
    participant L as "Lockdep current账本"
    participant C as "callback内代码"

    G-->>X: "callback具备资格并被取出"
    X->>L: "acquire rcu_callback_map"
    X->>C: "执行callback或延迟释放"
    C->>L: "按需查询callback身份"
    C-->>X: "返回"
    X->>L: "release rcu_callback_map"
```

callback map 只证明当前路径经过了 RCU 的延迟动作执行包装。对象为什么可以访问仍由该子系统的入口封闭、GP 和对象状态协议证明。

图中第 2 步发生在取得执行资格之后，第 6 步撤销执行包装，二者均不推动 GP。这里说的是已经核对的 Tree `rcu_do_batch()`、批量释放和 Tiny `rcu_reclaim_tiny()` 等接入点；不能仅因其他家族也叫 callback，就推定其每条执行路径都登记相同 map。

具体源码、Maple Tree 消费点和修改风险见 [`rcu_callback_map` 怎样标记延迟动作上下文](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.7_rcu_callback_map怎样标记延迟动作上下文)。

## 12.5\_配置怎样改变检查能力

```text
PROVE_LOCKING
    ├─select→ LOCKDEP
    ├─select→ DEBUG_LOCK_ALLOC
    └─PROVE_RCU的def_bool取此值
```

不能把这些配置视为一个开关：

- `DEBUG_LOCK_ALLOC=y` 时四个 map 和 acquire/release tracking 可以存在；
- `PROVE_RCU=y` 时 `RCU_LOCKDEP_WARN()` 才执行相应 RCU 动态检查；
- `DEBUG_LOCK_ALLOC=n` 时 map 定义消失，事件 wrapper 为空操作，held 查询使用保守 inline；
- 关闭检查不改变 RCU 功能契约，也不能用“查询返回 1”证明真实读侧存在。

更具体地说，`DEBUG_LOCK_ALLOC=n` 时普通和 BH held inline 返回 1，sched/any 查询仍取 `!preemptible()`；并不是所有查询都统一返回真。`PROVE_RCU=n` 的告警宏不在运行时求值其条件。相关 `extern` 声明留在头文件，也不代表关闭配置后仍有四个运行时实例；包装不使用这些对象，定义可以不进入最终程序。

完整配置矩阵见 [配置关闭时对象和查询怎样退化](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.8_配置关闭时对象和查询怎样退化)。

## 12.6\_建议阅读顺序与修改目标

1. 先读本章 12.1～12.3，分清实现所有权、功能状态、检查影子与有效性状态；
2. 进入[声明、定义、key与静态生命期](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.3_声明定义key与静态生命期)，理解四个全局身份为什么分开；
3. 阅读[进入与退出怎样写入检查器影子状态](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.4_进入与退出怎样写入检查器影子状态)，逐项核对 wrapper 参数和事件顺序；
4. 阅读[四个map怎样落到Lockdep当前账本](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.5_四个map怎样落到Lockdep当前账本)，再跳转 Lockdep 通用 held-stack 实现；
5. 分别追踪[读侧查询](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.6_held查询怎样消费三种读侧map)与[callback 上下文](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.7_rcu_callback_map怎样标记延迟动作上下文)；
6. 最后使用[修改影响矩阵](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.9_修改RCU适配层时必须保持什么)，判断新增域、改 wait type、改事件顺序或改配置分支会影响哪些调用方和验证。

读完后不应只会说“RCU 接入 Lockdep”，而应能从一个 map 的 `extern` 追到定义、key、事件登记、current 影子记录、查询消费者和关闭配置，并能预测局部修改的影响范围。

用两个反例检查这种能力。若把 release 移到 `__rcu_read_unlock()` 之后，检查器记录的范围便超出真实功能保护，退出诊断还可能遇到已经改变的上下文。若看到普通 held 查询返回 1 就在读侧之外长期保存对象指针，则把保守调试返回值当成了生命周期许可。两种改法都不能靠“没有告警”验收；需要同时核对功能边界、事件顺序、实际配置、检查器有效性和执行覆盖。

## 12.7\_边界与后续阅读

本章没有展开：

- Lockdep lock class 注册、依赖图、链缓存和 held stack 的通用函数体；这些内容回到 Lockdep 源码专题；
- RCU GP、QS 和 callback 资格算法；这些内容回到 Tree RCU 模块导读和关键函数实现；
- Sparse `__rcu` address space；它属于编译期类型检查，不是运行时 map，`__CHECKER__` 与 `rcu_check_sparse()` 的唯一实现讲解见[静态类型桥接实现](../source_explanations/P01_Linux_6.12_RCU_公共接口与检查机制源码详解.md#1.3.3_rcu_check_sparse静态类型桥接)；
- Maple Tree dead node 的完整生命周期；这里只展示它怎样消费 callback 身份。

总阅读索引：[Linux 6.12 RCU 源码总阅读索引](P01_Linux_6.12_RCU源码总阅读索引.md#1.6_建议的源码阅读顺序)。

具体实现：[Linux 6.12 RCU Lockdep适配层源码实现](../source_explanations/P04_Linux_6.12_RCU_Lockdep适配层源码实现.md#4.1_实现所有权与读者目标)。

稳定知识：[RCU 类型语义、Sparse 与 Lockdep](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/P23_RCU_类型语义_Sparse与Lockdep.md#23.6_Lockdep检查的是哪一个运行时条件)。
