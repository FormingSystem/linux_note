---
id: atlas.tracks.linux_kernel
title: "Linux内核机制学习路线"
kind: track
status: maintained
domains:
  - navigation
  - linux
  - kernel
---

# 第1章\_Linux内核机制学习路线

## 1.1\_路线目标

本路线面向能读懂 C 的变量、函数、数组和循环，但还没有内核整体模型的读者。先运行一个读取文本的程序，再追问数据从哪里来、请求由谁执行、状态放在哪里。等这条路径成立以后，再加入多个使用者、异步事件和设备拆除。你不需要先背完整的内核目录与接口表。

路线中的文件保持各自的权威位置，顺序体现学习依赖。当前已按新教材写法重写的是第一阶段的两篇基础正文；后续材料仍需按 [全量重构蓝图](../roadmaps/linux_textbook_refactor.md#1.2_全量阅读依赖与批次)分批校准，不能把进入路线理解为全书重构已经完成。

开始前建议先在[知识库专题阅读与评审地图](../maps/knowledge_review_map.md)查看各专题和章节的人工确认程度；路线中的顺序表示认知依赖，不表示对应内容已经完成评审。

## 1.2\_第一阶段\_内核边界与源码定位

先回答一个小问题：程序只有文件路径和数组，为什么能够读到内容？本阶段不要求安装自己编译的内核。第一个实验只需要普通 Linux 用户环境与 C 编译器；源码定位另需已核对身份的内核 Git 工作树，没有源码时仍可先完成程序实验。

1. [从读取一份文件认识内核](../../knowledge/linux/architecture/kernel_composition/linux内核概貌.md#1.1_先让程序读到几个字)：先得到 13 字节的可见结果，再区分进程、库函数、系统调用、打开状态与缓存。做完短读和空文件练习以后，应能说明成功输出证明了什么、没有证明什么。
2. [从问题找到源码文件](../../knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md#1.1_先区分源码目录与正在运行的系统)：带着读文件的问题定位公共文件操作、内存和设备代码；用固定提交核对文件，区分源码、配置、构建产物和运行系统。
3. 回顾时选读 [宏内核和微内核](../../knowledge/foundations/operating_systems/concepts/宏内核和微内核.md)，比较职责怎样组织；它不是先背完才能开始实验的分类前提。
4. 需要另一条阅读视角时，选读《奔跑吧 Linux 内核》编排中的 [Linux 系统基础知识](../../publications/books/running_linux_kernel/P01_linux系统基础知识.md)与 [内核引导和初始化](../../publications/books/running_linux_kernel/P03_内核引导和初始化.md)。

阶段结束时，尝试解释两个新现场：“程序读到了文件，但没有证据表明本次访问了硬盘”；“源码树没有内核映像，但源码仍可能完整”。如果能用本阶段的缓存和构建模型解释，就可以进入下一阶段；若仍把库函数与系统调用、源文件与产物当成同一物，回到对应实验重新预测。

准备编写驱动的读者，可在这里转入[内核模块构建与部署](../../engineering/build/kernel_modules/大纲.md#1.1_四章怎样连起来)，先完成最小 Hello 模块，再按[驱动开发路线](linux_driver_track.md)进入设备节点与 I/O；加载内核代码需要自己的适用实验环境，不是本阶段普通文件读取实验的附带步骤。

## 1.3\_第二阶段\_数据组织与对象生命周期

一次打开已经产生了需要保存和释放的状态。对象多起来以后，先要能找到它们，再要判断最后一个使用者什么时候离开。本阶段先学对象怎样组织，再学谁负责持有和释放；数据结构与生命期回答不同问题。

1. [Linux 双向循环链表](../../knowledge/linux/data_structures/单链表_linked_list/大纲.md#1.1_从一组任务走到容器选择)：先用三个任务区分对象与成员，再运行 C 模型观察摘除和重入；加入第二个修改者后解释锁、发布、失败回滚和寿命边界。固定版本证据从[源码索引](../../research/source_reading/linked_list/navigation/P01_Linux_6.12_链表源码阅读索引.md#1.2_由结论进入唯一实现)进入。历史目录名不表示 list_head 是单向链表。
2. 沿[哈希表路线](../../knowledge/linux/data_structures/哈希表_Hash_Table/大纲.md#1.1_沿问题增加约束)进入[桶与冲突](../../knowledge/linux/data_structures/哈希表_Hash_Table/P01_数据结构理论基础/P01_哈希表核心原理_空间与时间的终极博弈.md)，用完整 C 程序验证完整键比较与重新分桶；再结合节点学习[位宽与哈希计算](../../knowledge/linux/data_structures/哈希表_Hash_Table/P02_Linux_内核_5.10_核心实现/P03_算法之魂_哈希函数与位运算优化.md)，避免把桶号当作对象身份。
3. [Linux hlist](../../knowledge/linux/data_structures/哈希表_Hash_Table/P02_Linux_内核_5.10_核心实现/P02_内核基石_hlist非对称链表.md)解释入口槽与节点状态；具有 RCU 前提后，继续[动态容量](../../knowledge/linux/data_structures/哈希表_Hash_Table/P03_高级进阶与性能调优/P05_动态伸缩的rhashtable_无感扩容的艺术.md#5.1_先想清楚为什么不能只换一个数组)，用 C 模型观察跨链重扫，再做[接口回收实验](../../knowledge/linux/data_structures/哈希表_Hash_Table/P03_高级进阶与性能调优/P08_rhashtable接口与回收实验.md#8.1_先固定本例的拥有者)。最后用[子系统应用](../../knowledge/linux/data_structures/哈希表_Hash_Table/P04_内核实战与应用/P06_哈希表在内核子系统中的影子%28深度拆解篇%29.md#6.2_名称缓存匹配的是父目录中的一个名字)区分候选与业务判断，并用[固定桶模块](../../knowledge/linux/data_structures/哈希表_Hash_Table/P04_内核实战与应用/P07_内核模块实战指南.md#7.1_先限定谁拥有对象)闭合失败回滚。
4. 先读[树关系基础](../../knowledge/linux/data_structures/红黑树_rb-tree/P01_树的基本概念.md#1.2_什么是树)，再做[P16 表示与回收实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P16_普通树的表示与构建实验.md#16.7_运行预测与资源回收)，随后按[树结构路线](../../knowledge/linux/data_structures/红黑树_rb-tree/大纲.md#1.1_沿问题进入现有章节)依次进入 P02、P17～P21 的结构、四种遍历与查询，再进入 P03 → P22 → P23 的搜索、删除与验证，随后用[P04 访问计数](../../knowledge/linux/data_structures/红黑树_rb-tree/P04_为什么_BST_会退化.md#4.2.4_用节点访问次数观察退化)观察退化，再沿 P05/P24/P25 建立旋转与高度、由[P06 多路查找](../../knowledge/linux/data_structures/红黑树_rb-tree/P06_2-3-4_树_从多路平衡到红黑树的结构桥梁.md#6.4.7_用完整C程序观察区间下行)建立容量和区间，由[P30 插入](../../knowledge/linux/data_structures/红黑树_rb-tree/P30_2-3-4树插入与分裂时机.md#30.3_运行完整的两种插入)比较分裂时机、查重与失败不改树，由[P31 预修复删除](../../knowledge/linux/data_structures/红黑树_rb-tree/P31_2-3-4树预修复删除.md#31.3_运行完整预修复删除)解释容量下界和更新后的孩子槽，再由[P32 回溯删除](../../knowledge/linux/data_structures/红黑树_rb-tree/P32_2-3-4树下溢回溯与根收缩.md#32.3_内部零键节点怎样继续传播)比较暂时下溢与逐层返回，沿 P06 编码/[P07 性质实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P07_红黑树_把_2-3-4_树映射成二叉表示.md#7.3.9_让程序区分三种非法结构)建立颜色、黑高和高度口径，用[区间折叠](../../knowledge/linux/data_structures/红黑树_rb-tree/P07_红黑树_把_2-3-4_树映射成二叉表示.md#7.5.12_保持键区间的折叠实验)核对多路分组，再运行[P35 完整插入](../../knowledge/linux/data_structures/红黑树_rb-tree/P35_红黑插入与红红冲突上推.md#35.3_运行完整插入程序)观察叔红上推和左右镜像，再用[P36 完整删除](../../knowledge/linux/data_structures/红黑树_rb-tree/P36_红黑删除与缺黑位置传播.md#36.3_用完整程序删除图中的对象)追踪后继原位置与缺口父槽，然后读[P33 删除对照](../../knowledge/linux/data_structures/红黑树_rb-tree/P33_从多路删除到红黑缺口.md#33.1_在两套规则建立以后对照)，经[P08 任务排序契约](../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#8.2.7_把排序契约变成可观察结果)、[根值实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#2%29_观察根值复制与对象存活)及[父色整数实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#8.4.3_为什么颜色可以使用指针低位存储)建立表示，再沿[P09 双成员实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_两个嵌入成员还原同一个任务)学习嵌入成员与还原前提，并运行[比较契约](../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_让同一个比较规则走两种调用路径)区分重复键与回调形式，以[入口关闭和持有引用](../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_两个入口关闭之后谁还在使用对象)继续追踪对象寿命，再由[P37 完整框架](../../knowledge/linux/data_structures/红黑树_rb-tree/P37_构建rbtree调用者接口.md#37.16_运行完整的私有调用者框架)组合接口与失败清理，再由[P12 缓存一致性](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行缓存一致性实验)检查额外入口的维护条件，并以[区间摘要](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行完整区间摘要实验)观察每节点聚合不变量；[双线程交接](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_用两个线程观察复制值与删除)补齐访问窗口与结果寿命，再以[完整示例回访](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#12.5_Linux_内核_rbtree_示例代码)复核比较与失败退出，最后用[有界快照](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行有界快照检查器)检查联合不变量与证据范围，再沿[固定场景](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#12.8_Linux_rbtree_在内核中的典型使用场景)比较容器键和业务条件；再沿[B/B+ 页模型](../../knowledge/linux/data_structures/红黑树_rb-tree/P13_再扩展到_B_树与_B+_树.md#%281%29_运行等值路由与叶分裂模型)区分记录归属、分隔副本和叶链；进入[Maple 树模式](../../knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.3_struct_maple_tree_树对象本身)后分别追踪共享根、调用者保护和节点资源，接着用[节点分区实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P38_Maple节点中的范围与空洞.md#38.5_运行包含空槽的分区程序)检查 pivot 等值、空槽与空洞窗口，再用[编码模型](../../knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.5.4_用定宽整数观察错误掩码)判断低位属于哪种字段，随后沿[游标周期](../../knowledge/linux/data_structures/红黑树_rb-tree/P39_Maple操作游标的暂停与继续.md#39.3_沿S0到S6比较暂停与重置)比较暂停、重置及锁边界，再用[普通范围操作](../../knowledge/linux/data_structures/红黑树_rb-tree/P40_Maple普通接口中的范围与查询.md#40.2_同一棵树中的覆盖与拒绝覆盖)区分覆盖与条件插入，并在[写入准备](../../knowledge/linux/data_structures/红黑树_rb-tree/P41_Maple写入准备与锁边界.md#41.3_沿S0到S5区分位置与资源)中追踪资源、重试与锁责任，再用[VMA 边界实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.11.4_运行边界等价性实验)区分包装转发与输入前提，最后用[两棵树的撤销周期](../../knowledge/linux/data_structures/红黑树_rb-tree/P42_撤销映射中的两棵Maple树.md#42.3_沿S0到S5观察职责转移)辨认主入口与后续处理集合，进入 P10 查询、P26 插入与[P11 取消周期](../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.1.3_一轮取消经过哪些状态)，接着在[P27 遍历单元](../../knowledge/linux/data_structures/红黑树_rb-tree/P27_Linux有序遍历与整树销毁.md#27.2.9_运行完整遍历与销毁模块)比较有序取消与整树销毁，再沿[同键替换](../../knowledge/linux/data_structures/红黑树_rb-tree/P28_Linux同键替换与旧对象退出.md#28.2.7_运行同键替换观察模块)区分位置交接与对象回收，回到[P29 旋转完成边界](../../knowledge/linux/data_structures/红黑树_rb-tree/P29_普通旋转与Linux修复的完成边界.md#29.1_为什么没有一一对应的旋转调用)比较局部回调与整轮返回，随后沿 P12 工程约束、[P34 页请求实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P34_从多路节点到页级索引.md#34.3_运行页请求与未命中的计数模型)进入 P13 页布局，再用[P14 G/H 区间实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P14_Maple_Tree_与_VMA_管理.md#14.9.3_运行G与H的区间模型)比较范围契约，由[固定查询源码](../../research/source_reading/maple_tree/navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)进入 P15 的状态与接口。
5. [kref 要解决的问题](../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md)，先用[责任交接模型](../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.6.1_运行完整的责任交接模型)分辨同步借用与独立持有，用[一次工作交付](../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.16.1_运行一次真实工作交付)检查归还与回调退出，再以[回绕与饱和](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.5.1_用八位模型观察回绕的代价)辨别原子操作与引用契约，沿[成员地址与回调](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.9_container_of_是理解_kref_的关键)重用同一个请求，再沿[普通引用源码](../../research/source_reading/kref/navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)核对责任、旧值和回调，以[静态模块](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.14.2_运行一个不释放静态内存的完整模块)区分计数与存储寿命，再用[快照对照](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.17.1_运行快照与持有的对照程序)检验计数读值与访问权限，通过[对象模板](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.19_标准自定义引用对象模板)检验部分初始化清理，再继续生命周期阶段和组合专题。
6. [devres API](../../knowledge/linux/object_lifetime/devres/devres_API说明.md)。

阶段验收：能解释嵌入式节点、容器对象、所有权、引用计数和资源托管的边界。

## 1.4\_第三阶段\_并发与事件

上一阶段的一个对象，现在可能同时被两个任务使用：一个正在读取，另一个准备更新或销毁。沿着这条变化学习本阶段，先画具体交错，再选择同步或等待方式。下列源码入口用于模型成立后的核对，不要求初读时同时打开所有实现。

1. 从[同步和异步机制总纲](../../knowledge/linux/synchronization_and_asynchrony/大纲.md)先区分“约束并发状态”和“让事件继续推进”两类问题。
2. 阅读[内存顺序](../../knowledge/linux/synchronization_and_asynchrony/synchronization/memory_ordering/大纲.md)，再进入[锁机制](../../knowledge/linux/synchronization_and_asynchrony/synchronization/locks/大纲.md)，先区分可睡与不可睡上下文，再沿[锁源码总阅读索引](../../research/source_reading/locking/navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.6_建议阅读顺序)核对 spinlock、mutex 与 rwsem；随后用 [Lockdep 专题](../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/大纲.md#1.1_专题定位)和[源码索引](../../research/source_reading/lockdep/navigation/P01_Linux_6.12_Lockdep源码导读.md#1.6_建议阅读顺序)把锁序、IRQ 上下文和持锁前置条件转成动态验证证据。
3. 对照学习[seqcount/seqlock](../../knowledge/linux/synchronization_and_asynchrony/synchronization/sequence_counters/大纲.md)与[RCU](../../knowledge/linux/synchronization_and_asynchrony/synchronization/rcu/大纲.md)，理解读重试和延迟回收解决的是不同问题；版本化实现分别从[序列计数器源码总阅读索引](../../research/source_reading/sequence_counters/navigation/P01_Linux_6.12_序列计数器源码总阅读索引.md#1.5_建议阅读顺序)和 [RCU 源码总阅读索引](../../research/source_reading/rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.6_建议的源码阅读顺序)进入。
   希望在同一阅读界面连续完成 RCU 稳定机制、源码导读、唯一实现讲解与实验时，可改走 [RCU MarkBook 当前月刊](../../markbook/topics/rcu/latest.html)；月刊是派生快照，不替代上述权威入口和评审状态。
4. 阅读[等待队列与完成量](../../knowledge/linux/synchronization_and_asynchrony/synchronization/waiting_notification/大纲.md)，掌握条件等待、事件完成和唤醒规则，再从[等待与完成量源码总阅读索引](../../research/source_reading/waiting_notification/navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.5_建议阅读顺序)还原入队、wake 与 done 令牌。
5. 从[异步机制大纲](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/大纲.md)进入，按序阅读[中断机制](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/interrupts/大纲.md)与[工作队列](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/workqueue/大纲.md)，并用[工作队列源码总阅读索引](../../research/source_reading/workqueue/navigation/P01_Linux_6.12_工作队列源码总阅读索引.md#1.6_建议阅读顺序)核对执行上下文、pool、worker 和 flush。进入 ARM 异常与 GIC 硬件层时，从[外部资料索引](../../reference/external_resources/arm/README.md#1.4_版本选择与阅读顺序)选择对应架构和控制器规范；该索引只提供规范证据，不代替 Linux IRQ/softirq 实现阅读。GIC 硬件另按 [GICv3 独立专题](../../platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图)从零学习，以 GIC-600 r1p6 / GICv3.0 为第一目标，GICv4 后续扩展。
6. 阅读[定时与延迟执行](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/timers/大纲.md)，区分忙等待、睡眠、timer、hrtimer 与 delayed work。

阶段验收：面对一段内核代码，能判断其执行上下文、能否睡眠、需要哪类同步以及退出时如何取消异步工作。

## 1.5\_第四阶段\_设备与I\_O

回到第一阶段的读取程序，此时我们已经有对象、持有者、等待和事件这些工具，可以继续解释路径怎样变成一次打开、设备怎样接入，以及退出时怎样结束仍在进行的操作。公共文件机制与具体设备实现应当沿同一条请求联系起来。

1. 按序阅读[VFS 子系统专题](../../knowledge/kernel_subsystems/vfs/大纲.md)，建立 path、mount、dentry、inode、file、页缓存和回收的完整模型。
2. 按序阅读[Linux 设备模型专题](../../knowledge/linux/device_model/大纲.md)。
3. 阅读[错误指针专题](../../knowledge/linux/error_handling/error_pointer/大纲.md#1.1_四次认识变化)。
4. 阅读[poll 与 epoll 的区别](../../knowledge/linux/io_model/blocking_io/poll与epoll的区别.md)。
5. 按序阅读[异步通知](../../knowledge/linux/synchronization_and_asynchrony/asynchrony/async_notification/大纲.md)。
6. 阅读[Linux 内核日志](../../knowledge/kernel_subsystems/tracing/logging/Linux_内核日志.md)，建立最基本的观测手段。

阶段验收：能描述路径和打开文件怎样进入 I/O，设备怎样注册、匹配和暴露节点，以及阻塞唤醒和异步通知怎样接回用户接口。

## 1.6\_阅读方法

- 先观察一个最小实例，再解释参与对象和动作；理解基本契约后尽早使用最小正确接口，不必先读完全部内部实现。
- 运行前写下预测，运行后解释差异；章末至少挑一个修改条件或排错题，用自己的话说明原因，答案用于对照推理而非背诵输出。
- 对版本敏感的实现记录内核版本；稳定文档只保留跨版本成立的模型。
- 使用[仓库内容索引](../indexes/content_index.md)查找扩展材料，使用实验或源码证据验证关键结论。
