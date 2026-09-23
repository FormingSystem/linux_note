---
id: knowledge.linux.object_lifetime.kref.p06_release_回调与复杂销毁模式
title: "release 回调与复杂销毁模式"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
---

# 第6章\_release\_回调与复杂销毁模式

## 6.1\_本章主线

上一章已经能判断一次 get 是否成立、一次 put 是否触发回调，以及回调是否接过调用者的锁。现在给对象加上一项实际工作：它接受请求，把一项计算交给 worker，关闭时取消尚未开始的工作，等待已经开始的工作退出。此时只有一句“最后 put 后 kfree”还不够：**谁让 worker 停下来，等待期间又由谁保留对象？**

先保留一个有用的最小结论：普通 kref 的正常最后归还，在当前调用栈上调用类型指定的 release。然后补它未负责的部分。kref 不知道对象里的 work、timer、注册关系和子资源；若这些访问者没有纳入引用或借用协议，计数归零不会自动让它们消失。

本章讨论 **内嵌裸 kref 的私有对象**。后面的 `owned_job` 有自己的名称、工作和关闭协议，不是 `struct device`、`struct class` 或 `struct bus_type`。私有对象持有一个 device 引用时，应按该框架的接口归还；不能把私有对象的 release 当作 driver core 的 release 分发，也不能直接释放 device 的外壳。框架边界留给后续专章。

我们先完成一个没有全局查找入口、没有硬件、只有一个关闭管理者的实例，再逐项加入可见性、执行上下文、timer、注册回调和 RCU 的约束。读完本章，应能沿一次关闭过程指出：哪个动作禁止新访问，哪个动作等待旧访问，哪个动作归还责任，哪个动作最终回收存储。它们可能相邻，但不是同一件事。

## 6.2\_先看完整模板\_release\_只是最后一站

第 1 章已经用“每次成功交付拥有一份”的方式保留工作对象。现在换一个确有用途的设计：一个管理者长期拥有对象，worker 只在管理者规定的活动期里借用它。关闭者可以睡眠，也负责等待所有借用结束。这样不用给每次工作都追加引用，但管理者不能提早退出，也不能在自己的 worker 里同步等待自己。

先预测下面两种顺序。若关闭时工作还在队列中，它被取消，计算次数为 0；若 worker 已经抢先执行，关闭者等它完成，计算次数为 1。**两种结果都可以正确关闭**，因为本例约定关闭可以丢弃尚未开始的工作，并没有保证每个已接受请求都必须完成。若业务要求请求必达，应另选等待完成而不丢弃请求的协议。

### 6.2.1\_状态保存在谁那里

这里有三组相互约束的状态，不是一个计数就能表达的单一状态机：

- `job->ref.refcount.refs.counter` 保存管理者和独立调用者的份额；worker 不占其中一份。
- `job->stopping` 由关闭者在 `job->gate` 下写入，提交者在同一锁下读取。检查通过与 `queue_work()` 必须位于同一个锁窗口，否则关闭者可能在二者之间排空队列，提交者随后又排入旧对象。
- `job->work` 的排队/执行状态由 workqueue 管理；`job->completed` 是本例业务结果，由 worker 在 `gate` 下修改，关闭等待完成后才由仍持引用的观察者读取。结果字段不是“work 已经退出”的同步标志。

```mermaid
flowchart LR
    M["管理者：持初始份额"] -->|"gate 内设置 stopping"| G["job 内：gate 与 stopping"]
    U["调用者：持独立份额"] -->|"gate 内检查并提交"| G
    G -->|"仍开放时 queue_work"| Q["队列管理 job.work 的排队和执行"]
    Q -->|"调用 work.fn，传递嵌入成员地址"| W["worker：借用 job"]
    W -->|"gate 内增加 completed"| V["job 内：业务结果"]
    M -->|"锁外 cancel_work_sync 等待退出"| Q
    M -->|"等待完成后归还初始份额"| R["job.ref：普通引用状态"]
    U -->|"结束观察后归还独立份额"| R
    R -->|"最后 put 直接调用"| F["release：清理 name 与外壳"]
```

