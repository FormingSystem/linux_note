---
id: research.source_reading.rcu.linux_6_12_tasks_tiny
title: "Linux 6.12 Tasks RCU 与 Tiny RCU 源码调用链"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
topics:
  - synchronization
  - rcu
  - tasks_rcu
  - tiny_rcu
source_project: linux
source_version: "6.12.20"
---
# 第4章\_Linux\_6.12\_Tasks\_RCU与Tiny\_RCU源码调用链


## 4.1\_Linux\_6.12\_Tasks\_RCU与\_Tiny\_RCU源码调用链

### 4.1.1\_取证边界

本章核对本地只读源码树 `linux-imx-6.12`，顶层版本为 Linux 6.12.20。它服务于 [Tasks RCU 与 Tiny RCU 实现边界](../../../knowledge/linux/synchronization/rcu/P24_Tasks_RCU与Tiny_RCU实现边界.md)，不把 Tasks、Tasks Rude、Tasks Trace 与 Tiny 合并成一个抽象 GP。

### 4.1.2\_Tasks家族的共享骨架

`kernel/rcu/tasks.h:24-129` 定义：

- `struct rcu_tasks_percpu`：`cblist`、锁、lazy timer、irq work、blocked/exit list；
- `struct rcu_tasks`：`cbs_wait`、`tasks_gp_mutex`、GP 序列/状态、每 CPU 队列数组，以及 `pregp_func`、`pertask_func`、`postscan_func`、`holdouts_func`、`postgp_func`；
- `DEFINE_RCU_TASKS()`：为每个 flavor 生成独立状态实例。

请求到执行的公共路径是：

```text
call_rcu_tasks_generic()
    → 选择每CPU回调队列并入队
    → rcuwait_wake_up(cbs_wait)
    → rcu_tasks_kthread()
    → rcu_tasks_one_gp()
    → flavor指定的wait_gp()
    → 推进rcu_segcblist并执行成熟回调
```

同步接口 `synchronize_rcu_tasks_generic()` 仍通过回调/completion 等待对应 flavor；它没有把三种 flavor 的 GP 合并。

### 4.1.3\_经典Tasks的任务扫描证据

`kernel/rcu/tasks.h:902-1012` 的总注释和 `rcu_tasks_pregp_step()` 规定其 QS：自愿上下文切换、`cond_resched_tasks_rcu_qs()`、用户态和 idle。

`rcu_tasks_wait_gp():812-898` 的状态顺序是：

```text
RTGS_PRE_WAIT_GP
    → flavor pregp
RTGS_SCAN_TASKLIST
    → for_each_process_thread() / pertask
RTGS_POST_SCAN_TASKLIST
    → flavor postscan
RTGS_WAIT_SCAN_HOLDOUTS / RTGS_SCAN_HOLDOUTS
    → 反复检查holdout
RTGS_POST_GP
    → flavor postgp
```

`include/linux/sched.h:901-908` 的 `rcu_tasks_nvcsw`、`rcu_tasks_holdout`、`rcu_tasks_holdout_list` 和 `rcu_tasks_exit_list` 是任务状态载体。退出任务另经 `exit_tasks_rcu_start()`/`exit_tasks_rcu_finish()` 接入每 CPU exit list，避免它从全局任务表消失后被漏掉。

真实调用证据位于 `kernel/trace/ftrace.c:3182-3189`：源码在释放 trampoline 前说明仅在每 CPU 调度一个任务不够，并调用 `synchronize_rcu_tasks()` 等待既有任务自愿调度或进入用户态。

### 4.1.4\_Tasks\_Rude的主动路径

`kernel/rcu/tasks.h:1353-1358::rcu_tasks_rude_wait_gp()` 只有一条核心动作：

```text
统计在线CPU数量
    → schedule_on_each_cpu(rcu_tasks_be_rude)
    → 在所有在线CPU制造上下文切换证明
```

它不用经典 Tasks 的 holdout 扫描换取低干扰，而是显式支付 IPI/调度成本。

### 4.1.5\_Tasks\_Trace的读侧与探测

`include/linux/rcupdate_trace.h:37-84`：

