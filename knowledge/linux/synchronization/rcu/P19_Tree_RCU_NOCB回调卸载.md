---
id: knowledge.linux.synchronization.rcu.tree_nocb
title: "Tree RCU NOCB 回调卸载"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
topics:
  - rcu
  - nocb
  - no_hz_full
---

# 第19章\_Tree\_RCU\_NOCB回调卸载

## 19.1\_场景\_隔离CPU不希望执行回调批次

CPU3 被用于低延迟数据面，应用希望尽量减少 scheduler tick、softirq 和内核回调在该 CPU 上产生的尾延迟。数据面仍会删除对象：

```c
old = rcu_replace_pointer(flow_slot, new, true);
call_rcu(&old->rcu, flow_free_rcu);
```

普通非 offload 路径最终可能在 CPU3 的 `rcu_core()` 中调用 `rcu_do_batch()`。即使 callback 很短，一次大批量也会扰动 CPU3。

NOCB 把指定 CPU 的 **callback GP 管理与执行** 交给 kthread；它没有取消 CPU3 对普通 RCU reader/QS 的责任，也没有让 `call_rcu()` 变成零开销远端发送。

## 19.2\_卸载前后责任对比

| 责任 | 普通CPU | NOCB CPU |
| --- | --- | --- |
| reader执行约束/任务跟踪 | 本CPU/当前任务 | 不变，仍在本CPU/任务 |
| QS/EQS形成 | scheduler/context tracking | 不变 |
| CPU位上报 | 本CPU RCU路径或远端EQS扫描 | 不变 |
| callback入队 | 本CPU cblist | 本CPU的 offloaded cblist/bypass |
| callback等待目标GP | 本CPU `rcu_core`协助 | NOCB GP kthread |
| READY callback执行 | 本CPU softirq/rcuc | NOCB CB kthread |

NOCB 的名字是 no-callbacks-on-this-CPU 的工程目标，不是 no-RCU-accounting-on-this-CPU。

## 19.3\_两个线程与一个生产者路径

```mermaid
flowchart LR
    P["业务CPU call_rcu()<br/>生产callback"]
    B["rdp->nocb_bypass<br/>高频临时入口"]
    C["rdp->cblist<br/>权威分段状态"]
    G["rcuog NOCB GP kthread<br/>等待/加速目标GP"]
    X["rcuo NOCB CB kthread<br/>rcu_do_batch()"]
    GP["普通Tree RCU GP"]

    P -->|"低频直接或高频bypass"| B
    B -->|"flush合入"| C
    C -->|"最早所需gp_seq"| G
    G -->|"提出/等待GP"| GP
    GP -->|"完成代际"| G
    G -->|"推进DONE并唤醒"| X
    X -->|"提取并调用func"| C
```

实际线程可按组共享：`rcu_nocb_gp_kthread()` 可代表一组 offload CPU 管理 GP；每个 offload `rcu_data` 有对应 callback kthread/关联。线程布局取决于 grouping、CPU 数与配置，不能从一个线程名推导固定一对一关系。

## 19.4\_为什么需要bypass

若大量 CPU/中断同时向一个 offload CPU 的 `cblist` 入队，每次都取得 `nocb_lock`，生产者会在共享锁上排队。`nocb_bypass` 是一个由 `nocb_bypass_lock` 保护的普通 `rcu_cblist`，允许高频入队先聚集，再批量 flush 到权威 `rcu_segcblist`。

`call_rcu_nocb()` 大致在以下条件间选择：

```text
早期启动或低频、主锁容易取得
    → 直接进入cblist

bypass已经非空或单位时间入队过多
    → 进入nocb_bypass

bypass太旧、太满、出现非lazy callback或需推进
    → 取得两类锁并flush到cblist
```

旁路移除的是每次高频入队争用主锁的成本，换来额外链表、长度记账、flush时机和一致性协议。

## 19.5\_bypass不是第二个权威GP队列

callback 只有并入 `cblist` 后，才能由 `rcu_segcblist` 的 accelerate/advance 状态机绑定 GP 并进入 DONE。因此任何需要得出“当前所有 callback 到哪里了”的路径，必须先考虑 bypass：

- NOCB GP kthread 周期/条件 flush；
- `rcu_barrier_entrain()` 在尾随 callback 前 flush；
- CPU deoffload/hotplug 前 flush；
- bypass 老化或达到阈值时 flush；
- 新的非 lazy callback 需要避免被长 lazy 定时拖延时 flush。

若只读 `cblist` 长度而忽略 bypass，会漏掉已经由 `call_rcu()` 交付、但尚未进入分段队列的 callback。

## 19.6\_S0到S7\_一次offload\_callback生命周期

