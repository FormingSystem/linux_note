---
id: repository.readme
title: "Linux 内核与驱动学习笔记"
kind: reference
status: evolving
domains:
  - repository
---

# 第1章\_Linux\_内核与驱动学习笔记

这是一个个人计算机系统与 Linux 学习知识库，内容覆盖基础理论、Linux 通用机制、内核子系统、驱动模型、系统软件、平台实现、实验、项目和源码研究。

仓库里的 Markdown 文件是当前笔记形态，也是后续重新排版成 Word/PDF 的素材。这里更重视学习过程中的结构沉淀、问题记录和可持续整理，不追求每一篇一开始就是最终出版形态。

仓库采用知识本体、工程应用、实践验证、研究证据、导航编排和仓库治理分层的信息架构。知识正文只保存一次，专题和学习路线负责组织链接，实验负责验证，源码阅读负责提供版本证据。正文保留统一阅读序号，出版清单负责多文档顺序和跨文档连续性；`markbook/` 再把同一专题散落在这些职责层中的材料编排成按月冻结、可全文检索的聚焦电子书。

完整设计见：[仓库信息架构设计](governance/architecture/repository_information_architecture.md)。

每篇 Markdown 都包含 `id`、`title`、`kind`、`status` 和 `domains` 元数据。`id` 用于长期引用，正文继续采用统一章节序号保证阅读定位。可用 `./format.sh check all --summary` 同时检查标题、元数据、路径和链接。

## 1.1\_仓库定位

本仓库统一管理计算机系统基础、Linux 通用机制、内核子系统、驱动模型、系统软件、平台实现、实验、项目和源码研究。技术方向通过知识地图和学习路线组织，不通过分支隔离内容。

教材示例和配套验证优先使用 C/C++。可移植整数与容器模型使用标准编译器运行；Linux 文件操作、模块和硬件实验分别注明目标环境，宿主模型通过不代表目标实验已经执行。

远端仓库为：

```text
https://github.com/FormingSystem/linux_note.git
```

## 1.2\_目录说明

| 目录 | 内容 |
| --- | --- |
| `atlas/` | 知识地图、学习路线、索引和路线图 |
| `knowledge/` | 基础知识、Linux 机制、内核子系统、驱动模型和系统软件 |
| `engineering/` | 工程方法、构建、移植、调试、测试和发布流程 |
| `platforms/` | 架构、SoC、开发板和 BSP 差异 |
| `labs/` | 验证单一结论的最小可复现实验 |
| `projects/` | 多机制组合的完整项目 |
| `research/` | 源码阅读、调用链、调查和基准证据 |
| `reference/` | API、命令、术语、标准和外部资料 |
| `publications/` | 书籍、文章、编排清单、模板和构建产物 |
| `markbook/` | 按月冻结的专题电子书、编排清单与生成工具 |
| `tools/` | 编辑器、Obsidian、AI 和仓库工具说明 |
| `governance/` | 架构、规范、模板、模式和迁移记录 |
| `assets/` | 图片、图表、附件、数据集和归档文件 |
| `AGENTS.md` | 给 AI 协作者读取的项目上下文 |

## 1.3\_知识库阅读地图

知识库使用一份独立的 [专题阅读与评审地图](atlas/maps/knowledge_review_map.md) 同时管理全仓阅读入口、专题评审状态和章节评审进度。它覆盖知识正文、工程方法、平台实现、实验、源码研究、参考与出版物，先回答“应该从哪里开始”，再让读者沿“领域 → 专题 → 章节”选择和跳转。

### 1.3.1\_专题评审状态

评审状态使用三种标识：🔴 **未校正**（默认）、🟡 **人工评审中**、🟢 **评审完成**。这套状态是面向读者的内容可信度提示，与 Front Matter 中表示维护阶段的 `draft`、`evolving`、`maintained`、`archived` 不是同一套状态。

🔴 **未校正**（默认）：

