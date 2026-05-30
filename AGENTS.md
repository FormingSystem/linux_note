# Codex 项目上下文

这是给 AI 协作者和以后打开仓库时快速恢复上下文用的文件。进入本仓库后，优先阅读本文件，再查看 `README.md` 和 `docs/git-guide.md`。

## 项目定位

`linux-note` 是个人 Linux 学习笔记仓库，内容覆盖 Linux 内核、驱动开发、板级移植、工具配置和资料附录。Markdown 文件既是当前笔记形态，也是后续整理成 Word/PDF 的素材。

## 分支约定

- `master`：驱动开发主线，保留 `driver/`、`board/`、驱动实验、板级移植等内容。
- `obsidian_version`：Obsidian 管理和内核笔记整理分支，用于链接维护和重新排版；该分支已删除驱动部分，避免影响 Obsidian 中的内核笔记整理。

处理文件时注意：

- 不要在 `obsidian_version` 中主动恢复 `driver/` 目录，除非用户明确要求。
- 驱动相关新增内容优先放到 `master`。
- 内核主题、资料附录、索引、链接结构调整优先放到 `obsidian_version`。
- 通用规则文档可以在两个分支同步，但不要用大范围 merge 误带目录。
- 远端以 GitHub 为准，`origin` 指向 `https://github.com/FormingSystem/linux_kernel_and_driver_note.git`，不再推送 Gitee。

## 目录速览

- `kernel/`：Linux 内核主题笔记。
- `driver/`：通用驱动开发笔记，主要属于 `master`。
- `board/`：具体开发板、芯片平台、移植和驱动实验。
- `appendix/`：内核模型、数据结构、源码阅读、C 语言扩展等附录。
- `images/`：各主题图片资源。
- `tools/`：Typora、Obsidian、AI 使用方法、笔记规划等工具类说明。
- `docs/`：仓库协作规则、Git 规则等元文档。

## 写作与编辑偏好

- 保持中文笔记风格，优先写清楚概念、背景、操作步骤和结论。
- Markdown 链接和图片路径尽量使用相对路径，兼容 Obsidian 和普通 Markdown 阅读器。
- 章节型文件沿用现有命名风格，例如 `P01-主题.md`、`第1章_主题.md`。
- 修改目录结构时要格外小心，因为 Obsidian 链接会受影响。
- 不要随意删除已有笔记、图片和压缩包；如需清理，先说明原因。

## Git 提交约定

详细规则见 `docs/git-guide.md`。简要约定：

本仓库使用 `.githooks/commit-msg` 做本地提交信息校验，当前本地仓库应配置 `git config core.hooksPath .githooks`。

```text
<类型>(<范围>): <一句话说明>
```

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
