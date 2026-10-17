---
id: markbook.readme
title: "MarkBook 专题电子书"
kind: reference
status: evolving
domains:
  - publication
  - repository
---

# 第1章\_MarkBook\_专题电子书

## 1.1\_定位

MarkBook 把同一专题分布在知识正文、版本化源码阅读、唯一实现讲解、实验和交叉边界中的真实材料，编排成适合连续阅读、全文检索和打印的月度电子书。它是 **可复现的派生出版物**，不是第二份知识正文；修订内容时应修改源文件，下一个月刊再重新收录。

采用当前阅读器模板的期刊带有专业封面、分卷目录、章节导航、全文检索、阅读进度、VS Code Light 代码主题、离线 Mermaid 大图查看器、打印样式和来源台账。代码语法高亮在构建期完成，成书不依赖在线服务；未知语言可靠退化为纯文本。正文中的 Mermaid 保留自然尺寸并可局部横向滚动；点击“大图查看”后可以独立适屏、缩放和拖动，退出不会改变正文图尺寸。`publication.json` 记录源文件 SHA-256、仓库 `HEAD`、当时的工作树状态以及生成产物哈希，使读者可以判断该期究竟收录了什么。已经发布的月份保持原始阅读器快照，不因模板升级被静默改写。

## 1.2\_当前刊物

- [RCU MarkBook](topics/rcu/README.md)：从 RCU 的问题、通用契约和分类坐标，依次进入普通 Tree RCU、SRCU、Tasks 与 Tiny 的原理及当前已有 Linux 6.12.20 源码证据，最后用对象生命周期、工程应用、诊断和选择边界收束。

## 1.3\_月度发布契约

- 常规版本使用 `YYYY.MM`，发布日期固定为中国时区当月 1 日。
- 每月 1 日由维护者在本地已同步且干净的 `master` 上统一执行一次全部启用专题的发布；同专题同月份已经存在时，发布在任何写入前失败。
- 首次建刊可以显式传入 `--initial`；它只允许没有历史版本的专题生成中国时区当前月份的首版，可以在非 1 日执行。
- 只有维护者明确要求覆盖更新时才使用 `--overwrite`。普通更新进入下一月版本。
- 已生成的 HTML 不手工编辑；源文件变化进入下一期，由生成器为新一期建立完整产物与来源台账。旧期不会自动吸收新的正文。

月刊冻结解决的是“以后还能读到当时那一版”的问题。清单可以继续调整章节顺序，正文也可以继续修订；只有执行一次合法发布，生成器才把那一刻的来源编排成新版本，并更新专题目录台账与 `latest.html`。因此，当前清单、最新正文和最近一期成书可能不同，查阅时先选定自己需要哪一个。

上述干净 `master` 与远端同步要求属于仓库发布流程。生成器会记录 `HEAD` 和来源的未提交状态，并检查月份、重复版本及覆盖模式，**不会替维护者核对当前分支就是 `master` 或与远端同步**。生成成功也不能据此证明已满足 Git 发布条件。

仓库的 `.github/workflows/markbook_monthly.yml` 在中国时区每月 1 日 08:00 运行单测、试生成全部启用专题并校验来源与产物。该工作流只有仓库读取权限，**不得提交或推送 `master`**；runner 中的试生成结果在任务结束后丢弃，只回答“当前权威来源能否生成本月刊物”。这是有意的 Git 边界：GitHub 托管 runner 无法在本机离线、关机或工作区有修改时同步移动本地分支，让远端自动写 `master` 必然会制造本地落后窗口。

正式发布采用 **本地先形成提交、远端后接收提交** 的顺序。发布前先确认本地 `master` 与 `origin/master` 指向同一提交且工作区为空；生成、验证和提交都在本地完成，最后执行普通 push。若 push 被拒绝，本地发布提交仍完整保留，应先取得远端变化并处理，不得强推，也不得让远端自动生成一份本地不存在的提交。

## 1.4\_维护命令

以下维护步骤在仓库根目录执行，要求 Node.js 24 或更高版本及 npm。示例使用 Bash；`YYYY.MM` 要替换为本次中国时区的月份，不能原样复制。普通发布只能在当月 1 日执行。首次建刊和历史验证另见后文。

先核对 Git 前提：

```bash
# 必须输出 master；有未完成的专题修改时先完成正常评审与合入
git branch --show-current
# 必须没有输出
git status --short
git fetch origin
# 必须输出 0 0
git rev-list --left-right --count master...origin/master
```

