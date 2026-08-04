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

Electron 文件/文件夹 Markdown 工作台已于 2026-08-02 开始实现。ADR-0006～0015 均已接受：桌面层使用严格模式 TypeScript，工作区与文件能力由独立 C++20 Native Service 承担。

当前已经完成 **D0 桌面基础切片** 与 **D1A Windows 文件能力切片**。**D1B 安全编辑切片** 已按 ADR-0013 在 Windows 完成根句柄相对能力端口，目录枚举和正文读取不再按绝对路径重开；Linux `openat2` 实现已落地但尚无受支持环境验证，因此跨平台总体验收保持 `IN_PROGRESS`。**D1C 混合实时预览切片** 已按 ADR-0015 接通 Typora 式单一编辑面：当前块原位编辑、其他块及时渲染，`Ctrl+/` 往返完整源码；工作台、CodeMirror、普通 Markdown 与不同源 Mermaid Frame 共用 VS Code Dark+/Light+ 主题枚举，图表支持 50%～300% 局部缩放。功能和安全闭环已通过真实 Electron smoke，但预览性能门禁仍为 `IN_PROGRESS`。**D1-SAVE** 已按 ADR-0014 接通 Windows 安全替换、冲突保护和文档级关闭，协议已干净升级为 v4。Renderer 只获得显示路径、不透明 ID 和经 Main 验证的正文。

当前版本 **仍不是具备数据恢复保证的完整编辑器**。`Ctrl+S` 已保存发起时的不可变 CodeMirror 修订，外部变化会返回冲突且不覆盖；保存期间继续输入则新修改保持 Dirty。文件夹脏标签提供“保存并关闭 / 放弃修改 / 取消”，窗口关闭 Dirty 草稿仍要求显式放弃。恢复备份和 Hot Exit 尚未实现，硬崩溃仍会丢失未保存内存草稿。当前预览已覆盖普通 CommonMark/GFM 与 Mermaid；图片保持阻止占位，链接不可导航，KaTeX、代码高亮、Callout、Wiki 链接、本地资源和细粒度增量解析尚未开放。

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
| `D0-ADR` | 桌面、保存、预览、工作区、语言、正文分帧与安全路径能力决策 | `COMPLETE` | ADR-0006～0015 为 `accepted`；ADR-0013 收敛根句柄能力，ADR-0014 收敛安全替换，ADR-0015 取代三模式 UI 与 whole-document Frame 协议 | 目标变化时新增取代 ADR |
| `D0-WORKSPACE` | `apps/desktop + packages/* + native` 工程边界 | `COMPLETE` | npm workspaces、`@loop/ipc-contracts`、CMake 工程均可独立构建 | 加入依赖方向静态门禁 |
| `D0-ELECTRON` | Electron Main/Preload/Renderer 安全壳 | `COMPLETE` | 显式关闭 Node integration，开启 context isolation、sandbox、webSecurity；权限、导航和新窗口默认拒绝 | 打包阶段补 Electron fuses 断言 |
| `D0-PROTOCOL` | `loop-app://` 与 `loop-preview://` 打包资源协议 | `COMPLETE` | 独立非持久 session 注册协议；工作台资源受 CSP、`nosniff` 和路径边界约束；Preview 协议只允许 3 个固定资源并已有恶意路径测试 | 打包阶段复核签名产物中的协议资源清单 |
| `D0-NATIVE` | C++20 Native Service 与协议帧 | `COMPLETE` | 协议 v4 使用 16 字节复合帧、1 MiB 控制区、5 MiB 正文附件和 8 MiB 双端写队列；打开响应与保存请求正文方向严格区分，不解析 v3 | 增加更完整进程级故障注入与 Linux pipe 实机复跑 |
| `D0-HANDSHAKE` | Electron 到 C++ 服务握手 | `COMPLETE` | 烟雾测试穿过 Renderer → Preload → Main → C++ → Main → Renderer 并以状态 `ready` 退出 | 加入协议版本不匹配 E2E |
| `D0-WINDOWS` | Windows 开发构建 | `COMPLETE` | CMake 4.3.3 + MinGW GCC 14.2、Electron 43.2.0 本机验证通过；Native Service 静态链接 MinGW 运行时，无开发工具链 `PATH` 仍能启动 | 固定正式 Windows 发布编译器 |
| `D0-LINUX` | Ubuntu 22.04 开发构建 | `NOT_STARTED` | 尚无当前环境验证 | 在 Ubuntu 22.04 运行 CMake、CTest、TypeScript build 与 Electron smoke |
| `D0-SUPPLY` | 新桌面依赖安全门禁 | `IN_PROGRESS` | `npm audit --workspace @loop/desktop --workspace @loop/markdown-engine` 为 0；Electron/esbuild 安装脚本按精确版本显式允许 | SBOM、许可证清单、C++ 依赖审计与签名打包尚未完成 |

