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
- 在同一 CodeMirror 编辑面中原位编辑并及时渲染；`Ctrl+/` 往返完整源码模式。
- 实时预览 CommonMark/GFM、Mermaid、公式、Callout、Wiki 链接等内置能力。
- 默认手动保存，提供合并恢复备份、Hot Exit、本地历史和外部冲突处理。

文件和目录始终位于用户原来的磁盘位置。打开不是导入：应用不复制正文、不生成专题电子书、不创建训练内容包，也不要求目录包含回路专属配置。

## 1.2\_架构方向

- Electron Main 独占窗口、系统对话框、协议、更新和 Native Service 生命周期；C++20 Native Service 独占工作区能力、真实路径、文件、备份、监听、索引和搜索。
- Preload 只暴露固定类型用例，Renderer 保持 sandbox、context isolation 和无 Node。
- CodeMirror 负责编辑事务，Unified Worker 负责 safe HAST 与诊断，复杂 renderer 独立调度。
- 每次编辑只更新内存和预览；源文件默认只在 `Ctrl+S` 时写入，自动保存默认关闭。
- 恢复备份按空闲与最大间隔合并写入，不逐键落盘，也不伪装成源文件已保存。
- 第一阶段使用状态文件、恢复备份、本地历史和可删索引，不预先引入 SQLite。

## 1.3\_当前实现边界

仓库根 `src/` 中的 `0.1.0` 仍是 Vite 浏览器训练原型，包含 Bash/MSYS2 生命周期、IndexedDB、`banks/` 和专题电子书数据。新的 `apps/desktop + packages/ipc-contracts + packages/document-core + packages/markdown-engine + native` 已接通 Windows 纵向闭环：Electron 安全壳、C++20 Native Service、根句柄相对文件能力、交互式按需目录树、协议 v4 双向有界正文附件、CodeMirror 多文档会话、冲突检查安全保存，以及 Typora 式 CommonMark/GFM + Mermaid 混合编辑面均可运行。系统路径只在 Main 到 Native Service 的建权请求中出现，Renderer 只收到显示路径、不透明 ID 和经验证的正文。Linux `openat2` 与 `renameat/fsync` 实现已经落地，但当前机器没有受支持的 Ubuntu/WSL 环境，跨平台状态仍保持 `IN_PROGRESS`；预览的 1 MiB 性能门禁也尚未达标。

D1C 的预览由 Worker 生成有界顶层块、源码跨度和 safe HAST；当前块在原位显示源码，其他普通块由固定 DOM 映射及时渲染，Mermaid 则通过专用 MessageChannel 进入 `loop-preview://` 的无 Preload sandbox Frame。工作台内置 VS Code Dark+ 与 Light+ 两套主题，首次随系统明暗偏好选择，可由顶栏按钮切换当前会话；Mermaid 工具栏支持 50%～300% 放大、缩小、复位和适合宽度，溢出只在图表内部滚动。`Ctrl+/` 只切换同一 EditorState 的装饰，不创建另一份正文；原始 HTML 不执行，图片保持阻止占位，链接保持不可导航。`Ctrl+S` 已按 ADR-0014 执行 token、身份和摘要冲突检查后安全替换，BOM 与统一换行策略受控，混合换行必须显式选择；外部修改绝不静默覆盖。文件夹标签可关闭并撤销 Native 文档能力，Dirty 标签只允许“保存并关闭 / 放弃修改 / 取消”。恢复备份与 Hot Exit 仍未实现，硬崩溃仍可能丢失尚未保存的草稿，因此当前实现还不是具备数据恢复保证的正式编辑器。

这些旧能力只在重构期间保持可构建；桌面纵向闭环通过后会整体删除，不建立浏览器/桌面双运行、IndexedDB/文件双写、电子书适配或 `legacy` 包。准确差距见 [当前实现状态与版本边界](docs/architecture/implementation_status.md)。

## 1.4\_Windows一键启动桌面工作台

在资源管理器中进入本目录，双击：

```text
start_desktop.cmd
```

入口会检查 Node.js `22.12.0` 以上版本和 npm；依赖缺失或锁文件更新时执行 `npm install`；Native Service 缺失或源码较新时使用 `windows-mingw` preset 重新构建；全部就绪后通过 `loop-app://` 生产资源协议启动未打包 Electron 工作台。Windows Native Service 静态链接 MinGW C++ 运行时，双击入口不依赖开发终端临时注入的 MinGW `PATH`。它不占用 Vite 开发端口，因此不会因 `5173` 被其他进程使用而漂移到未经信任的 origin。失败时窗口会保留并显示缺少的工具，不会转入旧浏览器或 Bash 实现。

只检查环境与构建状态、不启动窗口时执行：

```powershell
.\start_desktop.cmd -check_only
```

该文件是未打包阶段的 Windows 开发便利入口，不是最终安装包启动器。当前已经支持手动安全保存，但仍无恢复备份或 Hot Exit；保存前的内存草稿在崩溃后不能恢复。

打开 Markdown 后可直接在混合编辑面中修改；`Ctrl+/` 切换完整源码，`Ctrl+S` 保存。顶栏“浅色主题 / 深色主题”按钮切换当前会话主题。Mermaid 图表上方提供缩小、百分比复位、放大、适合宽度和编辑源码；焦点位于图表内时也可使用 `Ctrl+-`、`Ctrl++`、`Ctrl+0`。

## 1.5\_当前0.1.0运行

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

## 1.6\_当前验证

```bash
npm run desktop:typecheck
npm run desktop:test
npm run desktop:build

cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --preset windows-mingw

npm run check:data
npm run build
git diff --check
```

前三条 CMake 命令在 `native/` 执行；Linux 使用 `linux-gcc` preset。CMake 首次配置会按固定摘要获取 `nlohmann/json`、Mbed TLS 和 libuv；业务层保持跨平台，只有根句柄相对授权与安全替换集中在 `filesystem_capability_port` 的 Windows/Linux 平台分支。`check:data` 只验证等待退役的训练内容闭包。桌面已有 typecheck、C++ unit、能力协调、Native 进程传输和 Electron smoke，后续仍需补齐 Linux 实机、Sanitizer、更多写入故障注入及正式打包门禁。

## 1.7\_设计文档

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

## 1.8\_许可与项目边界

当前公开开发版本使用仓库根目录 `GPL-2.0-only`，原创维护者为 FormingSystem，联系邮箱为 `lizhaojun97@qq.com`。二次开发应保留产品名称、原创来源、项目地址与修改说明；第三方依赖、用户 Markdown 和外部资料不因被回路打开而改变许可。

目标工程按可迁移到独立仓库的边界维护。`linux-note` 的知识目录、根快捷脚本、治理文件和 `practice.sources.json` 不进入桌面运行时。
