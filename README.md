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

正式知识库文档包含 `id`、`title`、`kind`、`status` 和 `domains` 元数据。`id` 用于长期引用，正文继续采用统一章节序号保证阅读定位。可用 `./format.sh check all --summary` 同时检查标题、元数据、路径和链接。可安装的 skill 按自身格式维护，不套用知识正文元数据。

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

地图使用 🔴 **未校正**、🟡 **人工评审中**、🟢 **评审完成**。具体定义与升级依据只在[地图说明](atlas/maps/knowledge_review_map.md)维护；它们表示博主的人工阅读进度，与 Front Matter 的维护阶段不同。AI 核对、脚本通过或正文重写不会自动升级人工状态。

### 1.3.2\_打开交互式地图

- [知识库专题阅读与评审地图](atlas/maps/knowledge_review_map.md)：在 Obsidian 中使用 MarkMind 打开后，可以折叠、缩放，并直接点击领域、专题、章节或支撑材料节点跳转；评审状态只在这份地图中维护。
- 普通 Markdown 阅读器会把同一文件显示为层级大纲，不依赖插件也能阅读和点击。
- 地图中的真实节点记录当前进度；新增章节从 🔴 开始，原有人工评审记录按开发者确认维护。

## 1.4\_阅读方式

本仓库从具体问题进入机制：先看到旧方案能做什么、何时出现缺口，再推导角色、状态和通信，最后核对实现与操作接口。已解释的先修知识只作回顾；需要理解当前结论的图、表、寄存器字段及运行步骤保留在正文，来源链接供进一步追溯。写作方法见[专题结构从零建立](tools/ai/codex/skills/build-linux-note-topic/references/topic-design.md#12-让专题结构本身也从零建立)。

从下面选择当前任务，再沿对应大纲连续阅读。完整专题范围见[内容索引](atlas/indexes/content_index.md)，章节人工进度见[阅读与评审地图](atlas/maps/knowledge_review_map.md)；首页不重复维护每一节的修改记录。

| 当前想解决的问题 | 起点与后续路线 |
| --- | --- |
| 第一次理解内核怎样为应用工作 | [读取文件的小程序](knowledge/linux/architecture/kernel_composition/linux内核概貌.md#1.1_先让程序读到几个字) → [从问题定位源码](knowledge/linux/architecture/source_tree/Linux_kernel_目录结构说明.md#1.1_先区分源码目录与正在运行的系统) → [内核学习路线](atlas/tracks/linux_kernel_track.md) |
| 编译、装载第一个模块，并区分模块与设备 | [模块工程](engineering/build/kernel_modules/大纲.md#1.1_四章怎样连起来) → [模块与设备入口](knowledge/linux/architecture/modules_and_device_nodes/大纲.md#1.1_沿三个问题进入正文) → [驱动框架](knowledge/driver_model/fundamentals/framework_model/大纲.md#1.1_四个问题怎样接起来) |
| 为应用提供可读写的设备服务 | [错误指针](knowledge/linux/error_handling/error_pointer/大纲.md#1.1_四次认识变化) → [字符设备](knowledge/driver_model/character_device/大纲.md) → [文件操作与映射](knowledge/driver_model/file_operations/大纲.md#1.1_沿对象寿命逐步增加约束)；[分类与属性实验](knowledge/linux/device_model/class_sysfs/大纲.md#1.1_从分类观察到可控数据入口)解释可见入口和撤销 |
| 理解设备登记、探测、依赖与移除 | [设备模型](knowledge/linux/device_model/大纲.md) → [设备树与平台资源](knowledge/driver_model/device_tree/大纲.md) → [GPIO Consumer](knowledge/driver_model/gpio_consumers/大纲.md)及对应子系统 |
| 为查找、排序和范围操作选择容器 | [链表](knowledge/linux/data_structures/单链表_linked_list/大纲.md#1.1_从一组任务走到容器选择) → [哈希表](knowledge/linux/data_structures/哈希表_Hash_Table/大纲.md#1.1_沿问题增加约束)；[树结构路线](knowledge/linux/data_structures/红黑树_rb-tree/大纲.md#1.1_沿问题进入现有章节)串起普通树、BST、旋转、多路树、红黑树、页级索引与 Maple 范围结构 |
| 判断多条执行路径怎样安全共享状态 | [执行路径交错模型](knowledge/linux/synchronization_and_asynchrony/P01_同一对象的多条执行路径.md#1.2_运行所有允许的交错) → [消息发布](knowledge/linux/synchronization_and_asynchrony/synchronization/memory_ordering/P01_READ_WRITE_ONCE_与_SMP_内存顺序原语.md#1.2.1_从准备到使用的四个阶段) → [同步与异步总纲](knowledge/linux/synchronization_and_asynchrony/大纲.md)，再按约束选择锁、等待、RCU 或工作队列 |
| 证明查找返回的对象何时可以释放 | [kref](knowledge/linux/object_lifetime/kref/大纲.md) → [devres](knowledge/linux/object_lifetime/devres/大纲.md) → [跨层寿命集成](knowledge/linux/object_lifetime/integration/P01_kobject_device_devres_kref_生命周期集成.md#1.9_用六条轨迹串起组合退出)；已有故障可从[诊断流程](knowledge/linux/object_lifetime/kref/P12_典型错误模式与调试线索.md#12.10.2_最小审查流程)回查 |
| 理解系统启动与板级移植 | [U-Boot](knowledge/system_software/uboot/大纲.md) → [i.MX6ULL 移植](platforms/arm/nxp/imx6ull/porting/大纲.md)或[RK3566 SDK](platforms/arm/rockchip/rk3566/environment/大纲.md)；根文件系统继续进入[Buildroot](knowledge/system_software/buildroot/大纲.md) |
| 从某个结论追到固定版本代码 | [Linux 源码基线](research/source_reading/linux/SOURCE_BASELINE.md#1.1_当前来源) → 专题 navigation 总索引与模块导读 → source_explanations 中唯一实现标题 |

正文中的完整宿主模型用于观察规则和反例，内核模块与板级步骤则注明目标环境。源代码可跳转、主机程序通过、目标模块编译通过和板上运行通过是不同证据；具体检查结果及未执行项见[重构工作记录](governance/migration/repository_textbook_refactor.md)，后续建设范围见[重构蓝图](atlas/roadmaps/linux_textbook_refactor.md)。

推荐使用 Typora 或 Obsidian 阅读。[Typora Code](https://github.com/FormingSystem/typora_code) 面向多文档、标题跳转与源码阅读提供工作台增强，配置、实现、测试和开发历史在独立仓库维护。正文图片可适配宽度、放大或全屏查看；图示保留理解所需内容，不靠删减关系迁就窗口。Obsidian 可用于维护链接，移动后仍需运行仓库链接检查。

需要按一本书连续阅读时，从[MarkBook 专题电子书](markbook/README.md#1.2_当前刊物)进入。它是按月冻结的编排快照，正文修订在知识、研究和实验的权威位置维护；当期来源哈希用于还原当时内容。



面向 RK3588 的硬件中断控制器学习，从 [GICv3 物理中断专题大纲](platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图)进入：十七篇连续教材从外设事件逐步讲到状态、配置、消息翻译与软件交接，最后以 UART 完成综合复盘，另有 RK3588 集成记录。正文先标明概念与实现层次，完成规范角色到物理载体的映射后，再解释配置和输入输出。各章配有纸面推演、主机算例或目标板观察步骤，关键概念附中英文术语与官方章节对照；寄存器采用[手册原位布局与访问表、中文字段对照、读表推导](platforms/arm/architecture/gic/P03_多核角色与架构编程接口.md#3.4_软件怎样访问GIC_地址访问与系统寄存器)的阅读顺序。GICv3 虚拟化、GICv4、逐函数源码与板上实验后续展开。与 Linux IRQ 管理的分工见[交叉阅读入口](atlas/roadmaps/gic_learning_plan.md#1.7_与现有中断章节的交叉阅读)。

## 1.5\_内容说明

知识正文建立机制，源码导读核对固定实现，实验记录保存条件与观察结果，调查和归档保留尚待消解的旧说法。阅读时先看文档身份与适用边界；不能仅凭标题编号、引言或总结判断内容是否由 AI 生成。

AI 可协助整理、扩写和核对，人工确认程度仍以[评审地图](atlas/maps/knowledge_review_map.md)为准。已通过自动检查的文档也可能存在解释遗漏；引用、模型结果和硬件实测分别标明证据范围。发现缺口时，回到相应权威正文补充问题、推导及依据，避免另建一份互相漂移的教程。

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

[Linux 源码基线](research/source_reading/linux/SOURCE_BASELINE.md#1.1_当前来源)记录官方仓库、不可变提交、配置边界和上游相对路径。`research/source_reading/linux/` 中按上游路径保存的源码材料用于核对；各专题的 `navigation/` 组织模块导读，`source_explanations/` 解释唯一的宏、字段和函数实现。阅读说明、裁剪代码和仓库补充注释均不代表上游官方原文。

### 1.9.2\_Linux\_kernel\_源码版权与许可证

Linux kernel 整体采用 GPL-2.0-only；具体文件仍须查看 SPDX 标识、版权声明以及 `COPYING`、`LICENSES/`。`Linux-syscall-note` 是特定用户空间 API 头文件的系统调用例外，不能把 `GPL-2.0-only WITH Linux-syscall-note` 泛化为全部内核源码的许可证。依据见[内核官方许可规则](https://docs.kernel.org/process/license-rules.html)，概念和条件见[本仓库 GPL 说明](reference/standards/gpl/GPL协议说明.md)。

上游源码的版权归其原权利人所有。仓库根目录许可证不会替第三方源码重新授权，也不会使这些代码变成本仓库原创内容。

### 1.9.3\_关于源码注释和改写

新增解释、图示和学习总结属于仓库整理工作；引用或改写的源码仍保留原有权利和许可条件。独立原创部分适用仓库许可证，不意味着混合文档、衍生作品或其中的第三方材料可以忽略上游条件。分发时应保留来源、版权、许可证和修改说明，并按实际文件组合判断适用条件。

带有仓库补充注释的实现文档用于辅助阅读，不能当作上游原始文件进行逐字或摘要比较。

### 1.9.4\_非官方声明

本仓库不是 Linux kernel 官方文档，也不是 Linux kernel 官方源码镜像。

仓库中的源码阅读内容仅用于个人学习、知识整理、源码分析和笔记沉淀。由于笔记中可能包含个人理解、阶段性判断、AI 辅助整理内容或尚未最终校对的材料，因此不保证所有解释都与 Linux kernel 官方实现意图完全一致。

如需确认 Linux kernel 的准确实现、许可证边界或最新源码状态，请以 Linux kernel 官方源码仓库、官方文档以及原始文件中的许可证声明为准。

### 1.9.5\_外部厂商资料

Arm、NXP 与 Rockchip 在本节中都是公司或品牌专名。Arm 架构规范、处理器核手册、芯片数据手册、技术参考手册和开发板配套资料等外部文件，其版权、商标和许可仍归这些公司及其他原权利人所有。公开可下载不等于允许本仓库再次分发，根目录的 `GPL-2.0-only` 也不会重新许可这些第三方文件。

除非文件附带明确允许再分发且与本仓库用途相容的许可，新引入的外部厂商资料只在 `reference/external_resources/` 保存来源、版本、版权边界和完整性校验信息，实际文件由使用者从权利人入口下载到 Git 忽略的 `.cache/`。这类文件不得放入普通 Git、Git LFS、GitHub Release 或其他公共镜像。

当前 Arm、RK3588 与 i.MX6ULL 资料入口见 [外部资料索引](reference/external_resources/arm/README.md#1.1_索引定位与存储边界)。独立 GIC 专题从 [GICv3 物理中断大纲](platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图)阅读，长期扩展由[建设路线](atlas/roadmaps/gic_learning_plan.md#1.6_建设落点与当前交付边界)维护；资料可按 `--profile rk3588_gicv3` 选取。