这张图中，停止状态通过共享字段和 mutex 传播，执行结束通过 workqueue 的同步取消返回传给管理者。kref 没有轮询 `completed`，也不会发送“请停止工作”的通知。同步取消内部的等待协议可沿[工作队列源码总索引](../../../../research/source_reading/workqueue/navigation/P01_Linux_6.12_工作队列源码总阅读索引.md#1.6_建议阅读顺序)进入；本章只组合它已提供的接口保证。

### 6.2.2\_运行一个由管理者等待借用退出的模块

下面是完整的 [note_kref_owned_work.c](../../../../labs/kernel/object_lifetime/materials/note_kref_owned_work.c)。成功创建才建立初始引用；三个资源申请中的任何一步失败，都沿已经取得的资源逆序清理，不对尚未初始化的 kref 执行 put。`kzalloc` 取得清零的外壳，`kstrdup` 复制并拥有名称，`INIT_WORK` 将嵌入工作项连接到回调；`GFP_KERNEL` 沿用前章可睡眠创建路径的分配约束。错误常量中，ENOMEM 表示资源申请失败，EBUSY 表示本次未追加工作，ESHUTDOWN 表示入口关闭，EIO 用于报告本例预期之外的结果。

```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct owned_job {
    char *name;
    struct kref ref;
    struct mutex gate;
    struct work_struct work;
    struct workqueue_struct *queue;
    bool stopping;
    unsigned int completed;
};

static unsigned int release_calls; /* 本模块的初始化过程串行读取统计。 */

static void owned_release(struct kref *ref)
{
    struct owned_job *job = container_of(ref, struct owned_job, ref);
    ++release_calls;
    kfree(job->name);
    kfree(job);
}

static void owned_put(struct owned_job *job)
{
    kref_put(&job->ref, owned_release);
}

static void owned_worker(struct work_struct *work)
{
    struct owned_job *job = container_of(work, struct owned_job, work);
    /* 借用由管理者保持到 cancel 返回；worker 没有自己的一份可 put。 */
    mutex_lock(&job->gate);
    ++job->completed;
    mutex_unlock(&job->gate);
}

static struct owned_job *owned_create(void)
{
    struct owned_job *job = kzalloc(sizeof(*job), GFP_KERNEL);
    if (!job)
        return NULL;
    job->name = kstrdup("managed-work", GFP_KERNEL);
    if (!job->name)
        goto free_job;
    job->queue = alloc_ordered_workqueue("note_owned", 0);
    if (!job->queue)
        goto free_name;
    mutex_init(&job->gate);
    INIT_WORK(&job->work, owned_worker);
    kref_init(&job->ref); /* 所有资源就绪后，才建立管理者的初始份额。 */
    return job;

free_name:
    kfree(job->name);
free_job:
    kfree(job);
    return NULL;
}

/* 调用者持独立引用。停止检查与实际排队必须处于同一个锁窗口。 */
static int owned_request(struct owned_job *job)
{
    int result;
    mutex_lock(&job->gate);
    if (job->stopping)
        result = -ESHUTDOWN;
    else
        result = queue_work(job->queue, &job->work) ? 0 : -EBUSY;
    mutex_unlock(&job->gate);
    return result;
}

/* 仅管理者调用一次；消耗初始份额。必须可睡眠且不能从本 work 调用。 */
static void owned_close(struct owned_job *job)
{
    struct workqueue_struct *queue;
    mutex_lock(&job->gate);
    job->stopping = true;
    queue = job->queue;
    mutex_unlock(&job->gate);

    /* 先关入口，再在锁外等 worker；无重排和其他生产者。 */
    cancel_work_sync(&job->work);
    destroy_workqueue(queue);
    mutex_lock(&job->gate);
    job->queue = NULL;
    mutex_unlock(&job->gate);
    owned_put(job); /* 此后关闭者不再访问 job。 */
}

static int __init note_owned_init(void)
{
    struct owned_job *job = owned_create();
    int submitted, after_close;
    if (!job)
        return -ENOMEM;

    kref_get(&job->ref); /* 模拟一个调用者，关闭后仍须归还这一份。 */
    submitted = owned_request(job);
    owned_close(job); /* 此后只凭调用者份额保留对象。 */
    after_close = owned_request(job);
    /* 已无 worker 写 completed；调用者的一份保护 name 和外壳。 */
    pr_info("note_owned: %s submit=%d closed=%d completed=%u\n",
            job->name, submitted, after_close, job->completed);
    owned_put(job);
    return submitted ? submitted : after_close == -ESHUTDOWN ? 0 : -EIO;
}

static void __exit note_owned_exit(void)
{
    pr_info("note_owned: release=%u\n", release_calls);
}

module_init(note_owned_init);
module_exit(note_owned_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("管理者保活并等待借用工作退出的完整实验");
```

`owned_request()` 的调用者必须已经有引用。它返回 0 只表示本次排队被接收，`-EBUSY` 表示这次没有追加一次执行，`-ESHUTDOWN` 表示已关闭；三者都不转交调用者自己的份额。worker 始终借用管理者保护的存储，所以没有“取消成功需要补 put”的票据。本例不能与每次工作持一份的模型混用。

`owned_close()` 只由初始份额的管理者调用一次。它先关闭提交窗口，再退出 `gate`，随后同步取消工作、销毁私有队列，最后归还管理者份额。如果仍持 `gate` 等待，worker 正需要该锁来结束计算，就会形成“关闭者等 worker，worker 等关闭者解锁”的循环；把等待放在锁外正是为了解开这条依赖。

模块中没有导出接口或其他生产者；worker 不重新投递。它的完整关闭发生在初始化返回前，卸载时只输出对象外的统计。把这些函数接到实际驱动时，还要建立外部调用者和模块代码的进入/退出协议，不能只复制这个 `module_exit()`。

在与运行内核匹配、已经准备好的可写构建环境中执行：

```bash
# KDIR 由当前构建环境设置，指向匹配目标内核的构建目录。
make -C "$KDIR" M="$PWD/labs/kernel/object_lifetime/materials" modules
sudo insmod labs/kernel/object_lifetime/materials/note_kref_owned_work.ko
sudo rmmod note_kref_owned_work
dmesg | tail -n 12
```

观察 `note_owned` 日志：`submit=0`，`closed=-108`（该固定内核的 `ESHUTDOWN`），`completed` 为 0 或 1，卸载输出 `release=1`。前两项说明提交与关闭后的入口行为，最后一项说明这个同步演示只回收一次；一次日志没有覆盖所有实际竞态。若编译或装载失败，先保留错误并核对构建目录、架构和运行版本，不能把宿主检查当作已成功装载。

本轮已完成 ARM 前端语法检查和宿主九组控制路径检查，含三处分配失败、两种模块顺序、关闭窗口中的再提交拒绝、重复工作及管理者最后退出。宿主的锁、原子和队列是显式顺序替身，没有真实线程；目标构建链接、装卸、调度竞争和内存序未验证。命令是读者复现实验的步骤，不是本轮执行报告。

### 6.2.3\_沿一轮关闭追踪到最后清理

本章用 S0～S5 标记这个具体对象的一轮过程；它细化的是本例的管理协议，不是内核字段中另存的一套枚举。

| 阶段 | 触发与写入 | 谁继续持有或读取 | 退出条件 |
| --- | --- | --- | --- |
| S0 准备 | 创建者取得外壳、name、queue，初始化 gate/work/ref | 管理者持初始一份 | 成功返回；失败只清理已取得资源 |
| S1 取得调用者份额 | 管理者在有效正引用保护下 get | 调用者取得第二份 | 责任交付完成 |
| S2 接受工作 | 调用者在 gate 下检查 stopping，向队列发布 work | worker 借用对象；管理者仍持初始一份 | 拒绝，或一次工作进入队列 |
| S3 关闭提交 | 管理者在 gate 下写 stopping=true | 后来的提交者在同锁下读到关闭并拒绝 | 退出 gate，此后无新的成功提交 |
| S4 排空借用 | 管理者在锁外 cancel；必要时等 worker 返回；销毁 queue 并置空 | worker 在等待完成前仍由管理者保活 | 已无排队或执行，私有队列退出 |
| S5 归还与清理 | 管理者 put，独立调用者结束后也 put | 最后归还者直接进入 release | name 和外壳各释放一次 |

```mermaid
sequenceDiagram
    autonumber
    participant U as 调用者
    participant M as 管理者
    participant J as job 状态
    participant W as workqueue 与 worker
    U->>J: S1 已取得独立份额
    U->>J: S2 gate 内检查开放
    U->>W: gate 内提交 job.work
    M->>J: S3 gate 内写 stopping=true 后解锁
    U->>J: 再提交读取 stopping，返回 ESHUTDOWN
    M->>W: S4 锁外 cancel_work_sync
    alt 工作尚未开始且被取消
        W-->>M: 移除 pending，返回 true
    else 工作抢先执行或已经完成
        W->>J: gate 内增加 completed 后退出
        W-->>M: 确认执行结束，返回 false
    end
    M->>W: 销毁已排空的私有队列
    M->>J: S5 put 管理者份额，仍有调用者一份
    U->>J: 读结果后 put 最后一份
    J-->>U: 同步调用 release，回收资源
```

在这个单次投递演示里，取消返回 true/false 可对应图中两支；一般 work 重排场景不能只凭这个布尔值重建整段执行历史。真正承担对象安全保证的是 **先封闭全部生产者，再等待没有旧执行者**，不是把 true 读成“安全”、false 读成“失败”。固定版本的[同步取消实现](../../../../research/source_reading/workqueue/source_explanations/P03_Linux_6.12_worker_flush与取消源码实现.md#3.5_cancel_work_sync撤销与等待)明确写出不存在竞争投递这一前提。

试着在纸上改两处代码：第一，把 `owned_put(job)` 从 S5 移到 S4 等待之前，且让管理者成为唯一持有者；此时尚在执行的 worker 会拿到已回收对象。第二，让 `owned_request()` 解锁后才排队；此时它可能在 S4 返回后重新发布 work。前者缺的是存储保留，后者缺的是关闭和发布的串行化，单独增加一个停止标志不能同时修复两者。

## 6.3\_release\_的责任域和非责任域

回到完整程序，release 只有三件事：从嵌入成员找回对象、释放拥有的名称、释放外壳。简短是前面协议成立的结果，而不是把复杂问题藏在“都已经停了”这句话后面：S3 给出不再接受工作的证据，S4 给出借用者已经退出的证据，S5 才让管理者离开。若省掉任一证明，回调再短也可能释放得太早。

它与第 1 章的持票 worker 有意不同。持票模型允许创建者在交付后先退出，代价是接收、拒绝、完成、取消都要明确归还对应份额；本章模型让 worker 不做 get/put，代价是单一管理者必须活到排空结束且能合法等待。需要独立异步活动继续存在时选择前者；明确由上层关闭者统一收敛的活动可以选择后者。不要为了减少一对原子操作，就删除上层并不存在的等待保证。

也不能把本例扩成“所有对象进入 release 以前一定已经不可见”的定理。上一章的非拥有索引由[最后回调接锁撤下入口](P05_基础_API_源码逐行讲解.md#5.8.2_kref_put_mutex%28%29_的典型用途)，它同样正确；本例则根本没有全局查找入口。哪些动作放在 release，取决于入口与计数的连接方式、执行上下文以及谁等待谁。

一般应把必须主动启动的业务关闭交给仍持有效责任的管理者，把最后资源归还交给 release。若一个注册关系自己拥有一份，却期望 release 先注销它，计数就会因为那一份始终不能归零；反过来，若注册回调没有任何保活依据，归零后仍能进来的回调会访问已释放存储。两种错误一边造成无法退出，一边造成过早退出，不能用同一条“在 release 加 unregister”同时修好。

release 可以选择延迟最终存储回收，也可以按已证明的协议完成脱链；它不能把归零对象重新当作有业务引用的对象发布。诊断能帮助发现违约，不能建立原来没有的同步。后续小节按这三个问题展开：外部入口怎样撤销、最后回调处于什么上下文、延迟访问者怎样退出。

## 6.4\_release\_的触发条件和基本释放范围

现在区分触发条件和清理清单。触发由引用协议提供；清单由对象的资源取得方式提供。两者都需要正确，但不能互相替代。

### 6.4.1\_release\_的触发条件

在正常、未损坏且每份责任恰好归还一次的 kref 周期里，最终归还将计数从 1 减到 0，随后调用本次 put 指定的回调。锁组合还会按上一章的协议交出锁。计数异常进入饱和告警等分支时，不能继续套用这条正常结论；[普通源码导读](../../../../research/source_reading/kref/navigation/P02_普通引用与归零回调导读.md#2.4_正常退出与异常收敛)将两种情况分开。

“没有剩余引用”不等于“世界上没有任何裸指针”。本例依 S4 证明 worker 已不再借用；RCU 对象则可能仍有旧读者处于受保护的借用区间，因而还不能回收其存储。release 自己也正通过裸地址做清理。正确要求是：**不能再把零计数身份当作活的业务引用继续取得；仍可能访问的地址必须有另一项尚未结束的存储保护依据。**

因此，禁止在普通 release 中用 get 或重新 init 把同一个已归零身份“救活”。如果释放策略需要把清理工作交给另一个执行者，传递的是明确保留存储的清理责任，要另行证明交付、失败和执行上下文；不是悄悄恢复旧业务对象的引用。对象池重新构造一个新身份也需要旧访问者已退出的独立前提。

### 6.4.2\_release\_负责释放什么

`owned_job` 的资源不是同时退出的。私有队列会执行借用对象的代码，必须先在 S4 停止和销毁；名称仍可被独立调用者观察，所以保留到 S5。外壳最后回收，因为前面所有成员地址都落在它里面。

这也说明“先释放子资源，再释放外壳”只有在依赖关系允许时才完整。若子资源的退出会回调父对象，就要把父对象保留到该回调结束；若独立调用者在关闭后仍允许读取某个缓冲区，就不能提前释放它。不能把所有指针放进一个统一的 kfree 列表，也不能把依赖关系全凭字段声明顺序决定。

| 本例资源 | 取得点 | 使用窗口 | 对应退出动作 |
| --- | --- | --- | --- |
| job 外壳 | `kzalloc` 成功 | 创建、提交、worker、关闭及独立调用者 | release 最后 `kfree(job)` |
| name | `kstrdup` 成功 | 对象创建后直到最后观察者退出 | release 先 `kfree(job->name)` |
| queue | 创建私有有序队列成功 | 允许提交直到同步取消返回 | S4 `destroy_workqueue`，随后成员置空 |
| 调用者份额 | 正常 get | 关闭后的拒绝与结果观察仍有效 | 调用者自行 put，不由关闭者代还 |

创建失败发生在 S0，尚未取得的资源没有退出动作；成功创建后的关闭发生在 S3～S5，管理者不能跳过等待直接执行与失败标签相同的 kfree。两个路径都“释放内存”，成立前提却不同。

### 6.4.3\_release\_只释放对象\_拥有\_的资源

字段类型本身不足以决定归属。`const char *name` 可能指向静态字符串，也可能指向另一个拥有者管理的内存；`char *name` 也不会自动宣布对象获得释放权。把它写进结构，只保存了一个地址。

| 取得与保存方式 | 对象实际承担的责任 | 退出时的方向 |
| --- | --- | --- |
| 借用静态字符串 | 不拥有该字符串存储 | 不对字符串 kfree |
| 私有 `kstrdup` / `kmalloc` 成功 | 拥有这一块分配 | 无使用者后按分配配对 kfree |
| 明确取得 file 的一份引用 | 归还当前对象持有的文件引用 | 按 file 契约执行 fput，不直接 kfree file |
| 明确取得 device 的一份引用 | 归还当前对象持有的设备引用 | 按 device 契约执行 put_device，不替换设备最终回调 |
| 只借用另一对象的成员地址 | 在对方承诺的有效窗口内使用 | 结束借用；不能凭地址擅自 put 或 free 对方 |

表中的“归还引用”不承诺被引用对象立即销毁，也不承诺任何上下文都能执行整个清理链。应分别核对该类型的空值约定、最后归还行为、回调与睡眠约束，不能把它们套用本例只清理 kmalloc 内存的回调。

练习：把完整程序的 `name` 改为借用字符串常量，要一起改变哪些地方？至少要改创建时的取得方式、申请失败出口和 release 的资源清单；若只改赋值却保留 `kfree(job->name)`，就把原本不拥有的存储当成了私有分配。再把名称改为借用另一个动态对象的成员：这一次除了清理清单，还必须增加对那个拥有者寿命的证明。


## 6.5\_外部可见性\_脱链应该由谁负责

对象如果还能从全局结构或外部子系统找到，release 就很容易变成悬挂指针制造点。这里先讲可见性撤销的责任边界。

### 6.5.1\_release\_前必须明确对象是否已经脱链

如果对象挂在全局结构里，例如：

```c
struct my_refobj {
	struct kref ref;
	struct list_head node;
	int id;
};
```

那么 release 时必须回答：

```text
对象是否还挂在 list/hash/xarray/idr 中？
```

如果对象已经释放，但全局结构里还留着指针，就会产生悬挂指针。

错误模型：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	kfree(refobj);
}
```

如果 `refobj->node` 还在链表里，那么链表中留下的节点就指向已经释放的内存。

后续 lookup 可能拿到一个已经释放的对象。

这类 bug 非常危险。


### 6.5.2\_release\_前脱链模型

一种常见设计是：

```text
release 之前必须已经从全局结构删除。
```

release 里只做检查：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	WARN_ON(!list_empty(&refobj->node));

	kfree(refobj);
}
```

删除路径负责脱链：

```c
static void my_refobj_remove(struct my_refobj *refobj)
{
	mutex_lock(&refobj_list_lock);
	list_del_init(&refobj->node);
	mutex_unlock(&refobj_list_lock);

	my_refobj_put(refobj);
}
```

这个模型的优点是：

```text
全局可见性撤销和最终释放分开；
release 只验证对象已经不可被 lookup；
代码边界清晰。
```

这里的生命周期顺序是：

```text
从全局结构删除
禁止新 lookup
释放管理者引用
等待已有引用自然归零
最后 release
```

这也是很多对象管理场景的常见模型。


### 6.5.3\_release\_内脱链模型

另一种设计是：

```text
最后一个 put 时，在 release 里完成脱链。
```

例如：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	list_del_init(&refobj->node);
	kfree(refobj);
}
```

这个模型必须满足额外条件：

```text
release 执行时必须持有保护 list 的锁；
不会有并发 lookup 正在无保护遍历；
不会出现重复 list_del；
锁状态必须明确。
```

所以它通常要配合：

```c
kref_put_mutex()
kref_put_lock()
```

或者调用者在最后 put 前已经持锁。

这种模型难度更高，因为 release 不再只是释放资源，还参与集合关系修改。

本章只建立边界：

```text
release 可以脱链，但必须有锁语义保证。
```

具体模板放到第 9 章展开。

------

## 6.6\_release\_的执行上下文和锁语义

release 在哪里执行，取决于最后一个 put 发生在哪里。上下文不清楚，复杂 release 就没有安全基础。

### 6.6.1\_release\_能否睡眠取决于最后\_put\_上下文

release 是否能睡眠，取决于它被什么上下文调用。

普通 `kref_put()` 可能出现在多种上下文：

```text
进程上下文
workqueue 上下文
中断上下文
软中断上下文
spinlock 持有状态
mutex 持有状态
RCU 读侧或更新侧路径
```

如果 release 里调用可能睡眠的函数，例如：

```c
cancel_work_sync(&refobj->work);
mutex_lock(&refobj->lock);
flush_workqueue(wq);
wait_for_completion(&refobj->done);
msleep(10);
```

那就必须保证：

```text
最后一个 kref_put() 不会发生在不能睡眠的上下文。
```

否则就可能出现：

```text
sleeping function called from invalid context
死锁
调度错误
锁依赖告警
```

所以 release 设计前必须先问：

```text
最后一个 put 可能在哪里发生？
```

这比 release 代码本身更重要。


### 6.6.2\_普通\_kref\_put\_下的\_release\_上下文

普通 `kref_put()` 不改变当前上下文。

也就是说：

```c
kref_put(&refobj->ref, my_refobj_release);
```

如果最后引用在进程上下文释放，那么 release 在进程上下文执行。

如果最后引用在中断上下文释放，那么 release 就在中断上下文执行。

如果最后引用在 spinlock 持有期间释放，那么 release 就在 spinlock 持有期间执行。

所以普通 `kref_put()` 的 release 不能假设：

```text
一定可以睡眠
一定没有锁持有
一定在进程上下文
一定可以调用复杂清理函数
```

release 是否能做复杂清理，取决于对象设计是否保证：

```text
最后一个 put 只会在允许该 release 行为的上下文发生。
```

如果不能保证，就要改变设计。

常见做法包括：

```text
把最后 put 限制在进程上下文
把复杂释放转移到 workqueue
把内存释放改成 RCU 延迟释放
避免 release 中调用可能睡眠的函数
```


### 6.6.3\_release\_在持\_mutex\_状态下执行

如果使用：

```c
kref_put_mutex(&refobj->ref, my_refobj_release, &refobj_lock);
```

那么最后一个 put 时，release 会在持有 `refobj_lock` 的状态下执行。

这意味着 release 必须知道：

```text
进入 release 时 mutex 已经被持有。
```

所以 release 里不能再无脑：

```c
mutex_lock(&refobj_lock);
```

否则可能死锁。

示例：

```c
static void my_refobj_release_locked(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	/* 这里假设 refobj_list_lock 已经持有 */
	list_del_init(&refobj->node);

	mutex_unlock(&refobj_list_lock);

	kfree(refobj);
}
```

这种模式非常敏感。

它有几个风险：

```text
release 负责解锁，调用点不直观；
锁平衡容易被后续维护破坏；
release 名字必须体现 locked 语义；
release 中不能随便调用会再次拿同一把锁的函数。
```

所以工程上建议：

```text
如果 release 依赖锁状态，函数名和注释必须明确。
```

例如：

```c
/*
 * Called with refobj_list_lock held.
 * Drops refobj_list_lock before returning.
 */
static void my_refobj_release_locked(struct kref *ref)
{
	...
}
```


### 6.6.4\_release\_在持\_spinlock\_状态下执行

如果使用：

```c
kref_put_lock(&refobj->ref, my_refobj_release, &refobj_lock);
```

最后一个 put 时，release 会在持有 spinlock 的状态下执行。

这比 mutex 版本更严格。

release 中不能调用可能睡眠的函数。

错误示例：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	cancel_work_sync(&refobj->work);   /* 错：可能睡眠 */
	kfree(refobj);
}
```

如果这个 release 被 `kref_put_lock()` 调用，就可能出问题。

spinlock 下 release 更适合做短小动作：

```text
从链表删除
标记状态
解除简单集合关系
释放不睡眠的资源
```

如果销毁流程复杂，常见做法是：

```text
持 spinlock 完成脱链；
释放 spinlock；
再在安全上下文中释放复杂资源。
```

或者把对象放到延迟释放队列，由 workqueue 执行真正释放。

------

## 6.7\_异步路径\_work\_timer\_callback\_的引用闭环

异步路径的问题通常不是 release 代码本身，而是 work/timer/callback 是否拥有引用、谁负责取消、谁负责 put 没有定义清楚。

### 6.7.1\_release\_和\_workqueue\_的收尾关系

如果对象里有 `work_struct`：

```c
struct my_refobj {
	struct kref ref;
	struct work_struct work;
};
```

必须明确：

```text
work 是否持有对象引用？
release 时 work 是否还可能运行？
release 是否需要 cancel_work_sync？
```

常见安全模型之一：

```text
投递 work 前 kref_get；
work 函数结束时 kref_put；
release 不需要 cancel_work_sync 保护 work 对对象的访问。
```

示例：

```c
static int my_refobj_queue_work(struct my_refobj *refobj)
{
	kref_get(&refobj->ref);

	if (!queue_work(system_wq, &refobj->work)) {
		my_refobj_put(refobj);
		return -EBUSY;
	}

	return 0;
}

static void my_refobj_workfn(struct work_struct *work)
{
	struct my_refobj *refobj = container_of(work, struct my_refobj, work);

	/* 使用 refobj */

	my_refobj_put(refobj);
}
```

这个模型里，work 自己持有引用。

所以只要 work 还没结束，对象就不会 release。


### 6.7.2\_release\_中\_cancel\_work\_sync\_的风险

有些设计会在 release 里取消 work：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	cancel_work_sync(&refobj->work);
	kfree(refobj);
}
```

这要求非常谨慎。

原因有两个。

第一，`cancel_work_sync()` 可能睡眠。

所以 release 必须保证在可睡眠上下文执行。

第二，如果 work 本身持有引用，并且 work 结束时才 put，那么 release 一般不会在 work 还持有引用时发生。

也就是说：

```text
如果 work 持有引用，release 发生时 work 理论上已经不再持有引用。
```

这时 release 里再 `cancel_work_sync()` 的意义需要重新审视。

更危险的是互相等待模型：

```text
work 等待某个引用释放
release 等待 work 结束
```

可能构成死锁。

所以 work 模型要二选一并写清楚：

```text
模型 A：work 持有引用，work 完成后 put，release 不负责等待 work。
模型 B：对象 owner 管理 work 生命周期，release 前已经保证 work 不再运行。
```

不要让 release 和 work 引用关系相互纠缠。


### 6.7.3\_release\_和\_timer\_的收尾关系

timer 比 work 更容易出错，因为 timer 回调可能在软中断上下文运行。

如果对象里有 timer：

```c
struct my_refobj {
	struct kref ref;
	struct timer_list timer;
};
```

必须明确：

```text
timer 回调是否持有引用？
timer 删除发生在哪个阶段？
release 能不能调用 del_timer_sync？
最后 put 可能是否发生在 timer 回调中？
```

一种常见模型：

```text
启动 timer 前增加引用；
timer 回调执行完释放引用；
取消 timer 成功时释放 timer 引用。
```

示例模型：

```c
static void my_refobj_start_timer(struct my_refobj *refobj)
{
	kref_get(&refobj->ref);
	mod_timer(&refobj->timer, jiffies + HZ);
}
```

timer 回调：

```c
static void my_timer_fn(struct timer_list *t)
{
	struct my_refobj *refobj = from_timer(refobj, t, timer);

	/* 使用 refobj */

	my_refobj_put(refobj);
}
```

取消路径必须处理：

```text
如果 timer 被成功取消，那么 timer 回调不会执行；
因此原本给 timer 的引用要由取消路径 put。
```

示意：

```c
static void my_refobj_cancel_timer(struct my_refobj *refobj)
{
	if (del_timer_sync(&refobj->timer))
		my_refobj_put(refobj);
}
```

这里的核心是：

```text
timer 引用必须有唯一释放者：
要么 timer 回调释放；
要么取消成功路径释放。
```

否则会少 put 或多 put。


### 6.7.4\_release\_不应该负责模糊的\_timer\_语义

错误倾向：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	del_timer_sync(&refobj->timer);
	kfree(refobj);
}
```

这不是绝对错误，但很容易掩盖生命周期不清晰。

你必须回答：

```text
timer 启动时是否 get？
timer 回调是否 put？
release 发生时 timer 是否可能还持有引用？
del_timer_sync 如果返回 1，是否需要 put timer 引用？
如果 release 在 timer 回调中触发，会不会 del_timer_sync 自己等待自己？
```

如果这些问题答不清楚，就不应该把 timer 收尾简单塞进 release。

更清晰的设计是：

```text
timer 的引用归属在启动、取消、回调路径中闭环；
release 只检查 timer 已经不再活动，或只释放对象本体。
```


### 6.7.5\_release\_和\_callback\_的关系

对象经常注册给某个子系统回调：

```c
register_callback(refobj, my_callback);
```

这时必须定义：

```text
子系统是否持有 refobj 引用？
callback 执行期间对象如何保证不被释放？
unregister_callback 是否等待正在运行的 callback 结束？
```

一种安全模型：

```text
注册前 kref_get，引用属于 callback 注册关系；
unregister 成功后 kref_put；
callback 执行期间由注册关系保证对象存在。
```

示例：

```c
static int my_refobj_register(struct my_refobj *refobj)
{
	int ret;

	kref_get(&refobj->ref);

	ret = register_callback(refobj, my_callback);
	if (ret) {
		my_refobj_put(refobj);
		return ret;
	}

	return 0;
}

static void my_refobj_unregister(struct my_refobj *refobj)
{
	unregister_callback(refobj);
	my_refobj_put(refobj);
}
```

这里必须确认：

```text
unregister_callback 返回后，不会再有新的 callback 进入；
正在运行的 callback 是否已经退出，也必须由子系统语义保证。
```

如果 unregister 只是不再新增 callback，但不等待已有 callback，那么还需要额外同步机制。


### 6.7.6\_release\_不能替代\_unregister

不要把 unregister 全部推到 release 里。

错误倾向：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	unregister_callback(refobj);
	kfree(refobj);
}
```

这类代码可能有问题。

因为 release 发生时已经没有合法引用了。

但 callback 注册关系本身通常就应该是一个引用来源。

如果对象还注册在外部子系统中，说明外部子系统可能还能回调它。

这时引用计数怎么能已经归零？

所以更合理的模型通常是：

```text
unregister 阶段撤销外部可见性；
unregister 释放注册关系引用；
最后一个 put 才进入 release。
```

也就是：

```text
release 不负责让对象不可见；
release 只处理对象已经不可见之后的最终销毁。
```

当然某些内核子系统有自己的特殊规则，但通用原则是：

```text
外部注册关系应该在 release 前明确撤销。
```

------

## 6.8\_RCU\_边界\_生命周期结束不等于内存立刻回收

RCU 场景下，kref 生命周期可以结束，但对象内存可能还要撑过 grace period。

### 6.8.1\_release\_和\_RCU\_的边界

如果对象可以被 RCU 读侧看到，那么 release 里不能简单：

```c
kfree(refobj);
```

因为 RCU 读侧可能仍然持有旧裸指针。

典型模型是：

```text
更新侧先从 RCU 可见结构中删除对象；
禁止新读者找到它；
已有 RCU 读者可能仍在临界区中；
等 grace period 后才能释放内存。
```

所以 release 里可能需要：

```c
kfree_rcu(refobj, rcu);
```

而不是：

```c
kfree(refobj);
```

示例结构：

```c
struct my_refobj {
	struct kref ref;
	struct rcu_head rcu;
	struct hlist_node node;
};
```

release：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	kfree_rcu(refobj, rcu);
}
```