## 1.4\_D1文件工作区纵向闭环

| ID | 交付项 | 状态 | 当前证据 | 下一门禁 |
| --- | --- | --- | --- | --- |
| `D1-CAPABILITY` | C++ 窗口作用域能力表 | `IN_PROGRESS` | 工作区、目录、条目、游标和文档均为窗口绑定 CSPRNG ID；根句柄与组件链由平台端口授权；刷新撤销旧条目但保留已开文档，`documents.close` 与工作区关闭分别撤权；Windows 竞态矩阵通过 | Ubuntu 22.04 验证 `openat2`、bind mount、rename、权限和资源释放 |
| `D1-OPEN-FILE` | 打开单个 Markdown | `COMPLETE` | 系统对话框只在 Main；`FileService` 在同一 libuv 句柄上执行 open/fstat/read/fstat/hash，并再次验证路径身份；正文附件不进入控制 JSON | Ubuntu 22.04 实机复跑读取竞态与权限矩阵 |
| `D1-OPEN-FOLDER` | 打开单个文件夹 | `IN_PROGRESS` | Windows 根句柄相对 `NtCreateFile`、句柄目录枚举与 128 位身份已实现；UNC、盘符大小写、junction 替换、根 rename、后代移出、同名替换、跨窗与分页测试通过；固定 50000 项/32 MiB 元数据上限 | Ubuntu 22.04 实机验证同设备 bind mount 与 `openat2` 失败关闭 |
| `D1-EXPLORER` | 文件树与标签 | `COMPLETE` | 目录逐级按需加载；每目录独立保存展开、错误、分页、刷新代次和折叠缓存；过期响应测试、ARIA tree 与方向键/Home/End/Enter/Space 已实现 | 大目录虚拟化和人工读屏验收进入 D6 |
| `D1-EDITOR` | CodeMirror 文档会话与 Dirty/保存状态 | `COMPLETE` | CodeMirror 独占草稿、选择、滚动、撤销历史与保存 Text 快照；`@loop/document-core` 管理单调修订、在途保存和 Dirty 转换；完整撤销或回到最新保存基线恢复 Clean，保存旧修订期间的新输入保持 Dirty | D2 接入备份修订但不得复制逐键全文到 React |
| `D1-PREVIEW` | Typora 式混合 Markdown 实时预览 | `IN_PROGRESS` | `@loop/markdown-engine` v3 在 Worker 内产生有界顶层块、源码跨度和 safe HAST，且不解析 v2；当前块源码、其余块原位渲染，非活动标签销毁派生 Frame 但保留 CodeMirror 会话，`Ctrl+/` 在同一 EditorState 上切换完整源码；VS Code Dark+/Light+ 切换以严格枚举重建派生装饰，不改变正文或 Dirty；Mermaid 11.16.0 经不同源 sandbox、nonce MessageChannel、strict 模式、文档配置拒绝、secure 键、SVG/CSS allowlist 和禁网 CSP 渲染，并在 Frame 内提供 50%～300%、25% 步长缩放与局部滚动；恶意 Markdown 与真实 smoke 均通过 | 1 MiB P95 仍高于 300 ms；D3 继续实现细粒度增量解析、缓存和复杂块调度，达到性能门禁后才能 `COMPLETE` |
| `D1-SAVE` | `Ctrl+S`、安全保存与文档关闭 | `IN_PROGRESS` | Windows 以根/父句柄创建临时文件、刷新、复制受支持元数据并用 `FileRenameInformationEx` 替换；token/身份/摘要冲突、BOM、LF/CRLF/mixed、只读、多硬链接、命名流、5 MiB、保存后 token 轮换、Dirty 关闭选择和 Electron 外部冲突 smoke 通过 | Ubuntu `renameat/fsync` 实机、磁盘满/占用/rename 后结果不确定故障注入和 ACL/属性矩阵 |

