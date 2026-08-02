---
id: tools.practice_tool.architecture.decision.0006
title: "ADR-0006：采用桌面优先的多进程与包级架构"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_ADR-0006\_采用桌面优先的多进程与包级架构

## 1.1\_状态

`proposed`

若接受，本决策取代 ADR-0001 中“单一浏览器应用结构已经足够、不采用包级工作区”的结论；Feature-first 仍保留在桌面 Renderer 的业务功能内部。

## 1.2\_背景

回路需要长期读取和编辑本地 Markdown、监听外部修改、安全保存、解析相对资源、恢复草稿并提供跨平台安装。浏览器文件选择和本地 HTTP 服务可以拼接出部分能力，但权限、路径、生命周期、资源协议和最终用户运行环境会持续分散。把 Node.js 文件服务嵌在 Vite 配置中也无法形成清晰的产品权限边界。

## 1.3\_决策

正式产品采用 Electron 桌面客户端。Electron Main 是唯一系统权限所有者，Preload 暴露固定类型接口，React Renderer 保持沙箱与上下文隔离。正式运行不启动本地 HTTP 服务、不打开外部浏览器，也不要求用户预装 Node.js 或 MSYS2。

工程按进程边界和稳定包组织为 `apps/desktop + packages/*`；Renderer 内部继续按产品 feature 组织。Vite 只用于 Renderer 的开发和构建。

## 1.4\_替代方案

- 继续浏览器 + Node 本地服务：可复用现状，但保留端口、同源、启动器和浏览器差异，文件工作台仍不是一等运行时。
- Tauri：安装包较小，但引入 Rust、系统 WebView 和新的 Windows/Linux 构建依赖；当前 React/Node 文件能力无法直接成为正式主进程。
- 完整原生 UI：系统集成强，但需要重写现有 React、CodeMirror 生态接入和 Markdown/Mermaid 预览，跨平台交互与无障碍维护范围更大。
- Electron 单进程并在 Renderer 开启 Node.js：开发快，但不可信 Markdown 一旦造成 XSS 就获得本机权限，不接受。

## 1.5\_后果

- 可以复用 React UI，同时建立稳定文件系统、窗口和协议能力。
- 发布包体积和内存占用高于系统 WebView 方案。
- Electron、Chromium 和 Node.js 安全更新成为持续发布责任。
- Main、Preload、Renderer、Worker 和 package 边界增加构建与测试复杂度。
- 旧浏览器正式运行、Vite 文件服务和 Bash 运行时入口需要在实现阶段删除，不保留双架构。