这里含义是：

```text
kref 生命周期已经结束；
但对象内存要等 RCU grace period 后再真正释放。
```


### 6.8.2\_kref\_归零和内存真正释放不是永远同一时刻

普通裸 kref 对象：

```text
last put -> release -> kfree -> 内存释放
```

RCU 对象：

```text
last put -> release -> kfree_rcu -> grace period 后内存释放
```

所以要区分两个概念：

```text
对象生命周期结束
对象内存真正归还
```

对于普通裸 kref 对象，这两者几乎连在一起。

对于 RCU 对象，它们中间隔着 grace period。

这不是说对象还能被使用。

对象生命周期已经结束。

只是为了保护 RCU 读侧旧指针，内存暂时不能回收。

所以 RCU 场景下要记住：

```text
refcount 到 0 后，对象不能再被 get 或重新发布；
但 struct kref 所在内存必须撑过 RCU grace period。
```

这个细节会在第 10 章专门展开。

------

## 6.9\_release\_的禁区\_检查和状态边界

这一组内容不是孤立错误清单，而是在说明 release 作为生命周期终点时，哪些动作已经太晚，哪些检查适合留下。

### 6.9.1\_release\_里不能重新发布对象

release 中最危险的错误之一是试图“复活对象”。

错误示例：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	kref_init(&refobj->ref);
	list_add(&refobj->node, &refobj_list);
}
```

这是错误的生命周期模型。

进入 release 说明：

```text
refcount 已经归零；
对象已经没有合法持有者；
对象正在销毁。
```

此时不能再：

```text
重新初始化 kref
重新加入 list/hash/xarray
重新注册 callback
重新投递 work
重新暴露给 lookup
```

如果需要对象池复用，也应该把“对象生命周期结束”和“内存块复用”分开。

例如：

```text
kref release 结束对象生命周期；
对象池管理内存块；
重新分配时创建一个新的对象生命周期。
```

不能在 release 里把同一个对象原地复活。


### 6.9.2\_release\_中的调试检查

复杂对象的 release 里适合放一些调试检查。

例如：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	WARN_ON(!list_empty(&refobj->node));
	WARN_ON(timer_pending(&refobj->timer));
	WARN_ON(refobj->registered);
	WARN_ON(refobj->running);

	kfree(refobj->buf);
	kfree(refobj);
}
```

