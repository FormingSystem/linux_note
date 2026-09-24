---
id: research.source_reading.memory_ordering.navigation.model_relations
title: "LKMM 公理、锁关系与验证边界"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
---

# 第9章\_LKMM公理\_锁关系与验证边界

本模块沿[总阅读索引](P01_Linux_6.12_LKMM_源码与模型导读.md#1.2_从源码接口到模型判定的完整链)进入，固定NXP官方linux-imx提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20。先完成[输入与事件分类](P08_LKMM输入_分类与工具配置.md#8.1_linux_kernel_def_把原语翻译成事件)，再追踪候选执行为何被公理接受或排除。

## 9.1\_linux\_kernel\_cat\_怎样组织公理

[`linux-kernel.cat`](../../linux/tools/memory-model/linux-kernel.cat) 包含或构造：

- 基础 `po/rf/co/fr` 及相关派生关系；
- acquire/release、fence、dependency、atomic 等顺序；
- happens-before（`hb`）；
- propagation（`prop`）与累积传播；
- RCU/SRCU 相关关系；
- coherence、atomic、hb、propagation、rcu 等一致性检查。

模型把“允许执行”定义为同时满足这些公理的关系图。研究时不要只查某个宏字符串，而要沿：事件标签 → 参与的关系 → 最终 acyclic/irreflexive 等约束追踪。

### 9.1.1\_先问候选执行违反了哪一种约束

前面已经给访问分类，现在还没有证明任何结果被禁止。cat中的关系名字是筛选和组合事件边的工具；只有走到实际检查点，才知道某张候选图为什么不能留下。先把四类检查放到不同问题上：

| 检查点 | 它追问的问题 | 不能据此宣称 |
| --- | --- | --- |
| coherence | 同一位置的程序顺序与rf/co/fr组合是否成环？ | 不同位置天然保持源码全序 |
| atomic | RMW的读写之间是否被外部写插入？ | 所有原子操作都有全屏障语义 |
| happens-before | 保留程序顺序、外部读取来源和特定传播边能否形成hb环？ | 任意po边或任意fr边都直接属于hb |
| propagation | 传播与强屏障组合形成的pb是否成环？ | 仅检查hb已经覆盖全部模型 |

这里rf表示读从哪次写取得值，co排列同一位置的写，fr从一次读连向其读取来源之后的写。它们描述候选执行，不是CPU运行时保存的日志。acyclic要求关系无环，empty要求筛出的关系没有边，irreflexive只排除事件到自身的边；三者不能都翻译成“没有循环”，否则会丢失检查的对象。

原子性检查尤其容易被名字误导：如果RMW的读先读到旧写，另一CPU的写位于其后，而RMW自己的写又在这次外部写之后，就在本应不可分割的读写之间插入了外部更新。atomic检查排除这种组合。它没有因此给该操作前后的所有其他访问添加顺序；原子性与内存顺序仍是两类保证。

### 9.1.2\_顺序与传播要沿端点追踪

先从acq-po和po-rel看端点：前者从取得访问走到其后的内存访问，后者从内存访问走到后面的发布访问。rmb、wmb和mb再按各自访问类别筛选屏障两端。并非整个po都被保留，尤其不能把控制依赖后的任意读取当作已有保证；ppo从依赖、覆盖关系、屏障和锁传递等分支选择实际保留的边。

跨参与者观察还需要传播关系。固定版本的cumul-fence不仅看本线程的两次访问，还把符合条件的外部读取来源纳入累积路径；prop再组合覆盖、累积屏障和外部读取来源。hb只接收其中去除自环后落回同一参与者的prop边，以及它另外列出的ppo和rfe。因而“读到了旧值，所以fr直接等于hb”是一条不存在的捷径。

沿MP坏结果核对：取得flag之后读到旧buf，产生从旧buf读通向新buf写的fr；发布侧顺序再把新buf写连接到flag发布，flag读从该发布取值。必须把这些边合成为符合定义的传播路径，才能回到取得侧形成hb回边。9.3将它与取得后的正向边合起来说明矛盾；不能省掉中间组合，只画一个未标出处的箭头。

这种阅读次序也解释了为何换一个原语就要重新证明：换成普通访问可能改变Marked端点资格，换成读屏障可能排除原本需要的写方向，换一个读取来源则可能切断rfe。结果集合变化来自具体边的变化，不来自API名字是否听起来更强。

### 9.1.3\_警告与候选拒绝不是一回事

cat后半还处理RCU关系以及Plain访问。RCU的rb检查为irreflexive，不能把它草率重述为另一遍acyclic hb；普通访问部分也有plain-coherence约束，并计算mixed-accesses与data-race诊断。

源码使用flag报告某类异常，用empty、acyclic、irreflexive表达一致性要求。看到一个data-race标志，不能自动说“工具已把这个输入当作没有任何行为的程序”；反过来，即使某结果存在，也不能把带诊断的测试当成无条件正确程序。验证报告应保留诊断与Observation，并解释测试是否处于预期使用范围。本页不把LKMM的普通访问处理规则偷换成C++标准对数据竞争的规则。

## 9.2\_lock\_cat\_为什么独立

[`lock.cat`](../../linux/tools/memory-model/lock.cat) 为锁 acquisition/release 建立前端分析和匹配关系，并检查自死锁等问题。`linux-kernel.cat` include 它以获得锁相关执行关系。

锁不是简单 fence：模型需要识别哪次 acquire 与哪些 release 可能对应，以及互斥对读取来源和一致性序的约束。将锁在 Litmus 中替换成 `smp_mb()` 会验证一个不同程序。

### 9.2.1\_从取得成功与观察失败分开读

设P0持有锁时，P1尝试取得同一把锁。模型必须区分P1失败地观察到“已锁住”，和真正取得锁以后进入临界区。固定lock.cat将成功取得表示为成对的LKR（读取部分）与LKW（写入部分），解锁为UL；失败trylock为LF；spin_is_locked返回真/假分别为RL/RU。RL被并入失败观察一类，不表示调用者取得了锁。

LKR具有acquire顺序，UL具有release顺序；LKW、LF、RL和RU本身没有这两种顺序属性。特别是看到spin_is_locked返回假，并不意味着自己拥有临界区资格：另一个参与者完全可能随后取得它。模型为这些观察选择读取来源，但不会把观察动作升级成加锁成功。

### 9.2.2\_先建立临界区再生成交接关系

lock.cat先把同一取得中的LKR/LKW配成RMW，再把成功写入LKW与对应UL连接为critical关系。重复取得同一锁却没有中间解锁会触发lock-nest约束；一个没有匹配成功取得的UL会产生unmatched-unlock诊断。两者分别承担候选约束与输入诊断，不应混为一个返回码。

接下来，失败观察和未锁住观察分别枚举可能的读取来源；生成co时先安排LKW，随后把UL插入，再让LKR从一致性序中的相邻初始写或解锁事件取值。最后重新得到rf及fr。这说明锁文件参与构造候选关系，而不仅是在现成图上加一条acquire边。

可以在纸上核对两个边界。第一，同一线程LKW之后、UL之前再出现同锁LKR，被嵌套取得约束排除；第二，测试结束仍持有一把锁，不等于必然缺少一条语法合法性规则：固定模型允许每个位置至多一个未匹配LKW，多于一个则不成立。因此不能强行给每个成功取得补一个虚构UL，以使图看起来闭合。模型允许有限测试在临界区结束，工程程序是否必须释放资源仍由其退出契约决定。

两个互斥临界区能够约束共享载荷访问，靠的是同一锁位置上的取得、释放和传递关系。把两端都改成mb后，锁位置、成功资格与临界区配对都消失了。即使还剩屏障顺序，也不再证明互斥。实际自旋锁如何排队和通知CPU请读锁实现专题；这里不把模型事件当作某种队列锁的内部字段。

## 9.3\_沿\_MP\_测试追踪一次判定

以实验中的 `MP+pooncerelease+poacquireonce.litmus` 为例：

1. `.def` 把 `WRITE_ONCE(buf)` 映射为 once Write；
2. 把 `smp_store_release(flag)` 映射为 release Write；
3. 把 `smp_load_acquire(flag)` 映射为 acquire Read；
4. `.bell` 给事件标记访问类别；
5. 寄存器条件选择 Rflag 从发布 Write 取值，Rbuf 从初始写取值；
6. 写buf→release写flag形成po-rel；读取flag→读取buf形成acq-po；旧buf读取经fr、发布侧累积边及flag的rfe合成prop中的同线程回边，进入hb；
7. 回边与取得侧ppo边形成hb环，违反acyclic hb；`Never`是由此解释的预期判定，不是本批执行的Observation记录。

无序版本把两端换成ONCE，关键release/acquire边消失，材料的预期为 `Sometimes`。实际结果须执行成对测试并保存工具输出；不能用预期注释冒充验证。具体读取来源与回边推导见[关系教材](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/memory_ordering/P08_LKMM事件_关系与一致性判定.md#8.4.1_为MP坏结果找到真实的回边)，其C子图检查器也不是herd7替代品。

## 9.4\_RCU\_模型能证明什么

LKMM 能把 RCU 读侧区间、`synchronize_rcu()`、指针发布/取得等纳入关系图。例如 `MP+onceassign+derefonce` 验证取得新 RCU 指针后不能看到对象初始化前的旧值。

但模型不会替真实代码验证：

- 指针是否在 RCU 读侧外逃逸；
- 回调是否在正确 GP 后执行；
- kref 与 root/子块所有权是否正确；
- 内存分配复用和所有错误路径；
- 当前 Tree RCU 实现的性能与进展性。

这些属于 [RCU 专题](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/大纲.md)和具体源码导读。

## 9.5\_官方文档证据

- [`Documentation/memory-barriers.txt`](../../linux/Documentation/memory-barriers.txt)：Linux 屏障、依赖、锁、等待、I/O 和体系结构边界；
- [`Documentation/atomic_t.txt`](../../linux/Documentation/atomic_t.txt)：atomic API、RMW、顺序后缀和失败路径；
- [`tools/memory-model/README`](../../linux/tools/memory-model/README)：herd7/klitmus7 需求和基本运行方法；
- [`tools/memory-model/Documentation/simple.txt`](../../linux/tools/memory-model/Documentation/simple.txt)：优先使用锁、per-CPU 和封装原语的工程路线；
- [`tools/memory-model/Documentation/litmus-tests.txt`](../../linux/tools/memory-model/Documentation/litmus-tests.txt)：Litmus 语法、用法和限制。

## 9.6\_模型限制

Linux 6.12 Litmus 文档明确说明，工具不准确模拟任意编译器优化，不支持同一变量多访问宽度、通用异常/中断、MMIO/DMA、自修改代码、动态内存分配以及所有原子变体。

因此证据链必须组合：

```text
ONCE/屏障源码定义
→ 编译器反汇编
→ LKMM Litmus
→ 目标架构实现
→ 必要时 klitmus7/硬件运行
→ 真实子系统状态机与生命周期审查
```

## 9.7\_更新流程

升级 Linux 基线时：

1. 记录新版本和原始位置；
2. 比较 `rwonce.h`、通用/目标架构 barrier 定义；
3. 比较 `linux-kernel.def/.bell/.cat/.cfg/lock.cat`；
4. 阅读 model README 对 herdtools7 版本的新要求；
5. 重新运行配套 manifest 全部 Litmus；
6. 重新生成 GCC/Clang/ARM 反汇编记录；
7. 更新知识正文中的版本映射，但不把版本细节写成跨版本机制。


完成后回到[总索引验收](P01_Linux_6.12_LKMM_源码与模型导读.md#1.15_本章验收)，再按真实场景选择编译器、模型或目标实验。