`fetch` 只更新远端跟踪信息，不会把本地分支变成远端版本。分支、工作区或差异计数不符合条件时，先处理实际差异，再继续；不要直接切换分支或重置来清空尚未完成的工作。

准备依赖并验证生成器，再生成当月产物：

```bash
npm ci --prefix markbook --ignore-scripts
npm test --prefix markbook
npm run publish:all --prefix markbook
```

`publish:all` 读取启用专题的清单，月份默认取中国时区当前月。重复月份会被拒绝；这时应检查已有发行记录，不能把 `--overwrite` 当作重试开关。生成器测试通过证明工具的受测行为；生成成功之后还要核对实际成书。

```bash
npm run verify --prefix markbook -- --all --release-month YYYY.MM --require-current-sources
git diff --stat
```

此处要求来源仍与刚生成时一致，避免生成之后又改正文却提交旧成书。还需打开本期 `index.html`，检查目录、图、代码和打印布局，并核对来源台账中的提交与来源状态。验证器能检查哈希、链接和 Mermaid 语法，不能替代阅读和人工版面检查。

确认产物后在本地提交，再推送：

```bash
git add -- markbook/runtime markbook/topics
git diff --cached --check
git diff --cached --stat
git commit -m "release(markbook): 发布 YYYY.MM 专题月刊" \
  -m "- 固定本期来源快照与离线阅读产物，完成来源一致性及成书核对"
git push origin HEAD:master
```

提交明细须与本次实际完成的检查一致。上述路径应只包含本次发布的运行时和专题产物；检查暂存区后再提交。若其他任务留下了无关暂存内容，先处理其归属，不能混入月刊提交。

首次建立一个新专题时，先创建符合 [manifest schema](schemas/manifest.schema.json) 的清单，再使用下面的命令模式。`TOPIC_ID` 是新清单的 `topic_id`，不是现有 `rcu` 的别名；RCU 已有历史版本，不能再用 `--initial` 建刊。

```bash
npm run publish --prefix markbook -- --topic TOPIC_ID --initial
```

省略月份时仍使用中国时区当前月份。`--initial` 只放宽每月 1 日的限制，不允许补造过去月份，也不放宽前面的 Git 发布流程。

已有期刊可以随时只读验证：

```bash
npm run verify --prefix markbook -- --topic rcu --release-month 2026.08
npm run verify --prefix markbook -- --all --release-month 2026.08
```

这里的 `--all` 指当前清单中启用的专题，不是遍历全部历史发行目录；若后来新增的专题没有 2026.08 期，验证该专题会失败，应改用确实存在该期的专题 ID。

| 验证方式或结果 | 可以确认什么 | 不应据此推断什么 |
| --- | --- | --- |
| 普通 `verify` 成功 | 已登记产物的哈希相符、书内本地链接可解析、Mermaid 语法通过 | 内容事实正确或已通过人工评审 |
| `current source differences` 大于 0 | 当今工作树中的来源与该期记录不同或已不存在 | 冻结产物损坏；历史期刊允许来源继续演进 |
| 加 `--require-current-sources` 成功 | 在上述检查之外，记录的来源文件也与当前工作树哈希一致 | 当前 manifest、模板或所有仓库文件都与发行时一致 |

来源台账保存路径、哈希与 Git 身份，供追溯使用，不嵌入每个来源文件的原始 Markdown 字节。干净来源可从记录的 Git 提交取回；若历史期标记有未提交来源，仅凭 `HEAD` 和哈希不能重建那部分原文，需要当时保留的来源快照。哈希检查也不是对台账真实性的数字签名验证。

显式覆盖使用 `--overwrite`，仅用于已存在的版本，执行前必须先获得开发者的覆盖授权；它可以重写历史月份，因而会破坏通常的冻结边界。详细字段由 [`manifest.schema.json`](schemas/manifest.schema.json) 约束，专题内容编排见 `manifests/`。

## 1.5\_目录职责

```text
markbook/
├── manifests/       # 一专题一份权威编排清单
├── runtime/         # 带版本号、不可原地替换的离线运行时
├── schemas/         # manifest 结构约束
├── scripts/         # 发布与验证工具
├── templates/       # 阅读器和打印模板
└── topics/          # 专题目录、版本目录和发行台账
```

本机 `node_modules/` 不进入 Git。版本目录中不得写入盘符、UNC、用户名、IP 或其他机器专属路径。
