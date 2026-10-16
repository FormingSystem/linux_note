---
id: knowledge.linux.asynchrony.workqueue.cmwq_object_ownership
title: "cmwq 对象层次与状态所有权"
kind: subsystem
status: evolving
domains: [linux, kernel]
topics: [asynchrony, workqueue, cmwq]
---

# 第3章\_cmwq对象层次与状态所有权

## 3.1\_为什么workqueue不是一组私有线程

如果每个子系统的队列都固定创建线程，空闲时浪费资源，阻塞时又可能不够用。cmwq 把“用户可见的属性与 flush 域”和“实际 worker 资源”解耦：多个 workqueue 可以映射到共享 pool，同时由中间的 pool_workqueue 维护各自并发和在途账本。

## 3.2\_五层对象

沿用采样设备：work_struct 是那一个排队代表，workqueue_struct 保存设备选择的属性和 flush 范围。实际线程可以和别的队列共用 worker_pool；那么同一个 pool 中属于不同 workqueue 的任务，就不能共用一份活动额度和完成账本。pool_workqueue（缩写 pwq）因此成为“一条逻辑队列与一个执行池的连接”，保存这份连接的计数与暂缓项。

最后还需要 worker 记录执行者身份：task 指向真实内核线程，current_work 指向当前回调对象。work 是任务描述，worker 是执行角色，workqueue 是管理范围；多个描述、多个范围和共享执行者之间的关系如下。

```mermaid
flowchart TD
    W["work_struct<br/>可重复投递的工作对象"] -->|排队时归属| PWQ["pool_workqueue<br/>wq与pool的连接和并发账本"]
    Q["workqueue_struct<br/>属性、flush域、pwq集合"] -->|管理连接集合| PWQ
    PWQ -->|提交可执行项| P["worker_pool<br/>worklist、idle与worker管理"]
    P -->|分配工作| K1["worker/kworker 1"]
    P -->|分配工作| K2["worker/kworker 2"]
    A["执行位置与调度属性"] -->|配置管理范围| Q
    A -->|选择可共享资源| P
```

`workqueue_struct` 不拥有一一对应的 kworker；`worker_pool` 也不只服务一个 workqueue。`pool_workqueue` 是两者之间的连接对象，保存 active/inactive 和 flush 相关状态。

## 3.3\_状态所有权表

| 对象 | 关键状态 | 谁主要修改 | 用于回答 |
| --- | --- | --- | --- |
| `work_struct` | data 中 pending/归属编码、entry、func | 提交、pool 和执行路径 | 这一个 work 当前在哪里 |
| `workqueue_struct` | flags、pwqs、flush 颜色、名称、属性 | 分配、属性更新、flush/destroy | 用户定义的执行域是什么 |
| `pool_workqueue` | pool/wq 指针、active 计数、inactive list、in-flight 颜色 | queue、激活、完成、flush | 该 wq 在该 pool 上能否继续激活 |
| `worker_pool` | lock、worklist、worker/idle 集合、运行计数 | queue 与 worker manager | 实际执行能力是否充足 |
| `worker` | task、current_work/current_pwq、scheduled list | worker loop | 当前 kworker 正在代表谁执行 |

`work_struct.data` 会在 off-queue 标志与指向 pwq 的编码之间切换，属于高度版本化实现。知识正文只保留“work 必须携带 pending 与执行归属”这一稳定模型，具体位定义进入源码实现文档。

## 3.4\_bound与unbound映射

如果采样数据与某 CPU 的本地状态密切相关，保持提交 CPU 的关联有利于局部性，形成 bound（绑定）执行域；若提交者频繁跨 CPU 移动，或计算任务需要在允许集合中调度，就可能选择 unbound（非固定绑定）执行域。改变的是执行位置策略，不是“是否允许睡眠”。

CPU mask 是允许执行的处理器集合；affinity scope 是按缓存或 NUMA 等拓扑划分亲和范围的选择，pod 则是一组具有该拓扑关系的 CPU。NUMA（Non-Uniform Memory Access，非均匀内存访问）表示处理器访问不同节点内存的成本可能不同。先认识这些属性的职责即可，第七章再比较严格亲和与调度灵活性的代价。

bound workqueue 通常按提交 CPU 选择 per-CPU pwq/pool，工作在该 CPU 关联的执行域推进。unbound workqueue 根据 attributes、CPU mask 和 affinity pod 选择共享 unbound pool；调度器仍可能决定具体运行 CPU。

```mermaid
flowchart LR
    B["bound wq"] --> C0["CPU0 pwq"]
    B --> C1["CPU1 pwq"]
    C0 --> P0["CPU0 worker pool"]
    C1 --> P1["CPU1 worker pool"]
    U["unbound wq"] --> UP["按attrs/pod选择的pwq"]
    UP --> SH["共享unbound worker pool"]
```

`WQ_UNBOUND` 不表示“没有 CPU 约束”或“每个 work 新建线程”，而是取消提交 CPU 的固定 per-CPU 绑定，改由属性与调度策略选择执行域。

## 3.5\_锁与读取规则

`worker_pool.lock` 保护 pool worklist、worker 状态和许多与 work 归属有关的字段；wq/pwq 的配置与指针发布另有锁和生命周期规则。读复制更新（Read-Copy Update，RCU）在这里支持连接指针的并发读取与延后回收，不意味着读到指针后可以随意修改对象；字段注释中的锁域标记必须结合该版本的注释图例阅读。

用一个反例检验所有权：设备 A 和设备 B 共用 pool，A 的活动额度耗尽，只应把 A 的新 work 留在它自己的 pwq 暂缓链，不能把整个 pool 停掉。否则共享线程资源就意外变成共享业务限流。反过来，创建独立 workqueue 也不能证明 A 已得到独占 CPU 或独占线程。

## 3.6\_源码入口

对象地图和建议阅读顺序见[工作队列对象与投递模块源码概念导读](../../../../../research/source_reading/workqueue/navigation/P02_Linux_6.12_工作队列对象与投递模块源码概念导读.md#2.2_五层对象与状态所有权)。字段裁剪只在[工作队列对象布局源码实现](../../../../../research/source_reading/workqueue/source_explanations/P01_Linux_6.12_工作队列对象布局源码实现.md#1.2_源码符号覆盖账本)展开。

## 3.7\_本章结论与下一问

cmwq 用 pwq 把逻辑 workqueue 与物理 worker pool 解耦，因此并发限制、flush 域和执行资源可以分别管理。下一章沿一个 work 追踪 pending 设置、pool 选择、active/inactive 分流和重新提交。

上一篇：[异步执行的抽象状态机](P02_异步执行的抽象状态机.md)。

下一篇：[work 投递、激活与 pending 合并](P04_work投递激活与pending合并.md)。
