---
id: tools.practice_tool.architecture.implementation_status
title: "当前实现状态与版本边界"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_当前实现状态与版本边界

## 1.1\_结论

Electron 文件/文件夹 Markdown 工作台已于 2026-08-02 开始实现。ADR-0006～0010 均已接受：桌面层使用严格模式 TypeScript，工作区与文件能力由独立 C++20 Native Service 承担。

当前已经完成 **D0 桌面基础切片** 与 **D1A 文件能力安全切片**：生产构建从 `loop-app://` 加载 sandboxed Renderer，Electron Main 独占系统选择器，C++ 服务建立窗口作用域能力并验证单个 Markdown 或目录根，Renderer 只获得显示标签、相对路径和不透明 ID。当前可以只读展示单文件元数据和目录首层分页结果；尚未实现正文编辑、Markdown 预览、保存或文件操作，不能把 D1A 描述为可用编辑器。

仓库根 `src/`、`banks/`、IndexedDB 与 Bash/MSYS2 链仍是等待退役的 `0.1.0` 浏览器训练实现。它只用于旧内容闭包和构建回归，不进入新桌面依赖图。

## 1.2\_状态记录规则

| 状态 | 含义 | 允许写入条件 |
| --- | --- | --- |
| `NOT_STARTED` | 尚无目标架构代码 | 只有设计或旧实现不算开始 |
| `IN_PROGRESS` | 已有代码，但验收证据不完整 | 必须列出缺失门禁 |
| `BLOCKED` | 存在当前无法自行解除的外部阻塞 | 必须列出阻塞方和解除条件 |
| `COMPLETE` | 当前条目的明确验收已全部通过 | 必须给出命令、测试或人工证据 |

每次实现后更新“状态、证据、下一门禁”和文末验证记录。状态表记录可验收结果，不记录按文件统计的忙碌程度。

## 1.3\_D0桌面基础

| ID | 交付项 | 状态 | 当前证据 | 下一门禁 |
| --- | --- | --- | --- | --- |
| `D0-ADR` | 桌面、保存、预览、工作区和语言决策 | `COMPLETE` | ADR-0006～0010 为 `accepted` | 目标变化时新增取代 ADR |
| `D0-WORKSPACE` | `apps/desktop + packages/* + native` 工程边界 | `COMPLETE` | npm workspaces、`@loop/ipc-contracts`、CMake 工程均可独立构建 | 加入依赖方向静态门禁 |
| `D0-ELECTRON` | Electron Main/Preload/Renderer 安全壳 | `COMPLETE` | 显式关闭 Node integration，开启 context isolation、sandbox、webSecurity；权限、导航和新窗口默认拒绝 | 打包阶段补 Electron fuses 断言 |
| `D0-PROTOCOL` | `loop-app://` 打包资源协议 | `COMPLETE` | 独立非持久 session 注册协议；CSP、`nosniff`、路径边界和固定 MIME 已建立 | 增加协议恶意路径自动化用例 |
| `D0-NATIVE` | C++20 Native Service 与协议帧 | `COMPLETE` | 协议 v2、1 MiB 单行 UTF-8 JSON 控制帧、自有标识符与协议字段统一 `snake_case`、严格字段、固定方法、稳定错误和 CTest 均通过 | 增加背压和异常退出测试 |
| `D0-HANDSHAKE` | Electron 到 C++ 服务握手 | `COMPLETE` | 烟雾测试穿过 Renderer → Preload → Main → C++ → Main → Renderer 并以状态 `ready` 退出 | 加入协议版本不匹配 E2E |
| `D0-WINDOWS` | Windows 开发构建 | `COMPLETE` | CMake 4.3.3 + MinGW GCC 14.2、Electron 43.2.0 本机验证通过 | 固定正式 Windows 发布编译器 |
| `D0-LINUX` | Ubuntu 22.04 开发构建 | `NOT_STARTED` | 尚无当前环境验证 | 在 Ubuntu 22.04 运行 CMake、CTest、TypeScript build 与 Electron smoke |
| `D0-SUPPLY` | 新桌面依赖安全门禁 | `IN_PROGRESS` | `npm audit --workspace @loop/desktop` 为 0；Electron/esbuild 安装脚本按精确版本显式允许 | SBOM、许可证清单、C++ 依赖审计与签名打包尚未完成 |

## 1.4\_D1文件工作区纵向闭环

