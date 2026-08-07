---
id: knowledge.linux.synchronization.rcu.nonpreempt_tree_source_mechanism
title: "非抢占式 Tree RCU 源码同步机制"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
topics:
  - synchronization
  - rcu
  - source_reading
source_project: linux
source_version: "6.12.20"
---

# 第6章\_非抢占式\_Tree\_RCU\_源码同步机制

第五章已经证明“非抢占 reader 不能跨任务切换 QS”，本章把每一步落实到 Linux 6.12.20 的地址、字段、写入者和调用链。研究对象仍是 `CONFIG_TREE_RCU=y && !CONFIG_PREEMPT_RCU`；已核对源码快照对应的 `.config` 虽然启用 PREEMPT_RCU，但相关非抢占分支存在于同一份源码中。

## 6.1\_源码边界与贯穿场景

版本证据来自 NXP 官方 [`linux-imx`](https://github.com/nxp-imx/linux-imx) 仓库发布标签 `lf-6.12.20-2.0.0`、提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0` 对应的源码快照，顶层 `Makefile` 为 Linux 6.12.20。本地工作树位置不进入仓库记录；统一身份和配置边界见 [Linux 源码阅读基线](../../../../research/source_reading/linux/SOURCE_BASELINE.md)。主要文件已经按上游相对路径保存在研究层：

| 文件 | 本章使用的证据 |
| --- | --- |
| [`include/linux/rcupdate.h`](../../../../research/source_reading/linux/include/linux/rcupdate.h) | 读侧封装、`rcu_assign_pointer()`、`rcu_dereference()` |
| [`kernel/rcu/tree.h`](../../../../research/source_reading/linux/kernel/rcu/tree.h) | `rcu_node`、`rcu_data`、`rcu_state` |
| [`kernel/rcu/tree.c`](../../../../research/source_reading/linux/kernel/rcu/tree.c) | 同步等待、GP、QS 检查、树形报告、EQS 扫描 |
| [`kernel/rcu/tree_plugin.h`](../../../../research/source_reading/linux/kernel/rcu/tree_plugin.h) | 非抢占与抢占配置分支 |
| [`kernel/rcu/update.c`](../../../../research/source_reading/linux/kernel/rcu/update.c) | `__wait_rcu_gp()` 与 `wakeme_after_rcu()` |

继续沿用以下应用代码：

```c
old_obj = rcu_replace_pointer(global_ptr, new_obj, true);
synchronize_rcu();
kfree(old_obj);
```

要解释的不是三个函数各自“有什么作用”，而是 `synchronize_rcu()` 怎样把一个栈上等待者连接到 GP，GP 怎样把 CPU 债务写进树，CPU 怎样形成并上报 QS，完成又怎样回到这个等待者。

## 6.2\_四层状态放在哪里

只列结构体和字段无法解释状态机。本节先固定一轮可以逐步推演的 GP，再把每一步写回具体地址。

### 6.2.1\_先固定四个逻辑CPU的现场

下面是第 5 章场景的源码层版本。它是为了展示 Tree RCU 分布式状态而构造的四逻辑 CPU 模型，不宣称 i.MX6ULL 单核硬件实际同时运行四个 CPU。假设该内核采用：

```text
CONFIG_NR_CPUS=4
CONFIG_TREE_RCU=y
CONFIG_PREEMPT_RCU=n
CONFIG_RCU_STRICT_GRACE_PERIOD=n
CONFIG_RCU_NOCB_CPU=n
```

并固定本例使用默认的 `rcu_normal_wake_from_gp=0`，即让 `synchronize_rcu()` 通过普通 callback 加 completion 等待，而不是混入 6.4.2 的直接唤醒优化。这样后文每一条箭头都只对应一条确定配置路径。

CPU1 的 reader 在更新前已经取得旧指针，并用测试门保持在最外层读区内：

```c
/* CPU1：非抢占式旧 reader。读区内不主动阻塞。 */
rcu_read_lock();
p = rcu_dereference(global_ptr);
complete(&cpu1_has_old_pointer);

while (!READ_ONCE(allow_cpu1_exit))
	cpu_relax();

use_obj(p);
rcu_read_unlock();
```

CPU0 上的写者确认 CPU1 已取得 `old_obj` 后替换入口并等待：

```c
/* CPU0：写者。 */
wait_for_completion(&cpu1_has_old_pointer);
old_obj = rcu_replace_pointer(global_ptr, new_obj, true);
synchronize_rcu();
kfree(old_obj);
```

这里的代码只保留决定状态归属的交错；对象分配、线程创建、CPU 绑定、退出清理和日志观察见[晚到读者与抢占读者的对象回收实验](../../../../labs/kernel/rcu/P01_晚到读者与抢占读者/README.md)。本节不会把省略的测试编排伪装成可直接加载的完整模块。

另外两个 CPU 的现场是：

- CPU2 正在执行普通内核代码，但没有进入任何旧 RCU 读区；
- CPU3 在 GP 开始时已经位于 idle/EQS；
- CPU0 的写者把同步请求登记后睡在 completion 上，CPU0 此后仍需在当前 GP 内提供一次有效 QS/EQS 证据，不能拿 GP 开始前发生的切换抵债。

为了把位图变化写成确定时间线，假设 4 个 CPU 都由同一个叶 `rcu_node` 覆盖，而且该节点同时是根节点。Linux 6.12 的 `tree.h` 明确允许小系统把层次折叠为单个 `rcu_node`。本例约定 bit0～bit3 分别代表 CPU0～CPU3，并固定后续证据到达顺序为 CPU0、CPU2、CPU3、CPU1：

```text
GP初始化：qsmask = 0b1111
CPU0报告：qsmask = 0b1110
CPU2报告：qsmask = 0b1010
CPU3的EQS被观察：qsmask = 0b0010
CPU1退出旧读区后报告：qsmask = 0b0000
```

这个顺序只是为了让读者逐位追踪地址；真实 CPU 可以并发上报，顺序不影响“最后一位清除后才能完成 GP”的条件。若系统有多层 `rcu_node`，叶节点归零后还要用自己的 `grpmask` 清父节点的一位，但每一位表达的债务语义不变。

### 6.2.2\_同一现场怎样穿过四层地址

```mermaid
flowchart TB
    W["第1层：CPU0写者任务栈<br/>struct rcu_synchronize<br/>completion=未完成"]
    G["第2层：全局rcu_state<br/>gp_seq进入本轮G<br/>GP kthread推进"]
    N["第3层：根/叶rcu_node<br/>gp_seq=G<br/>qsmask=0b1111"]

    subgraph LOCAL["第4层：每CPU状态与EQS证据"]
        D0["rcu_data[0]<br/>写者已睡眠<br/>等待后续QS/EQS"]
        D1["rcu_data[1]<br/>cpu_no_qs.norm=true<br/>CPU1仍运行旧reader"]
        D2["rcu_data[2]<br/>实际无旧reader<br/>但仍欠保守证明"]
        C3["CPU3 context_tracking.state<br/>GP开始时已在idle/EQS"]
        R1["CPU1当前任务寄存器/栈<br/>p=old_obj<br/>非抢占分支不登记任务身份"]
    end

    W -->|"head作为callback进入<br/>rcu_data[0].cblist"| D0
    D0 -->|"gp_seq_needed向上请求GP"| G
    G -->|"rcu_gp_init建立本轮等待集合"| N
    N -->|"bit0：要求CPU0证明"| D0
    N -->|"bit1：要求CPU1证明"| D1
    N -->|"bit2：要求CPU2证明"| D2
    N -.->|"bit3：等待CPU3证据"| C3

    D0 -->|"当前GP内后续QS/EQS<br/>清bit0：1111→1110"| N
    D2 -->|"下一次合法上下文切换<br/>清bit2：1110→1010"| N
    C3 -.->|"force-QS观察EQS代际<br/>清bit3：1010→0010"| N
    R1 -->|"先unlock，随后才允许合法QS"| D1
    D1 -->|"异步报告<br/>清bit1：0010→0000"| N

    N -->|"qsmask=0，根完成"| G
    G -->|"GP结束后callback成熟"| D0
    D0 -->|"wakeme_after_rcu()<br/>complete"| W
```

图中最容易误读的是 CPU1 的 `p=old_obj`：这个指针确实存在，却没有被写进 `rcu_data` 或 `rcu_node`。非抢占式 Tree RCU 保存的不是 reader 名单，而是一个执行约束：CPU1 在最外层读区结束前不能发生普通抢占式任务切换。因此，后来的 CPU1 QS 可以反向证明这个未登记的旧 reader 已经结束。

### 6.2.3\_四层表必须写出本例中的实际值

| 层次与具体地址                                                                     | 本例在 GP 开始时的状态                                                       | 谁在什么事件写入                                                                                                                       | 后续谁读取                                                        | 本例最终怎样退出                                                                               |
| --------------------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------ | -------------------------------------------------------------------------------------- |
| 写者等待层：CPU0 写者栈上的 `struct rcu_synchronize`，其中 `head` 链入 `rcu_data[0].cblist` | `completion` 未完成，写者睡眠；栈帧不能销毁                                        | CPU0 的 `__wait_rcu_gp()` 初始化 `head/completion` 并提交 callback；GP 后 callback 执行 `wakeme_after_rcu()`                              | callback 执行上下文用 `container_of()` 找回等待对象；CPU0 睡在 completion 上 | `wakeme_after_rcu()` 调用 `complete()`，CPU0 返回 `synchronize_rcu()` 后才允许 `kfree(old_obj)` |
| 全局协调层：`rcu_state.gp_seq`、`gp_flags` 与 GP kthread                            | callback 请求需要一个覆盖它的未来 GP；`gp_seq` 随后进入本轮代际 G 的进行态                   | 请求路径设置 GP 需求；GP kthread 在 `rcu_gp_init()`/cleanup 中开始、结束 G                                                                     | 所有 `rcu_node`、每 CPU 新 GP 检测、callback 推进路径                    | 根节点报告完成后结束 G，使等待 callback 获得执行资格                                                       |
| 节点共享层：本例唯一根/叶 `rcu_node.gp_seq/qsmask`                                      | `gp_seq=G`，`qsmask=0b1111`；这四位是 CPU 债务，不是四个 reader                  | `rcu_gp_init()` 置 `qsmask=qsmaskinit`；CPU 本地报告或远端 EQS 观察路径按 `1111→1110→1010→0010→0000` 清位                                      | `rcu_report_qs_rdp()`、`rcu_report_qs_rnp()` 与 GP kthread     | `qsmask=0`；若存在父节点则继续清父节点对应位，本例直接到根完成                                                   |
| 每 CPU/EQS 证据层：`rcu_data[0..3]`，以及独立的每 CPU `context_tracking.state`          | CPU0/CPU1/CPU2 的节点位先保守置位；CPU1 真有旧 reader，CPU0/CPU2 实际没有；CPU3 已在 EQS | 本 CPU `__note_gp_changes()` 写 `gp_seq/cpu_no_qs/core_needs_qs`；调度路径的 `rcu_qs()` 锁存本地 QS；idle/user/IRQ 路径更新 context-tracking 代际 | 本 CPU `rcu_check_quiescent_state()`，或 force-QS 对 EQS 快照的远端检查 | CPU0、CPU2 用后续 QS，CPU3 用 EQS 证据，CPU1 用“unlock 后才可能出现的 QS”分别清除自己的节点位                     |

四层之间不是包含关系，而是一条接力链：**写者等待对象提出需求 → 全局状态建立代际 → 节点状态保存共享债务 → 每 CPU/EQS 状态产生证据 → 节点归零 → 全局结束代际 → callback 唤醒原写者。** 把其中任何一层单独拿出来，都不能解释 `old_obj` 为什么此时可以释放。

### 6.2.4\_逐个CPU看本例为什么不是reader计数

| CPU | GP 开始瞬间的真实现场 | 非抢占式配置允许什么证明事件 | 地址变化与结果 |
| --- | --- | --- | --- |
| CPU0 | 写者已经提交同步请求并睡眠，本 CPU 没有旧 reader | GP 开始后的后续任务切换或进入 EQS | `rcu_data[0].cpu_no_qs.norm` 锁存 QS，随后清节点 bit0；写者睡眠本身若发生在 GP 开始前，不能冒充本轮 QS |
| CPU1 | 当前任务仍在最外层读区，寄存器/栈上的 `p` 指向 `old_obj` | 必须先执行 `rcu_read_unlock()`，以后才能发生合法任务切换 QS 或进入 EQS | CPU1 在此之前不能清 bit1；本例让它最后把 `0b0010` 清为 `0` |
| CPU2 | 正在执行内核代码，但没有任何旧 reader | 下一次合法任务切换就是足够的保守证明 | RCU 虽然多等了一次，但只降低活性；bit2 清除不依赖 reader 计数 |
| CPU3 | GP 开始时已经处于 idle/EQS | force-QS/context-tracking 路径确认 CPU 在本轮边界处于或经过 EQS | 不要求 CPU3 先运行一个 reader 或专门回调；EQS 代际证据使 bit3 清除 |

因此，`qsmask=0b1111` 不能翻译成“四个 CPU 都有旧 reader”。本例中只有 CPU1 真正持有 `old_obj`，Tree RCU 却先向四个 CPU 都收取证明；这种保守等待把“每次 reader 都登记身份”的高频共享写，转移成“每轮 GP 每个相关 CPU 至少交一次证据”的低频工作。

### 6.2.5\_同一现场切到抢占式配置会多出什么

现在只改变一个条件：启用 `CONFIG_PREEMPT_RCU=y`，并让 CPU1 的旧 reader 在 `use_obj(p)` 前被高优先级任务抢占。四个公共层仍然存在，但 **CPU1 的旧指针债务可以离开 CPU1 执行现场**，所以必须增加与 CPU 债务正交的任务状态：

```mermaid
flowchart LR
    E["同一事件<br/>CPU1的R-old仍持有old_obj<br/>调度器准备将它换出"] --> C{"当前配置"}

    C -->|"!CONFIG_PREEMPT_RCU"| N1["rcu_read_lock禁用普通抢占"]
    N1 --> N2["R-old不能沿合法普通抢占路径离开CPU1"]
    N2 --> N3["不产生blkd_tasks/gp_tasks债务<br/>CPU1位必须等unlock后的QS"]

    C -->|"CONFIG_PREEMPT_RCU=y"| P1["调度路径发现nesting仍大于0"]
    P1 -->|"先转移债务再承认CPU QS"| P2["task_struct记录blocked状态<br/>rcu_blocked_node指向叶节点"]
    P2 --> P3["任务加入rcu_node.blkd_tasks<br/>gp_tasks指向阻塞本轮GP的边界"]
    P3 --> P4["CPU1的qsmask位可以清除<br/>但gp_tasks非NULL仍阻塞GP"]
    P4 -->|"任务在任意CPU最终unlock"| P5["移除任务债务<br/>gp_tasks归NULL后才允许节点完成"]
```

| 同一问题 | 非抢占式 Tree RCU | 抢占式 Tree RCU |
| --- | --- | --- |
| CPU1 旧 reader 能否被普通抢占 | 不能；`rcu_read_lock()` 建立禁止普通抢占的执行约束 | 能；调度路径必须保存被换出任务仍在读区的事实 |
| CPU1 上下文切换以后，CPU 位能否清 | 合法旧 reader 不可能跨过这次切换，因此可以把切换当 CPU 证明 | 可以清 CPU 债务，但清位前必须先把仍在读区的任务登记为任务债务 |
| 旧 reader 债务存在哪里 | 不单独登记任务；借助 CPU 位和“读区不能跨 QS”的不变量间接证明 | `task_struct` 的 nesting/special/blocked-node 状态，加叶节点 `blkd_tasks/gp_tasks` |
| 节点完成条件 | `qsmask==0`，非抢占分支的 `rcu_preempt_blocked_readers_cgp()` 恒为 0 | `qsmask==0` 仍不够；当前 GP 对应的 `gp_tasks` 还必须为空 |

所以本章后续 S0～S10 的主线只讨论 **非抢占式配置**：普通 GP 债务可由 CPU/子树位完整表达。抢占式实现不是把 `qsmask` 改成任务位图，而是在保留 CPU 位图的同时增加任务债务轴；完整转移过程见[抢占式 Tree RCU 的问题与任务跟踪模型](P07_抢占式_Tree_RCU_问题与任务跟踪模型.md)和[抢占式 Tree RCU 源码同步机制](P08_抢占式_Tree_RCU_源码同步机制.md)。

## 6.3\_S0到S10\_一轮同步等待的真实状态机

| 阶段 | 触发 | 修改前后 | 写入者与地址 | 后续读取者 | 退出条件 |
| --- | --- | --- | --- | --- | --- |
| S0 | 旧对象仍发布 | `global_ptr=old_obj` | 业务模块 | reader | 写者完成新对象初始化 |
| S1 | 替换入口 | `old_obj -> new_obj` | 写者写业务入口 | 新 reader | 旧对象不再由正式入口发布 |
| S2 | `synchronize_rcu()` | 创建栈上 `rcu_synchronize`，callback 入队 | 写者/`__wait_rcu_gp()` 写 completion 与 `rcu_head` | 每 CPU callback/GP 请求路径 | 回调已绑定到未来 GP |
| S3 | callback 加速 | `gp_seq_needed` 推向根，`gp_flags|=INIT` | 本 CPU RCU 路径与 `rcu_start_this_gp()` | GP kthread | GP kthread 被唤醒或已有 GP 可承接 |
| S4 | GP 开始 | `rcu_state.gp_seq` 进入进行态 | GP kthread/`rcu_seq_start()` | 各节点与 CPU | 全局代际已建立 |
| S5 | 建立等待集 | 每节点 `qsmask=qsmaskinit`，`rnp->gp_seq=rsp->gp_seq` | `rcu_gp_init()` | 每 CPU GP 检测和报告路径 | 所有节点初始化完成 |
| S6 | CPU 感知新 GP | `rdp->gp_seq` 更新；`cpu_no_qs=true`、`core_needs_qs=true` | 本 CPU `__note_gp_changes()` | 本 CPU QS 与 core | CPU 知道自己欠证明 |
| S7 | 发生 QS/EQS | `cpu_no_qs.norm: true -> false`，或远端观察到 EQS 代际变化 | 本 CPU调度/tick/context tracking；EQS 可由 GP 扫描观察 | `rcu_check_quiescent_state()` 或 force-QS | 本地或隐式证据成立 |
| S8 | 提交本 CPU 证据 | `core_needs_qs: true -> false`；叶 `qsmask` 清本 CPU 位 | 当前 CPU `rcu_report_qs_rdp()` | 父节点报告路径 | 叶节点还有位则停止，否则向上 |
| S9 | 逐层汇聚 | 子节点完成后父 `qsmask` 清对应位 | `rcu_report_qs_rnp()` | 根报告路径 | 根 `qsmask=0` |
| S10 | GP 清理并交付 | `rcu_seq_end()`，callback 进入可执行段并调用 `complete()` | GP kthread、每 CPU RCU core、`wakeme_after_rcu()` | 原写者 | `wait_for_completion` 返回 |

这不是一条函数直接调用到底的同步栈。S2 的写者可能已经睡眠；S4～S10 分别由 GP kthread、远端 CPU 的调度/context-tracking 路径、本 CPU RCU core 和 callback 执行上下文接力完成。

## 6.4\_synchronize\_rcu()怎样提交并等待GP

### 6.4.1\_Linux\_6.12.20默认路径

`kernel/rcu/tree.c:4096` 的 `synchronize_rcu()` 完成 lockdep 检查后进入 `synchronize_rcu_normal()`。默认模块参数 `rcu_normal_wake_from_gp` 为 0，所以走：

```text
synchronize_rcu()
    -> synchronize_rcu_normal()
        -> wait_rcu_gp(call_rcu_hurry)
            -> __wait_rcu_gp()
                -> 初始化栈上 rcu_synchronize.completion
                -> call_rcu_hurry(&rs.head, wakeme_after_rcu)
                -> wait_for_completion_state()
```

`kernel/rcu/update.c:402` 的 `wakeme_after_rcu()` 用 `container_of()` 找回栈上的 `struct rcu_synchronize`，然后 `complete(&rcu->completion)`。因此默认路径的同步者不是自己扫描 CPU，而是把“唤醒我”登记成一个必须跨 GP 才能执行的 RCU callback。

`call_rcu_hurry()` 进入 `__call_rcu_common()`，把 callback 放入当前 CPU 的 `rcu_data.cblist`。回调加速路径最终调用 `rcu_start_this_gp()`：沿叶到根写 `gp_seq_needed`，需要新 GP 时对 `rcu_state.gp_flags` 设置 `RCU_GP_FLAG_INIT`，再由调用者执行 `rcu_gp_kthread_wake()`。

### 6.4.2\_中的直接唤醒优化分支

如果运行时参数 `rcu_normal_wake_from_gp` 非零，`synchronize_rcu_normal()` 不走普通 callback completion，而是：

```text
rcu_sr_normal_add_req(&rs)
    -> start_poll_synchronize_rcu()
        -> 请求GP
    -> wait_for_completion(&rs.completion)
```

请求存入 `rcu_state.srs_next`，`rcu_sr_normal_gp_init()` 在 GP 开始时划分等待批次，`rcu_sr_normal_gp_cleanup()` 在 GP 清理阶段交付完成。这个分支是 6.12 的实现优化，不改变“调用前读侧必须结束、后来读侧可以并发”的 API 语义。

## 6.5\_GP初始化怎样标记谁欠QS

`rcu_gp_kthread()` 睡在 `rcu_state.gp_wq`，观察到 `gp_flags & RCU_GP_FLAG_INIT` 后调用 `rcu_gp_init()`：

1. 在根节点锁保护下清请求标志。
2. `rcu_seq_start(&rcu_state.gp_seq)` 启动新代际。
3. 先把 hotplug 对 `qsmaskinitnext` 的变化应用到 `qsmaskinit`。
4. 按广度优先遍历全部 `rcu_node`。
5. 对每个节点执行 `rnp->qsmask = rnp->qsmaskinit`。
6. 执行 `WRITE_ONCE(rnp->gp_seq, rcu_state.gp_seq)`。

所以写者不需要先发现 reader：GP 先把当前参与集合都视为“尚未证明”，然后让实际执行过程逐步清除债务。在线集合变化由 `qsmaskinitnext`、hotplug 锁和 GP 初始化中的离线掩码共同处理，不是每轮直接无锁复制 `cpu_online_mask`。

## 6.6\_每CPU怎样感知新GP

本 CPU 的 `rcu_core()` 会执行 `rcu_check_quiescent_state(rdp)`，其第一步 `note_gp_changes()` 在叶 `rcu_node` 锁下进入 `__note_gp_changes()`：

```c
need_qs = !!(rnp->qsmask & rdp->grpmask);
rdp->cpu_no_qs.b.norm = need_qs;
rdp->core_needs_qs = need_qs;
rdp->gp_seq = rnp->gp_seq;
```

这些字段仍然不表示“本 CPU 当前有 reader”。它们表示：本 CPU 已看到该叶节点的新代际，而且节点的等待位说明它还欠一个本轮 QS。

## 6.7\_调度路径怎样形成本地QS

调度器 `kernel/sched/core.c::__schedule()` 在持有当前任务、尚未切换 `prev/next` 的位置关闭本地中断并调用：

```c
rcu_note_context_switch(preempt);
```

非 PREEMPT_RCU 分支的 `tree_plugin.h::rcu_note_context_switch()` 直接调用 `rcu_qs()`。`rcu_qs()` 只修改当前 CPU 的本地字段：

```c
if (this_cpu_read(rcu_data.cpu_no_qs.b.norm))
	this_cpu_write(rcu_data.cpu_no_qs.b.norm, false);
```

它不取得 `rcu_node` 锁，也不在调度器关键路径上逐层清树。这里完成的是 **本地锁存**：“这个 CPU 已经看见当前 GP 所需的 QS”。

为什么能够这样做？因为非抢占 reader 的 `__rcu_read_lock()` 调用 `preempt_disable()`，合法任务切换不能穿过仍在使用旧指针的读区。调度钩子不必知道被切走任务读取过哪个对象。

## 6.8\_用户态和idle怎样提供EQS证明

### 6.8.1\_Linux\_6.12的context-tracking状态

6.12.20 不再把主要 EQS 代际保存在 `rcu_data.dynticks`。`kernel/context_tracking.c` 使用每 CPU `context_tracking.state`，其中 `CT_RCU_WATCHING` 位区分 RCU 是否正在关注该 CPU：

- `ct_idle_enter()` 调用 `ct_kernel_exit(false, ...)`，进入 idle EQS。
- `__ct_user_enter()` 在启用相应 context tracking 时调用 `ct_kernel_exit(true, ...)`，进入 user EQS。
- `ct_kernel_exit_state()` 用有序的原子状态增量记录“不再 watching”。
- `ct_idle_exit()` / `__ct_user_exit()` 通过 `ct_kernel_enter()` 恢复 watching，且必须在可能使用普通 RCU 以前完成。

### 6.8.2\_远端怎样确认CPU已经经过EQS

force-QS 扫描并不只看一个布尔值。`rcu_watching_snap_save()` 用 acquire 语义保存远端 CPU 的 watching 代际；若快照已经表明 CPU 在 EQS，可以立即形成隐式证明。否则后续 `rcu_watching_snap_recheck()` 调用 `rcu_watching_snap_stopped_since()`，只有发现代际变化，才证明该 CPU 自快照后经过 EQS。

这避免了如下竞态：协调 CPU 第一次看见“watching”，远端迅速进入又退出 idle；只看当前布尔值可能错过这段经历，比较代际则能保留“至少经过一次 EQS”的历史证据。

普通调度时钟路径也能提供证明：非抢占分支的 `rcu_flavor_sched_clock_irq(user)` 在中断来自用户态或 idle 时调用 `rcu_qs()`。

## 6.9\_本地QS怎样异步进入rcu\_node树

`rcu_core()` 后续调用 `rcu_check_quiescent_state()`：

```text
note_gp_changes()
    -> 确认本 CPU 的 GP 代际
if (!core_needs_qs)
    -> 无债务，返回
if (cpu_no_qs.b.norm)
    -> 尚无本地证据，返回
rcu_report_qs_rdp(rdp)
```

`rcu_report_qs_rdp()` 先锁本 CPU 的叶 `rcu_node`，再次核对：

- `rdp->cpu_no_qs.b.norm` 必须已经为 false；
- `rdp->gp_seq` 必须等于 `rnp->gp_seq`；
- 本 CPU 的 `grpmask` 位必须仍在等待。

代际不匹配时，它拒绝把旧 QS 上报给新 GP，并重新令 `cpu_no_qs.b.norm=true`。核对成功才清 `core_needs_qs`，调用 `rcu_report_qs_rnp()`。

因此存在一个正常异步窗口：

```text
cpu_no_qs.b.norm 已经是 false
    但
rcu_node.qsmask 中本 CPU 位仍为 1
```

这不是状态矛盾；前者是本地证据，后者是共享汇聚状态。

## 6.10\_qsmask怎样逐层清到根

`rcu_report_qs_rnp(mask, rnp, gps, flags)` 在每一级都检查 `rnp->gp_seq == gps`，然后清除 `rnp->qsmask` 中的 `mask`：

```text
本级 qsmask 仍非零
    -> 还有兄弟 CPU/子节点欠证明，停止上报

本级 qsmask 变零
    -> 用本节点 grpmask 作为父节点中的一位
    -> 锁父节点并继续上报

到达根且 qsmask 变零
    -> rcu_report_qs_rsp()
    -> 设置 FQS 标志并唤醒 GP kthread
```

树形汇聚把争用限制在叶节点和偶尔向上的节点：每个 reader 不写树，每个 CPU 每轮通常只需报告一次；只有某个子树整体完成时才继续碰父层缓存行。

## 6.11\_GP完成怎样唤醒原写者

GP kthread 从 `rcu_gp_fqs_loop()` 返回后进入 `rcu_gp_cleanup()`：

1. 先把完成后的 `gp_seq` 广度优先传播到全部节点。
2. 断言 `rnp->qsmask` 已清空；非抢占分支也不存在阻塞当前 GP 的普通 reader 任务。
3. 对全局 `rcu_state.gp_seq` 调用 `rcu_seq_end()`，把 GP 置为完成态。
4. callback 分段随后因 GP 完成而推进到可执行状态。
5. 原 `synchronize_rcu()` 登记的 `wakeme_after_rcu()` 被调用，对栈上 completion 执行 `complete()`。
6. 写者从 `wait_for_completion_state()` 返回，才执行 `kfree(old_obj)`。

如果启用 `rcu_normal_wake_from_gp`，第 4～5 步由 `rcu_sr_normal_gp_cleanup()` 的专用同步等待者链完成；安全边界相同，交付路径不同。

## 6.12\_端到端源码时序

```mermaid
sequenceDiagram
    autonumber
    participant W as CPU0更新者
    participant CB as CPU0回调与rcu_data
    participant GK as GP kthread<br/>rcu_state
    participant N as rcu_node树
    participant C1 as CPU1调度与本地RCU
    participant CT as CPU2 context tracking

    W->>W: rcu_assign_pointer()<br/>取消发布old_obj
    W->>CB: synchronize_rcu()<br/>登记wakeme_after_rcu回调
    CB->>N: callback加速<br/>推进gp_seq_needed
    N->>GK: 设置GP_FLAG_INIT并唤醒
    GK->>GK: rcu_seq_start(gp_seq)
    GK->>N: qsmask=qsmaskinit<br/>复制节点gp_seq

    C1->>N: __note_gp_changes()<br/>本地记录gp_seq与QS债务
    C1->>C1: context switch<br/>rcu_qs清cpu_no_qs.norm
    C1-->>N: rcu_core异步调用<br/>rcu_report_qs_rdp

    CT->>CT: 进入idle/user EQS<br/>推进CT_RCU_WATCHING代际
    GK->>CT: force-QS读取远端快照
    GK->>N: 代表CPU2提交隐式QS

    N->>N: rcu_report_qs_rnp<br/>逐层清qsmask
    N-->>GK: 根完成并唤醒
    GK->>GK: rcu_gp_cleanup()<br/>rcu_seq_end(gp_seq)
    GK-->>CB: callback获得可执行资格
    CB-->>W: wakeme_after_rcu()<br/>complete等待者
    W->>W: kfree(old_obj)
```

## 6.13\_读侧接口在非抢占配置中的实际展开

`include/linux/rcupdate.h` 的配置分支是：

```c
static inline void __rcu_read_lock(void)
{
	preempt_disable();
}

static inline void __rcu_read_unlock(void)
{
	preempt_enable();
	if (IS_ENABLED(CONFIG_RCU_STRICT_GRACE_PERIOD))
		rcu_read_unlock_strict();
}
```

外层 `rcu_read_lock()` / `rcu_read_unlock()` 还包含 Sparse/lockdep 标记和 watching 合法性检查。核心结论是：普通分支提供执行约束，而不是向树登记 `task_struct` 或设置 `qsmask`。

## 6.14\_发布与取得原语的实际约束

`rcu_assign_pointer(p, v)` 在 `v` 不是编译期常量 `NULL` 时展开到：

```c
smp_store_release(&p, RCU_INITIALIZER(v));
```

它保证发布指针以前的对象初始化不会被移到发布之后；常量 `NULL` 分支使用 `WRITE_ONCE()`。

`rcu_dereference(p)` 进入 `rcu_dereference_check(p, 0)`，底层通过 `READ_ONCE(p)` 取得一次指针值，保留地址依赖顺序，并执行 Sparse/lockdep 检查。它既不复制对象，也不增加引用计数。发布/取得只保证新对象初始化的观察顺序；GP 才负责旧对象回收边界，两条轴不能互相替代。

## 6.15\_Linux\_5.10明显差异

跨版本成立的主线没有变化：`rcu_state.gp_seq`、节点 `gp_seq/qsmask/qsmaskinit`、每 CPU `gp_seq/cpu_no_qs/core_needs_qs`、调度 QS 和树形汇聚都已经存在。

明显差异是 EQS 状态位置：

| 版本 | EQS/watching主要状态 | 典型函数名 |
| --- | --- | --- |
| Linux 5.10 | `rcu_data.dynticks_nesting`、`dynticks_nmi_nesting`、`atomic_t dynticks` | `rcu_eqs_enter()`、`rcu_idle_enter()`、`rcu_user_enter()`、`rcu_momentary_dyntick_idle()` |
| Linux 6.12.20 | `context_tracking.state` 与 `CT_RCU_WATCHING`，`rcu_data` 保存 `watching_snap` 等 GP 观察值 | `ct_kernel_exit/enter()`、`ct_idle_enter/exit()`、`__ct_user_enter/exit()`、`rcu_momentary_eqs()` |

另一个可见差异是 6.12.20 增加了 `rcu_state.srs_next` 等普通同步等待者批处理状态以及 `rcu_normal_wake_from_gp` 直接唤醒优化；阅读 5.10 时不要查找这些 6.12 字段，应从该版本自己的同步等待实现追踪。

## 6.16\_十四项源码证据核对

| 要求 | Linux 6.12.20 证据 |
| --- | --- |
| 1. 同步提交和等待 | `tree.c::synchronize_rcu_normal()`；`rcupdate_wait.h::wait_rcu_gp`；`update.c::__wait_rcu_gp()/wakeme_after_rcu()` |
| 2. GP 开始 | `tree.c::rcu_gp_kthread()` → `rcu_gp_init()` |
| 3. 全局代际 | `tree.h::rcu_state.gp_seq`；`rcu_seq_start/end()` |
| 4. 节点等待集 | `tree.h::rcu_node.gp_seq/qsmask/qsmaskinit/qsmaskinitnext`；`rcu_gp_init()` |
| 5. CPU 感知 | `tree.c::__note_gp_changes()` 更新 `rdp->gp_seq` |
| 6. 本地 QS 债务 | `rcu_data.cpu_no_qs.b.norm/core_needs_qs` |
| 7. 上下文切换 | `sched/core.c::__schedule()` → `rcu_note_context_switch()` → 非抢占 `rcu_qs()` |
| 8. user/idle | `context_tracking.c::ct_idle_enter/__ct_user_enter/ct_kernel_exit()`；`tree.c::rcu_watching_snap_*()` |
| 9. 本地 QS | `tree_plugin.h` 非抢占分支 `rcu_qs()` 清 `cpu_no_qs.b.norm` |
| 10. CPU 报告 | `tree.c::rcu_check_quiescent_state()` → `rcu_report_qs_rdp()` |
| 11. 树形清位 | `tree.c::rcu_report_qs_rnp()` → `rcu_report_qs_rsp()` |
| 12. 唤醒同步者 | GP cleanup 推进 callback → `wakeme_after_rcu()` → `complete()` |
| 13. lock/unlock 展开 | `rcupdate.h` 的 `!CONFIG_PREEMPT_RCU` 分支；严格 GP 例外在 `rcu_read_unlock_strict()` |
| 14. 发布/取得 | `rcupdate.h::rcu_assign_pointer()` 与 `__rcu_dereference_check()` |

更长的逐函数源码摘录和 5.10 对照见[Linux 6.12 非抢占式 Tree RCU 源码调用链](../../../../research/source_reading/rcu/P02_Linux_6.12_非抢占式_Tree_RCU_源码调用链.md)。

上一篇：[非抢占式 Tree RCU 的问题与证明模型](P05_非抢占式_Tree_RCU_问题与证明模型.md)。

下一篇：[抢占式 Tree RCU 的问题与任务跟踪模型](P07_抢占式_Tree_RCU_问题与任务跟踪模型.md)。