| 阶段 | 触发 | 主要状态 | 执行者 | 退出条件 |
| --- | --- | --- | --- | --- |
| S0 登记 | offload CPU `call_rcu()` | callback进入bypass或cblist，总长度增加 | 业务CPU | callback被接管 |
| S1 唤醒判断 | 新callback/时间/长度阈值 | `nocb_gp_wq`、deferred wake状态 | 生产者/本地core | GP线程会处理 |
| S2 flush | `nocb_gp_wait()` 等 | bypass合入cblist | NOCB GP kthread | 权威队列完整 |
| S3 加速 | 检查最早 pending callback | 分配 `gp_seq`、提出GP需求 | NOCB GP kthread | 知道等待哪轮 |
| S4 等GP | 普通Tree RCU推进 | callback仍WAIT/NEXT_READY | GP线程 | 目标代际完成 |
| S5 advance | GP完成可见 | callback进入DONE | NOCB GP/锁保护路径 | READY非空 |
| S6 唤醒CB | DONE出现 | callback kthread等待条件成立 | NOCB GP kthread | CB线程运行 |
| S7 执行 | `nocb_cb_wait()` → `rcu_do_batch()` | callback func被调用、长度下降 | NOCB CB kthread | 批次完成/限流 |

## 19.7\_端到端时序

```mermaid
sequenceDiagram
    autonumber

    participant P as 隔离CPU3生产者
    participant B as CPU3 nocb_bypass
    participant C as CPU3 cblist
    participant G as rcuog GP kthread
    participant R as 普通RCU GP
    participant X as rcuo CB kthread

    P->>B: call_rcu(head, func)<br/>高频入bypass
    P->>P: 立即返回继续数据面
    G->>B: 到flush条件<br/>取得bypass锁
    B->>C: 批量并入分段cblist
    G->>C: accelerate并读取目标gp_seq
    G->>R: 请求/等待普通GP=N
    R-->>G: GP=N完成
    G->>C: advance(N)<br/>callback进入DONE
    G->>X: 唤醒callback线程
    X->>C: rcu_do_batch()<br/>提取DONE
    X->>X: 调用func释放旧对象
    Note over P,X: CPU3仍负责reader与QS<br/>但不执行这个callback批次
```

## 19.8\_配置与观察

启动参数示例：

```text
rcu_nocbs=3 nohz_full=3 isolcpus=managed_irq,3
```

三者不是同义参数：`rcu_nocbs` 只指定 callback offload；`nohz_full` 针对 tick 隔离；`isolcpus`/cpuset/调度 affinity 处理任务和中断放置。只配置 NOCB 不能保证 CPU3 没有其他内核扰动。

```bash
cat /proc/cmdline
cat /sys/devices/system/cpu/nohz_full 2>/dev/null
ps -eLo pid,psr,cls,rtprio,comm | grep -E 'rcuo|rcuog|rcuop'
grep -E 'CONFIG_RCU_NOCB_CPU=' /boot/config-"$(uname -r)"
```

线程名和 affinity 随版本/配置变化，源码中的 `rcu_nocb_gp_kthread()`、`rcu_nocb_cb_kthread()` 才是职责锚点。

## 19.9\_动态offload/deoffload为何复杂

Linux 6.12.20 提供 `rcu_nocb_cpu_offload(cpu)` 和 `rcu_nocb_cpu_deoffload(cpu)`。切换至少要同步：

```text
cblist的offloaded标志
NOCB锁与本地core并发
bypass是否已flush
GP/CB kthread是否已创建、park或唤醒
已经DONE与仍pending callback由谁执行
barrier是否正在给队列entrain哨兵callback
```

它不是改一个 cpumask 后立即完成的无状态操作。用户通常应优先用启动参数建立稳定隔离配置，只有明确的运行时管理场景才动态切换。

## 19.10\_性能取舍

| 收益 | 代价 |
| --- | --- |
| 隔离CPU不执行 callback 批次 | `call_rcu()`仍要本地/共享记账 |
| callback线程可放到 housekeeping CPU | 增加线程、唤醒和跨CPU缓存访问 |
| bypass降低高频主锁争用 | 增加暂存、flush延迟和内存峰值 |
| GP/CB职责分离 | 诊断需要同时检查两类线程和普通GP |

若 housekeeping CPU 资源不足，offload callback 会积压；被隔离 CPU 很干净，不等于系统整体回收吞吐足够。

## 19.11\_源码和trace入口

- `kernel/rcu/tree_nocb.h::call_rcu_nocb()`：生产者入口。
- `rcu_nocb_bypass_*()` / `rcu_nocb_flush_bypass()`：旁路与合并。
- `nocb_gp_wait()` / `rcu_nocb_gp_kthread()`：GP管理。
- `nocb_cb_wait()` / `rcu_nocb_cb_kthread()`：callback执行。
- `rcu_nocb_cpu_offload()` / `rcu_nocb_cpu_deoffload()`：动态切换。

上一篇：[Tree RCU 回调执行、批处理与限流](P18_Tree_RCU_回调执行_批处理与限流.md)。

下一篇：[Tree RCU 同步等待与 rcu_barrier](P20_Tree_RCU_同步等待与rcu_barrier.md)。