| ID | 交付项 | 状态 | 当前证据 | 下一门禁 |
| --- | --- | --- | --- | --- |
| `D1-CAPABILITY` | C++ 窗口作用域能力表 | `COMPLETE` | 每窗口独立随机 session；工作区、目录、条目和游标均为 CSPRNG ID；关闭、换窗、伪造和游标重放测试通过 | D1B 为文档正文和资源签发子能力补齐同等撤权测试 |
| `D1-OPEN-FILE` | 打开单个 Markdown | `COMPLETE` | 系统对话框只在 Main；普通文件、`.md/.markdown`、5 MiB、严格 UTF-8/BOM/NUL/换行和 SHA-256 基线测试通过 | D1B 增加受控正文读取，不复用控制帧传输大正文 |
| `D1-OPEN-FOLDER` | 打开单个文件夹 | `COMPLETE` | libuv 真实路径、文件身份和链接状态配合标准库枚举；UNC、盘符大小写、junction、rename、跨窗与分页测试通过；固定 50000 项/32 MiB 元数据上限 | Ubuntu 22.04 实机复跑 symlink、权限和 rename；`D0-LINUX` 仍未完成 |
| `D1-EXPLORER` | 文件树与标签 | `IN_PROGRESS` | Renderer 不接收绝对路径，已显示首层只读条目并支持继续分页；C++ 已可按 `directory_id` 枚举子目录 | 接通目录展开、折叠、刷新与分页状态；文件点击仍不得越过文档能力接口 |
| `D1-EDITOR` | CodeMirror 文档会话与 Dirty 状态 | `NOT_STARTED` | 无 | 输入只修改内存；撤销、修订和 Dirty 可测 |
| `D1-PREVIEW` | 普通 Markdown 实时预览 | `NOT_STARTED` | 无 | Worker 与隔离 Preview Frame 完成普通块闭环 |
| `D1-SAVE` | `Ctrl+S` 与安全保存 | `NOT_STARTED` | 无 | 期望身份/摘要、冲突和 safe write 故障测试通过 |

## 1.5\_后续里程碑

| 里程碑 | 范围 | 状态 |
| --- | --- | --- |
| `D2` | 合并恢复备份、Hot Exit、本地历史、外部冲突 | `NOT_STARTED` |
| `D3` | Unified Worker、safe HAST、源码定位、隔离 Preview Frame | `NOT_STARTED` |
| `D4` | Mermaid、KaTeX、代码高亮、Callout、Wiki 链接与本地资源 | `NOT_STARTED` |
| `D5` | 搜索、索引、文件操作、回收站和移动时链接更新 | `NOT_STARTED` |
| `D6` | Windows/Ubuntu 安装包、安全、性能、无障碍和故障注入 | `NOT_STARTED` |
| `D7` | 删除旧浏览器、训练、电子书、IndexedDB 和 Bash 最终用户链 | `NOT_STARTED` |

## 1.6\_已确认的目标删除项

D1 的打开文件夹、编辑、普通预览和手动保存闭环通过后，列出真实用户数据与发布风险，再删除而不是适配：

- 浏览器正式运行与本地 HTTP 文件服务。
- IndexedDB 业务主存储与正文副本。
- Bash/MSYS2 最终用户运行链和根快捷启动依赖。
- `banks/`、电子书、训练会话、复习、AI 内容生成与导入发布页面。
- `book.json`、`outline.md`、`chapters/`、训练计划和知识声明 Schema。
- 旧 Feature-first 根 `src/`；Renderer 内按 editor/explorer/preview 等 feature 重新建立结构。

除非开发者另外批准一次性导出工具，否则不实现双写、旧存储迁移层、旧 URL 转发、Node 文件备用服务或 `legacy` package。

## 1.7\_当前验证证据

2026-08-02 在 Windows 执行：

```text
cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --preset windows-mingw
  通过：C++20 严格告警构建，2/2 协议与工作区测试通过
  通过：UNC 与 junction 走 libuv 跨平台接口；关闭撤权、rename、伪造 ID、5 MiB 和严格 UTF-8 边界通过
  性能：10000 项目录首轮元数据枚举约 1164 ms，低于 2 秒目标

npm run desktop:typecheck
npm run desktop:test
npm run desktop:build
  通过：7 个 IPC contracts 测试、10 个 Main/分帧/sender/能力协调测试、类型检查与生产构建

$env:LOOP_DESKTOP_SMOKE_TEST='1'; npm run preview -w @loop/desktop
  通过：loop-app、sandboxed Renderer、Preload、Main 与 C++ 握手端到端可用

npm audit --workspace @loop/desktop
  通过：0 个已知漏洞

npm run check:data
npm run build
  通过：旧 0.1.0 内容闭包与生产构建
```

完整仓库 npm audit 仍报告旧浏览器 `react-router-dom 7.18.1` 链上的 2 个 high。该依赖不进入 `@loop/desktop`，但旧浏览器实现不得作为正式发布物；D7 删除旧代码后全仓审计必须归零。桌面 Renderer 当前 D1A JavaScript bundle 为 200.07 kB，gzip 后为 63.02 kB，CSS 为 3.72 kB、gzip 后 1.50 kB；尚未包含编辑器和预览。D1B 引入功能后必须重新记录分块、启动时间和内存数据，不能把只读工作台数值冒充最终性能基线。

## 1.8\_下一步

下一切片完成 `D1-EXPLORER` 的交互式按需目录树，并开始 `D1-EDITOR`：通过新的文档能力通道读取正文、建立 CodeMirror 内存会话、修订与 Dirty 状态。仍不把每次输入写入磁盘；`Ctrl+S`、safe write、备份和预览分别按后续门禁实现。

## 1.9\_相关设计

- [架构索引](README.md)
- [文件与文件夹工作区设计](product/file_and_folder_workspace.md)
- [Markdown 编辑与实时预览设计](product/markdown_editing_and_live_preview.md)
- [桌面运行时与文档服务设计](engineering/desktop_runtime_and_document_services.md)
- [桌面运行时安全与威胁模型](engineering/desktop_runtime_security_and_threat_model.md)
- [工程结构与模块边界](engineering/project_structure_and_module_boundaries.md)
- [架构决策记录](decisions/README.md)
