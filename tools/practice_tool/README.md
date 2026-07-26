---
id: tools.practice_tool
title: "回路 Loop 知识训练工具"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_回路\_Loop\_知识训练工具

**回路（Loop）** 是一个由外部知识源驱动的本地知识训练工具。中文正式名称为“回路”，英文正式名称为“Loop”。当前版本先把所选材料提炼成目录清晰的专题电子书，再进入提示提问、脱稿输出和专业案例训练；章节不复制完整知识正文，并保留稳定原文引用：

1. **提示提问**：在具体小场景中逐级给出提示，辅助形成局部因果模型。
2. **脱稿输出**：撤掉知识提示，独立重建边界清晰的时序、状态或通信模块。
3. **专业案例**：分析工程证据，给出诊断、方案、不可规避成本和选择边界。

长期验证平台为 Windows 的 MSYS2 UCRT64/UCRT32 Bash，以及 Linux 的 Ubuntu 22.04 Bash。三个环境统一使用 `start.sh` 作为便利编排入口；安装、运行和卸载分别由 `install.sh`、`run.sh` 和 `uninstall.sh` 独立负责。PowerShell 只负责在尚未具备 MSYS2 的 Windows 上下载、安装和准备 UCRT 环境，不直接运行训练任务；CMD 不属于正式使用平台。

当前随工具提供的首个示例内容来自 `linux-note`，训练单元位于：

```text
banks/linux/synchronization/rcu/foundation_model/
banks/linux/data_structures/rbtree/
banks/linux/data_structures/hash_table/
```

三个示范训练单元分别引用 `linux-note` 的 RCU、红黑树和哈希表权威正文。专题电子书经过筛选、去噪和重新组织，不是原文导入；界面展示知识源、稳定文档 ID 和相对路径，便于追根溯源。用户单元目录和训练模块属于本地组织行为，不改变外部知识库目录。

当前内容基线为 **3 本专题电子书、12 个学习章节和 12 个训练任务**。每本书包含目录大纲、四章 Markdown 正文、知识声明、关系、证据、章节核验和训练计划；每个专题另有两道提示提问、一项脱稿输出和一个专业案例。训练库提供用户单元目录，支持任意层级、拖拽或方向键排序、跨目录归类、回收以及页面会话内撤销和重做；单元可以设置为单一目录或多目录归属。

## 1.1\_独立化边界

工具目录按未来直接迁移为独立仓库的边界维护：

```text
practice_tool/
├── banks/                   # 可校验的模块化训练内容
├── docs/                    # 工具自己的运行与排障文档
├── schemas/                 # 内容数据协议
├── scripts/                 # 环境准备、统一平台层与内容检查
├── src/                     # 前端程序
├── start.sh                 # 首次安装与运行的便利编排入口
├── install.sh               # 独立安装/升级模块
├── run.sh                   # 纯运行模块，不执行安装
├── uninstall.sh             # 独立两级卸载模块
├── package.json
└── README.md
```

业务前端采用 Feature-first 结构：`src/app` 负责装配，`src/features` 按训练业务组织，`src/infrastructure` 隔离浏览器存储和外部访问，`src/shared` 只保存无业务归属的基础类型。详细规则见：

- [架构设计索引](docs/architecture/README.md)
- [当前实现状态与版本边界](docs/architecture/implementation_status.md)
- [工程结构与模块边界](docs/architecture/engineering/project_structure_and_module_boundaries.md)
- [专题电子书编写与提炼标准](docs/architecture/product/topic_ebook_editorial_standard.md)
- [知识提炼、训练适配与 AI 治理](docs/architecture/product/content_adaptation_and_ai_governance.md)
- [产品导航与交互设计](docs/architecture/product/navigation_and_interaction.md)
- [训练会话状态与持久化](docs/architecture/product/training_session_state_and_persistence.md)
- [复习调度与训练历史](docs/architecture/product/review_scheduling_and_history.md)
- [导入导出与数据安全](docs/architecture/engineering/import_export_and_data_safety.md)
- [本地服务安全与威胁模型](docs/architecture/engineering/local_service_security_and_threat_model.md)
- [无障碍、性能与产品验收标准](docs/architecture/engineering/accessibility_performance_and_acceptance.md)
- [架构决策记录](docs/architecture/decisions/README.md)