## 1.5\_后续里程碑

| 里程碑 | 范围 | 状态 |
| --- | --- | --- |
| `D2` | 合并恢复备份、Hot Exit、本地历史、外部冲突 | `NOT_STARTED` |
| `D3` | 增量 safe HAST、源码定位、复杂块调度与预览复用 | `IN_PROGRESS`：块协议、顶层源码跨度、变化相交块回退源码已完成；细粒度增量解析、缓存与调度未完成 |
| `D4` | Mermaid、KaTeX、代码高亮、Callout、Wiki 链接与本地资源 | `IN_PROGRESS`：Mermaid 安全渲染、双主题和局部缩放已完成；独立全屏/拖拽及其余 renderer 与本地资源未开始 |
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

2026-08-03 在 Windows 执行：

```text
cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --preset windows-mingw
  通过：C++20 严格告警构建，2/2 协议与工作区测试程序通过
  通过：复合帧任意分块、空附件、精确 1 MiB/5 MiB、错误头、截断、请求正文摘要/方向和协议 v3 拒绝
  通过：正文 BOM/BOM-only、LF/CRLF/mixed、空文件、无效 UTF-8、NUL、精确 5 MiB、读取期间 rename/replace/resize、64 文档能力上限、跨窗、伪造、刷新、文档关闭与工作区关闭撤权
  通过：Windows 根句柄端口拒绝非法组件、UNC、junction 交换、后代移出根与同名身份替换；根目录 rename 后能力仍锚定原对象
  通过：安全保存覆盖 token/摘要冲突、外部原地修改、提交前目标身份替换、BOM/换行、mixed 显式决策、多硬链接、命名流、规范 LF 输入、保存后身份与 token 迁移
  性能：10000 项目录首轮元数据枚举低于 2 秒目标

npm run desktop:typecheck
npm run desktop:test
npm run desktop:build
  通过：9 个 IPC contracts、5 个 document-core、7 个 markdown-engine、35 个桌面分帧/混合预览/Mermaid/状态模型/Main/sender/能力协调测试
  构建：Workbench Renderer JavaScript 845.97 kB（gzip 284.45 kB），Markdown Worker 161.94 kB，CSS 12.41 kB（gzip 3.08 kB）；隔离 Mermaid runtime 3464663 字节，只由可见 Mermaid Frame 加载

.\start_desktop.cmd -check_only
LOOP_DESKTOP_SMOKE_TEST=1 下执行 .\start_desktop.cmd
  通过：Windows 一键入口识别 Node.js/npm、复用已就绪依赖与 Native Service，并经 production preview 启动完整 Electron smoke；入口不依赖 Vite 开发端口
  通过：从 PATH 移除 CMake、GCC、MinGW Make 与 MinGW DLL 目录后，一键入口仍完成“打开文件夹 → 首层枚举 → 点击 smoke.md → 正文进入 CodeMirror”；页面不暴露绝对路径

npm run smoke:native-transport -w @loop/desktop
  通过：独立 Native 进程读取并安全保存精确 5242880 字节正文后正常退出；Windows 仅保留系统 PATH 时 Native 仍以退出码 0 正常启动；15 字节截断帧令服务以退出码 1 失败关闭

$env:LOOP_DESKTOP_SMOKE_TEST='1'
$smoke_root=New-Item -ItemType Directory ".local/d1s-smoke-$(New-Guid)"
Copy-Item 'apps/desktop/tests/fixtures/smoke.md' "$smoke_root/smoke.md"
$env:LOOP_DESKTOP_SMOKE_FIXTURE=(Resolve-Path "$smoke_root/smoke.md").Path
$env:LOOP_DESKTOP_SMOKE_FOLDER_FIXTURE=$smoke_root.FullName
npm run preview -w @loop/desktop
  通过：临时文件夹 fixture 穿过 Renderer → Preload → Main → Native → Main → Renderer，目录树点击后正文进入同一 CodeMirror 混合编辑面；普通 Markdown 原位渲染，Mermaid flowchart、sequenceDiagram 与 stateDiagram-v2 均在按视口实例化时生成 SVG，顶栏在系统初始 Dark+/Light+ 与另一主题间切换后 CodeMirror 和三个图形同步使用新令牌。真实可见 Frame 同时通过按钮与键盘验证 100%→125%、适合宽度、300% 最大值、50% 最小值与复位，两端按钮正确禁用且溢出只进入图表视口；smoke 按编辑器视口滚动收集三类图，避免把已被 CodeMirror 虚拟化销毁的旧 WebFrame 当成活动实例。`Ctrl+/` 往返完整源码，旧三模式按钮不存在。第一次 `Ctrl+S` 真正落盘并恢复 Clean；随后测试进程追加外部内容，第二次保存返回冲突、保留 Dirty、磁盘保留外部内容且不写入第二次编辑。Mermaid Frame 只有 `allow-scripts`、无 Preload、父页面不可读，原始脚本未执行，远程图片请求数为 0，页面未出现绝对路径

npm run benchmark:editor -w @loop/desktop
  7 次无 DOM CodeMirror State 样本：1 MiB 初始化 P95 36.156 ms、输入 P95 43.073 ms；5 MiB 初始化 P95 153.486 ms、输入 P95 59.911 ms

npm run benchmark:preview -w @loop/desktop
  7 次 Node/Unified 全量解析、v3 顶层块输出基线：约 20000 行的 1 MiB P95 359.521 ms；高节点密度 1 MiB P95 468.284 ms；5 MiB 普通长正文 P95 1300.758 ms

npm audit --workspace @loop/desktop --workspace @loop/markdown-engine
  通过：0 个已知漏洞

npm run check:data
npm run build
  通过：旧 0.1.0 内容闭包与生产构建
```

