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

每期 HTML 都带有专业封面、分卷目录、章节导航、全文检索、阅读进度、离线 Mermaid、打印样式和来源台账。`publication.json` 记录源文件 SHA-256、仓库 `HEAD`、当时的工作树状态以及生成产物哈希，使读者可以判断该期究竟收录了什么。

## 1.2\_当前刊物

- [RCU MarkBook](topics/rcu/README.md)：从 RCU 的问题与抽象模型，一直读到 Linux 6.12.20 的模块导航、唯一实现讲解和晚到/抢占读者实验。

## 1.3\_月度发布契约

- 常规版本使用 `YYYY.MM`，发布日期固定为中国时区当月 1 日。
- 每月 1 日统一执行一次全部启用专题的发布；同专题同月份已经存在时，发布在任何写入前失败。
- 首次建刊可以显式传入 `--initial`；它只允许没有历史版本的专题在非 1 日生成首版。
- 只有维护者明确要求覆盖更新时才使用 `--overwrite`。普通更新进入下一月版本。
- 已生成的 HTML 不手工编辑；源文件变化后由生成器完整重建并重写来源台账。

仓库的 `.github/workflows/markbook_monthly.yml` 在中国时区每月 1 日 08:00 运行单测、生成全部启用专题、校验来源与产物，然后以一次符合仓库提交规范的快进提交发布。当远端在构建期间前进、同月版本已经存在或任何校验失败时，推送会失败，不使用强制更新。

## 1.4\_维护命令

在仓库根目录执行：

```bash
npm ci --prefix markbook

# 每月 1 日生成全部启用专题
npm run publish:all --prefix markbook

# 首次建刊（仅当专题还没有任何版本）
npm run publish --prefix markbook -- --topic rcu --release-month 2026.08 --initial

# 验证某一期的产物和来源台账
npm run verify --prefix markbook -- --topic rcu --release-month 2026.08

# 验证当月全部启用专题
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
