---
id: tools.practice_tool
title: "回路 Loop Markdown 工作台"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_回路\_Loop\_Markdown\_工作台

## 1.1\_目标产品

**回路（Loop）** 的新目标是独立 Electron Markdown 桌面工作台：

- 新建 Markdown。
- 打开单个 Markdown。
- 打开单个文件夹并按需浏览其中的文件。
- 使用 CodeMirror 编辑原始 Markdown。
- 实时预览 CommonMark/GFM、Mermaid、公式、Callout、Wiki 链接等内置能力。
- 默认手动保存，提供合并恢复备份、Hot Exit、本地历史和外部冲突处理。

文件和目录始终位于用户原来的磁盘位置。打开不是导入：应用不复制正文、不生成专题电子书、不创建训练内容包，也不要求目录包含回路专属配置。

## 1.2\_架构方向

- Electron Main 独占文件系统、系统对话框、回收站、应用数据和更新权限。
- Preload 只暴露固定类型用例，Renderer 保持 sandbox、context isolation 和无 Node。
- CodeMirror 负责编辑事务，Unified Worker 负责 safe HAST 与诊断，复杂 renderer 独立调度。
- 每次编辑只更新内存和预览；源文件默认只在 `Ctrl+S` 时写入，自动保存默认关闭。
- 恢复备份按空闲与最大间隔合并写入，不逐键落盘，也不伪装成源文件已保存。
- 第一阶段使用状态文件、恢复备份、本地历史和可删索引，不预先引入 SQLite。

## 1.3\_当前实现边界

仓库中的 `0.1.0` 仍是 Vite 浏览器训练原型，包含 Bash/MSYS2 生命周期、IndexedDB、`banks/` 和专题电子书数据。它尚未实现 Electron、文件/文件夹能力、CodeMirror 保存闭环或桌面安装包。

这些旧能力只在重构期间保持可构建；桌面纵向闭环通过后会整体删除，不建立浏览器/桌面双运行、IndexedDB/文件双写、电子书适配或 `legacy` 包。准确差距见 [当前实现状态与版本边界](docs/architecture/implementation_status.md)。

## 1.4\_当前0.1.0运行

以下命令只用于退役前验证现有浏览器代码，不是新桌面客户端的交付方式。

在 MSYS2 UCRT64/UCRT32 Bash 或 Ubuntu 22.04 Bash 中：

```bash
./start.sh
```

查看帮助不会安装依赖或启动服务：

```bash
./start.sh --help
```

安装、纯运行和卸载也可以分别调用 `install.sh`、`run.sh` 和 `uninstall.sh`。环境细节见 [当前跨平台环境与排障](docs/environment_and_troubleshooting.md)。

## 1.5\_当前验证

```bash
npm run check:data
npm run build
git diff --check
```

`check:data` 只验证等待退役的训练内容闭包。新桌面工程建立后必须增加独立 `lint`、`typecheck`、`unit`、`integration`、Electron `e2e`、安全和故障注入门禁。

## 1.6\_设计文档

- [架构索引](docs/architecture/README.md)
- [架构决策记录](docs/architecture/decisions/README.md)
- [文件与文件夹工作区设计](docs/architecture/product/file_and_folder_workspace.md)
- [Markdown 编辑与实时预览设计](docs/architecture/product/markdown_editing_and_live_preview.md)
- [产品导航与交互设计](docs/architecture/product/navigation_and_interaction.md)
- [桌面运行时与文档服务设计](docs/architecture/engineering/desktop_runtime_and_document_services.md)
- [桌面运行时安全与威胁模型](docs/architecture/engineering/desktop_runtime_security_and_threat_model.md)
- [工程结构与模块边界](docs/architecture/engineering/project_structure_and_module_boundaries.md)
- [工作区文件操作与数据安全](docs/architecture/engineering/workspace_file_operations_and_data_safety.md)
- [无障碍、性能与产品验收标准](docs/architecture/engineering/accessibility_performance_and_acceptance.md)
- [跨平台与仓库独立性设计](docs/cross_platform_and_repository_independence.md)
- [当前实现状态与版本边界](docs/architecture/implementation_status.md)

## 1.7\_许可与项目边界

当前公开开发版本使用仓库根目录 `GPL-2.0-only`，原创维护者为 FormingSystem，联系邮箱为 `lizhaojun97@qq.com`。二次开发应保留产品名称、原创来源、项目地址与修改说明；第三方依赖、用户 Markdown 和外部资料不因被回路打开而改变许可。

目标工程按可迁移到独立仓库的边界维护。`linux-note` 的知识目录、根快捷脚本、治理文件和 `practice.sources.json` 不进入桌面运行时。
