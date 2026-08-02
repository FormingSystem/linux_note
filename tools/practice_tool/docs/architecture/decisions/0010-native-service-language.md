---
id: tools.practice_tool.architecture.decision.0010
title: "ADR-0010：选择 Native Service 实现语言"
kind: reference
status: maintained
domains:
  - tools
---

# 第1章\_ADR-0010\_选择\_Native\_Service\_实现语言

## 1.1\_状态

`accepted`

本决策于 2026-08-02 接受：Native Service 使用 C++20。Electron Main、Preload、React/CodeMirror、Markdown Worker 和隔离预览继续使用严格模式 TypeScript。

## 1.2\_共同边界

无论选择 Rust 还是 C++，Native Service 都作为随安装包发布的独立子进程，负责工作区能力表、真实路径、文件读写、安全保存、备份、监听、索引、搜索和哈希。它不负责 React、DOM、CodeMirror、Unified、Mermaid 或 KaTeX。

Main 与 Native Service 使用版本化、限长、可取消的本地帧协议。Main 只能启动安装包内固定定位的二进制；Renderer 不能直接连接 Native Service。禁止同时维护 Rust 和 C++ 两个服务，也禁止在 Node 中保留备用文件实现。

## 1.3\_比较

| 维度 | Rust | C++ |
| --- | --- | --- |
| 本任务的运行性能 | 与 C++ 同级，足以覆盖枚举、搜索、哈希和保存 | 与 Rust 同级，成熟编译器优化充分 |
| 内存与并发安全 | 所有权和类型系统默认阻止大部分悬垂、越界与数据竞争 | 依赖 RAII、代码审查、静态分析和 Sanitizer 才能持续约束 |
| 跨平台构建 | Cargo 与单一包生态较统一 | 需要明确 MSVC/Clang/GCC、CMake 和依赖管理组合 |
| 既有库复用 | 文件、监听、并发和搜索 crate 丰富 | 系统库、传统 C/C++ 库与既有工程资产更丰富 |
| Electron 隔离 | 独立进程，不受 Node ABI 约束 | 独立进程，不受 Node ABI 约束 |
| 崩溃风险 | panic 可以隔离，但 `unsafe` 和依赖仍需审计 | 未定义行为、内存破坏和异常边界需要额外治理 |
| 团队门槛 | 需要掌握所有权、生命周期和 async 生态 | 需要持续掌握对象生命周期、并发、ABI 和工具链差异 |

两者对 Renderer 输入延迟、DOM 更新和 Mermaid 渲染没有直接优势；这些性能仍由 TypeScript Worker、任务取消、块级复用和虚拟化解决。

## 1.4\_决策

Native Service 采用 C++20、CMake 和独立进程边界。依赖必须锁定版本与完整性；项目代码启用严格告警并将警告视为错误，Windows/Linux CI 逐步建立 clang-tidy、ASan/UBSan 和故障注入门禁。所有权使用 RAII 和值语义，禁止用裸 `new/delete` 表达对象所有权，禁止异常跨越进程协议边界。

C++ 服务只负责工作区能力、文件语义、备份、监听、索引与搜索，不实现 Markdown DOM、Mermaid 或 Renderer 状态。D1A 将协议提升为版本 `2`：使用单行 UTF-8 JSON，每个控制帧以换行结束且上限为 1 MiB；JSON 字符串中的换行必须转义。这样标准 C++ iostream 在 Windows 与 Linux 使用同一实现，不需要为标准输入输出切换平台私有二进制模式。协议拒绝未知字段、未知方法、空帧、超长帧、无效 UTF-8 和版本不匹配。

不为将来可能改用 Rust 预留双后端接口、备用实现或运行时选择。若以后确有数据证明需要替换语言，新增取代 ADR，保持协议语义后整体替换并删除 C++ 服务。

## 1.5\_实施基线

- Windows 开发基线：CMake 3.22 以上与支持 C++20 的 MSVC、Clang 或 MinGW GCC；发布编译器在打包 ADR 中固定。
- Ubuntu 22.04 开发基线：CMake 3.22 以上与 GCC/Clang C++20。
- `nlohmann/json 3.12.0` 负责 JSON 值与解析；Mbed TLS `3.6.4` 负责 SHA-256 和 CSPRNG；libuv `1.51.0` 负责跨平台真实路径、文件身份和链接状态。三个依赖都通过官方发行归档和固定 SHA-256 获取。
- 业务代码优先使用 C++ 标准库；标准库缺失或当前实现不一致的安全能力只允许通过单一跨平台依赖接口补齐，不在服务中散落 Win32、Linux syscall 或双分支实现。
- C++、TypeScript、IPC 和 JSON 中的项目自有标识符与字段使用 `snake_case`；只有语言、框架或第三方 API 强制的名称保留上游形式。
- 最终用户只获得随安装包发布的服务二进制，不需要 CMake、编译器或依赖管理器。
