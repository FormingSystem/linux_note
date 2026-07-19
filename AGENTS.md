---
id: repository.agents
title: "Codex 项目上下文"
kind: reference
status: evolving
domains:
  - repository
---

# 第1章\_Codex\_项目上下文

这是给 AI 协作者和以后打开仓库时快速恢复上下文用的文件。进入本仓库后，优先阅读本文件、`README.md` 和 `governance/architecture/repository_information_architecture.md`，再查看 Git 协作规则。

## 1.1\_项目定位

`linux-note` 是面向长期维护的计算机系统与 Linux 知识库，内容覆盖基础理论、Linux 通用机制、内核子系统、驱动模型、系统软件、工程方法、平台实现、实验、项目和源码研究。Markdown 文件既是知识本体，也是后续编排 HTML、Word 和 PDF 的素材。

## 1.2\_信息架构约定

- 知识正文按本质存入 `knowledge`，不按学习专题复制。
- 工程方法、平台差异、实验、综合项目和源码证据分别进入 `engineering`、`platforms`、`labs`、`projects` 和 `research`。
- 专题、路线和知识地图进入 `atlas`，只组织链接。
- 出版物通过 `publications/manifests` 编排，正文保留统一阅读序号，构建时校正跨文档连续性。
- 仓库规范进入 `governance`，图片和附件进入 `assets`。
- 目录表达主要归属，跨领域关系通过稳定文档 ID、元数据、索引和链接表达。
- 仓库不设置通用杂物目录，无法形成稳定结论的内容进入 `research/investigations`。

## 1.3\_目录速览

- `atlas/`：知识地图、学习路线、索引和路线图。
- `knowledge/`：基础知识、Linux 机制、内核子系统、驱动模型和系统软件。
- `engineering/`：工程方法、构建、移植、调试、测试和发布流程。
- `platforms/`：架构、SoC、开发板和 BSP 差异。
- `labs/`：验证单一结论的最小可复现实验。
- `projects/`：组合多个机制的完整项目。
- `research/`：源码阅读、调用链、调查和基准证据。
- `reference/`：API、命令、术语、标准和外部资料。
- `publications/`：书籍、文章、编排清单、模板和构建产物。
- `tools/`：编辑器、Obsidian、AI 和仓库工具说明。
- `governance/`：架构、规范、模板、模式和迁移记录。
- `assets/`：图片、图表、附件、数据集和归档文件。

## 1.4\_写作与编辑偏好

- 保持中文笔记风格，优先写清楚概念、背景、操作步骤和结论。
- 正文应多使用 Mermaid 流程图、时序图、状态图和关系图，将机制运行过程、状态变化、调用链及多组件关系直观呈现，提高文档的可读性；简单事实或一两句话即可说清的内容不必为了数量堆砌图表。
- Markdown 链接和图片路径尽量使用相对路径，兼容 Obsidian 和普通 Markdown 阅读器。
- 章节型文件统一使用下文的“文件与目录命名规范”。
- 修改目录结构时要格外小心，因为 Obsidian 链接会受影响。
- 不要随意删除已有笔记、图片和压缩包；如需清理，先说明原因。

## 1.5\_文件与目录命名规范

- 顶层和领域目录使用稳定英文 `snake_case` 名称。
- 章节型文件使用 `PXX_NAME`，非章节文件使用稳定语义名称。
- 路径禁止空格、中文标点、全角符号、连续下划线和首尾下划线。
- 技术名词中具有语义的半角符号可以保留，例如 `u-boot`、`C++` 和版本号中的 `.`。
- 文档中文标题保存在 Front Matter 的 `title` 和正文 H1 中。
- 正式文档必须具有稳定 `id`、`title`、`kind`、`status` 和 `domains`；使用 `./format.sh check metadata` 检查。
- 移动或重命名后必须同步更新 Markdown、Obsidian、Canvas、Base 和图片引用。
- 批量操作前检查冲突，操作后运行结构、链接和 `git diff --check` 检查。

## 1.6\_Markdown与出版序号规范

- H1：`第N章\_NAME`。
- H2：`N.N\_NAME`。
- H3：`N.N.N\_NAME`。
- H4：`(N)\_NAME`。
- H5：`N)\_NAME`。
- H6：`a)\_NAME`，超过 26 项后使用 `aa)`、`ab)`。
- Markdown 标题源码中的下划线必须写成 `\_`，避免被渲染器误解析为强调；格式化脚本会自动维护该转义。
- 每级序号在父标题内重新计数，标题层级不得超过 Markdown H6。
- 标题序号用于日常阅读定位，也是出版排版的基础，不得批量删除。
- `publications/manifests` 决定多文档顺序，构建阶段只校正跨文档连续性并重写锚点。
- 标题变化后必须同步维护 Markdown 和 Obsidian 锚点。

## 1.7\_格式化与链接维护

仓库根目录的 `format.sh` 是统一入口。移动或重命名文件后，应在提交前运行链接更新，使 Git 的重命名关系仍可用于定位新路径。