这些检查的意义是：

```text
release 发生时，对象应该已经完成撤销和收尾。
```

常见检查项：

```text
是否已经从链表删除
是否已经从 hash/xarray 删除
timer 是否还 pending
work 是否还可能运行
callback 是否已经 unregister
状态是否已经进入 stopped/dead
子资源是否仍然持有
```

这些 WARN 不是为了替代正确逻辑，而是为了尽早暴露生命周期协议错误。


### 6.9.3\_release\_和对象状态

有些对象会有状态字段：

```c
enum my_refobj_state {
	my_refobj_INIT,
	my_refobj_RUNNING,
	my_refobj_STOPPING,
	my_refobj_DEAD,
};

struct my_refobj {
	struct kref ref;
	struct mutex lock;
	enum my_refobj_state state;
};
```

release 可以检查状态是否已经进入终态：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	WARN_ON(refobj->state != my_refobj_DEAD);

	kfree(refobj);
}
```

但是 release 不应该依赖复杂状态迁移。

例如不要把主要 stop 流程放进 release：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	my_refobj_stop_hardware(refobj);       /* 可能太晚，也可能上下文不对 */
	kfree(refobj);
}
```

更清晰的模型是：

```text
remove/stop 路径负责让对象停止工作；
release 只验证对象已经停止，并释放内存和剩余资源。
```

