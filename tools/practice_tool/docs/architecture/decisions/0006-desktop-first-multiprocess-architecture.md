---
id: tools.practice_tool.architecture.decision.0006
title: "ADR-0006：采用桌面优先的多进程与包级架构"
kind: reference
status: maintained
domains:
  - tools
---

# 第1章\_ADR-0006\_采用桌面优先的多进程与包级架构

## 1.1\_状态

`accepted`

本决策于 2026-08-02 随桌面实现启动而接受，取代 ADR-0001 中“单一浏览器应用结构已经足够、不采用包级工作区”的结论；Feature-first 仍保留在桌面 Renderer 的业务功能内部。

## 1.2\_背景

回路需要长期读取和编辑本地 Markdown、监听外部修改、安全保存、解析相对资源、恢复草稿并提供跨平台安装。浏览器文件选择和本地 HTTP 服务可以拼接出部分能力，但权限、路径、生命周期、资源协议和最终用户运行环境会持续分散。把 Node.js 文件服务嵌在 Vite 配置中也无法形成清晰的产品权限边界。

## 1.3\_决策

正式产品采用 Electron 桌面客户端。Electron Main 持有窗口、系统对话框、子进程生命周期和协议注册权限；随安装包发布的 Native Service 持有工作区能力表、真实路径、文件读写、备份、监听、索引与搜索权限。Preload 只暴露固定类型用例接口，React Renderer 保持沙箱与上下文隔离。

Main 与 C++20 Native Service 使用版本化、限长、可取消的本地帧协议。Renderer 和 Preview Frame 都不能直接连接 Native Service。正式运行不启动本地 HTTP 服务、不打开外部浏览器，也不要求用户预装 Node.js、CMake、C++ 编译器或 MSYS2。

工程按进程边界和稳定包组织为 `apps/desktop + packages/* + native/*`；Renderer 内部继续按产品 feature 组织。Vite 只用于桌面 TypeScript 代码的开发和构建，CMake 构建 C++20 Native Service。语言边界由已接受的 ADR-0010 规定。

## 1.4\_替代方案

- 继续浏览器 + Node 本地服务：可复用现状，但保留端口、同源、启动器和浏览器差异，文件工作台仍不是一等运行时。
- Tauri：安装包较小且天然采用 Rust 后端，但系统 WebView 会扩大 Windows/Linux 渲染差异；本产品更看重一致的 CodeMirror、Unified、Mermaid 和隔离预览运行时，因此不选择系统 WebView。
- 完整原生 UI：系统集成强，但需要重写现有 React、CodeMirror 生态接入和 Markdown/Mermaid 预览，跨平台交互与无障碍维护范围更大。
- Electron 单进程并在 Renderer 开启 Node.js：开发快，但不可信 Markdown 一旦造成 XSS 就获得本机权限，不接受。

## 1.5\_后果

- 可以复用 React UI，同时建立稳定文件系统、窗口和协议能力。
- 发布包体积和内存占用高于系统 WebView 方案。
- Electron、Chromium 和 Node.js 安全更新成为持续发布责任。
- Main、Preload、Renderer、Worker、Native Service 和跨语言模块边界增加构建、协议与测试复杂度。
- 旧浏览器正式运行、Vite 文件服务和 Bash 运行时入口需要在实现阶段删除，不保留双架构。