以下内容属于当前 `linux-note` 的集成层，不属于工具核心：

- 知识库根目录的 `practice.cmd` 和 `practice.sh` 快捷入口。
- `knowledge` 中被题库引用的 Linux 权威正文。
- 本仓库的 `AGENTS.md`、治理规范、Atlas 和出版结构。

当前 `0.1.0` 开发版已经提供大厅、目录化训练库、单元详情、可恢复训练会话、专题学习加三阶段训练及总结导航、IndexedDB 自动保存，以及目录和训练模块的正式管理表单。设置页已经提供单元归类方式和撤销历史步数；复习、历史和知识源仍只有边界说明页，不能视为功能完成。精确完成度、验证证据和已知风险统一见 [当前实现状态与版本边界](docs/architecture/implementation_status.md)。

当前已经实现 **启动独立**：环境准备、依赖安装、校验、构建和运行均可在工具目录内完成。当前题库仍携带指向 `linux-note` 正文路径的来源信息；拆分仓库时可以保留稳定文档 ID，并把路径作为内容包集成配置处理，不能把外层相对路径写进工具核心代码。

### 1.1.1\_知识源契约

训练项目只通过三项信息引用材料：

```text
source_id       # 知识源稳定 ID
id              # 文档在知识源中的稳定 ID
path            # 相对于知识源根地址的路径
```

知识源真实位置由独立配置文件提供：

```json
{
  "schema_version": 1,
  "sources": [
    {
      "id": "my-notes",
      "title": "我的知识库",
      "kind": "filesystem",
      "location": "../my-notes"
    }
  ]
}
```

`kind` 当前允许 `filesystem` 和 `http`。文件系统相对地址以 **配置文件所在目录** 为基准解析，因此同一份配置在 Windows 与 Linux 上都不需要依赖工具所在仓库的目录层级。配置协议见 `schemas/knowledge_sources.schema.json`，可复制的示例见 `config/knowledge_sources.example.json`。

启动前通过同一个环境变量指定配置地址：

```bash
PRACTICE_SOURCE_CONFIG=/srv/notes/practice.sources.json ./start.sh
```

配置地址也可以是 HTTP/HTTPS URL；远程配置只能继续声明 `http` 知识源，不能借远程配置指向启动机器上的文件系统。没有指定环境变量时，工具只尝试读取自己的 `config/knowledge_sources.local.json`；该文件属于本机配置并被 Git 忽略。两者都不存在时工具仍可启动，但界面会明确显示“未配置知识源”。

## 1.2\_本地运行

训练工具可以不依赖外层知识库的启动逻辑独立运行。在 MSYS2 UCRT64/UCRT32 或 Ubuntu 22.04 中进入本目录后执行：

```bash
./start.sh
```

`start.sh` 只负责路由和首次运行编排：它先调用 `install.sh --if-needed`，安装模块返回成功后再把运行参数原样交给 `run.sh`。浏览器由 Vite 在服务开始监听后打开，不使用固定延时猜测服务是否就绪。

三个生命周期模块也可以独立使用：

```bash
# 只安装环境和依赖，不启动服务
./install.sh

# 只运行；缺少环境时明确退出，不会偷偷安装
./run.sh

# 只卸载
./uninstall.sh --minimal
```

`scripts/lib/platform_environment.sh` 是所有 Bash 脚本共享的平台环境层，集中提供 Ubuntu 22.04、MSYS2 UCRT64/UCRT32 的识别结果，工具根目录、本地运行时、缓存与离线表路径，CPU 架构、包管理器、Node.js 兼容线和下载源，以及下载、摘要校验、归档解压、路径解析与 MSYS2 Node.js 安装能力。