```bash
# 预览或应用全仓链接更新
./format.sh check links --summary
./format.sh fix links --summary

# 只扫描或更新一个 Markdown 文件
./format.sh check links --summary path/to/note.md
./format.sh fix links --summary path/to/note.md

# 全量执行标题、元数据、文件名和链接格式化
./format.sh fix all --summary
```

链接更新同时处理 Markdown 链接与图片、Obsidian Wiki 链接与嵌入链接，以及 `.obsidian`、Canvas、Base 文件中的已知重命名路径。目标不唯一或目标缺失时只报告，不自动猜测。

首次使用或环境变化后运行 `./format.sh doctor`。统一使用 `./format.sh install` 安装依赖：Linux 下自动选择系统包管理器，Windows 下必须在 MSYS2 Bash 中运行并使用 `pacman`；不支持 PowerShell、CMD 或 Git Bash。

## 1.8\_Git\_提交约定

详细规则见 `governance/conventions/git_guide.md`。简要约定：

本仓库使用 `.githooks/commit-msg` 做本地提交信息校验，当前本地仓库应配置 `git config core.hooksPath .githooks`。

```text
<类型>(<范围>): <中文一句话说明>
- 中文描述修改1
- 中文描述修改2
- ...
```
`.obsidian/workspace.json` 等 Obsidian 链接管理文件出现修改时，随对应内容一起提交。它们属于链接变化，不必单独拆分提交或写入提交说明。

校验正则：

```regex
^(add|update|rewrite|fix|structure|format|link|asset|meta|archive|chore)\([^)]+\): .+$
```

固定类型只能使用：`add`、`update`、`rewrite`、`fix`、`structure`、`format`、`link`、`asset`、`meta`、`archive`、`chore`。`<范围>` 可以自由编辑但不能为空；`<一句话说明>` 可以自由编辑，必须采用中文描述，且不能为空。

常见示例：

```text
add(kernel): 新增 Linux 内核数据结构笔记
update(driver): 补充字符设备驱动框架
fix(appendix): 修正红黑树章节链接
meta(git): 更新个人提交规则
structure(kernel): 调整内核章节目录结构
```

## 1.9\_代码提交

除了标注为源码引用部分，其他章节的包含的示例代码都采用中文注释说明。

## 1.10\_专题组织要求

- 专题必须具有唯一权威正文位置。同一机制不得在多个领域重复维护完整教程；其他文档只保留本场景特有的问题、结论和必要示例，并链接到权威专题。
- 专题目录必须包含 `大纲.md`。大纲使用 `kind: track`，负责说明专题定位、由浅入深的阅读阶段、真实存在的可点击章节和跨专题关系，不复制正文，也不罗列尚未落地的空想章节。
- 专题应优先按“问题背景与矛盾 → 核心概念 → 硬件或运行基础 → 状态/通知/同步机制 → 实现或变体 → 通用 API → 模板与应用 → 误用和边界复盘”组织；具体领域可以裁剪，但不得默认读者已经理解后文机制。
- 问题必须充分展开。应先说明现有方案解决了什么、代价在哪里、为什么仍有缺口，再引出新机制，避免先宣布 API 或优点后补背景。
- 正文应使用 Mermaid 流程图、时序图、状态图和关系图呈现运行过程；图必须帮助解释文字难以直观看出的状态变化、消息流、调用链或组件关系，不为凑数量添加装饰图。
- 单篇文档同时承担多个可独立学习的机制，或者已经明显影响连续阅读时，应拆成多个 `PXX_NAME` 文件；拆分是内容重组，不得以“精简”为由删除有效知识点。
- 不要为单个普通文件空套专题目录。只有内容已经形成稳定子领域、需要多篇正文或明确承担专题导航时才建立目录；目录名称必须准确并符合英文 `snake_case` 规范。
- 原专题中的桥接章节不能无脑删除。若它承担上一章、下一章或出版路线的连续性，应保留为短桥接文档，说明专题位置、最小模型和跳转路径。
- 专题外出现通用机制长篇复述时，应将通用部分收拢回权威专题；数据结构、驱动、对象生命周期等旁支只保留其交叉边界，例如 hlist 旧路径连续性、RCU lookup 取得 kref 等。
- 机制介绍应明确区分硬件能力、内核软件状态、通知方式、对象生命周期和 API 契约。不得用“空宏”“完全无通知”“全局读写锁”“固定等待时间”等未经源码或规范支持的绝对化比喻代替真实机制。
- 源码证据必须按上游源码树位置保存和引用，使读者能够建立内核目录位置感；版本相关结论要标明版本边界，通用知识正文与版本源码导读通过链接关联。
- 移动、拆分、合并或删除专题文件后，必须同步更新专题大纲、上一篇/下一篇导航、Atlas、出版清单、Markdown/Obsidian 链接和其他入口；提交前检查旧路径残留、缺失链接、重复 ID、元数据和 `git diff --check`。
- 同一次专题整改包含不同性质的修改时，尽量按 `structure`、`rewrite`、`link` 等类型拆分 Git 提交，便于审查结构变化、正文变化和导航变化。