```text
rcu_read_lock_trace()
    → current->trc_reader_nesting++

rcu_read_unlock_trace()
    → nesting--
    → 最外层且有special状态时
       rcu_read_unlock_trace_special(current)
```

`include/linux/sched.h:911-917` 保存 `trc_reader_nesting`、`trc_ipi_to_cpu`、`trc_reader_special`、`trc_holdout_list`、`trc_blkd_node` 和 `trc_blkd_cpu`。

`kernel/rcu/tasks.h:1452-1504` 描述 Tasks Trace GP：pregp 在 CPU hotplug 保护下收集运行任务和 blocked readers，postscan 处理退出竞态，holdout 循环等待，postgp 提供最终内存序。

远端运行读者的探测链：

```text
trc_wait_for_one_reader()
    → task_call_func()尝试稳定检查
    → 必要时smp_call_function_single()
    → trc_read_check_handler()
    → nesting==0：标记CHECKED
    → nesting>0：设置NEED_QS
    → 最外层unlock调用rcu_read_unlock_trace_special()还债
```

`kernel/bpf/trampoline.c:972-1018` 用 `rcu_read_lock_trace()`/`unlock` 包围 sleepable BPF 程序；`bpf_tramp_image_put():307-353` 说明 trampoline 的不同片段分别由 Tasks Trace、普通 RCU、percpu ref 和 Tasks RCU 保护。这是“多个生命周期域必须分别等待”的直接源码示例。

### 4.1.6\_Tiny的单CPU回调状态机

`kernel/rcu/Kconfig:32-38` 令 `TINY_RCU` 默认依赖 `!PREEMPT_RCU && !SMP`。

`kernel/rcu/tiny.c:30-43` 的全局 `rcu_ctrlblk` 只有一条回调链、`donetail`、`curtail` 和 `gp_seq`。调用关系是：

```text
call_rcu():171
    → 关本地中断追加到curtail

rcu_qs():52
    → donetail=curtail
    → gp_seq+=2
    → raise_softirq_irqoff(RCU_SOFTIRQ)

rcu_process_callbacks():108
    → 关中断摘出donetail以前的ready前缀
    → 开中断逐个rcu_reclaim_tiny()
```

`rcu_sched_clock_irq():71` 在 tick 从用户态而来时调用 `rcu_qs()`；若内核态仍有等待回调，则设置 resched 标记，促使唯一 CPU 到达调度边界。

`synchronize_rcu():138-160` 的注释给出简化证明：在非抢占 UP 上，合法调用不可能位于 RCU 读侧，因而调用现场本身已是 QS，只需推进 `gp_seq` 供轮询状态观察。

### 4.1.7\_配置与版本边界

目标源码树 `.config` 实际启用 `CONFIG_TREE_RCU=y` 和 `CONFIG_PREEMPT_RCU=y`，所以 Tiny 部分是对同版本实现文件的静态源码核对，不是该板配置上的运行轨迹。

Tasks 家族的任务字段、IPI 策略和 BPF/ftrace 组合属于版本敏感实现。向 Linux 5.10 对照时，应重新核对该版本的 `kernel/rcu/tasks.h`、`include/linux/sched.h` 和调用方，不能把 6.12 的字段集合和行号逐字套回去。

### 4.1.8\_复核问题

1. 为什么 Tasks RCU 扫描任务而普通 Tree RCU 优先汇聚 CPU QS？
2. `rcu_tasks_nvcsw` 的快照怎样证明任务已经经过自愿切换？
3. 为什么退出任务还需要单独的 per-CPU exit list？
4. Tasks Rude 用什么成本省掉了逐任务 holdout 等待？
5. 为什么 sleepable BPF 任务睡眠不能算 Tasks Trace 的读者结束？
6. `need_qs` 如何把 GP 线程的探测要求送到读者最外层 unlock？
7. Tiny RCU 的 `donetail` 与 `curtail` 分别划定什么集合？
8. 为什么 Tiny 的同步 API 可简化，而异步回调仍须等待 `rcu_qs()`？

上一篇：[Linux 6.12 抢占式 Tree RCU 源码调用链](P03_Linux_6.12_抢占式_Tree_RCU_源码调用链.md)。