其他脚本不得再次读取 `MSYSTEM`、`/etc/os-release`、`uname` 或猜测 Windows 盘符；需要 `node`、`npm`、`pacman`、`curl`/`wget`、`sha256sum`、`tar` 等工具时也必须从该环境层取得。这样平台差异只留在环境层，启动、升级、离线安装和补全保持一套 Unix/Bash 操作。

当前知识库根目录仍提供 `practice.cmd` 和 `practice.sh` 作为快捷入口，但它们只负责进入相应 Unix/Bash 环境并转发到本目录的 `start.sh`。训练工具的环境准备和启动逻辑不依赖仓库根目录，便于后续整体拆分为独立仓库。

进入工具根目录后，使用正式入口查看工具定位、命令选项、环境变量和常用示例：

```bash
./start.sh --help
```

`--help` 在环境检查之前处理，不会下载 Node.js、安装 npm 依赖、启动 Vite 或打开浏览器。

Windows 尚未安装 MSYS2 时，只在 PowerShell 中执行一次引导：

```powershell
powershell -ExecutionPolicy Bypass -File .\bootstrap_windows.ps1
```

引导完成后双击 `start_ucrt64.cmd`，它会进入官方 UCRT64 Bash 并转交 `start.sh`。当前官方 MSYS2 不提供 UCRT32；`start_ucrt32.cmd` 仅用于已经存在的自定义 UCRT32 环境，并会在环境不存在时明确退出。

首次在交互式 Bash 中正常执行 `./start.sh` 时，工具会自动把 Tab 补全安装到当前用户目录。也可以主动重新安装：

```bash
./start.sh --install-completion
```

安装器默认创建指向 **当前工具目录** 中 `scripts/completions/start.bash` 的符号链接；不支持符号链接时创建动态加载器，而不是复制静态快照。因此后续在当前目录执行 `git pull` 后，补全规则会直接跟随仓库更新。脚本子进程无法反向修改已经运行的父 Bash，所以首次安装所在的当前终端需要执行一次：

```bash
source <(./start.sh --completion bash)
```

补全覆盖全部正式选项，并会根据前一个参数补全 `--host`、`--port` 和 `--completion` 的可选值，不只是把某个缩写扩展成 `--upgrade`。安装器会幂等地在 `~/.bashrc` 登记动态加载行，因此后续新开的 Ubuntu 22.04 和 Windows MSYS2 UCRT64/UCRT32 Bash 都会自动加载，不需要每个终端重复执行 `source`。若当前终端尚未加载，只需执行上面的 `source` 一次。如需禁止正常启动时自动安装补全，可设置 `PRACTICE_AUTO_COMPLETION=0`。

打开后先进入 **大厅**。可以继续已有训练、搜索单元，并在训练库中用自己的多级目录组织单元。目录和单元支持拖拽及键盘排序；点击左侧单元只选中并定位右侧卡片，点击右侧卡片才进入详情。训练阶段保持推荐顺序，但专题学习、提示提问、脱稿输出、专业案例和总结都可以直接点击进入；每个阶段会恢复上次停留位置。

### 1.2.1\_主动升级运行环境

普通启动会复用已经满足最低兼容线的 Node.js。需要主动查询官方更新、重建工具本地运行时并重新校验项目依赖时执行：

```bash
./start.sh --upgrade
```

也可以只升级而不运行：

```bash
./install.sh --upgrade
```

`start.sh --upgrade` 会调用 `install.sh --upgrade`，安装完成后继续转入 `run.sh`；直接调用安装模块则只升级环境。升级仍按 `24 → 22 → 20 → 18` 从高到低尝试官方可用版本，清除旧的本机就绪标记并重新运行 `npm install`。它不会修改题库、知识正文、知识源配置或用户训练记录。

