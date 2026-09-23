---
id: labs.kernel.object_lifetime.materials
title: "对象生命周期实验材料"
kind: reference
status: evolving
domains:
  - linux
  - kernel
  - object_lifetime
---

# 第1章\_对象生命周期实验材料

## 1.1\_引用责任与交付

[reference_ownership.c](reference_ownership.c)对应[kref 问题入口的完整程序](../../../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.6.1_运行完整的责任交接模型)。在宿主目录用 `cc -std=c11 -Wall -Wextra -Werror -O2 reference_ownership.c -o reference_ownership` 编译并运行，保留断言。输出为 `accept=0 released=1` 和 `accept=1 released=2`，表示拒绝与接收两条路径分别完成一次最终回收。

[P04 规则回访](../../../../knowledge/linux/object_lifetime/kref/P04_kref_三条核心规则.md#4.16.1_传递给异步路径)在正文完整复用同一程序，重验六种责任安排与两条分配失败路径，比较借用、新增和转交；查找保证另由内核容器模型分析。

程序使用普通 C 分配对象和责任槽，按顺序安排动作，不是 Linux `kref` 的替代实现。字段借用、创建者与处理者两个结束顺序、直接转交和拒绝归还的推导见正文，不在此复制另一份教程。

本轮宿主检查包含六种所有权调度组合、两条受控分配失败路径和槽责任求和。它们未验证内核模块、真实 workqueue、并发访问、原子计数、内存序、饱和告警或 KASAN；没有主动执行 UAF。

## 1.2\_一次内核工作交付

[note_kref_work.c](note_kref_work.c)与[Makefile](Makefile)对应[P01 完整模块](../../../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.16.1_运行一次真实工作交付)。它把 ref 放在业务值之后的非首成员位置，沿[P02 地址关系](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.9_container_of_是理解_kref_的关键)检查两种回调还原；只投递一个新工作项，成功后创建者和 worker 各归还一份；卸载等待私有队列销毁，回调不重排。目标构建和观察步骤在正文；本轮未实际装卸目标模块。

ARM Clang 前端检查通过，纳入的 354 份头文件中非生成源码与官方固定提交无差异，生成头仍属于当前配置环境。宿主 C 控制夹具采用固定 kref_init/get/put 函数体，把底层计数、分配和队列明确替换为顺序模型，覆盖队列分配失败、对象分配失败、投递拒绝、worker 先结束、创建者先结束五条路径。此检查不等于目标构建链接、原子并发或 workqueue 实际运行。

## 1.3\_回绕与饱和的保守代价

[count_wrap.c](count_wrap.c)配合[P02 八位模型](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.5.1_用八位模型观察回绕的代价)，以 uint8_t 普通回绕和显式布尔饱和状态比较同一组责任事件。严格 C11 编译运行及 1001 组正常/饱和责任序列通过。该模型不分配/释放真实对象，不是 Linux refcount_t 的算法、位宽、并发或告警实现；不能拿模型结果证明内核竞态安全。

B04d 布局调整后重新通过 ARM 前端、354 份头文件固定源码差异检查和五条控制路径，宿主夹具另断言 ref 非首成员且 work 位于其后。复用的树专题 embedded_owner 程序重新运行通过；这些验证没有执行目标模块或检测真实并发。

## 1.4\_静态存储中的归零

[note_kref_static.c](note_kref_static.c)配合[P02 静态模块](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.14.2_运行一个不释放静态内存的完整模块)观察 1→2→1→0。计数归零同步调用清理回调，静态外壳由模块存储机制回收，回调不 kfree 它。构建、装卸命令和练习均在正文。

本轮 ARM 前端通过，348 份头文件中的 336 份非生成源码与固定提交无差异；宿主用固定类型、初始化宏、引用函数与顺序原子替身检查一次归零回调。自动运行时初始化与静态常量初始化通过严格 C11 编译；缺少内层花括号得到预期 missing-braces 诊断，裸宏赋值和非恒定静态初始化按预期编译失败。未执行目标构建链接、模块装卸或真实并发；宿主控制结果不能冒充目标日志。

## 1.5\_快照与持有的区别

[reference_snapshot.c](reference_snapshot.c)对应[P02 对照程序](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.17.1_运行快照与持有的对照程序)。第一轮只保留正计数快照，原持有者归还后对象回收；第二轮先追加观察者责任，原持有者退出后仍可访问，再由观察者完成最终归还。程序只打印对象外的回收记录，不故意访问悬空指针。

严格 C11 编译与两条顺序路径、两条受控分配失败检查通过；另以 C 枚举“减后另读”的六种保序交错，其中四种出现两个清理资格，与绑定本次减少旧值的两种顺序恰有一个资格对照。教学计数为普通整数，交错由测试顺序指定，不证明真实并发、内存序或目标内核行为。

## 1.6\_类型接口与部分初始化清理

[note_kref_object.c](note_kref_object.c)是[P02 完整对象模板](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.19_标准自定义引用对象模板)的配套模块。外壳与 data 分别申请；init 建立创建者份额，部分初始化失败和正常最后归还共用 release，先清理 data 再清理外壳。NULL put 是本类型明确允许的空槽操作，get 仍须非空且已有正引用保护。[P05 类型契约](../../../../knowledge/linux/object_lifetime/kref/P05_基础_API_源码逐行讲解.md#5.10_API_封装模板)复用这些已验证分支解释封装边界；本次仅重构讲解，材料程序未修改，不新增目标验证结论。

ARM 前端通过，354 份头文件中 342 份非生成源码与固定提交无差异。宿主 C 夹具采用实际模块代码和固定普通引用函数、明确的顺序原子/分配替身，覆盖成功、外壳分配失败、data 申请失败及空槽 put，检查释放顺序、回调次数、存活块归零和 ref 非首成员。未执行目标链接、装卸、真实分配器故障注入或并发。

## 1.7\_与C++管理型指针对照

[shared_ownership.cpp](shared_ownership.cpp)对应[P02 C++ 对照](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.23.1_用完整程序观察自动归还)，只使用 C++17 接口。严格编译运行观察到拷贝后两份、移动后原管理型对象为空、reset 后一份、异常退出后析构一次。get 得到的裸指针没有独立份额，全部计数断言限定在该顺序程序。

另用既有固定 kref 普通函数和顺序原子替身检查两种归还顺序，均只调用最后一次 put 传入的回调；把 kfree 的 const void * 签名直接传给 kref_put 的负例按预期触发函数指针不兼容诊断。未执行错误回调、悬空访问或真实并发。

## 1.8\_容器持有与撤下后继续使用

[note_kref_registry.c](note_kref_registry.c)对应[P02 单槽容器模块](../../../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.30.1_设计_A_容器持有引用)。同一 mutex 保护全局槽的发布、查找及清空，非空槽拥有一份；查找在锁内追加自己的份额，所以可以在撤下后继续使用。发布拒绝收回预留，容器归还放在锁外，回调不再操作槽。

ARM 前端通过，354 份头文件中 342 份非生成源码与固定提交无差异。宿主使用实际模块、固定普通引用函数及明确的顺序计数/锁/分配替身，覆盖分配失败、正常模块、满槽拒绝、先撤下后查找、两位读者的两个归还顺序；检查无存活块、槽为空、未持锁调用最终回收和引用归属。没有执行目标装卸、真实线程竞争、内存序或故障注入，release_calls 本身也只按本模块的同步观察场景使用。

## 1.9\_业务关闭与存储保留

[lifetime_protocol.c](lifetime_protocol.c)对应[P03 协议模型](../../../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.3.1_用C模型观察仍持有却被拒绝)。观察者账本分别保存存储、入口、业务许可和三份角色责任；它不是 kref 数据结构，也没有真实分配、锁、原子或 free。关闭后旧读者仍持一份，但新业务读取可被拒绝。

严格 C11 编译运行通过，覆盖读者先操作与关闭先发生两种顺序；额外检查“入口可见但业务拒绝”和“入口撤下但旧读者仍被许可”两个反例，最后均按各自收尾步骤清理一次。计数/状态断言只证明指定模型轨迹，不证明内核并发或硬件设备可用性。

## 1.10\_工作实例的归还票据

[work_ticket.c](work_ticket.c)对应[P03 失败与取消账本](../../../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#%287%29_所有权表要补充失败路径和取消路径)。在单次实例、无重新投递、取消者仍保留自己份额的范围内，比较拒绝、pending 取消、已完成、执行中等待完成，以及两种正常结束顺序。真实 workqueue 不维护这个外部 ticket 布尔账本。

严格 C11 编译运行六条路径通过，每条释放计数为 1；取消模型不自动归还被取消票据，而由取消者明确接管，false 也不触发盲目补 put。模型只按顺序模拟等待结果，无内核 work 执行、真实取消、重新投递竞态、分配器或原子实现验证。

## 1.11\_业务停止与引用退出

[note_kref_shutdown.c](note_kref_shutdown.c)对应[P03 完整关闭模块](../../../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.16_一个完整的生命周期模板)，由入口接管初始份额，lookup 锁内取得独立引用，关闭者清槽后依接管份额完成对象锁内的停止状态切换，旧读者得到拒绝后仍须归还。Makefile 已登记；目标构建与观察命令在正文。

ARM 前端检查通过，354 份依赖头中 342 份非生成源码与官方固定提交无差异。宿主采用固定普通引用函数与显式顺序替身检查八组：分配失败、完整模块、满槽拒绝及空槽关闭、两种读者退出顺序与操作/关闭先后的四种组合，以及清槽尚未停止的窗口。每组责任和分配收束，检查无持锁释放。宿主缺少 ESHUTDOWN，夹具显式取固定内核 errno.h 的 108 常量；未因此改写真实程序。未执行目标链接、装卸、真实并发、原子内存序或硬件关闭；示例仅单一关闭管理者，计数和统计为有限同步演示。

## 1.12\_条件取得的比较窗口

[conditional_take.c](conditional_take.c)对应[P05 完整条件模型](../../../../knowledge/linux/object_lifetime/kref/P05_基础_API_源码逐行讲解.md#5.7.2_kref_get_unless_zero%28%29_的使用场景)，使用 C11 原子比较交换和显式安排的计数变化观察零、正数、读后变零和正数重试四路径。严格 C11 编译运行通过；没有线程或回收，对象地址始终有效，不模拟 Linux 饱和和内存模型。

固定条件链的四个实际函数另由宿主顺序夹具核对六类 helper 路径（含 oldp、读后变零/正数以及溢出/已饱和）与两种 kref 返回结果；忽略返回值按预期触发编译诊断。底层比较交换和原子是明确替身，保留旧值与失败更新规则；未执行目标模块、真实并发、架构指令或内存序验证。

## 1.13\_非拥有索引与最后归还锁

[note_kref_locked.c](note_kref_locked.c)对应[P05 完整锁交接模块](../../../../knowledge/linux/object_lifetime/kref/P05_基础_API_源码逐行讲解.md#5.8.2_kref_put_mutex%28%29_的典型用途)。初始份额仍归创建者，单槽不拥有引用，所有归还经过同一 mutex 组合接口；最终回调清槽、解锁后回收。Makefile 已登记，构建与观察步骤在正文。

ARM 前端通过，354 份头文件中的 342 份非生成源码与官方固定提交无差异。宿主顺序夹具覆盖六组模块路径（含满槽拒绝、不误清旧入口、查找者抢先加入和两个读者退出顺序）及 mutex/spinlock 各五种分支，共十例；固定五函数与普通引用链保持真实主体。初次严格构建发现固定 unsigned/signed 常量比较告警，明确使用 Wno-sign-compare，固定 scripts/Makefile.extrawarn 在非 W=3 分支同样关闭该告警；不改函数体避警。锁、分配和原子为顺序替身，未验证真实竞争、调度、IRQ、PREEMPT_RT、目标装卸或内存序。

## 1.14\_管理者保活与借用工作退出

[note_kref_owned_work.c](note_kref_owned_work.c)对应[P06 完整模块](../../../../knowledge/linux/object_lifetime/kref/P06_release_回调与复杂销毁模式.md#6.2.2_运行一个由管理者等待借用退出的模块)。worker 不持独立引用，管理者保留初始份额到封闭投递和同步等待之后；停止检查与实际排队同锁，等待位于锁外。Makefile 已登记，正文展示完整程序与目标步骤。

ARM 前端通过，354 份头文件中 342 份非生成源码与固定提交无差异。宿主使用实际模块与固定普通引用 helper，原子、锁、分配和队列是显式顺序替身；九组检查覆盖三处分配失败、取消/执行两种模块顺序、两种关闭窗口、重复工作和管理者最后退出，均无遗留分配且最终回收次数正确。未执行目标构建链接、装卸、真实调度与内存序；同步替身中的回调执行只安排顺序，不声称测试了真实等待。

P06 的[入口和上下文回访](../../../../knowledge/linux/object_lifetime/kref/P06_release_回调与复杂销毁模式.md#6.5_外部可见性_脱链应该由谁负责)复用 registry、locked 和 owned_work 三个完整程序比较保护窗口；本批未改源码或增加运行范围，原宿主替身不能证明 IRQ、RT 或真实锁等待。

## 1.15\_定时器改期与退出责任

[timer_ownership.c](timer_ownership.c)对应[P06 四条顺序模型](../../../../knowledge/linux/object_lifetime/kref/P06_release_回调与复杂销毁模式.md#6.7.3_release_和_timer_的收尾关系)。外部账本分别记录 pending、running、shutdown、派生 work 和责任数，展示重复 get 改期泄漏、管理者等执行退出、删除后重启、最终关闭阻止 work 再启动。严格 C11 编译运行通过；不定义 NDEBUG，因为 assert 承担模型步骤与检查。无真实时钟、分配器、线程、中断或内核 timer 执行。

固定六个 timer helper 另以 C11 夹具覆盖八种删除/关闭 × pending × running 组合及旧名包装，检查返回值、清空 function 与等待先后；base 锁、队列摘除和运行者退出均为显式顺序替身，未覆盖 RT/LOCKDEP、实际阻塞或定时轮算法。八个唯一实现函数体去注释规范化后与固定源码一致；本批没有新增 ARM 模块或目标运行结论。

[P06 章末回访](../../../../knowledge/linux/object_lifetime/kref/P06_release_回调与复杂销毁模式.md#6.10.2_一个复杂_release_示例)继续使用 owned_work 原程序比较取消和执行路径，修改题与原始验证范围分开。RCU、诊断和资源拓扑收束未修改材料程序，不新增目标运行结论。

[P07 交付入口](../../../../knowledge/linux/object_lifetime/kref/P07_handoff_所有权转移模型.md#7.2.1_指针传递不等于引用转移)完整复用 reference_ownership.c，重新编译运行接收/拒绝两条路径通过；正文重新解释 share 与 move、异步借用窗口及接收者提前结束的契约。纸面直接转交变体不是已执行的新程序，顺序模型也没有创建并发消费者。

## 1.16\_完成事件与异步引用退出

[note_kref_completion.c](note_kref_completion.c)对应[P07 完整模块](../../../../knowledge/linux/object_lifetime/kref/P07_handoff_所有权转移模型.md#7.3.5_completion_场景里的引用归属)；Makefile 已登记，等待者初始份额跨越等待和取消，worker 预留由实际执行者或取消接管者归还。一次投递、无重排，无论事件及时与否都同步收尾后才读取结果和归还等待者。

ARM 前端通过，354 份依赖头中 342 份非生成源码与固定提交无差异。宿主实际模块/固定普通引用 helper 配合显式顺序替身通过七组：两处分配失败、拒绝发布、早完成、等待中完成、超时取消、超时后完成；最终回收一次且无遗留分配。替身未实现真实 completion 锁/等待和工作调度，没有目标链接/装卸、并发或内存序验证。

## 1.17\_同一请求的队列转交与共享

[note_kref_queue.c](note_kref_queue.c)对应[P07 完整双协议模块](../../../../knowledge/linux/object_lifetime/kref/P07_handoff_所有权转移模型.md#7.6.1_一个完整请求对象_handoff_示例)，Makefile 已登记。一个业务槽串起入队、出队与唯一 work，直接转交轮沿同一份移动，共享轮内部追加队列份额并保留创建者观察，flush 后读取结果。无外部入口，每轮结束再启动下一对象。

ARM 前端通过，354 份头中 342 份非生成源码与固定提交无差异。宿主实际模块及固定普通引用 helper、显式顺序锁/队列/分配/原子替身通过十组：三处分配失败、两轮执行拒绝、提前/稍后执行、两种满槽拒绝、重复入队与已消费阶段。检查无遗留分配、槽为空、引用/阶段保留、无持锁回收。未执行目标链接/装卸、实际多线程、多队列争用或内存序；程序接口前提不能由替身测试自动推广。

[P08 首次查找窗口](../../../../knowledge/linux/object_lifetime/kref/P08_lookup_场景与_kref_get_unless_zero%28%29.md#8.3.1_正确模型一_mutex/list_lookup_+_kref_get%28%29)复用 note_kref_registry.c，新增 S0～S5、两种先后顺序和重复撤下责任讲解；程序没有改动，继续采用原来的验证范围及目标未执行边界。

## 1.18\_整数索引的取得与撤下

[note_kref_xarray.c](note_kref_xarray.c)对应[P08 完整实验](../../../../knowledge/linux/object_lifetime/kref/P08_lookup_场景与_kref_get_unless_zero%28%29.md#8.5.2_xarray_lookup_的引用规则)，Makefile 已登记。发布预留映射份额，xa_lock 内 load/get，xa_erase 自行加锁后交回旧条目，重复撤下无第二次归还；xa_destroy 不代替对象 put。

模块和哈希/IDR 配对片段的 ARM 前端通过，合并 356 份头中的 344 份非生成源码与固定提交无差异。宿主实际模块与固定 xa_insert/xa_load/xa_erase 外层函数通过六组：对象分配失败、两类插入拒绝、正常 init、重复节点/编号与重复移除、移除先完成后查找。底层索引存储、锁、RCU、分配和原子为顺序替身，不包含真实 XArray 节点实现。IDR/哈希片段只做前端与源码契约核对。未执行目标链接/装卸、真实竞争或内存序。

[P13 XArray工程复用](../../../../knowledge/linux/object_lifetime/kref/P19_XArray身份与删除工程模板.md#19.1_整数索引与期待对象删除)使用同一完整模块，另给出同锁比较期待对象再__xa_erase的可选包装。三组新身份删除用例与六组既有用例通过，模块变体ARM前端通过；正式材料未改，未运行目标并发或真实节点算法。

## 1.19\_成员撤下与同步业务门

[note_kref_table.c](note_kref_table.c)对应[P09 完整链表服务](../../../../knowledge/linux/object_lifetime/kref/P09_kref_与锁的组合.md#9.6.1_一个完整的锁_+_kref_对象模板)，按集合锁→对象锁把 DYING 与摘链接成共同决定，只有实际摘下者在锁外归还表份额。创建者、表和读者分别结算；重复节点、重复编号与撤下后重新发布均明确拒绝。Makefile 已登记。

ARM 前端通过，354 份依赖头中的 342 份非生成源码与官方固定提交无差异。宿主六组检查执行实际模块、固定普通引用链和六个链表辅助函数，覆盖分配失败、完整周期、私有请求拒绝、重复发布、双对象请求与重复撤下、先撤下后查找；锁等级、引用数、节点状态和分配收束均检查。原子、锁、分配及部分链表操作为顺序替身，未执行真实线程、目标链接/装卸、IRQ/RT 或内存序。

[P13链表工程复用](../../../../knowledge/linux/object_lifetime/kref/P17_拥有型链表工程模板.md#17.1_拥有型链表的发布与撤下)使用同一note_kref_table.c，按接口契约及L0～L5解释一次发布、同ID拒绝与重复撤下；模块和既有六组夹具未改，本批不新增运行结论。

## 1.20\_RCU取得窗口与两种退休顺序

[rcu_take_window.c](rcu_take_window.c)对应[P10 两种退休顺序](../../../../knowledge/linux/object_lifetime/kref/P10_kref_与_RCU.md#10.2.1_RCU_和_kref_分别保护什么)。观察者账本分别保存入口/长期份额、旧读区、存储和待回收状态；比较先归零再 GP 与发布份额跨 GP，各安排读者先取得和撤下先发生。严格 C11 编译运行四条轨迹通过，每条一次 release、一次回收；旧读区阻止过早宣告 GP，跨 GP 的长期引用仍须归还。模型没有真实分配、原子、线程或内核 RCU，不属于 Makefile 的内核模块。

## 1.21\_RCU查找与回调代码退出

[note_kref_rcu.c](note_kref_rcu.c)对应[P10 完整查找模块](../../../../knowledge/linux/object_lifetime/kref/P10_kref_与_RCU.md#10.3.1_基础对象模型)。表单独拥有一份；查找在读区内条件增加，离开读区再等待对象 mutex，主动撤下只归还一次表份额，最后归还排出模块自有回调，退出及初始化失败在来源结束以后等待 rcu_barrier。Makefile 已登记。

ARM 前端通过，355 份头中的 343 份非生成源码与固定提交无差异。宿主执行实际模块、固定普通/条件引用链及 list_del_rcu，八组检查覆盖分配失败、正常周期、重复拒绝、两种取得顺序、撤下后查找、保留 next 和可选过滤的回滚。可选过滤也通过前端。底层锁/原子/分配/插入遍历和 RCU 回调调度为顺序替身，未执行目标链接/装卸、真实并发、GP 进度或内存序。

## 1.22\_kobject登记与类型清理

[note_kobject.c](note_kobject.c)对应[P11 完整kobject周期](../../../../knowledge/linux/object_lifetime/kref/P11_kref_refcount_t_kobject_的边界.md#11.3.1_kobject_不只是引用计数)。启用SYSFS且关闭DEBUG_KOBJECT_RELEASE时，添加一个无属性目录、追加观察者、撤下并归还初始份额，最后观察者归还才触发类型回调；其他配置直接拒绝。Makefile已登记。名称短暂出现，不主动发ADD事件，没有外部用户或设备注册。

ARM前端通过，354份头中342份非生成源码相对固定提交无差异。宿主使用固定九个kobject函数与普通引用链，七组包含两类配置拒绝、分配失败、添加失败、完整周期、隐式/显式撤下和NULL包装，检查对象/名字/目录与父引用收束。命名添加、sysfs、分配及诊断为顺序替身，未执行真实sysfs、事件、并发、目标装卸或调试延迟释放分支。

## 1.23\_设备注销与独立观察者

[note_device.c](note_device.c)对应[P11 设备模块](../../../../knowledge/linux/object_lifetime/kref/P11_kref_refcount_t_kobject_的边界.md#11.4.1_device_driver_core_已经封装好的对象模型)，初始化后分别处理命名/添加失败，成功时追加观察者，device_unregister消费初始化份额，观察者最后put才清理外壳。模块不绑定硬件驱动，要求SYSFS开启且DEBUG_KOBJECT_RELEASE关闭。Makefile已登记。

ARM前端通过，372份头中360份非生成源码相对固定提交无差异；八组宿主运行固定五个device函数与既有九个kobject函数，覆盖配置/分配/命名/添加失败、正常注销、三个release优先级、register与NULL包装。初始化/设备添加删除/命名/sysfs/devres为顺序替身，未验证完整driver core、真实解绑/事件、并发、目标链接或装卸。

## 1.24\_独立会话与设备份额

[note_session.c](note_session.c)对应[P11完整分层结构](../../../../knowledge/linux/object_lifetime/kref/P11_kref_refcount_t_kobject_的边界.md#11.5.4_一个典型的分层结构)：每个会话取得设备一份，多个会话拥有者只增加私有kref；注销关闭业务，已有会话可读取统计，最后会话归还桥接份额。Makefile已登记，要求SYSFS开启、DEBUG_KOBJECT_RELEASE关闭，模块没有硬件和外部入口。

十组宿主控制路径与ARM前端通过；372份头中360份非生成源码无固定提交差异。固定device/kobject/引用函数保留，锁、原子操作、登记/sysfs为顺序替身；未执行目标链接装卸、真实并发、驱动解绑或硬件。

## 1.25\_离线责任账本

[ownership_audit.c](ownership_audit.c)对应[P12诊断入口](../../../../knowledge/linux/object_lifetime/kref/P12_典型错误模式与调试线索.md#12.2.1_调试工具先导)，按单对象显式有序事件检查创建者与工作者份额，区分balanced、invalid_owner、leftover、incomplete和still_open。C11严格编译运行六条轨迹及两个练习变体；不执行真实get/put/free，不模拟内核计数实现，不证明日志完整度、多CPU顺序或诊断器实际覆盖。
[P12章末练习](../../../../knowledge/linux/object_lifetime/kref/P12_典型错误模式与调试线索.md#12.10.2_最小审查流程)增加转交后访问、归还后访问、记录不完整和区间未结束四项预测。原C11检查器不变，六个既有用例与四项练习复核通过；仍为单对象离线顺序模型。

## 1.26\_分层创建与阶段失败

[note_kref_create.c](note_kref_create.c)对应[P13分层模板](../../../../knowledge/linux/object_lifetime/kref/P16_对象创建与失败清理模板.md#16.2.2_模板二_alloc/init/get/put/release_分层模板)。外壳初始化、缓冲区准备和业务校验在发布前完成；fail_stage=0～3分别走正常、外壳失败、缓冲区失败和已有缓冲区后的失败，release统一清理可到达的部分状态。Makefile已登记，成功模式在init内完成所有使用，无外部入口。

八个宿主用例与ARM前端通过，354份头中342份非生成源码与固定提交无差异。宿主使用固定普通引用函数，模拟分配、错误指针和模块环境；未执行目标链接装卸或并发。失败装入不会留下可卸载模块，目标命令与预期在正文完整说明。

## 1.27\_拥有型哈希与IRQ保存

[note_kref_hash.c](note_kref_hash.c)对应[P13哈希模板](../../../../knowledge/linux/object_lifetime/kref/P18_拥有型哈希与IRQ工程模板.md#18.1_拥有型哈希与IRQ上下文)，四桶hlist在同一spin_lock_irqsave范围内维护成员、状态与短统计。发布另取表份额，重复撤下不多消费，关闭后旧拥有者请求拒绝。Makefile已登记，无外部IRQ入口。

七组顺序宿主检查与ARM前端通过；354头中342非生成头无固定提交差异。宿主锁/IRQ保存只是状态替身，不能作为真实中断或SMP证据；目标链接装卸、硬件和并发未执行。

## 1.28\_工作分享与转交对照

[note_kref_work_modes.c](note_kref_work_modes.c)对应[P20工作工程模板](../../../../knowledge/linux/object_lifetime/kref/P20_工作交付与关闭工程模板.md#20.1_先选择分享还是转交)。transfer=0为worker另取候选，transfer=1只在投递成功时交出创建者份额；同一worker仅put一次，模块退出等待唯一工作返回。Makefile已登记，无重复投递或外部生产者。

十例宿主检查和ARM前端通过，354头中342非生成源码无固定提交差异；工作调度为顺序替身，包含回调在提交返回前完成的安排。目标装卸、真实并发与模块退出竞争未执行。

## 1.29\_单次定时器交付与关闭

[note_kref_timer.c](note_kref_timer.c)对应[P21完整单次模板](../../../../knowledge/linux/object_lifetime/kref/P21_定时器交付与同步关闭模板.md#21.1_先把排队状态与对象责任分开)。管理者保留初始份额，每对象最多接受一次启动；回调或取消者结算timer份额，关闭本身不消费管理者。delay_ms可对照未到期关闭与已触发两条路径；Makefile已登记。

七组宿主协议与ARM前端通过，354头中342非生成源码无固定提交差异。定时器、锁和中断是顺序替身；目标装卸、真实等待/软中断和退出竞争未执行。timer_ownership.c原有周期借用模型保持不变，不把单次模块当作任意改期模板。
