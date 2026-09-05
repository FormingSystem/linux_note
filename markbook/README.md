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
- 首次建刊可以显式传入 `--initial`；它只允许没有历史版本的专题在非 1 日生成首版。
- 只有维护者明确要求覆盖更新时才使用 `--overwrite`。普通更新进入下一月版本。
- 已生成的 HTML 不手工编辑；源文件变化后由生成器完整重建并重写来源台账。

仓库的 `.github/workflows/markbook_monthly.yml` 在中国时区每月 1 日 08:00 运行单测、试生成全部启用专题并校验来源与产物。该工作流只有仓库读取权限，**不得提交或推送 `master`**；runner 中的试生成结果在任务结束后丢弃，只回答“当前权威来源能否生成本月刊物”。这是有意的 Git 边界：GitHub 托管 runner 无法在本机离线、关机或工作区有修改时同步移动本地分支，让远端自动写 `master` 必然会制造本地落后窗口。

正式发布采用 **本地先形成提交、远端后接收提交** 的顺序。发布前先确认本地 `master` 与 `origin/master` 指向同一提交且工作区为空；生成、验证和提交都在本地完成，最后执行普通 push。若 push 被拒绝，本地发布提交仍完整保留，应先取得远端变化并处理，不得强推，也不得让远端自动生成一份本地不存在的提交。

## 1.4\_维护命令

在仓库根目录执行：

```bash
# 发布前：以下两项都必须为空或显示 0 0
git status --short
git fetch origin
git rev-list --left-right --count master...origin/master

npm ci --prefix markbook

# 每月 1 日生成全部启用专题
npm run publish:all --prefix markbook

# 验证、在本地提交，然后普通推送
npm run verify --prefix markbook -- --all --release-month YYYY.MM
git add -- markbook/runtime markbook/topics
git diff --cached --check
git commit -m "release(markbook): 发布 YYYY.MM 专题月刊"
git push origin HEAD:master

# 首次建刊（仅当专题还没有任何版本）
npm run publish --prefix markbook -- --topic rcu --release-month 2026.08 --initial

# 验证某一期的产物和来源台账
npm run verify --prefix markbook -- --topic rcu --release-month 2026.08

# 单独验证指定月份的全部启用专题
npm run verify --prefix markbook -- --all --release-month 2026.08
```

显式覆盖使用 `--overwrite`，执行前必须先获得开发者的覆盖授权。详细字段由 [`manifest.schema.json`](schemas/manifest.schema.json) 约束，专题内容编排见 `manifests/`。

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
