---
id: knowledge.linux.asynchrony.workqueue.worker_concurrency_loop
title: "worker 并发管理与执行循环"
kind: subsystem
status: evolving
domains: [linux, kernel]
topics: [asynchrony, workqueue, worker_pool]
---

# 第5章\_worker并发管理与执行循环

## 5.1\_固定数量worker为什么不够

若 pool 只有一个 worker，它在 work function 中睡眠时，后续工作全部停顿；若预先创建很多 worker，大多数时间又浪费线程和内存。cmwq 让调度器钩子和 pool 状态感知 worker 是否正在运行/睡眠，按需唤醒或创建执行者。

沿用采样设备，A 的后处理正在等待一个可睡眠操作，池中另有独立的 B 已获得活动额度。A 的睡眠不应让 B 也无事可做。下面先看普通 bound 线程池的运行数调节：nr_running 记录参与这套调节的可运行 worker，不等于该设备的 active 工作数，也不等于所有线程数量；unbound、救援线程与 CPU-intensive 执行有不同记账条件。

## 5.2\_pool中的角色与状态

```mermaid
flowchart LR
    L["pool.worklist"] --> K["当前worker"]
    K -->|"设定执行状态与标志"| N["nr_running等并发状态"]
    K -->|"可能睡眠"| S["调度器worker钩子"]
    S -->|"sleeping/running钩子更新运行数"| P["worker_pool"]
    P -->|"唤醒idle worker"| I["idle_list"]
    P -->|"没有可用worker"| M["manager"]
    M -->|"创建新worker"| P
```

worker pool 维护 worklist、worker 集合、idle 集合和管理者状态；每个 worker 记录 `current_work/current_pwq`，让 flush、冲突检测和诊断知道哪个任务正在代表哪个 work 执行。

普通 worker 进入调度睡眠时，wq_worker_sleeping 钩子在检查排除标志后记录 sleeping，并在 pool 锁下减少 nr_running、尝试 kick_pool；恢复运行时，wq_worker_running 负责相应恢复。信息经调度路径主动更新，不是另一个线程反复轮询回调是否卡住。pool 中有可执行工作而普通运行数不足时，唤醒空闲 worker；创建与管理的代价留给必要时的管理路径。

## 5.3\_worker\_thread主循环

1. worker 被调度运行并进入 pool 锁保护区；
2. 若没有足够可运行 worker，必要时进入 `manage_workers()`；
3. 从 pool worklist 取工作，移入 worker 的 scheduled/当前状态；
4. `process_one_work()` 建立执行归属、锁依赖和 trace 状态；
5. 释放 pool 锁，调用 `work->func(work)`；
6. 重新取得 pool 锁，完成 in-flight/active 账本并激活后续 work；
7. 无工作时转为 idle 并睡眠。

工作函数在不持有 pool 内部锁时执行，否则任意可睡回调都会阻塞整个 pool 管理。

## 5.4\_端到端执行时序

```mermaid
sequenceDiagram
    autonumber
    participant Q as pool.worklist
    participant K as kworker
    participant P as pool与worker状态
    participant F as work function
    participant S as 调度器

    Q-->>K: worker取得work
    K->>P: 记录current_work/current_pwq
    K->>F: 释放pool锁后调用func
    F->>S: 可能主动睡眠
    S->>P: worker睡眠导致可运行数下降
    P->>P: 判断是否需要更多worker
    P-->>S: 唤醒idle或由manager创建
    S-->>F: 原worker恢复执行
    F-->>K: 业务回调处理完成并返回
    K->>P: 清执行归属、减少in-flight
    P->>Q: 必要时激活inactive work
```

## 5.5\_manager与mayday不是普通执行路径

manager 只在 pool 需要创建/管理 worker 时介入；worker 创建可能涉及内存分配和 kthread 建立。若内存回收依赖本 workqueue 前进，而新 worker 又因内存压力创建失败，就出现循环依赖。WQ_MEM_RECLAIM 的 rescuer/mayday 路径专门解决这一异常慢路径，不能把它描述成所有 work 的常规调度者。

manager 是由 worker 临时承担的管理角色，同一个 pool 同时至多一个管理者，并非每个 workqueue 旁固定驻留的专用调度线程。能直接唤醒空闲 worker 时不必重新分配线程；需要创建时才支付分配、初始化和调度成本。运行数调节帮助保持前进，也不承诺回调的开始时刻或最坏延迟。

## 5.6\_WQ\_CPU\_INTENSIVE的真实含义

如果 A 不是睡眠而是持续计算，普通运行数可能一直表明“已有执行者”，池中 B 因此不能像睡眠场景那样获得补偿。WQ_CPU_INTENSIVE 让 bound 队列运行该回调时不计入普通运行数调节，从而不靠 A 结束才允许其他普通工作启动。

它不豁免 workqueue 的 active 额度，开始执行前也仍受正常管理约束；对 unbound 队列，这个标志没有相同的作用。它不会自动创建专用 CPU，也不提高调度优先级；多个计算工作仍会竞争 CPU。若要求严格隔离，必须另行设计执行位置、调度与依赖，不能仅添加一个标志。

## 5.7\_工作函数的阻塞边界

work function 处于进程上下文通常可睡，但若它运行在 ordered/低 max_active 队列、持有其他路径 flush 所需锁，或者处于内存回收依赖链，阻塞会放大为整个队列停顿或死锁。审计时要画出“谁等待 work 完成、work 又等待谁”的依赖环，而不只检查函数是否允许 `schedule()`。

## 5.8\_源码入口

worker/pool 状态和执行循环见[worker 管理模块源码概念导读](../../../../../research/source_reading/workqueue/navigation/P03_Linux_6.12_worker管理模块源码概念导读.md#3.3_worker_thread与process_one_work)。函数体只在[worker、flush 与取消源码实现](../../../../../research/source_reading/workqueue/source_explanations/P03_Linux_6.12_worker_flush与取消源码实现.md#3.3_worker_thread与process_one_work执行边界)展开。

## 5.9\_本章结论与下一问

cmwq 通过 worker 睡眠感知、idle 唤醒和 manager 创建维持执行能力；工作函数能睡眠并不意味着任意依赖都安全。下一章研究 flush/cancel 如何在 work 仍可重排、多个 pwq 并行的条件下证明目标代际完成。

上一篇：[work 投递、激活与 pending 合并](P04_work投递激活与pending合并.md)。

下一篇：[flush、cancel 与颜色代际](P06_flush_cancel与颜色代际.md)。
