---
id: knowledge.linux.synchronization.rcu.integration_misuse
title: "RCU 调试、验证与集成误用"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
topics:
  - synchronization
  - rcu
---

# 第24章\_RCU\_调试验证与集成误用

“RCU 出问题”至少可能指四件不同的事：对象入口和生命周期协议错误、GP 没完成、回调已成熟但没执行，或者回调已排队但模块先卸载。它们的日志和修复方向完全不同。本章固定三个可重建的故障，再给出从对象到 GP、回调和卸载的诊断顺序。

上一章已经说明“无告警”不是内存安全证明。现在要把一次出错还原成可核对的事件：谁曾拥有对象、哪条异步链仍在运行、缺的是安全完成证据还是执行机会。所有坏代码均为静态反例；本章不要求在工作内核中触发UAF或无限读区。

## 24.1\_先建立四条互不等价的状态链

| 状态链 | 关键问题 | 典型现象 | 优先证据 |
| --- | --- | --- | --- |
| 对象可达性与所有权 | 旧入口是否先摘除；裸指针是否逃逸；引用是否唯一释放 | KASAN UAF、double-free、偶发数据损坏 | 对象地址、发布点、get/put、释放栈 |
| GP 安全性 | 相关 CPU/任务是否仍欠 QS | `RCU Stall`、`synchronize_rcu()` 久等 | `gp_seq`、`qsmask`、blocked reader、stall 输出 |
| 回调成熟与执行 | GP 已结束后，回调是否从 WAIT/NEXT_READY 进入 DONE 并得到 CPU 时间 | 回调积压、内存持续增长 | `rcu_segcblist`、RCU softirq、NOCB GP/CB kthread |
| 卸载与代码生命期 | 指向模块 text 的既有回调是否都已经执行 | 卸载后跳入已释放模块代码 | 阻止新回调的边界、`rcu_barrier()`、生产者退出 |

```mermaid
flowchart LR
    P["撤销旧入口并登记退休"] -->|"按所选协议等待"| G["对应RCU域的GP"]
    G -->|"提供旧借用者退出证据"| M["回调成熟"]
    M -->|"执行者获CPU时间并取出"| I["回调实际执行"]
    I -->|"如有后续交付，还需等待"| W["工作队列及其他异步清理完成"]
    K["kref等长期所有权归还"] -->|"满足同一销毁协议"| F["对象/模块资源最终释放"]
    W -->|"最后使用路径结束"| F
    U["停止并等待新生产者退出"] -->|"关闭继续登记的路径"| B["rcu_barrier等待既有回调"]
    B -->|"仅证明callback部分，不能跳过后续工作"| F
```

`synchronize_rcu()` 只跨过图中的 GP 边，不能自动替代回调执行、kref 归零或模块生产者停机。

这是一张证据依赖图，不规定所有对象都按GP→kref归零回收；上一章的先归零再排GP协议仍然成立。表中Tree字段只适用于相应构建，Tiny、SRCU和Tasks需分别选择自己的状态落点，不应在Tiny系统里寻找不存在的rcu_node树。

## 24.2\_故障一\_工作队列保存了读区外裸指针

这类故障的关键不是 reader 当场越界，而是它把只在读区内有效的借用指针交给了异步执行者。需要同时观察取得时刻、排队时刻、GP 完成和工作真正运行的顺序。

### 24.2.1\_能编译的错误代码

```c
struct service {
	struct kref ref;
	struct rcu_head rcu;
	int value;
};

struct service_work {
	struct work_struct work;
	struct service *service;
};

static struct service __rcu *current_service;

static int bad_queue_service(struct service_work *sw)
{
	rcu_read_lock();
	sw->service = rcu_dereference(current_service);
	rcu_read_unlock();

	/* 错误：队列只复制work地址，不会替service取得引用。 */
	return queue_work(system_wq, &sw->work) ? 0 : -EBUSY;
}
```

错误时序：

```mermaid
sequenceDiagram
    autonumber
    participant Q as 排队任务
    participant WQ as workqueue
    participant U as 更新者
    participant GP as 普通RCU后端

    Q->>Q: RCU内取得service地址
    Q->>Q: 退出RCU读侧<br/>没有取得长期引用
    Q->>WQ: 排队保存裸指针的work
    U->>U: 摘除current_service
    U->>GP: 完成普通GP
    GP-->>U: 旧短读者已结束
    U->>U: 释放service
    WQ->>WQ: 很久以后运行<br/>解引用已释放service
```

