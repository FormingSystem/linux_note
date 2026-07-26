---
id: tools.practice_tool
title: "回路知识训练工具"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_回路知识训练工具

“回路”是一个以模块化题库驱动的本地知识训练工具。当前版本把外部知识来源组织成三阶段训练，不在题库中复制完整知识正文：

1. **提示提问**：在具体小场景中逐级给出提示，辅助形成局部因果模型。
2. **脱稿输出**：撤掉知识提示，独立重建边界清晰的时序、状态或通信模块。
3. **专业案例**：分析工程证据，给出诊断、方案、不可规避成本和选择边界。

当前随工具提供的首个示例内容来自 `linux-note`，训练单元位于：

```text
banks/linux/synchronization/rcu/foundation_model/
```

它引用 `linux-note` 中 `knowledge/linux/synchronization/rcu/P01`～`P04` 的稳定文档 ID，覆盖 RCU 的问题来源、抽象机制、真实约束和 Tree RCU 通信总览。RCU 是当前内容包，不是工具自身的领域限制。

## 1.1\_独立化边界

工具目录按未来直接迁移为独立仓库的边界维护：

```text
practice_tool/
├── banks/                   # 可校验的模块化训练内容
├── docs/                    # 工具自己的运行与排障文档
├── schemas/                 # 内容数据协议
├── scripts/                 # 环境准备与内容检查
├── src/                     # 前端程序
├── start.cmd                # Windows 正式入口
├── start.sh                 # MSYS2/Linux 正式入口
├── package.json
└── README.md
```

以下内容属于当前 `linux-note` 的集成层，不属于工具核心：

- 知识库根目录的 `practice.cmd` 和 `practice.sh` 快捷入口。
- `knowledge` 中被题库引用的 Linux 权威正文。
- 本仓库的 `AGENTS.md`、治理规范、Atlas 和出版结构。

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

```powershell
$env:PRACTICE_SOURCE_CONFIG = "D:\notes\practice.sources.json"
.\start.cmd
```

```bash
PRACTICE_SOURCE_CONFIG=/srv/notes/practice.sources.json ./start.sh
```

配置地址也可以是 HTTP/HTTPS URL；远程配置只能继续声明 `http` 知识源，不能借远程配置指向启动机器上的文件系统。没有指定环境变量时，工具只尝试读取自己的 `config/knowledge_sources.local.json`；该文件属于本机配置并被 Git 忽略。两者都不存在时工具仍可启动，但界面会明确显示“未配置知识源”。

## 1.2\_本地运行

训练工具可以不依赖外层知识库的启动逻辑独立运行。在 Windows 下进入本目录后执行或双击：

```text
start.cmd
```

在 MSYS2/UCRT64 或 Linux 中进入本目录后执行：

```bash
./start.sh
```

工具自身的启动脚本会校验 Node.js 版本、安装首次运行所需的依赖并启动训练工具。浏览器由 Vite 在服务开始监听后打开，不使用固定延时猜测服务是否就绪。

当前知识库根目录仍提供 `practice.cmd` 和 `practice.sh` 作为快捷入口，但它们只负责转发到本目录的 `start.cmd` 和 `start.sh`。训练工具的环境准备和启动逻辑不依赖仓库根目录，便于后续整体拆分为独立仓库。

进入工具根目录后，使用正式入口查看工具定位、命令选项、环境变量和常用示例：

```bash
./start.sh --help
```

`--help` 在环境检查之前处理，不会下载 Node.js、安装 npm 依赖、启动 Vite 或打开浏览器。Windows 对应使用 `start.cmd --help`。

Bash Tab 补全可以安装到当前用户目录：

```bash
./start.sh --install-completion
```

安装器默认创建指向 **当前工具目录** 中 `scripts/completions/start.bash` 的符号链接；不支持符号链接时创建动态加载器，而不是复制静态快照。因此后续在当前目录执行 `git pull` 后，补全规则会直接跟随仓库更新。重新打开 Bash 后，输入 `./start.sh --` 再按 Tab 即可补全选项。只想在当前终端加载时执行：

```bash
source <(./start.sh --completion bash)
```

打开后先进入 **训练单元选择页**。可以按领域筛选或按题目、标签和模块名称搜索，然后选择单元进入三阶段训练。

### 1.2.1\_主动升级运行环境

普通启动会复用已经满足最低兼容线的 Node.js。需要主动查询官方更新、重建工具本地运行时并重新校验项目依赖时执行：

```cmd
start.cmd --upgrade
```

```bash
./start.sh --upgrade
```

`--upgrade` 会强制重新执行版本选择，仍按 `24 → 22 → 20 → 18` 从高到低尝试官方可用版本，清除旧的本机就绪标记并重新运行 `npm install`，随后正常启动平台。它不会修改题库、知识正文、知识源配置或用户训练记录。

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

第一次运行启动入口时会依次执行：

```text
定位 Node.js/npm 并检查 Node.js 主版本
    ↓
低于 v18 时按优先级准备官方兼容版本
    ↓
安装 practice_tool 项目依赖
    ↓
写入 .local/environment-ready-v3-node-compatible
    ↓
启动平台并打开浏览器
```

