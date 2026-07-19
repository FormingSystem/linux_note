# 第1章_Codex_项目上下文

这是给 AI 协作者和以后打开仓库时快速恢复上下文用的文件。进入本仓库后，优先阅读本文件、`README.md` 和 `governance/architecture/repository_information_architecture.md`，再查看 Git 协作规则。

## 1.1_项目定位

`linux-note` 是面向长期维护的计算机系统与 Linux 知识库，内容覆盖基础理论、Linux 通用机制、内核子系统、驱动模型、系统软件、工程方法、平台实现、实验、项目和源码研究。Markdown 文件既是知识本体，也是后续编排 HTML、Word 和 PDF 的素材。

## 1.2_信息架构约定

- 知识正文按本质存入 `knowledge`，不按学习专题复制。
- 工程方法、平台差异、实验、综合项目和源码证据分别进入 `engineering`、`platforms`、`labs`、`projects` 和 `research`。
- 专题、路线和知识地图进入 `atlas`，只组织链接。
- 出版物通过 `publications/manifests` 编排，知识正文不固化书籍章号。
- 仓库规范进入 `governance`，图片和附件进入 `assets`。
- 目录表达主要归属，跨领域关系通过稳定文档 ID、元数据、索引和链接表达。
- 仓库不设置通用杂物目录，无法形成稳定结论的内容进入 `research/investigations`。

## 1.3_目录速览

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

## 1.4_写作与编辑偏好

- 保持中文笔记风格，优先写清楚概念、背景、操作步骤和结论。
- Markdown 链接和图片路径尽量使用相对路径，兼容 Obsidian 和普通 Markdown 阅读器。
- 章节型文件统一使用下文的“文件与目录命名规范”。
- 修改目录结构时要格外小心，因为 Obsidian 链接会受影响。
- 不要随意删除已有笔记、图片和压缩包；如需清理，先说明原因。

## 1.5_文件与目录命名规范

- 顶层和领域目录使用稳定英文 `snake_case` 名称。
- 知识文件使用稳定语义名称，不把出版章号作为文件身份。
- 路径禁止空格、中文标点、全角符号、连续下划线和首尾下划线。
- 技术名词中具有语义的半角符号可以保留，例如 `u-boot`、`C++` 和版本号中的 `.`。
- 文档中文标题保存在 Front Matter 的 `title` 和正文 H1 中。
- 正式文档必须具有稳定 `id`、`title`、`kind` 和 `status`。
- 移动或重命名后必须同步更新 Markdown、Obsidian、Canvas、Base 和图片引用。
- 批量操作前检查冲突，操作后运行结构、链接和 `git diff --check` 检查。

## 1.6_Markdown与出版序号规范

- 知识正文 H1 使用文档标题，不写固定书籍章号。
- H2 至 H6 表达文档内部层级，不把某本书的全局章节号固化进知识正文。
- 实验步骤、连续教程和学习路线阶段可以使用局部顺序编号。
- `publications/manifests` 决定书籍章节顺序。
- 出版构建阶段生成连续章、节编号并重写内部锚点。
- 同一份知识文档进入不同出版物时可以获得不同编号。
- 标题变化后必须同步维护 Markdown 和 Obsidian 锚点。

## 1.7_格式化与链接维护

仓库根目录的 `format.sh` 是统一入口。移动或重命名文件后，应在提交前运行链接更新，使 Git 的重命名关系仍可用于定位新路径。

```bash
# 预览或应用全仓链接更新
./format.sh check links --summary
./format.sh fix links --summary

# 只扫描或更新一个 Markdown 文件
./format.sh check links --summary path/to/note.md
./format.sh fix links --summary path/to/note.md

# 全量执行 Markdown 标题、文件名和链接格式化
./format.sh fix all --summary
```

链接更新同时处理 Markdown 链接与图片、Obsidian Wiki 链接与嵌入链接，以及 `.obsidian`、Canvas、Base 文件中的已知重命名路径。目标不唯一或目标缺失时只报告，不自动猜测。

首次使用或环境变化后运行 `./format.sh doctor`。统一使用 `./format.sh install` 安装依赖：Linux 下自动选择系统包管理器，Windows 下必须在 MSYS2 Bash 中运行并使用 `pacman`；不支持 PowerShell、CMD 或 Git Bash。

## 1.8_Git_提交约定

详细规则见 `docs/git-guide.md`。简要约定：

本仓库使用 `.githooks/commit-msg` 做本地提交信息校验，当前本地仓库应配置 `git config core.hooksPath .githooks`。

```text
<类型>(<范围>): <中文一句话说明>
- 中文描述修改1
- 中文描述修改2
- ...
```
.obsidian\workspace.json  等obsidian的链接管理相关的文档出现修改，就随着任何修改一起提交，并不比出现在git修改描述里，它的更新属于是obsidian链接发生变动，不必单独做提交说明。

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

## 1.9_代码提交

除了标注为源码引用部分，其他章节的包含的示例代码都采用中文注释说明。
