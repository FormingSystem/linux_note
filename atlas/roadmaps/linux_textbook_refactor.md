---
id: atlas.roadmaps.linux_textbook_refactor
title: "Linux 教材重构蓝图与批次记录"
kind: track
status: evolving
domains:
  - navigation
  - linux
---

# 第1章\_Linux教材重构蓝图与批次记录

本轮目标是把 Linux 材料组织成可以连续学习的原创教材：借鉴《C Primer Plus》《C++ Primer Plus》循序渐进、实例带动、耐心解释和渐进练习的教学方式，不复制原书表达或目录。本文保存作者的工作型蓝图和批次结果；读者从 [Linux 内核机制学习路线](../tracks/linux_kernel_track.md)开始，人工评审结论仍只在 [评审地图](../maps/knowledge_review_map.md)维护。

## 1.1\_范围与基线

2026-09-21 盘点，Git 起点为 `d2a942520db0b5dd0ab9d8e71225119e1bbf5042`。计数包含大纲、参考页和正文，不等于已完成的教材章数：`knowledge/linux/` 207 篇、`knowledge/kernel_subsystems/` 27 篇、`knowledge/driver_model/` 48 篇，共 282 篇 Linux 主体材料。另有 `knowledge/system_software/` 18 篇作为系统构建扩展，`knowledge/foundations/` 28 篇按实际先修依赖审查。不能把“全部 Linux”缩成只处理名字叫 linux 的一个目录，也不因此改写无关基础知识。

本次采用 **rewrite**：用户先选定基础入口与阅读路线作为首批，随后授权全仓按同一标准持续推进。上面的统计保留初始 Linux 范围；当前全仓范围以[逐文件工作清单](../../governance/migration/textbook_refactor_inventory.json)为准，后续批次无须逐批等待确认。未实际处理的正文不标为已重构。已有人工批注、事实边界和有效知识点先盘点再处理。路径和稳定 ID 没有实际重组需要时保留，避免把改名数量当作进度。

