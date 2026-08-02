---
id: tools.practice_tool.architecture.decision.0008
title: "ADR-0008：分离 Markdown 编辑与实时渲染管线"
kind: reference
status: maintained
domains:
  - tools
---

# 第1章\_ADR-0008\_分离\_Markdown\_编辑与实时渲染管线

## 1.1\_状态

`accepted`

本决策于 2026-08-02 随桌面实现启动而接受。

## 1.2\_背景

源码编辑、预览解析、DOM 更新、Mermaid、公式和文件保存具有不同成本与失败方式。每次输入同步执行全文解析和复杂渲染会阻塞输入；把所见即所得状态当正文又会产生 Markdown 往返转换与未知语法丢失。

上一版草案提出完整私有 Render Model，但这会复制 MDAST/HAST 生态已有结构，并增加序列化、插件和测试负担。

## 1.3\_决策

- CodeMirror 6 管理 Markdown 源码、选择、撤销和编辑事务。
- Unified 管线在 Web Worker 中解析 MDAST/HAST，执行固定转换与 allowlist 清洗。
- Worker 返回按顶层块组织的最小 `PreviewDocument`：safe HAST、源码位置、摘要、嵌入描述符与诊断；不创造一套完整私有 AST。
- 工作台 Renderer 不渲染文档 DOM；它通过专用 MessageChannel 把 `PreviewDocument` 发送给无 Preload、无 Node、不同源且 sandboxed 的 Preview Frame。Frame 用固定组件映射渲染 safe HAST，Mermaid、KaTeX 和代码高亮是可取消、可缓存、失败隔离的内置 renderer。
- 输入后短防抖只更新预览，不读取或写入磁盘。编辑、预览、保存和备份修订分别记录，过期任务不得覆盖新状态。
- 第一阶段不实现隐藏源码的所见即所得，也不加载工作区或第三方可执行渲染插件。

## 1.4\_替代方案

- `textarea + react-markdown`：适合原型，无法稳定承载编辑事务、诊断、大文档与源码映射。
- Monaco：能力完整，但通用 IDE 服务与打包成本超过 Markdown 工作台需要。
- 完整私有 Render Model：隔离清晰，但重复生态标准树并扩大长期协议面。
- Worker 返回 HTML 字符串：传输简单，但源码定位、块级更新和安全组件映射较弱。
- 每次输入同步重绘全文：实现少，但输入延迟和复杂块闪烁不可控。

## 1.5\_后果

- 编辑输入、普通预览与复杂块独立调度，Mermaid 失败不阻断正文。
- CodeMirror 与 Unified 使用不同解析器，必须共享 fixture 检查语义和源码位置一致性。
- safe HAST allowlist 与固定组件映射成为安全边界，需要恶意输入回归测试。
- Preview Frame 与工作台之间需要稳定、窄且双向校验的消息协议，但预览 XSS 不再直接接触文件 IPC。
- 第一版先以完整源码快照和块级复用实现；只有性能基准失败时才引入增量解析协议。