- 内容可能来自个人笔记、资料摘录和实验记录，也可能包含部分 AI 生成或辅助整理；红色不表示整篇内容都是 AI 生成。
- 内容入库前至少经过一次基础人工筛选，但尚未完成系统性的阅读、核对和校正，准确性、观察视角、组织方式及已有批注都可能存在偏差。
- 这些材料仍可用于了解专题范围、建立初步观察视角和发现后续线索；关键结论应结合官方文档、源码或实验继续复核。

🟡 **人工评审中**：

- 当前正在阅读和更新的章节。由于我很多时候会遇到一些新信息而为了了解这些信息做跳跃阅读，保证做到系统化阅读和理解，而非跳过这些旁支理解问题。所以可能会有各种评审记录。

🟢 **评审完成**：

- 这是我阅读完，完整体系之后觉得问题不大的专题，可放心阅读。在标记该专题的时候，我会需要做到2-3次人工阅读专题，确定阅读方式，源码追踪，知识点标记等完善之后才会标记。如你所见，哪怕我已经看过很多专题，但是都还在处于 🔴 阶段。

新专题和新章节一律从 🔴 开始，只有博主明确开始或完成人工评审后才能升级。专题状态由章节状态人工汇总，详细定义与当前进度只在独立地图维护，避免 README、专题元数据和多份表格相互漂移。

### 1.3.2\_打开交互式地图

- [知识库专题阅读与评审地图](atlas/maps/knowledge_review_map.md)：在 Obsidian 中使用 MarkMind 打开后，可以折叠、缩放，并直接点击领域、专题、章节或支撑材料节点跳转；评审状态只在这份地图中维护。
- 普通 Markdown 阅读器会把同一文件显示为层级大纲，不依赖插件也能阅读和点击。
- 地图已经展开所有正式专题的真实 `PXX` 章节；当前 🟢 **kref** 和 🟢 **红黑树** 各 15 章、🟢 **GNU C 扩展**、🟢 **U-Boot** 的 2 篇个人实验材料，以及地图 1.6、1.7、1.9 下已确认的工程方法、平台实现、14 篇个人实验、参考与出版物材料均保持原状态；🟡 **Lockdep** 的 P01～P09 已完成人工通读和批注，重构后仍处于待复核的人工评审中，新增加的 Lockdep 与 Sparse 实验从 🔴 **未校正** 开始，其余地图节点保持 🔴 默认状态。

## 1.4\_阅读方式