GP 没有出错：`service_work` 在 GP 开始前已经退出读侧，RCU 有权不再把它当读者。工作队列也没有出错：它只承诺调度 `work_struct`，不理解 `service` 的所有权。

步骤1、2只完成一次临时借用，步骤3没有增加对象份额；步骤6的GP证据因此允许步骤7释放，而步骤8才运行的work已经错过保护窗口。这里假定发布引用是最后一份，更新者按既定释放协议完成回收；图不表示更新者可以绕开kref直接free。

### 24.2.2\_正确的引用交接

选择上一章模型A：发布入口持初始引用，最后put再排GP。除此以外还要解决 **工作项自身由谁持有**。如果复用同一个sw，先写 `sw->service` 再调用queue_work，返回false时可能已经覆盖了旧工作的对象指针；归还新引用并不能修复旧责任。因此以下骨架每次分配独立sw，初始化后只投递一次，成功后由worker独占并释放。

```c
static struct workqueue_struct *service_wq;

static void service_release(struct kref *ref)
{
	struct service *service;

	service = container_of(ref, struct service, ref);
	kfree_rcu(service, rcu);
}

static void service_workfn(struct work_struct *work)
{
	struct service_work *sw;

	sw = container_of(work, struct service_work, work);
	use_service(sw->service);
	kref_put(&sw->service->ref, service_release);
	kfree(sw);  /* 独立工作项的唯一销毁者。 */
}

static int queue_service(void)
{
	struct service *service;
	struct service_work *sw;

	/* 本接口运行于允许睡眠的过程上下文。 */
	sw = kmalloc(sizeof(*sw), GFP_KERNEL);
	if (!sw)
		return -ENOMEM;
	INIT_WORK(&sw->work, service_workfn);

	rcu_read_lock();
	service = rcu_dereference(current_service);
	if (service && !kref_get_unless_zero(&service->ref))
		service = NULL;
	rcu_read_unlock();
	if (!service) {
		kfree(sw);
		return -ENOENT;
	}

	sw->service = service;
	if (!queue_work(service_wq, &sw->work)) {
		/* sw从未交给其他生产者；失败时仍由本调用者持有。 */
		kref_put(&service->ref, service_release);
		kfree(sw);
		return -EBUSY;
	}
	return 0;
}
```

发布入口持有初始引用；更新者摘除旧入口后归还该引用。lookup reader 只在 RCU 防止对象物理释放的窗口内执行 `kref_get_unless_zero()`。最后 put 的 release 是唯一释放入口，但它调用 `kfree_rcu()`，继续等待仍可能只拿到裸地址、尚未来得及 get 的并发 lookup reader。

初始化时先创建私有 `service_wq`，检查创建失败，再开放 `queue_service()` 入口；关闭时先阻止并等待在途投递者退出，再排空/销毁该私有队列。`use_service()`是业务占位函数，只使用传入的有效引用，不另存裸指针；需要硬件操作许可时仍另行检查。这里不提供单项取消，若增加取消功能，要给“取消后worker不再运行”的分支安排sw和引用的归还者。正常有效的一次性work应成功排队，false分支保留失败清理；它不授权重复投递同一个sw。

## 24.3\_故障二\_模块只等GP\_却没有等自己的回调执行

这里对象内存可能已经满足 GP 条件，真正危险的是 callback 函数体仍位于即将卸载的模块文本段。诊断必须把“callback 已成熟”和“callback 已实际调用完”分开。

### 24.3.1\_为什么synchronize\_rcu()不够

模块删除对象时排队：

以下为另一独立骨架，`module_obj`包含node和rcu成员；对象只撤下一次，调用者已持结构更新锁，其他读者使用匹配RCU遍历。代码不把list_del本身当作写者互斥。

```c
static void module_obj_free_rcu(struct rcu_head *head)
{
	struct module_obj *obj;

	obj = container_of(head, struct module_obj, rcu);
	kfree(obj);
}

static void module_obj_remove(struct module_obj *obj)
{
	list_del_rcu(&obj->node);
	call_rcu(&obj->rcu, module_obj_free_rcu);
}
```

`module_obj_free_rcu()` 的函数地址位于模块 text。即使卸载路径调用 `synchronize_rcu()`，它只能保证一次相关 GP 已过去；已经排队的回调可能成熟后仍留在 per-CPU/NOCB 回调队列，尚未真正执行。此时卸载模块会留下指向已释放 text 的函数指针。

