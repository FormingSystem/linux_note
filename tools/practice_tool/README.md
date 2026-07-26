---
id: tools.practice_tool
title: "回路知识训练工具"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_回路知识训练工具

`practice_tool` 将仓库权威知识组织成三阶段训练，不复制知识正文：

1. **提示提问**：在具体小场景中逐级给出提示，辅助形成局部因果模型。
2. **脱稿输出**：撤掉知识提示，独立重建边界清晰的时序、状态或通信模块。
3. **专业案例**：分析工程证据，给出诊断、方案、不可规避成本和选择边界。

首个训练单元位于：

```text
banks/linux/synchronization/rcu/foundation_model/
```

它引用 `knowledge/linux/synchronization/rcu/P01`～`P04`，覆盖 RCU 的问题来源、抽象机制、真实约束和 Tree RCU 通信总览。

## 1.1\_本地运行

最简单的使用方法是在仓库根目录双击：

```text
practice.cmd
```

脚本会自动安装首次运行所需的依赖、启动训练工具并打开浏览器。

打开后先进入 **训练单元选择页**。可以按领域筛选或按题目、标签和模块名称搜索，然后选择单元进入三阶段训练。

### 1.1.1\_第一次启动

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
tools/practice_tool/.local/environment-ready-v1
tools/practice_tool/node_modules/
```

然后重新运行根目录入口。

自动安装支持：

- Windows：优先使用系统已有 Node.js；缺失时通过 `winget` 安装 Node.js LTS。
- MSYS2/UCRT64：可以发现现有 Windows Node.js；缺失时直接通过 MSYS2 `pacman` 安装当前环境对应的 Node.js 包，不使用 `sudo`，也不要求启用 Windows 开发者模式。
- Linux：依次支持 `apt-get`、`dnf` 和 `pacman`。

若设备没有受支持的包管理器，启动器会停止并提示手动安装 Node.js LTS，不会继续执行不完整的启动流程。

> MSYS2 的标准安装目录通常由当前用户直接维护，安装软件包使用 `pacman -S`，不应在前面添加 `sudo`。脚本会通过 `MSYSTEM` 和 `MINGW_PACKAGE_PREFIX` 识别 UCRT64、MINGW64 或 CLANG64，并选择匹配的软件包。

跨平台完整说明、虚拟机克隆、代理排障和常见错误见：[环境与故障排查](docs/environment_and_troubleshooting.md)。

在 MSYS2/UCRT64 中，可以从仓库根目录执行：

```bash
bash ./practice.sh
```

也可以手动运行：

```bash
cd tools/practice_tool
npm install
npm run dev
```

训练记录保存在浏览器 `localStorage`，不会在每次作答后污染 Git 工作区。清理浏览器站点数据会删除本地记录。

## 1.2\_内容检查

```bash
npm run check:data
npm run build
```

内容检查验证单元文件、三阶段题目结构、稳定 ID、引用关系和重复题目 ID。

## 1.3\_新增单元

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

## 1.4\_环境脚本

| 文件 | 职责 |
| --- | --- |
| `../../practice.cmd` | Windows 双击入口 |
| `../../practice.sh` | MSYS2/Linux 仓库根目录入口 |
| `scripts/install_environment.cmd` | Windows Node.js 自动安装 |
| `scripts/install_environment.sh` | MSYS2/Linux Node.js 自动安装 |

实际程序和题库全部留在 `tools/practice_tool`。仓库根目录的两个文件只负责定位工具并启动，不保存业务逻辑或题目。

## 1.5\_评分含义

- `需要重建`：关键因果链无法独立启动。
- `部分输出`：主方向正确，但缺少参与者、状态位置、顺序或边界。
- `完整输出`：不看提示即可覆盖任务的必要输出。

评分是下一轮训练的调度输入，不代表知识本身的正确答案。