因为 release 发生的时机取决于最后一个引用，不一定是适合停硬件、关中断、等待线程的时机。


### 6.9.4\_release\_不应承担过多业务逻辑

release 的职责应该尽量收敛：

```text
释放对象拥有的资源；
执行最终一致性检查；
释放对象内存。
```

不建议在 release 里做大量业务动作，例如：

```text
重新配置硬件
发送复杂消息
等待远端响应
启动新任务
重新注册对象
执行复杂状态机迁移
```

原因是：

```text
release 的触发点由最后一个 put 决定；
最后一个 put 可能出现在你不期望的上下文；
release 越复杂，越难保证上下文、锁和错误路径正确。
```

更好的分层是：

```text
stop/remove 阶段处理业务停止；
unregister/unlink 阶段撤销外部可见性；
drain 阶段等待异步路径退出；
release 阶段做最终资源释放。
```

------

## 6.10\_推荐销毁阶段和完整示例

回到本章开头的完整模板：复杂对象应该把销毁拆成阶段，而不是把所有动作塞进 release。

### 6.10.1\_复杂对象销毁的推荐阶段

对于复杂对象，推荐把销毁拆成多个阶段，而不是全塞进 release。

典型阶段：

```text
1. stop：阻止对象继续产生新动作。
2. unregister：从外部子系统撤销注册。
3. unlink：从 list/hash/xarray 等全局结构删除。
4. drain：等待或取消 work/timer/callback。
5. put：释放管理者引用。
6. release：最后一个引用归零后释放对象资源。
```

