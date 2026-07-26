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
PRACTICE_SOURCE_CONFIG=/srv/notes/practice.sources.json bash ./start.sh
```

配置地址也可以是 HTTP/HTTPS URL；远程配置只能继续声明 `http` 知识源，不能借远程配置指向启动机器上的文件系统。没有指定环境变量时，工具只尝试读取自己的 `config/knowledge_sources.local.json`；该文件属于本机配置并被 Git 忽略。两者都不存在时工具仍可启动，但界面会明确显示“未配置知识源”。

## 1.2\_本地运行

训练工具可以不依赖外层知识库的启动逻辑独立运行。在 Windows 下进入本目录后执行或双击：

```text
start.cmd
```

在 MSYS2/UCRT64 或 Linux 中进入本目录后执行：

```bash
bash ./start.sh
```

工具自身的启动脚本会自动安装首次运行所需的依赖、启动训练工具并打开浏览器。

当前知识库根目录仍提供 `practice.cmd` 和 `practice.sh` 作为快捷入口，但它们只负责转发到本目录的 `start.cmd` 和 `start.sh`。训练工具的环境准备和启动逻辑不依赖仓库根目录，便于后续整体拆分为独立仓库。

打开后先进入 **训练单元选择页**。可以按领域筛选或按题目、标签和模块名称搜索，然后选择单元进入三阶段训练。

### 1.2.1\_第一次启动

第一次运行启动入口时会依次执行：

```text
定位 Node.js/npm
    ↓
缺失时调用系统包管理器安装 Node.js LTS
    ↓
安装 practice_tool 项目依赖
    ↓
写入 .local/environment-ready-v1
    ↓
启动平台并打开浏览器
```

`.local/environment-ready-v1` 是本机环境就绪标记，不进入 Git。标记存在且 `node_modules` 仍然存在时，后续启动不会再次执行环境安装和依赖检查。

如果手动删除了 `node_modules`，启动器会清除旧标记并重新安装依赖。如果要主动重建环境，可以删除：

```text
.local/environment-ready-v1
node_modules/
```

然后重新运行本目录的 `start.cmd` 或 `start.sh`。

自动安装支持：

- Windows：优先使用系统已有 Node.js；缺失时通过 `winget` 安装 Node.js LTS。
- MSYS2/UCRT64：可以发现现有 Windows Node.js；缺失时直接通过 MSYS2 `pacman` 安装当前环境对应的 Node.js 包，不使用 `sudo`，也不要求启用 Windows 开发者模式。
- Linux：依次支持 `apt-get`、`dnf` 和 `pacman`。

若设备没有受支持的包管理器，启动器会停止并提示手动安装 Node.js LTS，不会继续执行不完整的启动流程。

> MSYS2 的标准安装目录通常由当前用户直接维护，安装软件包使用 `pacman -S`，不应在前面添加 `sudo`。脚本会通过 `MSYSTEM` 和 `MINGW_PACKAGE_PREFIX` 识别 UCRT64、MINGW64 或 CLANG64，并选择匹配的软件包。

设计边界见：[跨平台与仓库独立性设计](docs/cross_platform_and_repository_independence.md)。

跨平台启动、虚拟机克隆、代理排障和常见错误见：[环境与故障排查](docs/environment_and_troubleshooting.md)。

在 MSYS2/UCRT64 中，可以直接从训练工具目录执行：

```bash
cd path/to/practice_tool
bash ./start.sh
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