联网安装遵循就近源优先原则：默认先尝试国内 `npmmirror` 的 Node.js 镜像与 npm 仓库，失败后自动回退到 `nodejs.org` 和 `registry.npmjs.org` 官方源。该选择只作用于本次训练工具安装，不修改用户的全局 npm 配置。需要按所在国家、组织内网或自建镜像调整时，可用空格分隔的 `PRACTICE_NODE_DIST_SOURCES` 和 `PRACTICE_NPM_REGISTRIES` 环境变量覆盖源顺序。

### 1.2.2\_离线运行时缓存

联网下载成功的 Node.js 官方包会保留在：

```text
.local/downloads/node/v<完整版本>/
```

后续升级或重建环境会先检查缓存、校验 SHA-256，再决定是否联网。离线设备可以在其他机器访问 `https://nodejs.org/dist/`，下载当前平台归档和同一版本目录中的 `SHASUMS256.txt`，例如：

```text
.local/downloads/node/v24.18.0/
├── SHASUMS256.txt
└── node-v24.18.0-linux-x64.tar.gz
```

Windows 对应放置 `node-v24.18.0-win-x64.zip`。版本号、平台和 CPU 架构必须与文件名一致，不应自行改名。`.local/` 已整体加入 `.gitignore`，运行时、离线包和校验文件都不会被 Git 识别。

环境安装开始时会等待 5 秒，可选择自动下载、手动指定一个包或读取离线包表。表格模板为：

```text
config/offline_node_packages.example.tsv
```

复制为被 Git 忽略的 `config/offline_node_packages.local.tsv` 后填写：

| 列 | 含义 |
| --- | --- |
| `enabled` | `1` 表示参与安装，`0` 表示示例或停用 |
| `platform` | `linux` 或 `windows` |
| `arch` | `x64`、`arm64` 等官方架构名 |
| `archive` | 官方归档路径 |
| `checksums` | 同版本官方 `SHASUMS256.txt` 路径 |

`archive` 和 `checksums` 可以是绝对路径，也可以是相对于 `practice_tool` 根目录的路径。Linux 根据后缀支持 `.tar.gz`、`.tgz` 和 `.tar.xz`；Windows支持 `.zip`，存在 `7z.exe` 或 `7za.exe` 时也支持 `.7z`。安装器先核对官方文件名中的版本、平台和架构，再校验 SHA-256，最后检查解压后的 `node`、`npm` 标准结构。

### 1.2.3\_第一次启动

第一次运行 `start.sh` 时会依次执行：

```text
start.sh 调用 install.sh --if-needed
    ↓
定位 Node.js/npm 并检查 Node.js 主版本
    ↓
低于 v18 时按优先级准备官方兼容版本
    ↓
安装 practice_tool 项目依赖
    ↓
写入 .local/environment-ready-v3-node-compatible
    ↓
start.sh 转交 run.sh
    ↓
启动平台并打开浏览器
```

`.local/environment-ready-v3-node-compatible` 是本机环境就绪标记，不进入 Git。标记存在且 `node_modules` 仍然存在时，后续启动不会再次执行依赖安装。Node.js 版本在每次启动时都会重新检查，不兼容版本不能因为存在就绪标记而绕过门禁。

如果手动删除了 `node_modules`，启动器会清除旧标记并重新安装依赖。如果要主动重建环境，可以删除：

```text
.local/environment-ready-v3-node-compatible
node_modules/
```

然后重新运行本目录的 `start.sh`。

自动安装支持：

- MSYS2 UCRT64/UCRT32：复用兼容的 MSYS2 Node.js；缺失或过旧时通过 `pacman` 安装当前环境对应的软件包，不使用 `sudo`。
- Ubuntu 22.04：依次查询各下载源的 `latest-v24.x`、`latest-v22.x`、`latest-v20.x` 和 `latest-v18.x`，每个版本先尝试国内镜像、再尝试官方源，选择当前架构存在且能够成功下载的最高版本，校验 SHA-256 后安装到 `.local/runtime`，不替换系统 Node.js。

