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
| B03 对象组织：data_structures 33 篇 | C 指针、对象与资源 | 从查找和更新需求选择链表、哈希表或树，解释节点与容器关系 | [数据结构](../../knowledge/linux/data_structures)；链表、哈希及子系统应用、综合模块、树原章节和依赖拆分单元已完成本轮逐篇审查与适用验证；教学语言复核已完成，目标未运行与研究算法边界逐批记录。继续 B04，不表示全仓完成 |
| B04 生命周期：object_lifetime 20 篇 | 能辨别对象、入口和使用者 | 解释引用何时取得、由谁放弃、什么时候可销毁 | [对象生命周期](../../knowledge/linux/object_lifetime)；B04a/b 已完成 P01 全篇问题链、责任模型与一次工作交付，P02 全章已按单元冷读并完成计数、布局、普通接口、类型模板和容器发布重构，固定源码、C/C++ 实验与导航已同步；P03 全章已完成业务、存储、交接和完整关闭协议重构，P04 三条规则已按借用、转交与查找协议收束，P05 接口全章已收束初始化、普通/条件取得、锁交接及类型契约，P06 已完成管理者等待、可见性、上下文、异步退出、RCU 排序和诊断，P07 已完成借用/转交、真实异步场景、成功失败契约和双协议完整请求，继续 P08 lookup 及后续机制逐篇审查 |
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