`.local/environment-ready-v3-node-compatible` 是本机环境就绪标记，不进入 Git。标记存在且 `node_modules` 仍然存在时，后续启动不会再次执行依赖安装。Node.js 版本在每次启动时都会重新检查，不兼容版本不能因为存在就绪标记而绕过门禁。

如果手动删除了 `node_modules`，启动器会清除旧标记并重新安装依赖。如果要主动重建环境，可以删除：

```text
.local/environment-ready-v3-node-compatible
node_modules/
```

然后重新运行本目录的 `start.cmd` 或 `start.sh`。

自动安装支持：

- Windows：复用 Node.js 18 以上版本；缺失或过旧时按有序下载源取得并校验兼容 ZIP，全部失败后才使用 `winget` 作为最终后备。
- MSYS2/UCRT64：复用兼容的 Windows/MSYS2 Node.js；缺失或过旧时通过 MSYS2 `pacman` 安装当前环境对应的软件包，不使用 `sudo`。
- 普通 Linux：依次查询各下载源的 `latest-v24.x`、`latest-v22.x`、`latest-v20.x` 和 `latest-v18.x`，每个版本先尝试国内镜像、再尝试官方源，选择当前架构存在且能够成功下载的最高版本，校验 SHA-256 后安装到 `.local/runtime`。该过程不替换系统 Node.js，也不依赖发行版仓库中的版本。

普通 Linux 下载需要 `curl` 或 `wget`、`sha256sum`、`tar` 和 `gzip`。某个源不可用时先切换下一源；某个版本找不到、下载失败或没有当前架构归档时再尝试下一个版本。只有所有 **仍满足最低兼容线** 的来源和版本都不可用时才停止；不能为了表面启动而退回到 Vite 无法运行的 Node.js 12。

> MSYS2 的标准安装目录通常由当前用户直接维护，安装软件包使用 `pacman -S`，不应在前面添加 `sudo`。脚本会通过 `MSYSTEM` 和 `MINGW_PACKAGE_PREFIX` 识别 UCRT64、MINGW64 或 CLANG64，并选择匹配的软件包。

设计边界见：[跨平台与仓库独立性设计](docs/cross_platform_and_repository_independence.md)。

跨平台启动、虚拟机克隆、代理排障和常见错误见：[环境与故障排查](docs/environment_and_troubleshooting.md)。

在 MSYS2/UCRT64 中，可以直接从训练工具目录执行：

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

训练记录保存在浏览器 `localStorage`，不会在每次作答后污染 Git 工作区。清理浏览器站点数据会删除本地记录。

## 1.3\_内容检查

```bash
npm run check:data
npm run build
```

内容检查验证单元文件、三阶段题目结构、稳定 ID、引用关系和重复题目 ID。
同时检查知识源配置 Schema 示例，确保 Windows、Linux 和外部仓库使用同一份配置协议。

## 1.4\_新增单元

每个单元使用四个文件：

```text
unit.json
guided_questions.json
model_tasks.json
professional_cases.json
```

所有可选择单元统一登记在：

```text
banks/index.json
```

新增题库目录并登记索引后，界面会自动发现和加载单元，不需要修改页面代码。

索引中的主要分类字段为：

| 字段 | 用途 |
| --- | --- |
| `domain` | 顶层知识领域，例如 `linux` |
| `topic` | 领域内主题，例如 `synchronization` |
| `module` | 可独立选择的机制模块，例如 `rcu` |
| `level` | 单元层次，例如 `foundation` |
| `tags` | 用户搜索时使用的中文或英文关键词 |
| `unit_file` | 单元入口文件相对 `banks/` 的路径 |

用户不需要知道题库目录结构。平台首页会把这些字段转换为领域筛选、关键词搜索和单元卡片。

- `unit.json` 只保存单元身份、权威正文引用和三个阶段的入口。
- `guided_questions.json` 保存轻量场景、递进提示和最小模型骨架。
- `model_tasks.json` 保存无提示输出任务、输出约束和核验问题。
- `professional_cases.json` 保存工程背景、证据、问题和专业评审维度。

题目通过稳定文档 ID 引用权威正文。正文路径只用于打开源文档，不作为题目永久身份。

## 1.5\_环境脚本

| 文件 | 职责 |
| --- | --- |
| `start.cmd` | Windows 正式启动入口 |
| `start.sh` | MSYS2/Linux 正式启动入口 |
| `scripts/install_environment.cmd` | Windows Node.js 自动安装 |
| `scripts/install_environment.sh` | MSYS2/Linux Node.js 自动安装 |
| `../../practice.cmd` | 当前知识库的 Windows 快捷入口，只转发到 `start.cmd` |
| `../../practice.sh` | 当前知识库的 MSYS2/Linux 快捷入口，只转发到 `start.sh` |

程序、题库协议、环境准备和启动逻辑全部留在工具目录。当前知识库根目录的两个文件不保存工具逻辑，移除后不影响从工具目录直接启动；拆分成独立仓库时不应复制这两个快捷入口。

## 1.6\_评分含义

- `需要重建`：关键因果链无法独立启动。
- `部分输出`：主方向正确，但缺少参与者、状态位置、顺序或边界。
- `完整输出`：不看提示即可覆盖任务的必要输出。

评分是下一轮训练的调度输入，不代表知识本身的正确答案。
