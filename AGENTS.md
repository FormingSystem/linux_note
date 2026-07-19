# 第1章_Codex_项目上下文

这是给 AI 协作者和以后打开仓库时快速恢复上下文用的文件。进入本仓库后，优先阅读本文件，再查看 `README.md` 和 `docs/git-guide.md`。

## 1.1_项目定位

`linux-note` 是个人 Linux 学习笔记仓库，内容覆盖 Linux 内核、驱动开发、板级移植、工具配置和资料附录。Markdown 文件既是当前笔记形态，也是后续整理成 Word/PDF 的素材。

## 1.2_分支约定

- `master`：驱动开发主线，保留 `driver/`、`board/`、驱动实验、板级移植等内容。
- `obsidian_version`：Obsidian 管理和内核笔记整理分支，用于链接维护和重新排版；该分支已删除驱动部分，避免影响 Obsidian 中的内核笔记整理。

处理文件时注意：

- 不要在 `obsidian_version` 中主动恢复 `driver/` 目录，除非用户明确要求。
- 驱动相关新增内容优先放到 `master`。
- 内核主题、资料附录、索引、链接结构调整优先放到 `obsidian_version`。
- 通用规则文档可以在两个分支同步，但不要用大范围 merge 误带目录。
- 远端以 GitHub 为准，`origin` 指向 `https://github.com/FormingSystem/linux_kernel_and_driver_note.git`，不再推送 Gitee。

## 1.3_目录速览

- `kernel/`：Linux 内核主题笔记。
- `driver/`：通用驱动开发笔记，主要属于 `master`。
- `board/`：具体开发板、芯片平台、移植和驱动实验。
- `appendix/`：内核模型、数据结构、源码阅读、C 语言扩展等附录。
- `images/`：各主题图片资源。
- `tools/`：Typora、Obsidian、AI 使用方法、笔记规划等工具类说明。
- `docs/`：仓库协作规则、Git 规则等元文档。

## 1.4_写作与编辑偏好

- 保持中文笔记风格，优先写清楚概念、背景、操作步骤和结论。
- Markdown 链接和图片路径尽量使用相对路径，兼容 Obsidian 和普通 Markdown 阅读器。
- 章节型文件统一使用下文的“文件与目录命名规范”。
- 修改目录结构时要格外小心，因为 Obsidian 链接会受影响。
- 不要随意删除已有笔记、图片和压缩包；如需清理，先说明原因。

## 1.5_文件与目录命名规范

本规范适用于仓库内全部文件和目录。目标是让路径同时兼容 Obsidian、普通 Markdown 阅读器和命令行工具。

- 中文文字可以保留；禁止出现在路径中的是中文标点、全角符号和其他可能干扰 Markdown 链接解析的符号。
- 路径中禁止出现空格；需要分隔语义时统一使用单个半角下划线 `_`。
- 已使用 `Pxx` 编号的章节统一采用 `PXX_NAME`：`P` 大写、编号固定两位、编号与名称之间使用一个 `_`，例如 `P01_文件名字.md`。
- `P1_主题`、`P001_主题`、`P01 - 主题`、`P01-主题` 和 `P01_-_主题` 均不符合规范，应整理为 `P01_主题`。超过 `P99` 时再统一讨论扩展方案，不提前混用三位编号。
- 已使用“第几章”“第几部分”或“第几篇”的文件与目录也必须转换为 `PXX_NAME`，例如 `第1章_主题.md`、`第1部分_主题`、`第1篇_主题` 均转换为 `P01_主题`，仓库内不保留多套编号体系。
- 没有 `Pxx`、“第几章”“第几部分”或“第几篇”编号的现有文件和目录，不补编号、不改写原有名称结构；仅清除空格、中文标点和非法符号等链接风险。
- 中文括号 `（）`、方头括号 `【】` 转为半角括号 `()`；其余中文标点通常转为 `_`，并合并连续下划线。
- 技术名词内部原有且具有语义的半角符号可以保留，例如 `u-boot`、`rb-tree`、`C++`、`gpio+pinctrl` 和版本号中的 `.`。
- 禁止使用 `_-_`、连续下划线、首尾下划线，以及仅为视觉排版加入的连接符。
- 新建或重命名路径后，必须同步更新 Markdown 链接、Obsidian Wiki 链接、图片引用和 `.obsidian` 中记录的路径。
- 批量调整前先检查命名冲突，调整后执行链接检查和 `git diff --check`；不得仅重命名文件而遗留失效链接。

仓库根目录提供 Bash 入口，文件路径实现位于 `scripts/normalize_paths.sh`：

```bash
# 仅预览文件和目录名
./format.sh check paths

# 执行重命名并同步更新链接
./format.sh fix paths
```

脚本预览输出 `待重命名文件：0`，表示当前受版本控制的路径已符合规范。新增内容也必须直接采用本规范命名，不应依赖事后批量修复。

## 1.6_Markdown标题序号规范

Markdown 标题采用适合后续合并并转换为 HTML、Word 或 PDF 的出版物连续序号。`PXX_NAME.md` 中的 `XX` 是章号，正文必须包含对应的文档章标题。

```markdown
# 第5章_文件系统构建与定制
## 5.1_引言
### 5.1.1_为什么需要文件系统
#### (1)_根文件系统的职责
##### 1)_用户空间支持
###### a)_基础目录
```

- H1：`第N章_NAME`，章号取自文件名的 `PXX`。
- H2：`N.N_NAME`。
- H3：`N.N.N_NAME`。
- H4：`(N)_NAME`。
- H5：`N)_NAME`。
- H6：`a)_NAME`、`b)_NAME`，超过 26 项后使用 `aa)`、`ab)`。
- 每级序号在父标题内重新计数，标题层级不得超过 Markdown 原生 H6。
- 标题中的中文文字可以保留，但标题正文不使用空格、中文标点、全角符号或装饰 Emoji；序号与标题之间使用 `_`。
- `PXX` 文件缺少章标题时，格式化脚本补充 H1，并将原有标题下移一级，使各文件合并后章号保持连续。
- 无 `PXX` 文件名的独立 Markdown 保留自身文档边界，并从第 1 章开始编号。
- 标题修改后必须同步维护 Markdown 锚点和 Obsidian Wiki 锚点。

统一入口支持全仓和单文件操作：

```bash
# 全量预览或修复：文件名 + Markdown 标题
./format.sh check all --summary
./format.sh fix all --summary

# 仅处理 Markdown 标题
./format.sh check headings --summary
./format.sh fix headings --summary

# 仅处理一个 Markdown 文件
./format.sh check headings appendix/GNU_environment/buildroot/P05_文件系统构建与定制.md
./format.sh fix headings appendix/GNU_environment/buildroot/P05_文件系统构建与定制.md
```

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