Ubuntu 22.04 下载需要 `curl` 或 `wget`、`sha256sum`、`tar` 和 `gzip`。某个源不可用时先切换下一源；某个版本找不到、下载失败或没有当前架构归档时再尝试下一个版本。只有所有 **仍满足最低兼容线** 的来源和版本都不可用时才停止；不能为了表面启动而退回到 Vite 无法运行的 Node.js 12。

> MSYS2 的标准安装目录通常由当前用户直接维护，安装软件包使用 `pacman -S`，不应在前面添加 `sudo`。脚本通过 `MSYSTEM` 和 `MINGW_PACKAGE_PREFIX` 识别 UCRT64/UCRT32，并选择匹配的软件包。

设计边界见：[跨平台与仓库独立性设计](docs/cross_platform_and_repository_independence.md)。

跨平台启动、虚拟机克隆、代理排障和常见错误见：[环境与故障排查](docs/environment_and_troubleshooting.md)。

在 MSYS2 UCRT64/UCRT32 中，可以直接从训练工具目录执行：

```bash
cd path/to/practice_tool
./start.sh
```

也可以绕过启动器手动运行：

```bash
cd path/to/practice_tool
npm install
npm run dev
```

训练会话、作答进度、用户创建的单元目录、训练模块和全局训练库设置保存在浏览器 `IndexedDB`，不会在每次作答后污染 Git 工作区。撤销和重做入口固定在全局顶部导航，具体历史仍只保留在提供该能力的当前页面会话中，默认上限 1000 步，不写入 IndexedDB。旧版工作区数据会从 `localStorage` 一次性迁移；清理浏览器站点数据仍会删除全部本地记录。

## 1.3\_内容检查

```bash
npm run check:data
npm run build
```

内容检查验证单元、书籍、章节、知识声明、关系、证据、章节核验和三阶段训练结构，并检查稳定 ID、章节依赖、训练绑定、引用闭包及重复 ID。Markdown 章节必须存在并达到能够承载完整主题推导的最低长度。
同时检查知识源配置 Schema 示例，确保 Windows、Linux 和外部仓库使用同一份配置协议。

## 1.4\_新增单元

每个单元使用以下内容包：

```text
unit.json
book.json
outline.md
chapters/
knowledge/
  claims.json
  relations.json
  source_map.json
training/
  plan.json
  chapter_checks.json
  guided_questions.json
  model_tasks.json
  professional_cases.json
```

所有可选择单元统一登记在：

```text
banks/index.json
```

新增题库目录并登记索引后，界面会自动发现和加载单元，不需要修改页面代码。

索引字段只用于描述随版本发布的示范训练单元，不代表用户知识分类：

| 字段 | 用途 |
| --- | --- |
| `domain` | 顶层知识领域，例如 `linux` |
| `topic` | 示例内容主题，例如 `data-structures` |
| `module` | 示例训练单元标识，例如 `rbtree` |
| `level` | 单元层次，例如 `foundation` |
| `tags` | 用户搜索时使用的中文或英文关键词 |
| `unit_file` | 单元入口文件相对 `banks/` 的路径 |

用户不需要知道示范内容目录结构。平台首页使用这些字段完成搜索和单元卡片展示；用户自己的上下级分类由训练工作区独立管理。

- `unit.json` 只保存单元身份、权威正文引用、电子书和三个训练阶段入口。
- `book.json` 保存书籍版本、有序章节和治理文件入口。
- `outline.md` 保存面向读者的学习地图，`chapters/` 保存重新提炼的章节正文。
- `knowledge/` 保存声明、关系和原文证据，`training/plan.json` 绑定书籍版本和训练内容。
- `training/chapter_checks.json` 保存每章核验问题、开放联想和拓扑记忆。
- `training/guided_questions.json` 保存轻量场景、递进提示和最小模型骨架。
- `training/model_tasks.json` 保存无提示输出任务、输出约束和核验问题。
- `training/professional_cases.json` 保存工程背景、证据、问题和专业评审维度。