源码研究、平台记录、实验和出版编排在全仓范围中分别按证据、平台边界、可复现过程和阅读组织的职责审查；不把它们全部套成同一种教材，也不把稳定正文与版本化导读改成镜像，不回写历史刊物。Codex skill 主源与仓库副本的同步关系见 [AGENTS.md](../../AGENTS.md#1.12_Codex_skill_仓库备份)。

## 1.2\_全量阅读依赖与批次

各批描述未来工作范围，不预建尚不存在的章节链接。表内入口均指向现有材料；“待进入”表示尚未完成本轮重写，与人工评审状态无关。

| 批次与范围 | 读者带入的认识 | 本批需要获得的能力 | 现有入口与执行状态 |
| --- | --- | --- | --- |
| B01 基础入口：architecture 中的概貌、源码树及 Atlas 路线 | 能读变量、函数、数组与循环；尚不认识内核对象 | 从读文件区分应用、内核、硬件；按问题找源码，区分源文件与产物 | [内核概貌](../../knowledge/linux/architecture/kernel_composition/linux内核概貌.md)、[源码树](../../knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md)；本批实施，验证结果见 1.5 |
| B02 模块与设备节点：architecture 其余 3 篇、error_handling 初始 2 篇、驱动 fundamentals 与 misc | 用户与内核边界、源码身份 | 区分装入代码、注册服务和建立访问入口；能恢复一个失败的最小模块实验 | [模块与设备节点](../../knowledge/linux/architecture/modules_and_device_nodes)、[错误处理](../../knowledge/linux/error_handling)、[驱动基础](../../knowledge/driver_model/fundamentals)；分批推进，已完成组与下一项见 1.5 |
| B03 对象组织：data_structures 33 篇 | C 指针、对象与资源 | 从查找和更新需求选择链表、哈希表或树，解释节点与容器关系 | [数据结构](../../knowledge/linux/data_structures)；链表十篇已完成本轮作者审查和适用验证，哈希桶、计算、hlist、RCU 旧路径及动态表机制已重构，新增动态接口实验；此前十六项教学语言复核已完成，子系统应用、综合模块与树仍待逐篇审查 |
| B04 生命周期：object_lifetime 20 篇 | 能辨别对象、入口和使用者 | 解释引用何时取得、由谁放弃、什么时候可销毁 | [对象生命周期](../../knowledge/linux/object_lifetime)；待进入 |
| B05 并发与事件：synchronization_and_asynchrony 125 篇 | 单个操作及对象生命期 | 从两条交错路径推出同步、等待、通知、延迟执行与回收；以具体状态完成证明 | [同步与异步总纲](../../knowledge/linux/synchronization_and_asynchrony/大纲.md)；待进入，内部再按依赖拆批；kernel_subsystems/irq 当前无正式文件，不建立占位入口 |
| B06 文件与观测：io_model 5 篇、kernel_subsystems/vfs 与 tracing | 读文件主线、等待与对象持有 | 串起路径、打开实例、数据、阻塞、缓存及日志证据 | [VFS](../../knowledge/kernel_subsystems/vfs/大纲.md)、[I/O 模型](../../knowledge/linux/io_model)、[观测](../../knowledge/kernel_subsystems/tracing)；待进入 |
| B07 设备与驱动：device_model 17 篇，driver_model 中 character_device、device_tree、gpio、gpio_consumers、input、platform_bus | 内核公共机制、文件入口 | 区分硬件描述、注册、匹配、请求处理和拆除，完成一个有恢复路径的设备实例 | [设备模型](../../knowledge/linux/device_model/大纲.md)、[驱动路线](../tracks/linux_driver_track.md)；待进入，各设备家族单独校准 |
| B08 系统构建扩展：system_software 18 篇及实际依赖的工程、平台记录 | 代码、配置、产物和运行系统的区别 | 解释引导程序、内核、根文件系统怎样接力，并能判断运行物是否来自预期构建 | [系统软件](../../knowledge/system_software)、[系统地图](../maps/linux_system_map.md#1.5_系统启动与构建)；待进入 |

```mermaid
flowchart LR
    a[读文件的可见结果] -->|建立应用与内核边界| b[源码与模块]
    b -->|识别对象和持有者| c[数据结构与生命周期]
    c -->|加入第二个执行者| d[并发与事件]
    d -->|解释等待和回收| e[文件与观测]
    e -->|把公共机制接到硬件| f[设备与驱动]
    b -->|追踪配置和构建产物| g[系统构建]
    f -->|结合目标平台验证| g
```

这是一条教材主线，不禁止按问题选读。中断上下文、内存顺序、锁、等待、RCU 和检查器有交叉依赖，B05 必须先建立正常执行、可睡眠条件和基本同步，再展开变体；不能把整批压成一章。

## 1.3\_共同写法与质量基准

教学代码与配套验证优先 C/C++，非必要不用 Python。C01a 已将六个既有整数、容器和发布模型改为五份 C11 程序及一份 C++17 程序；C01b 将设备与文件观察改为七份 C11 程序及一份 C++17 程序，完成此前十六项退回材料的适用检查。C01b 已编译 ARM 用户空间对象，未进行目标链接和运行；具体未通过的宿主链接解析观察及其边界保留在工作记录。历史检查记录保留，当前状态以逐文件清单为准。

每批先写出读者已经能解释什么、目前会怎样预测、本章要改变哪个认识。选一个足够小的贯穿任务，先得到一个可见结果，再改变一个条件。正文就地解释代码、输入、返回值和失败原因；旁支在需要时进入，避免先列完整分类再要求读者理解。

章末回顾应回答本章问题，练习按“预测 → 小修改 → 排错 → 迁移”选择合适梯度。答案给出判断过程与边界，不能只贴标准代码。参考页仍以查询为主，不要求每个条目编故事。规则和校准方法统一使用 [讲解写法与读者理解验收](../../tools/ai/codex/skills/build-linux-note-topic/references/explanation-quality.md)。

每批完成条件是正文已经兑现章节契约、示例经过适用验证、入口同步且局限如实记录。作者自查不等于真实读者试读；自动检查也不能升级人工评审状态。

## 1.4\_首批章节契约与内容去向

| 章节 | 贯穿任务与新增认识 | 原材料的处理 |
| --- | --- | --- |
| 内核概貌 | 读取一份文本，区分程序、进程、库函数、系统调用、文件状态、缓存及硬件；改变输入后预测结果 | 保留硬件、架构差异、进程、内存、文件系统、权限、挂载、驱动、模块等主题；按因果重新组织，纠正架构优劣泛化、实时性保证、所有文件操作都需 fd 等过度结论；原架构图保留为辅助视图 |
| 源码目录 | 由读文件问题定位文件处理、内存与硬件目录；判断缺少产物是否表示源码不完整 | 原 37 个条目按源码职责、配置构建、生成产物、版本管理归类；纠正 usr、firmware、git、调度目录和块层文件等错误，保留原 RCU 启动链深读入口并补总索引 |
| 内核路线与入口 | 明确第一篇从哪里读、两章之后能做什么、哪些材料只用于深化 | 保留原各专题入口与版本源码链接；把名词列表转为带问题、先修和退出条件的路线，避免暗示后续旧章已按新标准改完 |

术语顺序：程序 → 正在运行的进程 → 用户态与内核态 → 库函数与系统调用 → 路径与打开状态 → 缓存与设备 → 源文件与构建产物。源码追踪止于当前章节所需的目录、身份及契约，不提前展开 RCU 或文件系统内部算法。

## 1.5\_批次结果与续接点

B01 的两篇正文与阅读路线已重写。概貌从短文本读取进入应用、内核与设备的边界；源码树从同一次读取的问题进入目录定位。两章均包含预测、操作、解释、练习和解答。保留原文件路径与稳定 ID，已同步首页、Atlas 导航、系统地图、内容索引及评审地图的显示名称；原有出版物与 RCU 入链仍有效，无需为不变路径改写调用方。没有改写出版快照，也没有改变人工评审结论。

内容统计用于追踪本次 rewrite，不作为质量或内容守恒的替代证明：概貌从 250 行、24 个标题变为 200 行、9 个标题，新增 1 个完整 C 示例和 2 张 Mermaid 图；源码树从 245 行、37 个标题变为 136 行、8 个标题，新增职责与产物表、只读定位实验及 1 张 Mermaid 图。原先按名词反复列点的段落收拢为连续叙述，错误结论按 1.4 中的去向纠正；这不是仅排版任务，不用逐行一致作为验收标准。

本轮验证与边界：

- Codex 主源同步：6 个文件更新、1 个参考文件新增；完成后文件集与规范化内容一致。单向同步工具的 6 项临时仓库测试通过；skill 结构校验通过。
- 教学示例：从正文提取 C 程序，经 Windows MinGW GCC 的 C11 编译及警告检查；短输入、超长输入、空文件、含零字节输入、缺失文件和参数错误共 6 个场景通过。只验证了可移植 C 逻辑，未把它称为 Linux 系统调用跟踪或缓存实测。
- 源码定位：虚拟机恢复后，只读核对本地 NXP 仓库及官方固定提交，定位实验命中预期文件与读取系统调用定义。本地 3 笔实验提交不作为技术分析基线；本地配置不冒充官方发布配置。没有修改外部源码树。
- 两篇正文的连续阅读严格审计通过；4 张 Mermaid 图（含本蓝图）通过仓库所用 Mermaid 11.17.0 解析，未声称经过浏览器视觉验收。
- 概貌术语审计通过。源码树审计保留两项有理由的提示：VFS 是明确前置章已建立且本章回顾的概念，不重复整套定义；LICENSES 是已就地解释用途的目录名称，不是需要编造全称的缩写。其余首次使用问题已经补充或调整。
- 元数据、标题与链接检查通过；MSYS2 中 `format.sh doctor` 发现缺少 `python3`，因此使用本机 Python 直接执行三个仓库脚本内的原始检查程序，而未宣称统一 Bash 入口已通过。skill 目录按仓库约定排除正文标题与元数据格式化，但参与链接检查。
- 通用 `audit_topic.py` 假定目标具有 `大纲.md` 和 `PXX` 文件，不适用于本批保留身份的两篇独立入口；另会把既有忽略目录 `.cache/` 内快照误计为重复 ID。未为通过此检查制造空大纲或改动无关缓存；本批改以真实文件的元数据、标题、链接与连续阅读审计验收。

以上是作者自查与工具验证，尚无真实初学者试读结论。本批不暂存或提交。开发者随后将范围扩展至全仓，逐文件清单与续接入口见[全仓教材重构记录](../../governance/migration/repository_textbook_refactor.md#1.1_范围与恢复入口)。

B02a 已将旧 Makefile 教程重构为[模块构建与部署四章](../../engineering/build/kernel_modules/大纲.md#1.1_四章怎样连起来)，旧入口保留必要机制桥接。已完成检查与未执行的目标实验详见[本批结果](../../governance/migration/repository_textbook_refactor.md#1.4.1_B02a模块构建与部署)。B02 的模块与设备节点正文、字符设备模板仍在推进，其他批次不能因此标记完成。

B02b 已完成[字符设备读写契约](../../knowledge/driver_model/character_device/P05_文件操作契约与数据路径.md)及对应固定版本 I/O 证据，先补齐模板需要的部分复制、记录提交和互斥范围前提；设备号 P02 只修复节点创建路径，尚未完成全章重写。模型验证、源码核对、审计保留项与下一组见[本次续接记录](../../governance/migration/repository_textbook_refactor.md#1.4.2_B02b读写契约与复制进度)。

B02c 已将 P10 的混合模板拆成[有限窗口](../../knowledge/driver_model/character_device/P10_字符设备驱动模板.md)和[P13 环形流](../../knowledge/driver_model/character_device/P13_流式字符设备与等待通知模板.md)，同时重写 P11 的四种入口验证与 P12 分层排错，提供完整材料。ARM 语法、回调边界模型、注册回滚与文档检查的实际范围见[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.3_B02c窗口与环形流模板及运行排错)。尚未执行目标 Kbuild、装卸和并发实验；两份旧模块入口和 P02 仍待完成，不把本批成果等同于 B02 或字符设备全专题完成。

B02d 完成两份旧入口的内容迁移与正文重写，并重写字符设备 P02；新增[模块与设备入口大纲](../../knowledge/linux/architecture/modules_and_device_nodes/大纲.md#1.1_沿三个问题进入正文)，保留人工批注，以次号解码程序和只读观察衔接服务、路径及多实例身份。内容去向、固定源码核对与验证边界见[本批记录](../../governance/migration/repository_textbook_refactor.md#1.4.4_B02d模块入口与设备身份)。下一组进入错误指针与驱动基础，B02 和全仓仍未完成。

B02e 已把[错误指针专题](../../knowledge/linux/error_handling/error_pointer/大纲.md#1.1_四次认识变化)重构为返回契约、编码边界、资源回滚与完整观察实验四篇，并保留查询页。纠正 NULL/错误值混用、自动回滚误解及虚构源码，配套固定版本源码导读。源码、宿主模型、ARM 语法检查与未执行目标实验的边界见[本批记录](../../governance/migration/repository_textbook_refactor.md#1.4.5_B02e错误指针与资源失败)。下一组进入驱动框架、kobject 与 misc，随后处理 class 和 file_operations 长文，B02 与全仓仍未完成。

B02f 完成[驱动框架入门](../../knowledge/driver_model/fundamentals/framework_model/大纲.md#1.1_四个问题怎样接起来)：从多实例分离职责，追踪登记与绑定，再用完整 sysfs 属性和 misc 字符模块解释对象、回调及模块寿命。已核对固定源码并完成 ARM 语法、宿主失败路径与文档检查；真实目标装卸尚未执行。内容去向、保留理由和证明边界见[本批记录](../../governance/migration/repository_textbook_refactor.md#1.4.6_B02f驱动框架与两个最小入口)。下一组为 class 与 file_operations 两篇长文；B02 与全仓仍未完成。

B02g 已把原 file_operations 长文重构为[打开、迭代与映射四章](../../knowledge/driver_model/file_operations/大纲.md#1.1_沿对象寿命逐步增加约束)，原路径保留为成员参考，配套三个完整模块、映射用户程序与版本源码说明。ARM 语法、宿主分支、材料一致性和文档检查通过；真实 Linux 运行未执行。原知识去向、源码副本差异修正、保留项及证据边界见[本批记录](../../governance/migration/repository_textbook_refactor.md#1.4.7_B02g文件操作与打开寿命)。下一批为尚未完整冷读的 struct_class；全仓清单仍有待审项，不把本批视为全部完成。

B02h 完整冷读并重构 struct_class 长文，原路径保留[成员参考](../../knowledge/driver_model/fundamentals/kernel_driver_mechanisms/data_strcuture_说明/struct_class.md)，连续正文进入设备模型下的[class 与 sysfs 五篇教材](../../knowledge/linux/device_model/class_sysfs/大纲.md#1.1_从分类观察到可控数据入口)。两份完整模块分别建立纯属性分类和属性控制读取，固定源码解释发布与活动寿命。ARM、宿主分支及文档验证与未执行目标实验详见[本批记录](../../governance/migration/repository_textbook_refactor.md#1.4.8_B02h分类对象与属性事务)。下一批进入 B03 单链表；其他字符/设备模型正文及全仓仍待逐项审查。

B03a 已完整冷读并重构[链表九章与大纲](../../knowledge/linux/data_structures/单链表_linked_list/大纲.md#1.1_从一组任务走到容器选择)，以三个任务串起节点、拓扑、并发、发布、失败回滚和容器选择。保留稳定路径、人工批注与评审状态，新增完整材料及版本源码导读。实际验证和局限见[B03a 工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.9_B03a链表拓扑与发布)。下一批为哈希表及 hlist 的版本、算法和接口审查；树专题尚待逐章冷读，B03 与全仓均未完成。

B03b 完成哈希 P01 的完整 C 桶模型与 P03 的固定宽度计算教材，并同步纠正 P06 的 PID 版本事实；后者仍保留待审。源码、模型、导航和验证边界见[B03b 记录](../../governance/migration/repository_textbook_refactor.md#1.4.10_B03b哈希桶与计算契约)。开发者新增 C/C++ 优先要求，已将此前涉及 Python 教学示例的十六项退回待修订，下一批先处理这些实际程序和说明，再继续 hlist。此为新要求下的复核，不抹去历史检查结果，也不把此前验证视为新要求已经满足。

B03c 完成 hlist 入口槽与 RCU 旧路径两章、两份 C 程序及一个完整模块，新增节点模块导读与三份唯一实现讲解。人工批注及有效知识的去向、固定证据、检查结果和未执行目标实验见[B03c 记录](../../governance/migration/repository_textbook_refactor.md#1.4.13_B03c节点入口槽与RCU旧路径)。下一批继续动态容量、子系统应用与综合模块，全仓清单仍有待审项。

B03d 重写动态表 P05，并新增[P08 接口回收实验](../../knowledge/linux/data_structures/哈希表_Hash_Table/P03_高级进阶与性能调优/P08_rhashtable接口与回收实验.md#8.1_先固定本例的拥有者)。固定源码纠正三类标记混淆和无依据性能承诺，C 模型重放链尾重扫；证据、验证与未执行项见[B03d 记录](../../governance/migration/repository_textbook_refactor.md#1.4.14_B03d动态表迁移与接口回收)。下一批继续子系统应用和综合模块，全仓尚未完成。

B03e 完成 P06 子系统身份比较与 P07 固定桶完整模块：保留近期 PID 版本修正及人工批注，增加 C 完整身份模型、失败回滚与五篇源码导读/实现说明。证据、检查和目标运行限制见[B03e 记录](../../governance/migration/repository_textbook_refactor.md#1.4.15_B03e子系统身份与固定桶完整实验)。下一组进入树结构逐章冷读；全仓尚未完成。

B03f 完整冷读树 P01，保留原有术语、图和表示推导，将存储与实验移入[P16](../../knowledge/linux/data_structures/红黑树_rb-tree/P16_普通树的表示与构建实验.md#16.7_运行预测与资源回收)，补齐 C 程序的分配失败与回收，原人工状态不变。范围、保留理由与验证见[B03f 记录](../../governance/migration/repository_textbook_refactor.md#1.4.16_B03f树关系与表示实验)。下一批继续 P02 二叉树；未把首章拆分称为全树专题完成。

B03g 完整冷读 P02，并沿[二叉结构、四种遍历与查询路线](../../knowledge/linux/data_structures/红黑树_rb-tree/大纲.md#1.1_沿问题进入现有章节)拆为 P02/P17～P21，保留二十一张图和手工推演，补齐九份 C/C++ 完整材料的资源、容量和查询边界。内容去向、保留依据、验证与未执行项见[B03g 记录](../../governance/migration/repository_textbook_refactor.md#1.4.17_B03g二叉遍历与查询边界)。下一批进入 P03 搜索树；全仓尚未完成。

B03h 完整冷读 P03，将搜索插入、[删除回接](../../knowledge/linux/data_structures/红黑树_rb-tree/P22_BST删除与子树回接.md#22.2_从图中的替换走到地址上的回接)与[排序验证](../../knowledge/linux/data_structures/红黑树_rb-tree/P23_BST验证与高度边界.md#23.6_运行全子树边界反例)组织为三个单元，保留原二十八张图及批注，补齐三份 C/C++ 程序。证据与检查、内容去向和未执行项见[B03h 记录](../../governance/migration/repository_textbook_refactor.md#1.4.18_B03h搜索树插入删除与验证)。下一批从 P04 退化继续；全仓尚未完成。

B03i 完成[P04 退化观察](../../knowledge/linux/data_structures/红黑树_rb-tree/P04_为什么_BST_会退化.md#4.2.4_用节点访问次数观察退化)，新增完整 C 计数实验，区分相对失衡、维护代价和静态构建边界，保留原图解及人工重点并修正多路家族关系。证据、保留理由与验证见[B03i 记录](../../governance/migration/repository_textbook_refactor.md#1.4.19_B03i退化观察与平衡代价)。下一批进入 P05 旋转，全仓尚未完成。

B03j 完整冷读 P05，先完成[普通单旋与互逆实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P05_旋转的作用与局部重排.md#5.4.11_用根引用运行一对互逆动作)，保留图解及四个 C 示例，补齐根槽、写入依赖、独占前提和 C++ 完整程序。七键全排列检查及未执行项见[B03j 记录](../../governance/migration/repository_textbook_refactor.md#1.4.20_B03j普通左右旋与入口槽)。组合条件、AVL 诊断、Linux 证据与拆章继续推进，P05 仍保留待完成，全仓尚未完成。

B03k 将全部组合单元组织为[P24 形状判断](../../knowledge/linux/data_structures/红黑树_rb-tree/P24_组合旋转与形状判断.md#24.1_章节内容说明)，保留十一张图和内部例子，纠正形状即失衡的混淆，提供三份完整 C/C++ 材料与双旋拒绝检查。内容去向、验证及边界见[B03k 记录](../../governance/migration/repository_textbook_refactor.md#1.4.21_B03k组合旋转与形状诊断边界)。下一项继续 AVL 高度诊断与实现，P05 和全仓尚未全部完成。
