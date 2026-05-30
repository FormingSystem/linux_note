# Linux 内核与驱动学习笔记

这是一个个人 Linux 学习笔记仓库，内容覆盖 Linux 内核基础、内核数据结构、内核机制、驱动开发、板级移植、工具配置和资料附录。

仓库里的 Markdown 文件是当前笔记形态，也是后续重新排版成 Word/PDF 的素材。这里更重视学习过程中的结构沉淀、问题记录和可持续整理，不追求每一篇一开始就是最终出版形态。

## 分支说明

当前主要维护这些分支：

| 分支 | 用途 |
| --- | --- |
| `master` | 驱动开发主线，保留 `driver/`、`board/`、驱动实验、板级移植等内容 |
| `obsidian_version` | Obsidian 管理和内核笔记整理分支，用于链接维护、目录重排和内核主题笔记整理 |
| `rough_version` | 粗整理/临时整理分支 |
| `upgrade_change_list_head_for_kernel` | 早期链表相关内容升级整理分支 |

远端以 GitHub 为准，`origin` 指向：

```text
https://github.com/FormingSystem/linux_kernel_and_driver_note.git
```

## 目录说明

| 目录 | 内容 |
| --- | --- |
| `kernel/` | Linux 内核入门和主题笔记 |
| `driver/` | 通用驱动开发笔记 |
| `board/` | 具体开发板、芯片平台、移植和外设实验 |
| `appendix/` | 数据结构、内核模型、源码阅读、C 语言扩展等附录 |
| `images/` | 笔记中引用的图片资源 |
| `tools/` | Typora、Obsidian、AI 使用方法、笔记规划等工具说明 |
| `docs/` | Git 规则、协作说明等仓库元文档 |
| `AGENTS.md` | 给 AI 协作者读取的项目上下文 |

## 阅读方式

推荐使用 Typora 或 Obsidian 阅读。

Typora 适合单篇 Markdown 阅读和排版预览；部分 Markdown 语法和样式效果需要 Typora 支持。相关配置见：

```text
tools/typora配置/
```

Obsidian 主要用于维护 Markdown 链接。移动文件、重排目录时，优先使用 Obsidian 内部操作，让链接能够自动跟踪更新。

## 内容说明

- 任何以 Markdown 存在的文件，都可以视为笔记雏形。
- 如果某些章节以“第 1-n 章”组织，并带有明显引言、总结或分层结构，通常说明它经过 AI 辅助整理。
- AI 生成或辅助整理的痕迹不一定全部删除，因为它有时能作为阅读节奏点，提醒读者在某个模块处停下来总结。
- 笔记内容会尽量保证自己读过、理解过、能复用，但不保证所有主题都覆盖到足够宽或足够深。
- 如果某个主题不够细，可以继续把相关 Markdown 交给 AI 或资料源二次扩展。

## Git 提交规则

仓库使用本地 Git hook 校验提交信息。首次克隆后建议执行：

```bash
git config core.hooksPath .githooks
```

提交信息格式：

```text
<类型>(<范围>): <中文一句话说明>
```

类型固定为：

```text
add update rewrite fix structure format link asset meta archive chore
```

示例：

```text
add(kernel): 新增 Linux 内核链表基础笔记
update(driver): 补充字符设备驱动框架说明
fix(appendix): 修正红黑树章节链接
structure(obsidian): 调整内核笔记目录层级
meta(git): 更新个人提交规则
```

详细规则见：

```text
docs/git-guide.md
```

## 常用 AI

常用 AI 工具：

1. ChatGPT
2. Gemini
3. DeepSeek

AI 主要用于主题拆解、章节扩写、概念对比、代码解释和结构整理。使用 AI 生成内容后，仍然需要人工阅读、校对和重排。

## 参考资料

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