B03l 将诊断与回溯组织为[P25 AVL 更新传播](../../knowledge/linux/data_structures/红黑树_rb-tree/P25_AVL高度诊断与更新传播.md#25.23_运行完整高度维护程序)，保留原推导并补齐缓存、停止条件、完整 C 程序和多层删除观察。518400 组插删次序、失败回收及其他边界见[B03l 记录](../../governance/migration/repository_textbook_refactor.md#1.4.22_B03lAVL高度诊断与更新传播)。下一项为固定版本 Linux 旋转证据与 P05 收束，全仓仍待继续。

B03m 先完成[P10 查找与旧路径实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P10_Linux_6.12_内核_rbtree_查找与返回边界.md#10.2.10_用完整C程序观察相等节点和旧路径)，建立[固定源码查询入口](../../research/source_reading/rbtree/navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)，并同步 P05/P12 的并发事实。具体保留与纠错、验证及边界见[B03m 记录](../../governance/migration/repository_textbook_refactor.md#1.4.23_B03m查找契约与旧入口路径)。P05、P10、P12 均未标记整章完成，下一项继续插入修复的忠实源码与唯一展开；全仓继续。

B03n 修订 P10 的红叶接入与插入修复，补齐完整私有内核模块，并恢复固定上游语句与共享父槽的唯一实现。范围、C 算法验证及目标未运行边界见[B03n 记录](../../governance/migration/repository_textbook_refactor.md#1.4.24_B03n插入修复与父槽交接)。下一项先将查询与插入完整组织为两个阅读单元，再继续删除、后继、替换与 P05 收束；P10 仍 pending，全仓尚未完成。

B03o 将已修订内容完整组织为[P10 查询](../../knowledge/linux/data_structures/红黑树_rb-tree/P10_Linux_6.12_内核_rbtree_查找与返回边界.md#10.1_从业务对象走到一条查询路径)与[P26 插入](../../knowledge/linux/data_structures/红黑树_rb-tree/P26_Linux红叶接入与插入修复.md#26.1_章节内容说明)，保留 193 个代码围栏和全部有效推导，完成路径/锚点同步。范围和未执行项见[B03o 记录](../../governance/migration/repository_textbook_refactor.md#1.4.25_B03o查询与插入阅读分工)。下一项 P11 删除、后继与替换，全仓仍未完成。


B03p 完成[P11 删除周期与完整模块](../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.3.12_用完整模块观察取消请求)，保留原中文源码注释到唯一实现，修正后继身份、重复键、缺黑父槽和回调中间态。范围、有限 C 验证及目标未运行边界见[B03p 记录](../../governance/migration/repository_textbook_refactor.md#1.4.26_B03p对象摘除与缺黑修复)。P11 遍历/替换及整章组织继续 pending，全仓尚未完成。


B03q 完成[P27 遍历与销毁单元](../../knowledge/linux/data_structures/红黑树_rb-tree/P27_Linux有序遍历与整树销毁.md#27.2.9_运行完整遍历与销毁模块)，以四键漏访反例解释中序取消与后序 safe 的不同条件，补齐完整模块、失败清理和唯一遍历实现。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.27_B03q有序推进与整树销毁)区分宿主/ARM 检查与目标未运行。下一项 P11 替换及最终分章，P11 与全仓仍未完成。


B03r 完成[P28 同键替换单元](../../knowledge/linux/data_structures/红黑树_rb-tree/P28_Linux同键替换与旧对象退出.md#28.2.7_运行同键替换观察模块)，按 R0～R4 解释节点复制、回指、发布、缓存入口和旧对象回收，补齐完整 C 模块与唯一源码。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.28_B03r同键替换与旧对象退出)区分有限串行检查与目标/并发未验证。下一项 P11 最终分章，随后继续 P05 证据收束与工程扩展，P11 与全仓尚未完成。


B03s 将已修订内容完整组织为[P11 删除](../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.1.3_一轮取消经过哪些状态)、[P27 遍历](../../knowledge/linux/data_structures/红黑树_rb-tree/P27_Linux有序遍历与整树销毁.md#27.1_从一次取消走到整轮处理)和[P28 替换](../../knowledge/linux/data_structures/红黑树_rb-tree/P28_Linux同键替换与旧对象退出.md#28.1_从保存地址走到交接地址)，保留全部有效推导、图与完整模块，同步入口和标题定位。守恒检查与边界见[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.29_B03s删除遍历与替换阅读分工)。下一项 P05 历史源码收束，然后继续尚待冷读的红黑理论/接口与 P12 工程扩展；全仓尚未完成。


B03t 完成 P05 收束：普通单旋与既有完整材料保留，内核部分移入[P29 旋转完成边界](../../knowledge/linux/data_structures/红黑树_rb-tree/P29_普通旋转与Linux修复的完成边界.md#29.1_为什么没有一一对应的旋转调用)，在修复前提建立后解释回调中间态、比较辅助接口与可证版本差异。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.30_B03t普通旋转与内核完成边界)保存内容去向和验证限制。下一项从 P06 多路与红黑桥梁开始实际冷读，再推进 P07～P09/P12；全仓尚未完成。


B03u 完成 P06 的 6.1～6.4 读者入口、容量/区间和[完整 C 查找](../../knowledge/linux/data_structures/红黑树_rb-tree/P06_2-3-4_树_从多路平衡到红黑树的结构桥梁.md#6.4.7_用完整C程序观察区间下行)，保留五张原图，用高度界和有限模型区分定义、维护与性能。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.31_B03u多路节点与区间查找)说明实际范围；P06 的更新、红黑映射和工程扩展仍 pending，下一项继续 6.5 插入的两种分裂时机，不能把前四节称为整章完成。


B03v 将 P06 插入单元完整组织为[P30 两种分裂时机](../../knowledge/linux/data_structures/红黑树_rb-tree/P30_2-3-4树插入与分裂时机.md#30.3_运行完整的两种插入)，保留十一张图与推演，用完整 C++ 程序补齐临时容量、查重、精确资源预留和失败不改树。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.32_B03v多路插入与分裂时机)说明形状差异、保留依据与有限验证；P06 删除、红黑映射和扩展仍 pending，下一项继续 6.6 删除。


B03w 完整冷读 P06 原 6.6.1～6.6.9，组织为[P31 预修复删除](../../knowledge/linux/data_structures/红黑树_rb-tree/P31_2-3-4树预修复删除.md#31.3_运行完整预修复删除)，保留四组图解、补齐内部命中三分支和资源寿命，提供完整 C++ 程序。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.33_B03w多路预修复删除)记录有限验证及范围；P06 的自底向上、红黑对照和后续扩展继续 pending，不把本单元称为全部删除重构完成。


B03x 将 P06 自底向上单元组织为[P32 下溢回溯](../../knowledge/linux/data_structures/红黑树_rb-tree/P32_2-3-4树下溢回溯与根收缩.md#32.3_内部零键节点怎样继续传播)，保留原图例并补齐零键内部节点、孩子移交、完整 C++ 程序和同场景比较。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.34_B03x多路下溢回溯)记录范围与验证；下一项为 P06 红黑删除对照的前置、事实与阅读位置，映射及后续扩展仍 pending。


B03y 冷读并修订 P06 叶层/编码/回顾，将成熟删除比较组织为[P33](../../knowledge/linux/data_structures/红黑树_rb-tree/P33_从多路删除到红黑缺口.md#33.1_在两套规则建立以后对照)，安排在 P07 定义之后，修正合并传播、阶段容量、后继身份与哨兵父槽。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.35_B03y多路编码与红黑缺口)区分黑高算术检查与完整实现；P06 页级索引/Maple 和 P07 全篇仍 pending，下一项继续 P06 6.9 实际冷读。


B03z 完整冷读 P06 原 6.9，组织为[P34 页级索引](../../knowledge/linux/data_structures/红黑树_rb-tree/P34_从多路节点到页级索引.md#34.3_运行页请求与未命中的计数模型)，保留原概念和应用推演，修正页/缓存层次、相等键路由、定位符与读写比较；完整 C++ 四页模型解释请求减少为何不等于未命中同比减少。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.36_B03z页级索引与访问成本)记录官方资料和固定 ext4 格式证据。下一项 P06 6.10 Maple；P06 整章及全仓仍未完成。

B03aa 完整冷读 P06 原 6.10 与 P14，将重复的 VMA 概念收拢至[P14 范围实例](../../knowledge/linux/data_structures/红黑树_rb-tree/P14_Maple_Tree_与_VMA_管理.md#14.9.3_运行G与H的区间模型)，保留 G/H 推演并修正权限、pivot、TLB 与历史数据边界；新增 C++ 区间模型和[固定查询源码](../../research/source_reading/maple_tree/navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.37_B03aa范围契约与Maple入口)区分有限模型与目标运行。P06 分批内容已审完，下一项按依赖继续 P07；P15 只迁移四个查询函数体，其余仍待实际冷读。

B03ab 实际冷读 P07 的 7.1～7.3、7.4.1 和 7.6，补[静态性质检查器](../../knowledge/linux/data_structures/红黑树_rb-tree/P07_红黑树_把_2-3-4_树映射成二叉表示.md#7.3.9_让程序区分三种非法结构)、统一黑高/高度口径并完成节点数归纳证明。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.38_B03ab红黑性质与高度证明)保留其余长单元为 pending，下一项继续 7.4.2 插入冲突；本批不把完整图或有限枚举当成全章完成。

B03ac 完整冷读 P07 原 7.4.2，将二十三张图与上下文提问组织为[P35 插入](../../knowledge/linux/data_structures/红黑树_rb-tree/P35_红黑插入与红红冲突上推.md#35.1_从检查一棵树走到增加一个键)，补完整 C++ 程序、黑贡献证明、根收尾与多层上推；P07 保留桥接。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.39_B03ac红黑插入与角色上推)区分算法验证与目标运行，下一项继续 P07 删除缺黑，整章及全仓仍未完成。

B03ad 完整冷读 P07 原 7.4.3，组织为[P36 删除周期](../../knowledge/linux/data_structures/红黑树_rb-tree/P36_红黑删除与缺黑位置传播.md#36.1_移走对象为何不一定马上产生缺口)，保留三十七张原图与全部分支，补缺口父槽、黑贡献证明、完整 C++ 删除及身份练习。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.40_B03ad红黑删除与缺口父槽)记录边界；下一项继续 P07 其余概念、映射和回顾，整章与全仓尚未完成。

B03ae 完整冷读 P07 剩余概念、映射与回顾，保留三十一张图，补[区间折叠实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P07_红黑树_把_2-3-4_树映射成二叉表示.md#7.5.12_保持键区间的折叠实验)，纠正编码容量、左倾变体和普通旋转的颜色边界。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.41_B03ae红黑映射与全章收束)记录各单元审查依据；P07 经连续批次完成适用检查，下一项 P08 工程结构，全仓仍未完成。

B03af 完成 P08 的入口与选型单元（8.1、8.2），以[任务排序实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#8.2.7_把排序契约变成可观察结果)建立对象身份、复合键和索引所有权，纠正固定版本比较辅助接口与历史场景边界。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.42_B03af业务排序与工程入口)列出检查与限制；P08 的 8.3、8.4 仍待完整冷读和重构，整章保持 pending，下一项继续节点表示。

B03ag 保留 P08 已讲清的根初始化推导，补[根值与对象寿命实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#2%29_观察根值复制与对象存活)，修正空树下行、游离约定和增广删除文件归属。[记录](../../governance/migration/repository_textbook_refactor.md#1.4.43_B03ag根值与游离约定)区分宿主执行、ARM 前端检查和未执行目标验证；P08 后半已完整冷读，但父色布局、重复实现片段与其他结构单元仍需整改，整章保持 pending。

B03ah 收束 P08 剩余表示单元，补[父色整数实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#8.4.3_为什么颜色可以使用指针低位存储)及[布局状态导读](../../research/source_reading/rbtree/navigation/P07_节点布局与编码状态导读.md#7.2_沿一个节点的成员周期读写字段)，集中十八处固定定义片段，纠正第二低位、位宽、重复键、缓存取舍和无锁查询的前提。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.44_B03ah父色编码与表示收束)列出保存依据与验证边界；P08 各单元已实际冷读和适用检查，下一项 P09，全仓继续。

B03ai 冷读并修订 P09 的 9.1 与 9.2.1～9.2.4，以[双成员 C 实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_两个嵌入成员还原同一个任务)区分偏移、成员身份、const 和对象复制；保留原布局图，修复复合键图的双左孩子错误。新增固定 container_of 实现讲解，细节见[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.45_B03ai嵌入成员与对象还原)。P09 后续比较、性能、寿命及接口单元继续，整章仍 pending。

B03aj 完成 P09 的 9.2.5～9.2.7 冷读与修订，补[比较契约 C 实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_让同一个比较规则走两种调用路径)，将无条件地址排序改为稳定业务 id，区分插入方向和旋转后等价区间，并展开间接调用与缓存地址依赖。[批次记录](../../governance/migration/repository_textbook_refactor.md#1.4.46_B03aj比较政策与成本边界)记录验证和保留依据；下一项节点寿命，P09 仍 pending。

B03ak 收束 P09 的 9.2.8～9.2.9，[双索引寿命 C 模型](../../knowledge/linux/data_structures/红黑树_rb-tree/P09_Linux_6.12_内核_rbtree_嵌入式节点与使用者接口.md#%281%29_两个入口关闭之后谁还在使用对象)以 S0～S5 连接入口槽、引用持有和最终回收，修正 RCU 片段的旧字段改写与锁/RCU/引用计数的混用。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.47_B03ak入口引用与回收条件)记录有限验证；下一项 9.3，整章仍 pending。

B03al 将 P09 原使用者长单元独立为[P37 调用者框架](../../knowledge/linux/data_structures/红黑树_rb-tree/P37_构建rbtree调用者接口.md#37.16_运行完整的私有调用者框架)，保留图解并修正重复键、比较、根槽、移除输出与并发边界；补完整内核 C 示例、U0～U5 和失败清理。P09 留有阅读桥接，原人工状态不变，P37 默认红色；[记录](../../governance/migration/repository_textbook_refactor.md#1.4.48_B03al调用者框架与完整示例)说明验证及未执行目标项。下一项 P12 工程扩展，全仓继续。

B03am 完成 P12 引入与 cached 单元冷读，以[完整缓存模块](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行缓存一致性实验)演示缓存独立不变量、插入/删除返回和普通删除反例，新增固定模块导读及唯一实现入口。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.49_B03am最左缓存与一致性)记录验证边界；P12 增强树、并发和后续单元仍 pending。

B03an 完成 P12 增强单元，以[闭区间模块](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行完整区间摘要实验)推导摘要的维护代价、三个回调和 A0～A5，补齐旋转继承前提、后继两段传播及偏大也会漏查的反例；[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.50_B03an子树摘要与增强回调)区分宿主语义与目标运行。下一项 12.4 外部同步，P12 与全仓仍未完成。

B03ao 完成 P12 并发单元冷读，以[双线程 C 实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_用两个线程观察复制值与删除)区分锁内副本与对象寿命，并核对查重事务、向下漏查、父链及 RCU 发布边界；[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.51_B03ao并发观察与结果寿命)保留有限验证范围。下一项 12.5 完整示例的职责与调用关系，P12 和全仓继续。

B03ap 冷读 P12 12.5，按九个原知识单元[回访完整调用者示例](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#12.5_Linux_内核_rbtree_示例代码)，统一 P37 程序的比较函数并收拢重复片段，保留遍历/销毁与寿命边界；[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.52_B03ap调用者实例回访与比较收敛)逐项说明去向和重验。下一项 12.6 验证，P12 和全仓仍未完成。

B03aq 完成 P12 12.6/12.7 冷读与改写，用[完整 C 检查器](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#%281%29_运行有界快照检查器)建立已知对象、独立摘要、祖先界与错误返回，再以具体反例收束接口误区；[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.53_B03aq联合不变量与有限证据)保留验证限制。下一项 12.8 固定内核场景与章末收束，P12/全仓继续。

B03ar 完成 P12 剩余[固定内核场景](../../knowledge/linux/data_structures/红黑树_rb-tree/P12_Linux_6.12_内核_rbtree_工程扩展_并发与验证.md#12.8_Linux_rbtree_在内核中的典型使用场景)和章末收束，核对十份官方固定文件与十八个函数，纠正调度排序、timerqueue 返回、epoll 注册/就绪及范围端点的混同；[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.54_B03ar真实调用场景与P12收束)记录版本和未执行项。P12 经连续批次完成实际审查，下一项 P13，树专题与全仓仍未完成。

B03as 完成 P13 全篇冷读与改写，保留二十键贯穿实例和七张机制图，纠正 B 树记录重复、B+ 相等路由与分裂分隔混用；[完整页模型](../../knowledge/linux/data_structures/红黑树_rb-tree/P13_再扩展到_B_树与_B+_树.md#%281%29_运行等值路由与叶分裂模型)验证固定高度下的插入、查询及失败不改状态。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.55_B03as页记录归属与叶分裂)列出检查和边界。下一项 P15；树专题和全仓继续。

B03at 完成 P15 开篇及 15.1～15.3，按固定实现纠正树根直存、外部锁声明与 RCU 节点复用的混同；[树模式导读](../../research/source_reading/maple_tree/navigation/P03_树对象与模式选择.md#3.2_从未发布到受保护使用)连接七个唯一函数和字段定义。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.56_B03at共享树与模式责任)记录范围与未运行项。下一项 15.4 节点布局及 15.5 编码，P15 与全仓仍未完成。

B03au 将 P15 15.4 落实为[P38 节点与空洞](../../knowledge/linux/data_structures/红黑树_rb-tree/P38_Maple节点中的范围与空洞.md#38.1_从一个共享根继续向下)，保留七 VMA 地址图并修正 NULL 分区、叶/非叶职责、构建容量及 union 解释；完整 C++ 模型与固定布局分别验证语义和字节边界。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.57_B03au节点分区与空洞成本)记录证据和局限。下一项 P15 15.5 编码，全仓继续。

B03av 完成 P15 15.5 [字段编码](../../knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.5.4_用定宽整数观察错误掩码)，区分节点类型、父槽、两种根标记、保留 entry 与独立错误状态，补齐完整 C11 整数模型及固定 helper。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.58_B03av字段编码与状态边界)列出静态证据和模型限度。下一项 15.6/15.7 游标状态周期，P15 与全仓仍未完成。

B03aw 完成 P15 15.6 的[P39 游标周期](../../knowledge/linux/data_structures/红黑树_rb-tree/P39_Maple操作游标的暂停与继续.md#39.3_沿S0到S6比较暂停与重置)拆分与实际改写，核对 15.7 八段查询场景并修正快速点查状态承诺；完整私有模块和源码模块按 S0～S6 比较 pause/reset 与边界。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.59_B03aw游标暂停与查询状态)保留目标未运行限制。下一项 P15 15.8 普通 API，全仓继续。

B03ax 完成 P15 15.8 的[P40 普通接口](../../knowledge/linux/data_structures/红黑树_rb-tree/P40_Maple普通接口中的范围与查询.md#40.2_同一棵树中的覆盖与拒绝覆盖)拆分与实际改写，原三个小节保留桥接，八个普通函数改为唯一源码展开。完整 C 模块比较覆盖、拒绝覆盖、两种清除及终止游标；[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.60_B03ax普通接口范围契约)区分前端/宿主和未执行的目标验证。下一项 P15 15.9 高级写入资源与锁协议，全仓继续。

B03ay 完成 P15 15.9 的职责改写与[P41 写入准备](../../knowledge/linux/data_structures/红黑树_rb-tree/P41_Maple写入准备与锁边界.md#41.3_沿S0到S5区分位置与资源)完整单元，以外部锁私有模块串起准备、取消、兑现与清理；原 API 分组与分层图保留并纠正边界，两处 vma_find 函数体移为唯一实现入口。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.61_B03ay写入准备与资源协议)记录验证限制。下一项 P15 15.10/15.11 的 VMA 接入与包装契约，全仓继续。

B03az 完成 P15 15.10/15.11 的 VMA 接入与边界改写，保留原两节入口和两张图的教学任务，补状态所有权、两种初始化、错误映射与完整[C 边界实验](../../knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.11.4_运行边界等价性实验)。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.62_B03azVMA游标与边界适配)记录固定包装验证及真实目标限制。下一项 P15 15.12～15.17 查询场景与收束，全仓继续。

B03ba 冷读并收束 P15 15.12～15.17：查询/前驱例子按固定契约保留，补两个输出与返回范围边界；原 E/F/G 撤销进入[P42 两棵树](../../knowledge/linux/data_structures/红黑树_rb-tree/P42_撤销映射中的两棵Maple树.md#42.3_沿S0到S5观察职责转移)，总图纠正为职责图，未落地的旧第16～19章安排替换为真实模块与研究边界。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.63_B03ba查询收束与撤销两棵树)记录八函数、分区模型和未执行项。P15 及 B03 数据结构正文完成本轮逐篇审查；下一项 B04 kref 问题入口与生命周期路线，全仓继续。

B04a 进入 kref：P01 前六节从同步借用推进到独立持有，以[完整 C 模型](../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.6.1_运行完整的责任交接模型)解释引用份额、成功/拒绝交付及 S0～S5 回收周期，保留后半章与原人工状态。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.64_B04a引用责任与交付周期)记录固定两份证据和有限测试；下一项 P01 1.7～1.20 的访问边界及异步反例，整章仍 pending。

B04b 收束 P01 1.7～1.20：修正字段锁、设备在线、lookup 取引用窗口、计数快照和 handoff 的证明边界，用[完整内核 C 模块](../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.16.1_运行一次真实工作交付)解释预留、拒绝归还及回调代码退出。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.65_B04b访问边界与异步交付)记录五路径替身检查及未执行目标验证。P01 经两批完成作者审查，人工状态保留；下一项 P02 源码结构与权威实现入口，全仓继续。

B04c 实际重写 P02 2.1～2.6，按固定版本建立对象/kref/refcount/atomic 层次，纠正仅为类型包装的解释，以[八位 C 模型](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.5.1_用八位模型观察回绕的代价)展开假零、饱和泄漏及模型/实现差异。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.66_B04c计数层次与饱和契约)记录 1001 组模型检查和证据范围。下一项 P02 2.7～2.12 嵌入布局与回调；P02 仍 pending，全仓继续。

B04d 实际重写 P02 2.7～2.12，保留原阅读批注，沿[非首成员的回调地址](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.9_container_of_是理解_kref_的关键)讲清嵌入存储、类型配对、独立控制块的额外责任及初始引用；复用既有宏讲解和双成员实验。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.67_B04d嵌入地址与回调契约)记录五路径与 ARM 重验边界。下一项 P02 2.13 的具体实现及与 P05/P11 的重复展开，P02 和全仓仍未完成。

B04e 重构 P02 2.13 并建立[普通引用源码路线](../../research/source_reading/kref/navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)，将同版本普通函数/类型集中为唯一展开，保留 P02/P05/P11 的应用与概念职责，纠正 signed_wrap、must_check 与返回值边界。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.68_B04e普通引用链与编译边界)记录 21 个固定 helper 边界与编译诊断。下一项 P02 2.14 静态初始化及后续单元，相关整章与全仓仍未完成。

B04f 完成[P02 静态初始化](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.14.2_运行一个不释放静态内存的完整模块)及 P05 对应入口，补逐层宏证据、自动/静态存储区分与完整模块。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.69_B04f静态初始化与存储寿命)记录语法正反例和验证边界；下一项 P02 2.15 以后，整章与全仓继续。

B04g 完成[P02 普通引用与观察](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.17.1_运行快照与持有的对照程序)及 P05 5.5～5.6，补完整 C 对照、带动作的状态地址图和归还时序。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.70_B04g普通操作与计数快照)记录边界与验证；下一项 P02 2.19 以后的完整对象模板，整章与全仓仍未完成。

B04h 完成[P02 对象模板](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.19_标准自定义引用对象模板)、类型封装、成员布局与容器对照，新增完整模块并修正告警后继续解引用的错误模式。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.71_B04h对象模板与失败清理)记录失败路径验证；下一项 2.23 C++ 对照与后续资源/发布边界。

B04i 完成[P02 C++ 对照](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.23.1_用完整程序观察自动归还)以及回调选择、kfree 签名、成员位置和多机制状态边界。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.72_B04i管理动作与清理策略)记录 C++ 程序与 C 回调正反例；下一项 P02 2.28 以后的创建、发布及全章收束。

B04j 完成[P02 发布与容器责任](../../knowledge/linux/object_lifetime/kref/P02_源码入口与结构定义.md#2.30.1_设计_A_容器持有引用)及章末收束；本轮整章审查完成，人工评审状态不因此升级。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.73_B04j容器发布与P02收束)记录控制路径及未执行项。下一项 P03 生命周期状态机，P05/P11 局部修复后仍待后续完整审查。

B04k 重构[P03 开篇与初始责任](../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.3.1_用C模型观察仍持有却被拒绝)，以完整 C 模型区分入口、业务许可和存储保留，修复直接发布和初始份额的绝对化解释。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.74_B04k生命周期状态轴与初始责任)记录模型边界；下一项 P03 3.5～3.10，整章仍 pending。

B04l 重构[P03 普通引用与清理](../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.7.1_release_阶段_对象销毁点)，以已有完整实验回访前提、返回值和资源退出，补链表诊断的具体状态前提。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.75_B04l普通归还与清理诊断)记录固定函数观察；下一项 P03 3.8～3.10 责任表及错误交付。

B04m 重构[P03 责任表与错误交付](../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#%287%29_所有权表要补充失败路径和取消路径)，保留全部持有者及十项责任表阅读入口，以 C 模型约束取消语义，补完成路径另取份额时队列份额的退出。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.76_B04m工作票据与责任转交)记录六条轨迹；下一项 P03 3.11 以后的业务关闭、完整流程和章末收束。

B04n 收束[P03 完整关闭周期](../../knowledge/linux/object_lifetime/kref/P03_kref_生命周期状态机.md#3.16_一个完整的生命周期模板)，保留十八节及全部子标题职责，补入口转交、对象锁内停止、旧读者拒绝与最终清理的同一组阶段。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.77_B04n业务关闭与P03收束)列出八组控制检查和目标未执行项；P03 完成本轮适用审查，下一项 P04，人工评审状态未变，全仓继续。

B04o 完成[P04 三条规则](../../knowledge/linux/object_lifetime/kref/P04_kref_三条核心规则.md#4.12_mutex/list_lookup_的最小模型)全篇冷读和改写，保留二十节阅读任务，明确直接转交、短借用、拒绝出口以及三种查找协议的保证与代价。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.78_B04o三条规则与协议前提)记录固定文档证据和复用实验重验；下一项 P05 后续接口，全仓继续。

B04p 改写[P05 条件取得](../../knowledge/linux/object_lifetime/kref/P05_基础_API_源码逐行讲解.md#5.7.2_kref_get_unless_zero%28%29_的使用场景)，以完整 C11 模型解释比较失败为何重查，并建立[固定条件模块](../../research/source_reading/kref/navigation/P03_条件取得与查找窗口导读.md#3.2_从观察到自己持有)与唯一实现。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.79_B04p条件取得与比较重试)记录正常和异常分支边界；下一项 P05 锁组合，P05 整章与全仓仍未完成。

B04q 完成[P05 锁交接](../../knowledge/linux/object_lifetime/kref/P05_基础_API_源码逐行讲解.md#5.8.2_kref_put_mutex%28%29_的典型用途)和固定实现，纠正先归零后加锁的旧顺序，完整模块串起非拥有索引、慢路径重查与回调解锁；同步修正 P09 对应事实入口。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.80_B04q最后减少与锁交接)记录六模块组、十分支与未执行目标项。下一项 P05 剩余初始化、封装和章末收束，P09 全篇仍待后续重构。

B04r 收束[P05 类型契约与章末应用](../../knowledge/linux/object_lifetime/kref/P05_基础_API_源码逐行讲解.md#5.10_API_封装模板)，修正初始化形式与存储期混同、告警后继续解引用、查找封装与内存序的过度保证；已验证完整程序保留。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.81_B04r类型契约与P05收束)说明保留依据和验证范围；P05 本轮作者审查完成，下一项 P06，人工状态不变，全仓继续。

B04s 重构[P06 关闭与资源边界](../../knowledge/linux/object_lifetime/kref/P06_release_回调与复杂销毁模式.md#6.2.2_运行一个由管理者等待借用退出的模块)的 6.1～6.4，以完整内核模块串起关闭提交、等待借用、管理者归还和最后清理。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.82_B04s管理者等待与资源归属)记录九组控制检查和未执行目标项；P06 仍 pending，下一项可见性、上下文与异步重启边界，全仓继续。

B04t 完成[P06 可见性与上下文](../../knowledge/linux/object_lifetime/kref/P06_release_回调与复杂销毁模式.md#6.5_外部可见性_脱链应该由谁负责)的 6.5～6.6，保留全部阅读任务，用既有完整程序比较三种查找协议、回调锁交接和自等待。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.83_B04t入口撤下与回调上下文)记录保留与未执行验证；下一项 6.7 异步启动，P06 仍 pending。

B04u 重构[P06 异步退出](../../knowledge/linux/object_lifetime/kref/P06_release_回调与复杂销毁模式.md#6.7.3_release_和_timer_的收尾关系)全单元，加入 timer 改期/关闭的 C 模型及[固定源码模块](../../research/source_reading/kref/navigation/P05_定时器重启与退出导读.md#5.2_从排队到最终关闭)，修正把启动次数、回调次数和引用数直接对应的错误。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.84_B04u异步票据与定时器重启)记录分支证据和目标未验证项；下一项 6.8～6.12，P06/P13 与全仓仍未完成。

B04v 收束[P06 回收排序与诊断](../../knowledge/linux/object_lifetime/kref/P06_release_回调与复杂销毁模式.md#6.8.1_release_和_RCU_的边界)的 6.8～6.12，保留两份完整程序及固定实现，修正把 RCU 对象一律延迟 release、把未告警当退出证明等概括。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.85_B04v回收排序与P06收束)记录无须改写的依据和验证边界；P06 本轮作者审查完成，下一项 P07，全仓继续。

B04w 冷读 P07 全篇，先完成[交付责任入口](../../knowledge/linux/object_lifetime/kref/P07_handoff_所有权转移模型.md#7.2.1_指针传递不等于引用转移)的 7.1～7.2：用完整 C 程序区分地址复制、独立份额、指定份额转交和借用窗口。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.86_B04w交付责任与借用窗口)记录适用检查；下一项 7.3 真实异步接收，P07 仍 pending。

B04x 完成[P07 异步交付场景](../../knowledge/linux/object_lifetime/kref/P07_handoff_所有权转移模型.md#7.3.5_completion_场景里的引用归属)的 7.3，以完整模块比较事件、超时、工作退出和引用归还；修正 running/pending、timer 改期、集合并非自动持有等旧概括。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.87_B04x完成事件与异步交付)记录七组替身检查及目标未执行项。下一项 7.4 契约与后续模型，P07 仍 pending。

B04y 收束[P07 契约与完整请求](../../knowledge/linux/object_lifetime/kref/P07_handoff_所有权转移模型.md#7.6.1_一个完整请求对象_handoff_示例)的 7.4～7.8，新增同一请求的转交/共享模块，以十组宿主控制路径和 ARM 前端核对入队、出队、执行及拒绝。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.88_B04y请求双协议与P07收束)记录前提与未执行目标项；P07 本轮作者审查完成，下一项 P08，全仓继续。

B04z 冷读 P08 全篇，先重写[8.1～8.3 查找取得窗口](../../knowledge/linux/object_lifetime/kref/P08_lookup_场景与_kref_get_unless_zero%28%29.md#8.3.1_正确模型一_mutex/list_lookup_+_kref_get%28%29)，用既有完整登记模块区分地址期限、正计数和独立份额，补齐两种交错及重复撤下规则。后续条件取得与各容器错误已记入[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.89_B04z拥有型查找与首次取得)，P08 仍 pending。

B04aa 完成[P08 条件取得单元](../../knowledge/linux/object_lifetime/kref/P08_lookup_场景与_kref_get_unless_zero%28%29.md#8.4.3_kref_get_unless_zero%28%29_仍然需要锁或_RCU)，按同一非拥有索引解释地址保护、锁外归零、回调等待与比较重试，复用完整 C11 模型。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.90_B04aa条件取得与回调等待)保留验证边界；继续 8.5 容器，P08 未标完成。

B04ab 完成[P08 三类容器](../../knowledge/linux/object_lifetime/kref/P08_lookup_场景与_kref_get_unless_zero%28%29.md#8.5.2_xarray_lookup_的引用规则)，新增完整 XArray 模块及固定整数索引源码导读，纠正重复加锁、重复撤下与 IDR 字段发布顺序。[工作记录](../../governance/migration/repository_textbook_refactor.md#1.4.91_B04ab容器锁与编号发布)列出六组替身检查与目标边界；继续 8.6～8.9，P08 仍 pending。