当前机器没有受支持的 MSYS2 Bash 或 WSL/Ubuntu 22.04 环境，因此未运行仓库根 `./format.sh check all --summary`，也未用 Git Bash 冒充正式环境。`git diff --check` 已通过；Linux CMake、CTest、TypeScript build 与 Electron smoke 仍归入 `D0-LINUX` 未完成门禁。

完整仓库 npm audit 仍报告旧浏览器 `react-router-dom 7.18.1` 链上的 2 个 high。该依赖不进入 `@loop/desktop`，但旧浏览器实现不得作为正式发布物；D7 删除旧代码后全仓审计必须归零。上述编辑器性能是无 DOM `EditorState` 基线，预览性能是 Node 内的同步 Unified 基线；二者都不等同于安装包中的 Chromium Worker、`EditorView`、结构化复制、Frame DOM、内存或输入延迟发布数据。1 MiB 约 20000 行基线仍高于 300 ms 目标，D3 必须用块级增量缩小重算范围，D6 仍须在正式硬件上补齐完整性能与无障碍验收，不能用当前数字宣称发布性能达标。

## 1.8\_下一步

下一切片先在受支持 Ubuntu 22.04 环境验证已经落地的 `openat2`、句柄目录枚举与 `renameat/fsync`，并为双平台保存补磁盘满、文件占用、最终 rename/flush 结果不确定和元数据保持故障注入；这些门禁通过前 D1-SAVE 与跨平台能力继续保持 `IN_PROGRESS`。随后进入 D2 的合并恢复备份、Hot Exit 与本地历史，使崩溃和窗口关闭不再依赖用户放弃内存草稿；D3 在现有 v3 块协议和源码跨度之上增加细粒度增量解析、缓存与复杂块调度，D4 继续实现 KaTeX、代码高亮、Callout、Wiki 链接和本地资源。

## 1.9\_相关设计

- [架构索引](README.md)
- [文件与文件夹工作区设计](product/file_and_folder_workspace.md)
- [Markdown 编辑与实时预览设计](product/markdown_editing_and_live_preview.md)
- [桌面运行时与文档服务设计](engineering/desktop_runtime_and_document_services.md)
- [桌面运行时安全与威胁模型](engineering/desktop_runtime_security_and_threat_model.md)
- [工程结构与模块边界](engineering/project_structure_and_module_boundaries.md)
- [架构决策记录](decisions/README.md)
