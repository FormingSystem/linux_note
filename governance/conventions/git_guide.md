---
id: governance.conventions.git_guide
title: "Git协作与提交规范"
kind: reference
status: evolving
domains:
  - governance
---

# 第1章_Git协作与提交规范

本规范用于保持知识库历史清晰、结构调整可审计，并确保 Markdown、Obsidian 和出版构建所依赖的路径关系完整。

## 1.1_统一内容主线

基础理论、Linux 机制、内核子系统、驱动模型、系统软件、平台、实验和源码研究在同一内容主线统一管理。技术方向通过 `atlas` 中的知识地图和学习路线组织，不使用长期分支隔离内容。

功能分支用于完成边界明确的修改，完成检查后合并回主线。分支名称应表达任务，例如：

```text
structure/repository-architecture
rewrite/linux-concurrency
add/input-labs
fix/obsidian-links
```

## 1.2_提交前检查

首次克隆后启用版本化 Git hook：

```bash
git config core.hooksPath .githooks
```

提交前至少运行：

```bash
./format.sh doctor
./format.sh check all --summary
git status
git diff --check
git diff --stat
git diff
```

重点确认：

- 本次变更符合仓库信息架构。
- 文档没有重复保存已有知识正文。
- Markdown、Obsidian、Canvas、Base 和图片链接有效。
- 文件移动使用 `git mv` 并保留重命名关系。
- 图片、压缩包、配置和其他二进制文件确实需要提交。
- 没有编辑器缓存、构建产物和本机绝对路径。

## 1.3_提交粒度

一次提交只形成一个可以独立说明和回退的结果，例如：

- 新增一项知识或一个实验。
- 重写同一机制的一组文档。
- 调整一个领域的目录结构。
- 修复链接或资源路径。
- 更新仓库规范或工具。

设计规范、结构迁移、内容重写和出版产物应根据结果边界拆分提交。

## 1.4_提交信息

提交信息格式：

```text
<类型>(<范围>): <中文一句话说明>
```

校验正则：

```regex
^(add|update|rewrite|fix|structure|format|link|asset|meta|archive|chore)\([^)]+\): .+$
```

| 类型 | 适用场景 |
| --- | --- |
| `add` | 新增知识、实验、项目或资料 |
| `update` | 补充已有内容 |
| `rewrite` | 重写内容或大幅调整表达 |
| `fix` | 修正错误、断链和路径 |
| `structure` | 调整目录、归属、文件名或结构 |
| `format` | 调整 Markdown 和出版格式 |
| `link` | 维护文档、Obsidian 和资源链接 |
| `asset` | 新增或整理图片、附件和数据 |
| `meta` | 修改 README、AGENTS 和治理规范 |
| `archive` | 归档历史资料和过时内容 |
| `chore` | 不改变知识含义的维护工作 |

范围优先使用稳定领域名称：

```text
foundations
linux
kernel
driver
system
platform
labs
research
publication
governance
tools
assets
obsidian
```

示例：

```text
add(labs): 新增等待队列唤醒实验
rewrite(linux): 重构引用计数生命周期说明
structure(driver): 拆分Input子系统与驱动应用
fix(platform): 修正iMX6ULL移植图片路径
meta(governance): 更新知识库信息架构
link(obsidian): 同步目录迁移后的工作区路径
```

## 1.5_推荐工作流

```bash
git switch -c <类型>/<任务名>

# 编辑、移动或新增内容

./format.sh check all --summary
git diff --check
git status
git add <明确的文件>
git diff --cached
git commit -m "<类型>(<范围>): <中文说明>"
```

移动或重命名大量文档时：

```bash
git mv <旧路径> <新路径>
./format.sh fix links --summary
./format.sh check all --summary
```

## 1.6_Obsidian配置

- `.obsidian` 中与链接、工作区、视图和仓库结构相关的变更随对应内容提交。
- Obsidian 路径更新属于结构变更的组成部分，不需要拆成单独提交。
- 缓存、临时状态和与仓库无关的插件数据不得提交。
- 脚本更新 Obsidian 配置后必须重复扫描，确认更新过程具有幂等性。

## 1.7_结构与命名

- 目录和文件归属遵守 `governance/architecture/repository_information_architecture.md`。
- 顶层与领域目录使用英文 `snake_case`。
- 章节型文件使用 `PXX_NAME`，正文保留统一阅读序号；非章节文件使用稳定语义名称。
- 路径禁止空格、中文标点、全角符号和连续下划线。
- 大范围结构调整单独提交，方便使用 `git log --follow` 查看历史。

## 1.8_不提交的内容

- 临时测试输出和编辑器缓存。
- 未确认来源或授权的大体积资料。
- 本机绝对路径和个人凭据。
- 可以由构建过程重新生成的中间文件。
- 没有入口说明、环境说明和预期结果的零散实验文件。

确需提交二进制资源时，应放入 `assets` 的对应分类，并在关联文档中说明来源和用途。
