---
id: governance.conventions.git_guide
title: "Git协作、提交与发布规范"
kind: reference
status: stable
domains:
  - governance
---

# 第1章\_Git\_协作、提交与发布规范

## 1.1\_目标

Git 历史必须直接表达“哪个项目的哪个模块产生了什么可验证结果”。知识正文与回路（Loop）工具共享仓库期间使用同一语法，但通过稳定范围分离；不得再使用 `add`、`update`、`rewrite`、`structure`、`meta` 等无法稳定表达软件变更性质的旧类型。

一次提交必须能够独立审查和回退。实现所必需的测试、Schema、帮助与文档应随实现一起提交；独立治理政策、纯内容重写或发布动作单独提交。

## 1.2\_分支与工作区

`master` 是可发布主线，只接受通过验证的提交。任务使用短生命周期分支：

```text
feat/practice-module-import
refactor/practice-lifecycle
security/practice-updater
content/rcu-grace-period
docs/repository-git
```

并行任务优先使用独立 worktree，避免多个进程共享索引和工作区：

```bash
git worktree add ../work-practice-lifecycle \
  -b refactor/practice-lifecycle
```

任务完成后基于远端主线整理：

```bash
git fetch origin
git rebase origin/master
git switch master
git merge --ff-only <任务分支>
```

主线只允许快进更新。已经确认或推送的提交不得用 `git reset` 改写；撤销使用格式化的 `revert` 提交。未推送的临时修正可以使用 `git commit --fixup` 和 `git rebase --autosquash`。

## 1.3\_提交标题

统一格式：

```text
<类型>[(<project>/<module>)]!?: <中文结果>
[可选：标题无法完整表达时才添加]
- 描述1
- 描述2
```

`(<project>/<module>)` 整体可选，不设项目、模块或语言白名单。需要定位归属时，按照本次修改的真实对象自由填写；变更横跨整个仓库且不存在准确范围时可以省略。范围允许只写项目，也允许写“项目/模块”两层，中文和英文都合法，只禁止会破坏提交语法的空白、括号、冒号和额外斜杠。当前仓库维护者习惯使用与目录一致的英文标识，但这只是个人习惯，不是其他使用者必须遵守的校验规则。`!` 表示破坏性变更。

允许的类型：

| 类型 | 用途 |
| --- | --- |
| `feat` | 新增用户或开发者可观察的能力 |
| `fix` | 修复错误行为 |
| `refactor` | 调整实现或架构，不以新增功能为主 |
| `perf` | 改善性能或资源占用 |
| `security` | 修改权限、校验、供应链或安全边界 |
| `content` | 新增或重写知识正文、题库和研究内容 |
| `docs` | 修改使用说明、设计文档和治理规范 |
| `test` | 新增或调整测试与验证工具 |
| `build` | 修改依赖、构建、安装和打包 |
| `ci` | 修改自动检查与发布流水线 |
| `release` | 发布版本和同步版本清单 |
| `revert` | 撤销已经确认的历史提交 |
| `chore` | 不改变产品、知识或构建语义的维护 |

范围示例仅用于展示命名方式，不构成固定列表：

```text
practice/install
practice/runtime
practice/ui
practice/bank
practice/update
practice/security
knowledge/rcu
knowledge/kernel
repository/git
repository/format
repository/obsidian
publication
```

标题示例：

```text
feat(practice/bank): 支持按模块和训练阶段导入题库
refactor(practice/runtime)!: 移除旧启动流程并统一生命周期入口
fix(practice/install): 修正Ubuntu离线包摘要校验
security(practice/update): 限制浏览器只能执行固定更新动作
content(knowledge/rcu): 补充宽限期状态汇聚过程
docs(repository/git): 更新分支与提交规范
release(practice): 发布回路0.2.0
docs: 更新仓库级使用说明
```

禁止使用“更新信息”“修复一些问题”“同步修改”等无法定位结果的描述。

## 1.4\_提交正文

标题能够完整表达单一结果时，不得为了格式强制增加正文。只有存在多个结果、验证结论、风险或破坏性影响，无法在标题中清楚表达时才增加正文。正文中的每一行必须使用 `- ` 列表，不允许混入分段标题或散文。

需要正文时使用：

```text
refactor(practice/runtime)!: 拆分生命周期入口
- install.sh独立负责环境与依赖
- run.sh只运行已就绪的平台
- npm run check:data
- npm run build
- 删除旧的隐式安装调用方式
```

正文不得使用“描述1”之类占位内容，不得重复标题，也不得把一句话机械拆成标题和正文两部分。

## 1.5\_提交粒度

一个提交只形成一个结果：

- 功能实现连同必要 Schema、测试与使用说明。
- 一个可独立说明的知识模块新增或重写。
- 一个完整且可验证的架构重构。
- 一项安全边界调整。
- 一次版本发布。

不得按文件类型机械拆分“代码提交”“文档提交”“信息更新提交”。也不得把多个无关模块塞进一个笼统提交。

长任务中的临时修复使用 `fixup!` 提交，合入主线前 autosquash；主线不保留“补一下”“再修一次”式噪声历史。

## 1.6\_提交前验证

先启用版本化配置：

```bash
git config core.hooksPath .githooks
git config commit.template governance/templates/git_commit_message.txt
```

知识库通用检查：

```bash
./format.sh doctor
./format.sh check all --summary
git diff --check
git status --short
git diff --stat
git diff
```

修改回路（Loop）工具时至少运行：

```bash
cd tools/practice_tool
npm run check:data
npm run build
cd ../..
git diff --check
```

提交前必须检查暂存区，而不是只看工作区：

```bash
git add <明确文件>
git diff --cached --check
git diff --cached
```

带 shebang 且设计为直接执行的 Shell 脚本必须以 Git 模式 `100755` 提交；只供 `source` 的脚本库使用 `100644`。

## 1.7\_撤销与历史修改

- 工作区修改错误：直接编辑修正。
- 未推送的最后一次提交：允许 `git commit --amend`。
- 未推送分支中的临时提交：使用 `fixup` 和 `autosquash`。
- 已推送或已交付提交：使用 `revert(<范围>): <中文结果>`。
- 主分支禁止 `git reset`、强制移动分支和普通强推。
- 只有经过开发者明确授权的仓库级历史迁移才能使用 `--force-with-lease`，执行前必须创建并验证完整 bundle 备份。

## 1.8\_回路版本与仓库版本

回路使用独立语义版本，发布标签采用：

```text
loop-v0.2.0
```

知识库快照使用日期标签：

```text
linux-note-v2026.07
```

回路发布提交必须同步 `package.json`、`package-lock.json`、`config/release.json`、CHANGELOG 和相关 Schema，并完成构建验证。

## 1.9\_不提交的内容

- 临时测试输出、编辑器缓存和构建产物。
- `.local`、`node_modules`、`dist` 和日志。
- 本机绝对路径、个人凭据和访问令牌。
- 未确认来源或授权的大体积资料。
- 没有入口说明、环境说明和验证结论的零散实验。
- 可由构建过程重新生成的中间文件。