示意流程：

```text
my_refobj_destroy()
   |
   +-- 设置 stopping 状态
   +-- unregister callback
   +-- del_timer_sync / cancel_work_sync
   +-- 从全局结构 unlink
   +-- my_refobj_put(manager ref)
          |
          +-- 若还有用户引用：等待后续 put
          |
          +-- 若最后引用：release
```

这个模型的优点是：

```text
对象先不可见；
异步路径先收敛；
已有引用自然退出；
最后 release 只做最终释放。
```


### 6.10.2\_一个复杂\_release\_示例

下面给一个相对合理的复杂对象模型。

对象：

```c
struct my_refobj {
	struct kref ref;
	struct mutex lock;
	struct list_head node;

	char *name;
	void *buffer;

	bool registered;
	bool stopping;
};
```

销毁前撤销：

```c
static void my_refobj_unregister(struct my_refobj *refobj)
{
	mutex_lock(&refobj->lock);
	refobj->stopping = true;
	mutex_unlock(&refobj->lock);

	if (refobj->registered) {
		unregister_callback(refobj);
		refobj->registered = false;
	}

	mutex_lock(&refobj_list_lock);
	if (!list_empty(&refobj->node))
		list_del_init(&refobj->node);
	mutex_unlock(&refobj_list_lock);

	my_refobj_put(refobj);
}
```

