# 第1章_个人开发_Git_提交规则

这份规则用于 `linux-note` 个人笔记仓库。目标不是把流程搞复杂，而是让每次提交都能说明“改了什么、为什么改、属于哪个学习方向”，以后回看历史时不用猜。

## 1.1_分支定位

当前仓库主要按两个长期分支维护：

| 分支 | 用途 | 内容边界 |
| --- | --- | --- |
| `master` | 驱动开发主线 | 保留 `driver/`、`board/`、驱动实验、板级移植、外设调试等内容 |
| `obsidian_version` | Obsidian 管理与内核笔记整理 | 用于 Obsidian 链接管理、内核主题笔记重排；该分支已删除驱动部分，避免驱动目录干扰后续排版 |

原则：

- 驱动相关内容优先在 `master` 修改。
- 内核主题、附录、资料索引、Obsidian 链接整理优先在 `obsidian_version` 修改。
- 不要把 `master` 的 `driver/` 目录直接合回 `obsidian_version`，除非明确决定恢复驱动笔记。
- 两个分支都可以更新通用文档，例如 `README.md`、`docs/`、工具说明，但提交信息要写清楚属于“规则/索引/排版”还是“内容新增”。
- 远端以 GitHub 为准，`origin` 应指向 `https://github.com/FormingSystem/linux_kernel_and_driver_note.git`，不再推送 Gitee。

## 1.2_提交前检查

本仓库使用版本化 Git hook 校验提交信息：

```bash
git config core.hooksPath .githooks
```

当前本地仓库已经配置完成。如果重新克隆仓库，需要重新执行上面的命令，让 `.githooks/commit-msg` 生效。

每次提交前建议按这个顺序看一遍：

```bash
git status
git diff --stat
git diff
```

重点确认：

- 只提交本次真正想保存的文件。
- 图片、压缩包、Obsidian 配置等大文件是有意加入的。
- 没有误删另一个分支应该保留的目录。
- Markdown 链接和图片路径尽量使用相对路径，方便 Obsidian 和普通阅读器同时打开。

## 1.3_提交粒度

一次提交只做一类事情：

- 新增一篇笔记。
- 整理一组同主题笔记。
- 修复错别字、链接、图片路径。
- 调整目录结构。
- 更新项目规则或工具说明。

避免把“新增驱动实验 + 重排内核章节 + 修改 README + 清理图片”塞进一个提交。真要一起做，也建议拆成多个 commit。

## 1.4_提交信息格式

推荐格式：

```text
<类型>(<范围>): <一句话说明>
```

校验正则：

```regex
^(add|update|rewrite|fix|structure|format|link|asset|meta|archive|chore)\([^)]+\): .+$
```

规则说明：

- `<类型>` 固定只能使用下表中的类型，不允许自造。
- `<范围>` 可以自由填写，用来标记目录、主题、芯片、模块、平台或自定义标记，但不能为空。
- `<一句话说明>` 可以自由编辑，必须采用中文描述，用一句话说明本次提交保存了什么，且不能为空。

固定类型：

| 类型 | 适用场景 |
| --- | --- |
| `add` | 新增文档、章节、条目、资料 |
| `update` | 补充已有内容，小幅增加说明 |
| `rewrite` | 重写内容、重构表达、大幅改写章节 |
| `fix` | 修正错别字、错误描述、坏链接、图片路径 |
| `structure` | 调整目录、章节顺序、文件命名、文件位置 |
| `format` | 调整 Markdown 排版、表格、标题层级、列表格式 |
| `link` | 维护 Obsidian 链接、Markdown 引用、图片引用路径 |
| `asset` | 新增、替换、整理图片、附件、压缩包等资源 |
| `meta` | 修改 README、Git 规则、AGENTS、仓库说明等元信息 |
| `archive` | 归档旧资料、草稿、过时内容 |
| `chore` | 其他不改变笔记正文含义的维护性修改 |

范围可以任意填写，也可以参考一级或二级目录名：

- `kernel`
- `driver`
- `board/nxp`
- `board/rk3566`
- `appendix`
- `tools/typora`
- `docs`

示例：

```text
add(kernel): 新增 Linux 内核常用数据结构章节
update(driver): 补充字符设备驱动框架笔记
fix(board/nxp): 修正 imx6ull u-boot 移植图片路径
structure(appendix): 重排红黑树章节顺序
meta(git): 增加个人提交规则
link(obsidian): 调整笔记链接管理说明
chore(.gitignore): 忽略本地临时文件
```

## 1.5_推荐工作流

### 1.5.1_在_master_写驱动内容

```bash
git switch master
git pull origin master
# 修改 driver/、board/ 等驱动相关内容
git status
git add <files>
git commit -m "add(driver): 新增 xxx 驱动实验笔记"
git push origin master
```

### 1.5.2_在_obsidian_version_整理内核和链接

```bash
git switch obsidian_version
git pull origin obsidian_version
# 修改 kernel/、appendix/、docs/、README.md 等内容
git status
git add <files>
git commit -m "structure(kernel): 调整 xxx 章节链接结构"
git push origin obsidian_version
```

## 1.6_跨分支同步规则

如果某个通用文件需要两个分支都保留，例如 `README.md`、`docs/git-guide.md`、`AGENTS.md`：

1. 先在当前分支提交。
2. 切到另一个分支。
3. 使用 `git cherry-pick <commit-id>` 同步这一个提交。
4. 检查是否误带入当前分支不需要的目录。

示例：

```bash
git log --oneline -5
git switch master
git cherry-pick <commit-id>
git status
```

不要为了同步一个规则文件，直接把整个 `master` 合并进 `obsidian_version`。

## 1.7_Obsidian_使用注意

- `obsidian_version` 分支用于 Obsidian 链接管理和重新排版，目录结构可以围绕阅读体验调整。
- 移动 Markdown 文件时，优先用 Obsidian 内部移动功能，让链接自动更新。
- `.obsidian/` 配置只有在确实影响阅读或链接管理时再提交。
- 临时草稿、缓存、无关插件状态不建议提交。

## 1.8_文件命名建议

- 章节型笔记保留当前风格：`P01-主题.md`、`第1章_主题.md`。
- 板级、芯片、外设相关文件名尽量包含对象：如 `imx6ull`、`rk3566`、`lcd`、`enet`。
- 图片文件名尽量描述图的用途，不使用纯数字截图名。
- 大范围改名或移动目录时单独提交，方便以后用 `git log --follow` 查历史。

## 1.9_不建议提交的内容

- 临时测试输出。
- 编辑器缓存。
- 未确认来源的大体积资料。
- 与当前学习主题无关的下载包。
- 只在本机有效的绝对路径配置。

如果必须提交二进制文件，例如板级工具压缩包、截图、参考图片，提交信息里写清楚用途。
