---
id: tools.practice_tool.architecture.decision.0012
title: "ADR-0012：采用有界隔离预览协议"
kind: reference
status: maintained
domains:
  - tools
---

# 第1章\_ADR-0012\_采用有界隔离预览协议

## 1.1\_状态

`superseded`

本决策于 2026-08-03 在 D1-PREVIEW 开发前完成评审并接受。

2026-08-03，ADR-0015 以版本 `3` 块协议和 Typora 式混合编辑面取代本决策的版本 `1` whole-document Frame 数据与完整预览面板。源码、树和 Worker 预算继续由新决策继承；Preview Frame 收窄为 Mermaid 等复杂块的隔离 renderer。

## 1.2\_背景

ADR-0008 已确定 CodeMirror、Markdown Worker、safe HAST 与隔离 Preview Frame 的职责，但还没有规定首个普通预览切片的消息生命周期和资源上限。若直接实现，容易出现四类架构漂移：工作台 Renderer 为方便而插入 HTML；连续输入产生无界 Worker 任务；旧修订覆盖新预览；不同源 Frame 仍通过宽松 `postMessage` 接收任意对象。

D1-PREVIEW 只需要完成 CommonMark/GFM 普通正文闭环。D3 才增加按块增量复用、源码定位和复杂块调度，D4 才开放本地资源、Mermaid、公式、Callout 与 Wiki 链接。因此当前协议必须安全、可验证，但不能提前冻结 D3 的块协议。

## 1.3\_决策

- 编辑器停止输入约 150 ms 后才把最新 CodeMirror 内存快照交给预览协调器。React 不在逐键状态中保存正文，预览不读取磁盘。
- 单个 Worker 同时只处理一个完整快照，并只保留一个最新待处理请求。新修订会取代尚未开始的旧请求；已开始的旧结果仍会被修订门禁丢弃。任务超过 5 秒时终止并重建 Worker，不允许解析队列或失败状态无界增长。
- D1-PREVIEW 使用标准 HAST `root` 作为 whole-document `safe_hast`，不传 HTML 字符串、DOM、React 元素或私有完整 AST。D3 将干净替换为带 block、source span 和增量复用的数据结构，不维护双协议。
- 源码按 UTF-8 计量，上限 5 MiB；safe HAST 上限为 100000 个节点、64 层深度、12 MiB 结构化 JSON 估算值。字符串、属性数和诊断数另设局部上限。任一门禁失败只返回稳定诊断，不向 Frame 发送部分树。
- Unified 固定管线只启用 CommonMark 与 GFM。原始 HTML 不进入 HAST；图片转换为不发起请求的文字占位；链接只保留经过分类的 `http`、`https`、片段和相对 Markdown 目标，并在 D1 中保持不可导航。危险 scheme 和超长 URL 被移除并产生诊断。
- Preview Frame 固定加载 `loop-preview://preview/` 下三个打包资源，协议处理器不提供目录通配读取。Frame 无 Preload、无 Node，iframe 只授予 `sandbox="allow-scripts"`；CSP 从 `default-src 'none'` 开始，并把网络、表单、弹窗、下载、顶层导航和父页面 DOM 排除在能力外。
- 工作台在 Frame 每次加载后创建一次专用 `MessageChannel` 和高熵会话 nonce，只向该次加载转移一个端口。双方严格校验协议版本、nonce、消息枚举、文档 ID、修订和预算；连接完成后不再使用窗口级广播传输文档。
- Frame 用固定元素和属性 allowlist 逐节点调用 `createElement`、`createTextNode` 与受控属性设置，不使用 `innerHTML`。Frame 只能回报 ready、rendered、render_error 和 inert_link_activated；工作台仍把链接动作视为未开放能力。

## 1.4\_失败语义

| 失败 | 所有者 | 可见结果 | 后续行为 |
| --- | --- | --- | --- |
| 源码或树超限 | Worker/协调器 | 当前修订显示“预览已暂停”与诊断 | 后续较小的新修订可恢复 |
| 解析异常 | Worker | 当前修订显示解析失败 | Worker 保持可用，除非异常退出 |
| 超时或 Worker 异常退出 | 协调器 | 当前修订显示预览不可用 | 终止并重建一次 Worker |
| 过期修订返回 | 协调器 | 无 UI 回退 | 丢弃，不发送给 Frame |
| Frame 握手或消息无效 | 工作台/Frame | 预览隔离错误 | 关闭端口，本次 Frame 不再渲染 |
| 单节点渲染无效 | Frame | 清空当前树并回报错误 | 不保留部分、不执行降级 HTML |

## 1.5\_评审结论

本决策保持了五个既有边界：草稿仍归 CodeMirror；Markdown 语义仍归 Worker；工作台 DOM 不接收文档节点；Preview Frame 没有文件或桌面能力；D1-PREVIEW 不写磁盘。资源上限、取消、过期响应、Frame 身份和失败恢复均可在纯 TypeScript、协议策略测试与 Electron smoke 中分别验证，因此允许进入实现。

## 1.6\_后果

- D1-PREVIEW 可以安全覆盖标题、段落、列表、引用、代码、表格、删除线和任务列表，并在连续输入后更新内存草稿预览。
- 5 MiB 极限文档可能因为 HAST 预算暂停预览，但编辑器仍可继续工作；这比主线程卡死或无界内存增长更符合失败关闭原则。
- D1 不显示本地或远程图片、不打开链接，也不提供源码—预览定位；这些能力分别留给 D3/D4，不用临时接口伪装完成。
- whole-document 传输会复制完整安全树。D3 以实测结果设计块级增量协议，并直接替换本协议的数据部分。

## 1.7\_相关决策

- [ADR-0006：采用桌面优先的多进程与包级架构](0006-desktop-first-multiprocess-architecture.md)
- [ADR-0008：分离 Markdown 编辑与实时渲染管线](0008-separate-markdown-editing-and-rendering.md)
- [ADR-0011：分离有界控制区与正文附件](0011-bounded-control-and-body-frames.md)