release：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	WARN_ON(refobj->registered);
	WARN_ON(!list_empty(&refobj->node));

	kfree(refobj->buffer);
	kfree(refobj->name);
	kfree(refobj);
}
```

这里 release 没有负责：

```text
unregister
unlink
stop
```

它只是检查这些动作已经完成，然后释放资源。

这是一种更容易维护的模型。


### 6.10.3\_release\_过度复杂的反例

反例：

```c
static void my_refobj_release(struct kref *ref)
{
	struct my_refobj *refobj = container_of(ref, struct my_refobj, ref);

	mutex_lock(&refobj->lock);
	refobj->stopping = true;
	mutex_unlock(&refobj->lock);

	unregister_callback(refobj);
	del_timer_sync(&refobj->timer);
	cancel_work_sync(&refobj->work);

	mutex_lock(&refobj_list_lock);
	list_del_init(&refobj->node);
	mutex_unlock(&refobj_list_lock);

	kfree(refobj->buffer);
	kfree(refobj);
}
```

这段代码看起来“完整清理”，但问题很多：

```text
release 可能在不能睡眠的上下文执行；
unregister_callback 是否可能等待未知；
del_timer_sync 是否可能和 timer 回调互等；
cancel_work_sync 是否可能和 work 引用互等；
release 中拿多个锁，锁顺序复杂；
对象到 release 时还 registered/linked，说明前面撤销阶段不清晰。
```

这种 release 不是绝对不能写，但必须有非常严格的上下文和锁证明。

普通工程中更建议把这些动作前移到 destroy/remove 阶段。

------

## 6.11\_命名\_注释和检查清单

最后把 release 的上下文要求写进函数名、注释和检查清单，方便以后代码审查。

### 6.11.1\_release\_函数命名建议

release 函数名最好表达对象类型和上下文。

普通 release：

```c
static void my_refobj_release(struct kref *ref)
```

如果 release 依赖锁状态：

```c
static void my_refobj_release_locked(struct kref *ref)
```

如果 release 使用 RCU 延迟释放：

```c
static void my_refobj_release_rcu(struct kref *ref)
```

如果 release 不能睡眠：

```c
static void my_refobj_release_atomic(struct kref *ref)
```

名字不是语义保证，但能提醒维护者：

```text
这个 release 不是普通 kfree 路径；
它有特殊上下文要求。
```

配合注释更清晰：

```c
/*
 * Called when the last reference is dropped.
 * The object must already be unlinked from refobj_list.
 * May sleep.
 */