本仓库从零建立专题概念，不默认读者知道模块、平台、架构或代际之间的关系。阅读先沿“现实问题 → 朴素方案 → 缺口 → 新角色及其关系 → 一次完整过程 → 具体实现”推进，再比较已经分别讲清的对象；不必先背分类表。写作与冷读检查遵循[专题结构从零建立的方法](tools/ai/codex/skills/build-linux-note-topic/references/topic-design.md#12-让专题结构本身也从零建立)。

推荐使用 Typora 或 Obsidian 阅读。

准备运行第一个内核模块时，从[内核模块构建与部署](engineering/build/kernel_modules/大纲.md#1.1_四章怎样连起来)建立目标身份、构建与装载闭环，再沿[模块与设备入口](knowledge/linux/architecture/modules_and_device_nodes/大纲.md#1.1_沿三个问题进入正文)区分代码、服务、路径与多实例身份，进入字符设备和 I/O。

[错误指针专题](knowledge/linux/error_handling/error_pointer/大纲.md#1.1_四次认识变化)从普通 C 的返回契约进入编码、可选资源与失败回收，并用无硬件依赖的模块观察四条路径；[固定版本源码索引](research/source_reading/error_pointer/navigation/P01_Linux_6.12_错误指针源码阅读索引.md#1.2_按问题进入源码)将值转换与驱动清理分开核对。

[驱动框架入门](knowledge/driver_model/fundamentals/framework_model/大纲.md#1.1_四个问题怎样接起来)从多个实例共用一份代码进入登记、探测与解绑，再用完整的 sysfs 属性和 misc 字符设备实验区分可见入口、对象引用和模块寿命。

[文件操作教材](knowledge/driver_model/file_operations/大纲.md#1.1_沿对象寿命逐步增加约束)以 dup/关闭、readv/pread 和只读映射三个完整实验，区分打开上下文、请求位置与后备页寿命；原成员清单保留为固定版本查询页。

[class 与 sysfs 教材](knowledge/linux/device_model/class_sysfs/大纲.md#1.1_从分类观察到可控数据入口)从两个无设备节点的分类对象开始，再让属性控制字符读取，解释节点发布、权限、并发与撤销；配套完整模块及固定版本源码导读。

[Linux 链表教材](knowledge/linux/data_structures/单链表_linked_list/大纲.md#1.1_从一组任务走到容器选择)从三个任务的成员连接进入双向循环、共享修改、初始化发布和失败回滚，再比较子系统封装与容器代价；配有可运行的宿主模型、完整模块和[固定版本源码索引](research/source_reading/linked_list/navigation/P01_Linux_6.12_链表源码阅读索引.md#1.2_由结论进入唯一实现)。

[哈希表路线](knowledge/linux/data_structures/哈希表_Hash_Table/大纲.md#1.1_沿问题增加约束)从按编号查找进入桶与冲突；完整 C 程序观察键比较、重新分桶、位宽和 hlist 入口槽，RCU 模块追踪旧路径、回调与卸载边界，并由[计算源码索引](research/source_reading/hash_table/navigation/P01_Linux_6.12_哈希计算源码阅读索引.md#1.2_从问题选择入口)核对当前版本。动态表继续用 C 模型解释跨链重扫，并提供独立的[接口回收实验](knowledge/linux/data_structures/哈希表_Hash_Table/P03_高级进阶与性能调优/P08_rhashtable接口与回收实验.md#8.1_先固定本例的拥有者)。[子系统导读](research/source_reading/hash_table/navigation/P05_子系统索引身份与寿命导读.md#5.1_先区分索引任务与业务结论)继续比较候选、引用和业务状态，固定桶模块补齐增删查改与失败回滚。目标模块运行仍待验证，全仓其他专题继续逐项审查。

[树结构入门](knowledge/linux/data_structures/红黑树_rb-tree/大纲.md#1.1_沿问题进入现有章节)先建立父子、路径与子树，再用[普通树 C 实验](knowledge/linux/data_structures/红黑树_rb-tree/P16_普通树的表示与构建实验.md#16.7_运行预测与资源回收)比较存储表示并核对分配失败和递归回收。随后沿[四种遍历与查询路线](knowledge/linux/data_structures/红黑树_rb-tree/大纲.md#1.1_沿问题进入现有章节)运行完整 C/C++ 程序，比较处理时机、所有权及队列容量。[搜索树实验](knowledge/linux/data_structures/红黑树_rb-tree/P03_二叉搜索树_BST.md#3.7_运行查找与插入)继续检验查重、删除回接与全局排序；[退化计数实验](knowledge/linux/data_structures/红黑树_rb-tree/P04_为什么_BST_会退化.md#4.2.4_用节点访问次数观察退化)进一步区分键序与高度保证；[左右旋实验](knowledge/linux/data_structures/红黑树_rb-tree/P05_旋转的作用与局部重排.md#5.4.11_用根引用运行一对互逆动作)继续观察根槽、父链和中间子树；[组合旋转](knowledge/linux/data_structures/红黑树_rb-tree/P24_组合旋转与形状判断.md#24.11_内部子树上的_LR_重排反例)再区分方向识别与真实失衡，[AVL 高度实验](knowledge/linux/data_structures/红黑树_rb-tree/P25_AVL高度诊断与更新传播.md#25.23_运行完整高度维护程序)补齐状态更新与停止条件。[查找路径实验](knowledge/linux/data_structures/红黑树_rb-tree/P10_Linux_6.12_内核_rbtree_查找与返回边界.md#10.2.10_用完整C程序观察相等节点和旧路径)继续区分重复键、旧入口漏查和对象寿命，具体语句沿[固定源码索引](research/source_reading/rbtree/navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)核对；[五组插入模块](knowledge/linux/data_structures/红黑树_rb-tree/P26_Linux红叶接入与插入修复.md#26.3.15_在内核模块中观察五组插入)与[修复源码导读](research/source_reading/rbtree/navigation/P03_红叶接入与冲突修复导读.md#3.2_一轮插入怎样推进)进一步解释红叶、颜色上推和回调中间态；随后用[取消模块](knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.3.12_用完整模块观察取消请求)区分对象身份、缺黑与游离标记；[遍历与销毁模块](knowledge/linux/data_structures/红黑树_rb-tree/P27_Linux有序遍历与整树销毁.md#27.2.9_运行完整遍历与销毁模块)接着解释中序 next 与后序 safe 的修改边界。[同键替换](knowledge/linux/data_structures/红黑树_rb-tree/P28_Linux同键替换与旧对象退出.md#28.2.7_运行同键替换观察模块)进一步区分树位置、旧地址与回收条件。三项任务已分别组织为 P11 删除、P27 遍历、P28 替换，按大纲连续阅读。随后以[普通旋转与内核完成边界](knowledge/linux/data_structures/红黑树_rb-tree/P29_普通旋转与Linux修复的完成边界.md#29.1_为什么没有一一对应的旋转调用)回看父链、回调与版本差异。[多路节点查找实验](knowledge/linux/data_structures/红黑树_rb-tree/P06_2-3-4_树_从多路平衡到红黑树的结构桥梁.md#6.4.7_用完整C程序观察区间下行)补齐容量、区间和节点/键比较的区别。[多路插入实验](knowledge/linux/data_structures/红黑树_rb-tree/P30_2-3-4树插入与分裂时机.md#30.3_运行完整的两种插入)进一步比较溢出上推与预分裂，检查重复键和容量不足。[多路预修复删除](knowledge/linux/data_structures/红黑树_rb-tree/P31_2-3-4树预修复删除.md#31.3_运行完整预修复删除)继续观察借位、合并、根收缩和失败不改树。[下溢回溯实验](knowledge/linux/data_structures/红黑树_rb-tree/P32_2-3-4树下溢回溯与根收缩.md#32.4_运行完整回溯删除)再追踪内部零键节点、逐层报告与孩子移交。[多路与红黑缺口对照](knowledge/linux/data_structures/红黑树_rb-tree/P33_从多路删除到红黑缺口.md#33.3_用黑高收支检查父层是否仍有缺口)在性质建立后区分局部等高与父层吸收，补齐对象移位边界。[页级索引模型](knowledge/linux/data_structures/红黑树_rb-tree/P34_从多路节点到页级索引.md#34.3_运行页请求与未命中的计数模型)进一步用同一范围区分逻辑请求、驻留状态和物理块位置。[VMA 区间实验](knowledge/linux/data_structures/红黑树_rb-tree/P14_Maple_Tree_与_VMA_管理.md#14.9.3_运行G与H的区间模型)继续区分精确命中、向后查询、权限切分与空洞，并由[固定查询源码索引](research/source_reading/maple_tree/navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)解释返回指针的期限。[红黑性质实验](knowledge/linux/data_structures/红黑树_rb-tree/P07_红黑树_把_2-3-4_树映射成二叉表示.md#7.3.9_让程序区分三种非法结构)用同一棵树区分红红、黑高和祖先区间错误，并统一 NIL 与高度口径。[红黑插入实验](knowledge/linux/data_structures/红黑树_rb-tree/P35_红黑插入与红红冲突上推.md#35.3_运行完整插入程序)继续把红叶、叔红上推与内外侧旋转落实为完整 C++ 程序。[红黑删除实验](knowledge/linux/data_structures/红黑树_rb-tree/P36_红黑删除与缺黑位置传播.md#36.3_用完整程序删除图中的对象)追踪缺口父槽、后继身份和三种停止条件。[红黑映射实验](knowledge/linux/data_structures/红黑树_rb-tree/P07_红黑树_把_2-3-4_树映射成二叉表示.md#7.5.12_保持键区间的折叠实验)比较同一多路节点的两种二叉编码，保留孩子区间并区分左倾变体。[任务排序实验](knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#8.2.7_把排序契约变成可观察结果)区分复合键、索引与对象，并核对 Linux 比较辅助接口。[根值实验](knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#2%29_观察根值复制与对象存活)观察浅复制、局部清根与缓存入口。[父色编码实验](knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#8.4.3_为什么颜色可以使用指针低位存储)明确低位、位宽和游离约定，[布局导读](research/source_reading/rbtree/navigation/P07_节点布局与编码状态导读.md#7.2_沿一个节点的成员周期读写字段)连接固定定义与状态周期。[双成员 C 实验](knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_两个嵌入成员还原同一个任务)继续观察不同节点地址还原同一个任务，以及 const、复制与成员身份的边界。[比较契约实验](knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_让同一个比较规则走两种调用路径)用稳定 ID 与整数边界区分键政策和调用方式，性能结论保留实际测量前提。[双索引寿命模型](knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_两个入口关闭之后谁还在使用对象)继续追踪入口引用、读者持有与最后回收。[调用者完整框架](knowledge/linux/data_structures/红黑树_rb-tree/P37_构建rbtree调用者接口.md#37.16_运行完整的私有调用者框架)把插入持有权、复制查询、计数和初始化失败清理接成私有模块。[缓存一致性实验](knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行缓存一致性实验)继续比较普通左链与首地址，演示树正确而缓存过时的反例。[区间摘要实验](knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行完整区间摘要实验)接着追踪三个增强回调和不回溯查询的错误分支。[双线程复制实验](knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_用两个线程观察复制值与删除)区分锁内观察与锁外寿命。[示例回访](knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#12.5_Linux_内核_rbtree_示例代码)已统一到 P37 完整框架与 P27 遍历材料。[有界检查器](knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行有界快照检查器)已区分结构、颜色、缓存和摘要故障。[固定调用场景](knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#12.8_Linux_rbtree_在内核中的典型使用场景)已核对调度、定时器、块层和 epoll；P12 分批审查收束。[B/B+ 页布局](knowledge/linux/data_structures/红黑树_rb-tree/P13_再扩展到_B_树与_B+_树.md#%281%29_运行等值路由与叶分裂模型)已用完整 C++ 模型核对记录归属、等值路由与分裂。模块待目标装卸，其余章节继续逐篇审查。

[Maple 树根与模式](knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.3_struct_maple_tree_树对象本身)进一步区分共享根、外部锁声明和 RCU 节点资源路径，对应[固定源码导读](research/source_reading/maple_tree/navigation/P03_树对象与模式选择.md#3.2_从未发布到受保护使用)；[节点与空洞实验](knowledge/linux/data_structures/红黑树_rb-tree/P38_Maple节点中的范围与空洞.md#38.5_运行包含空槽的分区程序)已补齐 NULL 槽、包含式 pivot 和窗口裁剪；[编码整数模型](knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.5.4_用定宽整数观察错误掩码)已区分节点、父关系、entry 与操作状态；[游标暂停与继续](knowledge/linux/data_structures/红黑树_rb-tree/P39_Maple操作游标的暂停与继续.md#39.3_沿S0到S6比较暂停与重置)已用完整私有模块区分 pause/reset 和边界状态；[普通接口范围契约](knowledge/linux/data_structures/红黑树_rb-tree/P40_Maple普通接口中的范围与查询.md#40.2_同一棵树中的覆盖与拒绝覆盖)已补覆盖政策、两种清除与终止游标；[高级写入准备](knowledge/linux/data_structures/红黑树_rb-tree/P41_Maple写入准备与锁边界.md#41.3_沿S0到S5区分位置与资源)已区分资源成功、放锁重试与映射发布；[VMA 边界实验](knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.11.4_运行边界等价性实验)已核对局部游标、半开转换与错误映射；[撤销映射中的两棵树](knowledge/linux/data_structures/红黑树_rb-tree/P42_撤销映射中的两棵Maple树.md#42.3_沿S0到S5观察职责转移)进一步区分主索引撤下与对象退出；树专题本轮逐篇审查已收束，继续对象生命周期批次。[kref 责任交接](knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.6.1_运行完整的责任交接模型)从同步借用引出独立持有，以完整 C 模型比较接收、拒绝及最后归还；[一次工作交付模块](knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.16.1_运行一次真实工作交付)继续区分业务对象寿命与回调代码寿命；P01 已完成本轮实际审查，[回绕与饱和模型](knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.5.1_用八位模型观察回绕的代价)进一步说明引用原语的异常边界；[成员地址与回调](knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.9_container_of_是理解_kref_的关键)复用同一请求与已有固定宏实验，[普通引用固定源码](research/source_reading/kref/navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)已建立独立模块导读与唯一实现入口，后续静态初始化、条件接口与组合章节按依赖继续。

字符设备的[读写契约](knowledge/driver_model/character_device/P05_文件操作契约与数据路径.md)用字节记录解释短传输、复制失败和提交时机；对应的[版本源码入口](research/source_reading/character_device/navigation/P01_Linux_6.12_字符设备源码阅读索引.md#1.2_由问题进入模块导读)分别组织模块导读与唯一实现讲解。

[有限窗口模板](knowledge/driver_model/character_device/P10_字符设备驱动模板.md)、[构建运行](knowledge/driver_model/character_device/P11_构建运行与验证.md)与[环形流模板](knowledge/driver_model/character_device/P13_流式字符设备与等待通知模板.md)分别落实位置、设备入口和等待通知，附完整材料及渐进实验。

第一次进入 Linux 内核，可以从 [读取一份文件的小程序](knowledge/linux/architecture/kernel_composition/linux内核概貌.md#1.1_先让程序读到几个字)开始，再读 [从问题找到源码文件](knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md#1.1_先区分源码目录与正在运行的系统)。两篇基础正文已采用实例、逐步讲解、预测与练习相结合的教材写法；后续材料沿 [内核学习路线](atlas/tracks/linux_kernel_track.md)阅读，整体改写按 [重构蓝图](atlas/roadmaps/linux_textbook_refactor.md)分批推进。

[Typora Code](https://github.com/FormingSystem/typora_code) 是针对本仓库文档跳转链接多、源码阅读与修改频繁的场景制作的 Typora 阅读适配工具，围绕多文档阅读、链接定位、源码查看与编辑提供工作台增强。工具的配置、实现、测试和开发历史均在独立仓库维护，本仓库仅保留此链接与背景介绍。

Obsidian 主要用于维护 Markdown 链接。移动文件、重排目录时，优先使用 Obsidian 内部操作，让链接能够自动跟踪更新。

专题阅读从 [知识库导航](atlas/home.md#1.1_按目标进入)或 [仓库内容索引](atlas/indexes/content_index.md#1.2_Linux通用机制)进入。内核并发与事件先从[同步和异步机制总纲](knowledge/linux/synchronization_and_asynchrony/大纲.md)分流；锁、序列计数器、等待/完成量、RCU、Lockdep 与工作队列都在各自权威专题保存跨版本正文。需要核对 Linux 6.12.20 实现时，从[锁](research/source_reading/locking/navigation/P01_Linux_6.12_锁源码总阅读索引.md#1.6_建议阅读顺序)、[序列计数器](research/source_reading/sequence_counters/navigation/P01_Linux_6.12_序列计数器源码总阅读索引.md#1.5_建议阅读顺序)、[等待与完成量](research/source_reading/waiting_notification/navigation/P01_Linux_6.12_等待与完成量源码总阅读索引.md#1.5_建议阅读顺序)、[工作队列](research/source_reading/workqueue/navigation/P01_Linux_6.12_工作队列源码总阅读索引.md#1.6_建议阅读顺序)、[Lockdep](research/source_reading/lockdep/navigation/P01_Linux_6.12_Lockdep源码导读.md#1.6_建议阅读顺序)或 [RCU](research/source_reading/rcu/navigation/P01_Linux_6.12_RCU源码总阅读索引.md#1.6_建议的源码阅读顺序)的总阅读索引进入，再从模块导读跳到唯一函数实现标题。

需要连续阅读一个完整专题时，从 [MarkBook 专题电子书](markbook/README.md#1.2_当前刊物)进入。MarkBook 只做月度快照和出版编排，不替代上述权威正文；书中显示的版本、来源哈希和工作树状态用于还原当期内容。

面向 RK3588 的硬件中断控制器学习，从 [GICv3 物理中断专题大纲](platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图)进入：十篇中文命名正文从外设事件逐步讲到状态、配置、消息翻译与软件交接，另有 RK3588 集成记录。正文可独立阅读，关键概念附中英文术语与官方章节对照；GICv3 虚拟化、GICv4、逐函数源码与板上实验后续展开。与 Linux IRQ 管理的分工见[交叉阅读入口](atlas/roadmaps/gic_learning_plan.md#1.7_与现有中断章节的交叉阅读)。

## 1.5\_内容说明

- 任何以 Markdown 存在的文件，都可以视为笔记雏形。
- 如果某些章节以“第 1-n 章”组织，并带有明显引言、总结或分层结构，通常说明它经过 AI 辅助整理。
- AI 生成或辅助整理的痕迹不一定全部删除，因为它有时能作为阅读节奏点，提醒读者在某个模块处停下来总结。
- 专题或章节是否已经由博主完整阅读和整理，以[知识库专题阅读与评审地图](atlas/maps/knowledge_review_map.md)中的评审状态为准；没有标为 🟢 **评审完成** 的内容，不应被理解为已经通过人工正确性确认。
- 已完成评审的笔记会尽量保证自己读过、理解过、能复用，但仍不保证所有主题都覆盖到足够宽或足够深。
- 如果某个主题不够细，可以继续把相关 Markdown 交给 AI 或资料源二次扩展。

## 1.6\_Git\_提交规则

仓库使用本地 Git hook 校验提交信息。首次克隆后建议执行：

```bash
git config core.hooksPath .githooks
git config commit.template governance/templates/git_commit_message.txt
```

提交信息格式：

```text
<类型>[(<project>/<module>)]!?: <中文结果>

- 描述1
- 描述2
```

每次提交必须在标题后空一行，再写至少一条 `- 描述` 明细。明细是修改总结，应说明解决的问题、关键改动或改后结果，不能只重复标题，也不能用文件清单、文件数量或增删行数代替。下方示例展示标题写法，实际提交仍须补齐明细。

类型固定为：

```text
feat fix refactor perf security content docs test build ci release revert chore
```

示例：

```text
feat(publication/export): 支持按清单导出专题文档
refactor(repository/format)!: 统一文档检查入口
fix(reference/download): 修正外部资料摘要校验
content(knowledge/rcu): 补充宽限期状态汇聚过程
docs(repository/git): 更新分支与提交规范
```

详细规则见：

```text
governance/conventions/git_guide.md
```

## 1.7\_常用\_AI

常用 AI 工具：

1. ChatGPT
2. Gemini
3. DeepSeek

AI 主要用于主题拆解、章节扩写、概念对比、代码解释和结构整理。使用 AI 生成内容后，仍然需要人工阅读、校对和重排。

## 1.8\_参考资料

本文档和相关笔记主要参考：

| 书名 | 作者 | ISBN |
| --- | --- | --- |
| 《奔跑吧 Linux 内核入门篇》第二版 | 笨叔、陈悦 | 978-7-115-55560-1 |
| 《Linux 内核深度解析》 | 余华兵 | 978-7-115-50411-1 |
| 《Linux 设备驱动开发详解：基于最新的 Linux 4.0 内核》 | 宋宝华 | 978-7-111-50789-5 |

网络资料参考：

| 资料 | 来源 | 备注 |
| --- | --- | --- |
| Linux 驱动开发指南 | 正点原子 | 网络资料 |
| Linux 驱动开发指南 | 北京讯为电子 | 网络资料 |
| Linux 驱动开发指南 | 嘉立创-泰山派 | 网络资料 |



## 1.9\_版权与来源声明

本仓库是个人 Linux 内核与驱动学习笔记仓库，主要内容包括原创学习笔记、源码阅读记录、结构化整理、图示说明、实验记录和 AI 辅助整理后的 Markdown 文档。

仓库中的原创笔记、图示、分析说明和结构化整理内容，除特别说明外，按照本仓库根目录许可证发布。



### 1.9.1\_Linux\_kernel\_源码相关内容说明

本仓库的部分目录中可能包含基于 Linux kernel 源码整理的阅读材料，例如：

```text
research/source_reading/linux/
```

该目录名称保留为当前仓库历史结构，不代表其中内容是 Linux kernel 官方源码仓库，也不代表该目录下所有文件都是未经加工的原始源码文件。

其中以 `.md` 形式存在的文件，通常是基于 Linux kernel 源码文件整理出来的源码阅读笔记、源码摘录、源码注释、结构说明或个人理解记录。它们可能包含 Linux kernel 原始源码内容，也可能包含仓库作者添加的阅读注释、解释文字、章节标题、Markdown 排版和学习总结。

因此，这类文件应理解为：

```text
Linux kernel 源码阅读注释文档
```

而不是 Linux kernel 官方发布的原始源码文件。

### 1.9.2\_Linux\_kernel\_源码版权与许可证

Linux kernel 原始源码版权归其原作者和 Linux kernel contributors 所有。

Linux kernel 源码部分遵循其原始许可证声明，通常为：

```text
GPL-2.0-only WITH Linux-syscall-note
```

具体许可证应以 Linux kernel 原始源码中的以下信息为准：

```text
COPYING
LICENSES/
各源码文件中的 SPDX-License-Identifier
各源码文件中的 copyright 声明
```

本仓库不会通过根目录许可证重新授权 Linux kernel 原始源码内容，也不会将 Linux kernel 原始源码内容声明为本仓库作者原创内容。

### 1.9.3\_关于源码注释和改写

如果某些 `.md` 文件中包含 Linux kernel 源码内容，并在源码附近加入了个人阅读注释、解释性文字、Markdown 标题或结构化说明，则这些新增内容属于本仓库作者的学习整理内容。

但被引用、摘录或改写排版的 Linux kernel 源码本身，仍然保持其原始版权和许可证属性，不因出现在本仓库中而改变。

对于这类文件，应按以下方式理解：

```text
原始 Linux kernel 源码部分：遵循 Linux kernel 原始许可证；
新增阅读注释、解释、图示和学习总结：遵循本仓库原创内容许可证；
整体文档不得被理解为 Linux kernel 官方文件。
```

### 1.9.4\_非官方声明

本仓库不是 Linux kernel 官方文档，也不是 Linux kernel 官方源码镜像。

仓库中的源码阅读内容仅用于个人学习、知识整理、源码分析和笔记沉淀。由于笔记中可能包含个人理解、阶段性判断、AI 辅助整理内容或尚未最终校对的材料，因此不保证所有解释都与 Linux kernel 官方实现意图完全一致。

如需确认 Linux kernel 的准确实现、许可证边界或最新源码状态，请以 Linux kernel 官方源码仓库、官方文档以及原始文件中的许可证声明为准。

### 1.9.5\_外部厂商资料

Arm、NXP 与 Rockchip 在本节中都是公司或品牌专名。Arm 架构规范、处理器核手册、芯片数据手册、技术参考手册和开发板配套资料等外部文件，其版权、商标和许可仍归这些公司及其他原权利人所有。公开可下载不等于允许本仓库再次分发，根目录的 `GPL-2.0-only` 也不会重新许可这些第三方文件。

除非文件附带明确允许再分发且与本仓库用途相容的许可，新引入的外部厂商资料只在 `reference/external_resources/` 保存来源、版本、版权边界和完整性校验信息，实际文件由使用者从权利人入口下载到 Git 忽略的 `.cache/`。这类文件不得放入普通 Git、Git LFS、GitHub Release 或其他公共镜像。

当前 Arm、RK3588 与 i.MX6ULL 资料入口见 [外部资料索引](reference/external_resources/arm/README.md#1.1_索引定位与存储边界)。独立 GIC 专题从 [GICv3 物理中断大纲](platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图)阅读，长期扩展由[建设路线](atlas/roadmaps/gic_learning_plan.md#1.6_建设落点与当前交付边界)维护；资料可按 `--profile rk3588_gicv3` 选取。