### 24.3.2\_完整卸载顺序

```c
static void module_stop(void)
{
	/* 1. 在业务协议下关接纳，注销并等待外部入口退出。 */
	close_producer_gate();
	unregister_driver_entry();

	/* 2. 永久停止timer重装，再同步会被它投递的work。 */
	timer_shutdown_sync(&producer_timer);
	cancel_work_sync(&producer_work);

	/* 3. 摘除剩余对象并排队其RCU回调。 */
	remove_all_module_objects();

	/* 4. 等待此前排队的普通RCU回调实际调用完毕。 */
	rcu_barrier();

	/* 5. 此后才允许模块text和私有资源消失。 */
}
```

顺序不能交换：若 `rcu_barrier()` 后仍有 producer 能排队新回调，barrier 返回只覆盖调用前已有回调，新回调仍会指向即将卸载的模块代码。若使用 `call_srcu()` 或 Tasks Trace 回调，则必须使用对应域的 barrier，不能用普通 `rcu_barrier()` 跨域代替。

`close_producer_gate()`、`unregister_driver_entry()`和 `remove_all_module_objects()`是需要具体驱动实现的占位操作，不是Linux公共API。前两个必须共同保证旧入口执行者也已离开，最后一个必须在正确更新锁下只撤下一次。固定6.12.20的 `timer_shutdown_sync()` 除等待在途timer外还禁止以后重新arm，适合最终退出；仅 `del_timer_sync()` 不能独自阻止另一路work重新装定时器。调用者不得持有这些回调完成所需的锁，且关停后的timer不再被本实例正常使用。若还有其他work、IRQ或后续清理队列，须按上一章D0～D5分别收敛，这段单timer/单work骨架不能自动覆盖它们。

## 24.4\_故障三\_永不退出的旧读者只会卡住\_不会被超时释放

```c
static void broken_reader(void)
{
	rcu_read_lock();
	while (!READ_ONCE(stop_reader))
		cpu_relax();
	rcu_read_unlock();
}
```

在非抢占式 Tree RCU 中，这个循环阻止所在 CPU 提供可跨过该读者的普通 QS；在 PREEMPT_RCU 中，任务若被抢占，会进入 `rcu_node.blkd_tasks/gp_tasks` 并以任务债务继续阻塞 GP。两种配置的状态位置不同，安全结论相同：

```text
没有足够QS/任务退出证明
    → GP不能完成
    → synchronize_rcu()继续等待
    → call_rcu()回调继续积压
    → 可能触发stall warning
```

force-QS、IPI、boost 和 stall 检测只能催促或诊断。它们不会在超时后伪造证明并提前释放对象。可复现实验见 [晚到读者与抢占读者的对象回收实验](../../../../../labs/kernel/rcu/P01_晚到读者与抢占读者/README.md)。

这是仍未退出的相关旧读者场景，不是“任何时候新增读者都必然拖住当前GP”。Tiny同样不能跨过未结束的普通读侧，但不具备这里的Tree树和GP线程。不要在非抢占单CPU上指望另一个普通任务来改stop_reader解除循环；它可能根本得不到CPU。实验应使用可恢复、受控且与配置匹配的安排，不能把无限循环当成通用复现命令。

## 24.5\_端到端诊断流程

前面的故障分别来自使用期逃逸、完成条件选错和活性停滞。统一诊断时应先确认运行配置，再重建同一对象或同一 GP 的时间线，最后才用 trace、lockdep、stall 日志或内存错误报告验证假设。

### 24.5.1\_D0\_先确认运行配置和flavor

```bash
zgrep -E 'CONFIG_(TREE|TINY|PREEMPT)_RCU|CONFIG_TASKS(_TRACE|_RUDE)?_RCU|CONFIG_RCU_NOCB_CPU' /proc/config.gz
```

若目标系统没有 `/proc/config.gz`，改查发布的内核配置或构建树 `.config`。不能用源码中存在 `PREEMPT_RCU` 分支推导运行内核已经启用它。

### 24.5.2\_D1\_先画对象时间线

记录同一个对象地址的五个事件：

```text
分配/初始化
    → 发布到哪个入口
    → 从所有合法入口摘除
    → 提交哪个flavor的GP/回调
    → 谁执行唯一最终释放
```

