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

当前代码仍是 `0.1.0` Vite 浏览器训练工具；新的 Electron 文件/文件夹 Markdown 工作台尚未实现。本文更新时间为 2026-08-02。

ADR-0009 已接受“新建文件、打开文件、打开单个文件夹”的产品模型，并移除专题电子书、训练内容包和正文导入副本。ADR-0006～0008 的 Electron、分层保存和编辑/预览管线仍处于 proposed。任何旧页面或构建通过都不能被描述成新桌面架构已完成。

## 1.2\_当前0.1.0代码事实

现有代码具有：

- React/Vite 浏览器应用与 Feature-first `src/`。
- Bash/MSYS2/Ubuntu 启动、安装、运行与卸载脚本。
- 本地 HTTP 知识文件读取和浏览器正式入口。
- IndexedDB 中的训练、分类、会话与用户电子书数据。
- `banks/` 下 3 个训练单元、12 个电子书章节、12 个训练任务。
- 已有 Markdown 阅读、Mermaid 独立查看、路由和工作台滚动布局。

这些能力只用于识别待删除代码、避免在重构前误报完成度。它们不进入目标工程包，也不要求新架构兼容数据或行为。

## 1.3\_目标差距

| 目标能力 | 状态 |
| --- | --- |
| Electron Main/Preload/Renderer 安全壳 | 未开始 |
| Windows 与 Ubuntu 正式安装包 | 未开始 |
| 空窗口、新建、打开文件、打开文件夹 | 未开始 |
| 窗口作用域文件/文件夹能力表 | 未开始 |
| 文件树按需枚举与最近打开 | 未开始 |
| CodeMirror 文档会话与 Dirty 状态 | 未开始 |
| 手动保存、可选 Auto Save、safe write | 未开始 |
| 合并恢复备份、Hot Exit、本地历史 | 未开始 |
| 外部修改、三方比较与冲突处理 | 未开始 |
| Unified Worker、safe HAST、源码定位 | 未开始 |
| Mermaid、KaTeX、Callout、Wiki feature | 未开始 |
| `loop-app://` 与 token 化 `loop-resource://` | 未开始 |
| 工作区搜索、链接诊断与移动时更新 | 未开始 |
| 回收站、符号链接/硬链接和故障注入 | 未开始 |
| 安全、性能、无障碍与跨平台 E2E | 未开始 |

## 1.4\_已经确认的目标删除项

桌面纵向闭环通过后删除，而不是适配：

- 浏览器正式运行与本地 HTTP 文件服务。
- IndexedDB 业务主存储与正文副本。
- Bash/MSYS2 最终用户运行链和根快捷启动依赖。
- `banks/`、电子书、训练会话、复习、AI 内容生成与导入发布页面。
- `book.json`、`outline.md`、`chapters/`、训练计划和知识声明 Schema。
- 旧 Feature-first 根 `src/`；Renderer 内重新按 editor/explorer/preview 等 feature 建立结构。

删除发生前需要列出真实用户数据与发布风险。除非开发者另外批准一次性导出工具，否则不实现双写、旧存储迁移层、旧 URL 转发或 legacy package。

## 1.5\_下一实施顺序

1. 接受或修订 ADR-0006～0008。
2. 做 Electron 安全壳与 Windows/Ubuntu 打包 spike。
3. 做文件保存 spike：普通文件、symlink、hard link、权限、占用、磁盘满与断电点。
4. 建立“打开文件夹 → 文件树 → 打开 Markdown → CodeMirror → 普通预览 → `Ctrl+S`”纵向闭环。
5. 加入合并恢复备份、Hot Exit、本地历史和外部冲突。
6. 加入 Worker、safe HAST、源码定位、Mermaid、公式和链接。
7. 完成搜索、文件操作、资源插入、回收站和链接更新。
8. 两平台验收通过后删除全部旧浏览器/训练实现并更新包、脚本、Schema 和说明。

## 1.6\_当前验证证据

2026-08-02 在 `tools/practice_tool` 运行：

```text
npm run check:data
  通过：3 个旧训练单元、12 个旧电子书章节、12 个旧训练任务

npm run build
  通过：TypeScript 检查与 Vite 生产构建
  警告：现有 Mermaid 等产物存在超过 500 kB 的 chunk
```

该结果只证明旧代码尚可构建和旧内容闭包有效，不证明 Electron、文件保存、Hot Exit、安全、性能或桌面交互完成。

## 1.7\_相关设计

- [架构索引](README.md)
- [文件与文件夹工作区设计](product/file_and_folder_workspace.md)
- [Markdown 编辑与实时预览设计](product/markdown_editing_and_live_preview.md)
- [桌面运行时与文档服务设计](engineering/desktop_runtime_and_document_services.md)
- [桌面运行时安全与威胁模型](engineering/desktop_runtime_security_and_threat_model.md)
- [架构决策记录](decisions/README.md)