题目通过稳定文档 ID 引用权威正文。正文路径只用于打开源文档，不作为题目永久身份。

## 1.5\_环境脚本

| 文件 | 职责 |
| --- | --- |
| `start.sh` | 首次安装、运行、升级和卸载命令的便利编排入口 |
| `install.sh` | 独立准备 Node.js 与 npm 项目依赖，不启动服务 |
| `run.sh` | 独立运行已安装的平台，环境缺失时拒绝隐式安装 |
| `bootstrap_windows.ps1` | PowerShell 冷启动引导，只安装和准备 MSYS2 |
| `start_ucrt64.cmd` | 可双击的 UCRT64 Bash 转发入口 |
| `start_ucrt32.cmd` | 已有自定义 UCRT32 环境的兼容转发入口 |
| `scripts/install_environment.sh` | 安装模块内部使用的 MSYS2/Linux Node.js 运行时安装器 |
| `scripts/lib/platform_environment.sh` | 平台识别、路径、工具和公共环境函数的唯一来源 |
| `uninstall.sh` | MSYS2/Linux 两级卸载与所有权核对 |
| `uninstall_windows.ps1` | 退出 MSYS2 后清理仅由工具安装的 MSYS2 根环境 |
| `../../practice.cmd` | 当前知识库的 Windows 快捷入口，只转发到 UCRT64 Bash 中的 `start.sh` |
| `../../practice.sh` | 当前知识库的 MSYS2/Linux 快捷入口，只转发到 `start.sh` |

程序、题库协议、环境准备和启动逻辑全部留在工具目录。当前知识库根目录的两个文件不保存工具逻辑，移除后不影响从工具目录直接启动；拆分成独立仓库时不应复制这两个快捷入口。

## 1.6\_评分含义

- `需要重建`：关键因果链无法独立启动。
- `部分输出`：主方向正确，但缺少参与者、状态位置、顺序或边界。
- `完整输出`：不看提示即可覆盖任务的必要输出。

评分是下一轮训练的调度输入，不代表知识本身的正确答案。

## 1.7\_更新与卸载

版本与安全清单随 Git 发布，浏览器只能读取。服务在后台低频检查仓库更新，仅在发现新提交时显示提示；用户主动点击后才执行 `git pull --ff-only`，工作区不干净时自动拒绝。

```bash
# 保留隔离运行环境与下载缓存
./uninstall.sh --minimal

# 额外删除仅由工具从无到有安装的软件与缓存
./uninstall.sh --clean
```

外部已有软件在更新前必须取得用户确认。即使用户同意更新，其所有权也登记为 `external-updated`，干净卸载不会删除。完整设计见 [安全、版本更新与软件生命周期设计](docs/security_version_update_and_lifecycle.md)。

## 1.8\_版权与授权

回路（Loop）的原创程序代码、用户界面、原创文案、原创文档和训练题库结构由 FormingSystem 维护。当前随仓库公开的开发版本采用 `GPL-2.0-only`：

```text
原创：回路（Loop）
Copyright © 2026 FormingSystem · GPL-2.0-only
二次开发请保留来源、原始项目地址及修改说明。
```

联系邮箱：`lizhaojun97@qq.com`

希望改动进入官方版本时，应先联系维护者对齐需求；独立分叉可以按许可证进行，但必须明确为非官方版本。未来新版本可以采用不同发布策略，已经发布的版本仍保持其发布时许可证。第三方依赖和外部知识材料继续遵循各自许可证。完整边界见 [版权、开源与贡献声明](COPYRIGHT.md) 和仓库根目录 [GPL-2.0 许可证](../../LICENSE)。