static void my_refobj_release(struct kref *ref)
{
	...
}
```

或者：

```c
/*
 * Called with refobj_list_lock held.
 * Must drop refobj_list_lock before returning.
 * Must not sleep before dropping the lock.
 */
static void my_refobj_release_locked(struct kref *ref)
{
	...
}
```


### 6.11.2\_release\_注释应该写什么

复杂对象建议在 release 附近写清楚：

```text
1. release 是否可能睡眠。
2. release 是否要求对象已经脱链。
3. release 是否要求 callback 已经 unregister。
4. release 是否要求 work/timer 已经停止。
5. release 是否在持锁状态下调用。
6. release 是否释放锁。
7. release 最终是 kfree、kmem_cache_free 还是 kfree_rcu。
```

示例：

```c
/*
 * Lifetime:
 * - refobj_list holds the initial manager reference while linked.
 * - lookup obtains references under refobj_list_lock.
 * - work users take their own references before queue_work().
 * - remove unlinks the object, stops external callbacks, and drops
 *   the manager reference.
 *
 * Release:
 * - Called when the last reference is dropped.
 * - object must already be unlinked.
 * - No work or timer may still own a reference.
 * - May sleep.
 */
static void my_refobj_release(struct kref *ref)
{
	...
}
```

这类注释可以直接服务代码审查。


### 6.11.3\_release\_的最小检查清单

写 release 前，先回答下面问题。

#### (1)\_资源归属

```text
哪些字段由对象分配？
哪些字段只是借用？
哪些字段需要 put，而不是 kfree？
哪些字段是静态内存？
```

#### (2)\_外部可见性

```text
对象是否还在 list/hash/xarray/idr？
对象是否还注册在外部子系统？
对象是否还能被 callback 找到？
对象是否还能被 RCU 读侧看到？
```

#### (3)\_异步路径

```text
work 是否可能还在运行？
timer 是否可能还 pending？
callback 是否可能并发进入？
中断路径是否可能使用对象？
```

#### (4)\_上下文

```text
最后一个 put 可能发生在哪里？
release 是否可能睡眠？
release 是否可能在 spinlock 下执行？
release 是否会拿 mutex？
release 是否会调用等待函数？
```

#### (5)\_最终释放

```text
使用 kfree？
使用 kmem_cache_free？
使用 kfree_rcu？
是否需要先释放子资源？
是否有 WARN_ON 检查？
```

------

## 6.12\_本章小结

本章讲的是复杂 release 模式。

核心结论：

```text
release 是对象生命周期的最终收口点，不只是 kfree 包装函数。
```

release 里可以做：

```text
释放对象拥有的子资源；
检查对象是否已经脱链；
检查 work/timer/callback 是否已经收敛；
释放对象本体；
必要时使用 kfree_rcu 延迟释放内存。
```

release 里不应该做：

```text
重新发布对象；
重新初始化 kref；
盲目 unregister 外部关系；
无证明地等待 work/timer；
在 spinlock 下调用可能睡眠的函数；
释放并不属于对象的资源；
承载复杂业务状态机。
```

复杂对象更推荐把销毁拆成阶段：

```text
stop
unregister
unlink
drain
put
release
```

责任域可以压缩成这张表：

| 阶段 | 主要责任 | 不应该混进去的事 |
| --- | --- | --- |
| destroy/remove | 停止业务、撤销注册、脱链、收敛异步入口 | 不应该假装对象已经没有旧引用 |
| kref 引用计数 | 等所有持有者 put，找到最后一个引用释放点 | 不负责设备状态机和字段互斥 |
| release | 最终检查、释放对象拥有的资源、释放对象本体 | 不应该重新发布对象或承担复杂业务停止流程 |

本章最关键的一句话：

```text
release 应该发生在对象已经不可见、异步路径已经有明确引用归属、最后一个引用已经消失之后。
```

下一章进入：

```text
第 7 章：handoff 所有权转移模型
```

重点会从 release 转向另一类高频错误：

```text
对象指针交给 workqueue、timer、队列、callback 后，
这个引用到底归谁？
成功路径谁 put？
失败路径谁 put？
handoff 后当前路径还能不能访问？
```

------

专题导航：[kref 引用计数机制章节大纲](大纲.md)。

上一篇：[基础 API 源码逐行讲解](P05_基础_API_源码逐行讲解.md#5.15_本章小结)。

下一篇：[handoff 所有权转移模型](P07_handoff_所有权转移模型.md)。