若对象带 kref，再记录初始发布引用、每次逃逸 get/put 和 release。若是 root + 多 block，分别画 root GP 与 block kref；不要拿一个“引用数”概括所有权图。

地址可被分配器复用，因此同时记录对象代号或分配代次、CPU/任务、事件时间和入口身份，避免把两代同地址对象拼成一个生命周期。日志需要足够覆盖申请到销毁；缺失事件可能是没执行、没启用、缓冲覆盖或采集窗口不含该阶段，不能直接解释为机制没有发生。

### 24.5.3\_D2\_用stall输出定位欠债者

Linux 6.12 的 stall 信息来自 `kernel/rcu/tree_stall.h` 等路径。阅读时先找：

- 哪个 flavor 和 GP 序号停滞；
- 哪些 CPU 仍在等待集合中；
- CPU 是否长期关中断、禁抢占或没有调度；
- PREEMPT_RCU 是否报告 blocked task；
- GP kthread 自身是否长期得不到运行机会。

诊断最后一项时，先用 [GP 全局生命周期模块导读](../../../../../research/source_reading/rcu/navigation/P03_Linux_6.12_Tree_RCU_GP全局生命周期模块源码概念导读.md#3.7_一次唤醒怎样进入主循环和初始化)和 [force-QS 与 Stall 模块导读](../../../../../research/source_reading/rcu/navigation/P05_Linux_6.12_Tree_RCU_force_QS与Stall模块源码概念导读.md#5.1_为什么GP已经在等还要有force_QS)区分模块职责，再分别进入 [普通 GP 长期任务的主循环](../../../../../research/source_reading/rcu/source_explanations/P05_Linux_6.12_Tree_RCU_GP全局生命周期源码实现.md#5.8_rcu_gp_kthread串联一轮物理GP)和 [force-QS 与 Stall 源码实现](../../../../../research/source_reading/rcu/source_explanations/P07_Linux_6.12_Tree_RCU_force_QS与Stall源码实现.md#7.2_源码符号覆盖账本)，区分“线程没有请求而正常休眠”“已被唤醒但尚未获调度”“运行在 FQS 等待阶段”三种状态，并定位活动时间、欠债 CPU/任务和 stall 分类。不要看到 stall 就直接增大超时参数；延长阈值只能减少告警频率，不能修复永不退出的读侧、关中断循环或 GP kthread 饥饿。

### 24.5.4\_D3\_按本机available\_events选择tracepoint

在隔离测试机的有权限shell中执行下面的独立采集段。前提是tracefs已由系统挂载于该路径，并支持instances；脚本不代替挂载或内核配置。它创建自己的事件缓冲，按实际存在的事件启用，最后保存结果并移除本次实例。

```bash
(
    set -eu
    trace_root=/sys/kernel/tracing
    test -r "$trace_root/available_events"
    test -d "$trace_root/instances"
    grep '^rcu:' "$trace_root/available_events"

    result_dir=$(mktemp -d ./rcu_trace.XXXXXX)
    instance_dir="$trace_root/instances/rcu_note_$$"
    mkdir "$instance_dir"
    cleanup_trace() {
        printf '0\n' > "$instance_dir/tracing_on"
        rmdir "$instance_dir"
    }
    trap cleanup_trace EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
    printf '0\n' > "$instance_dir/tracing_on"
    enabled=0
    for event in rcu_grace_period rcu_quiescent_state_report rcu_callback \
                 rcu_invoke_callback rcu_batch_end; do
        event_dir="$instance_dir/events/rcu/$event"
        if test -f "$event_dir/enable"; then
            cat "$event_dir/format" > "$result_dir/$event.format"
            printf '1\n' > "$event_dir/enable"
            enabled=$((enabled + 1))
        else
            printf '本内核无事件：%s\n' "$event"
        fi
    done
    test "$enabled" -gt 0
    printf '1\n' > "$instance_dir/tracing_on"
    printf '在另一终端执行已准备好的有限测试，结束后按回车。\n'
    read -r finished
    printf '0\n' > "$instance_dir/tracing_on"
    cat "$instance_dir/trace" > "$result_dir/trace.txt"
    for stats in "$instance_dir"/per_cpu/cpu*/stats; do
        test -f "$stats" || continue
        printf '%s\n' "$stats" >> "$result_dir/buffer_stats.txt"
        cat "$stats" >> "$result_dir/buffer_stats.txt"
    done
    printf '结果保存在：%s\n' "$result_dir"
)
```

具体事件取决于内核配置。只有部分事件存在时，输出只能支持该部分路径，不能称完整诊断；PREEMPT_RCU还可按相同步骤增加 `rcu_preempt_task` 和 `rcu_unlock_preempted_task`。先读保存的format解释字段，再结合对象代次和GP序号关联；不要只按两个相近时间戳猜测同一对象。

固定版本的 `rcu_invoke_callback` 在普通callback调用前记录，表示即将调用，**不证明该函数已经返回**。batch结束事件描述一批处理结果，也不自动给出每个对象后续work的完成证明；编码释放、批量kfree及不同后端可能走其他事件。缓冲统计存在丢失/覆盖时，应缩短采集或调整测试，再重新判断证据是否完整。本轮没有在目标tracefs运行这段采集流程。

### 24.5.5\_D4\_根据状态链选择动态检查器

| 工具或配置 | 能帮助发现 |
| --- | --- |
| `CONFIG_PROVE_RCU` / lockdep | 错误读侧上下文、可疑 dereference 条件、部分非法同步等待 |
| Sparse `make C=1/C=2` | `__rcu` address-space 类型误用 |
| `CONFIG_DEBUG_OBJECTS_RCU_HEAD` | 同一 `rcu_head` 重复排队等回调对象错误 |
| KASAN | 测试实际覆盖到的 UAF/越界 |
| KCSAN | 测试实际覆盖到的数据竞争 |
| rcutorture | RCU 实现、配置和异常交错压力；不理解业务对象所有权 |

rcutorture 的实现位于 `kernel/rcu/rcutorture.c`，自动化脚本位于 `tools/testing/selftests/rcutorture/`。它适合验证内核 RCU 子系统和配置组合，不替代驱动专用的删除、卸载和引用逃逸测试。

## 24.6\_组合机制的职责矩阵

| 机制 | 它解决什么 | 它不解决什么 |
| --- | --- | --- |
| RCU | 发布/取得、短读侧生命期、GP 后回收 | 多写者互斥、任意字段一致性、读区外生命期 |
| spinlock/mutex | 更新串行化和复合不变量 | 旧裸读者的延迟回收 |
| kref/refcount | 已成功取得的长期所有权 | 从共享入口到安全 get 之间的竞态窗口 |
| workqueue/timer | 把动作安排到未来上下文 | 被保存对象和模块 text 的生命期 |
| devres | 设备解绑时的资源托管 | RCU GP、回调执行、硬件停止顺序 |
| `rcu_barrier()` | 等调用前已经排队的对应普通 RCU 回调执行 | 新回调、其他 flavor、没有排队成回调的旧读者用途 |

## 24.7\_交付前故障注入核对表

| 操作 | 期望观察 |
| --- | --- |
| 读者已取得旧指针，更新者随后撤下 | 旧读者在保护内继续安全使用；新查找按发布/取得协议读取合法版本 |
| 在读者尚未进入读区前延迟它 | 它不因未来的读取而成为当前旧读者；GP可在其他条件满足后完成 |
| 读者已进入本轮相关旧读区，但尚未dereference | 仍不能仅凭“没取指针”跳过其退出条件；与上一行区别在进入时刻 |
| 在本轮相关 PREEMPT_RCU 读侧内强制抢占 | CPU QS与blocked task债务分离，旧任务退出证据仍必须取得 |
| 延迟回调执行而非 GP | 对象不提前释放；能区分“GP完成”和“回调执行” |
| 在模块退出边界并发排队回调 | producer 先停止，barrier 后不再出现新模块回调 |
| 对复合 root 复用/替换部分 block | 旧 root 的 GP 前，各 block 的版本引用不被提前归还 |

这些条目是需要测试设计兑现的观察目标，不是本轮已执行结果。复核时先写预计事件序列，再标出实际抓到的事件、缺失区间和检查器状态。比如只出现invoke而没有callback返回证据，应暂时定位到“开始执行或正在执行”，不能报告“回收全部完成”；另一个对象复用了同一地址，也不能当作前一个对象复活。

上一篇：[RCU 类型语义、Sparse 与 Lockdep](P23_RCU_类型语义_Sparse与Lockdep.md)。

下一篇：[RCU 内存序、误用与选择边界](P25_RCU_内存序_误用与选择边界.md)。
