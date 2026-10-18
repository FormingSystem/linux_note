---
applyTo: "**"
---
# Mermaid 文档工作流

本文件指导仓库文档中的 Mermaid 图，不要求所有协作环境都安装同一个编辑器扩展。写作语义、标签、树图和颜色规则以 [Mermaid 输出约定](../../tools/ai/application/mermaid_require.md) 为准；先遵守仓库 AGENTS.md 和本次用户授权，再按实际可用工具选择验证方式。

## 先确定图源和读者任务

1. 说明图要回答的问题、对象与箭头含义。先建立必要背景，再用图解释状态、通信或关系。
2. 教材图源默认放在对应 Markdown 的 `mermaid` 围栏中；独立素材才使用 `.mmd` 或 `.mermaid`。不为工具偏好复制一份无人同步的图源。
3. 检查图型声明、引号、括号、边标签和对象命名；时序图使用可定位的步骤编号。
4. 使用当前环境实际可用的 Mermaid 解析与渲染工具，记录版本，再目视核对裁切、连线和语义。只通过解析不能证明画面与机制正确。
5. 如果没有目标阅读器，只报告实际验证过的环境，不声称 Typora、VS Code、GitHub 全部通过。图片宽度由读者的工作台适配，不为宽度删减必要信息。

## 按当前工具能力验证

在 VS Code 且 Mermaid Chart 扩展已启用时，可以使用扩展提供的预览或语言模型工具。在其他环境，可以使用已有的本地 Mermaid 运行时、CLI 或浏览器渲染；缺少扩展不构成停止文档编辑的理由，也不能伪造一次扩展调用。

2026-10-18 核对 [扩展官方 package.json](https://github.com/Mermaid-Chart/vscode-mermaid-chart/blob/main/package.json) 时，其声明版本为 2.8.2，提供下列语言模型工具。这里是上游声明核对，不是本机已安装版本的证明：

| 工具名 | 作用 | 使用条件 |
| --- | --- | --- |
| `get-syntax-docs-mermaid` | 获取图型语法说明 | 当前客户端实际暴露此工具；陌生语法也可查 Mermaid 官方文档 |
| `mermaid-diagram-validator` | 检查 Mermaid 语法 | 已有完整图源，工具确实可调用 |
| `mermaid-diagram-preview` | 生成预览 | 客户端支持该工具，预览结果仍须检查 |

不要把表中的字符串当作所有代理环境都存在的 API。未安装或未暴露时使用实际可用的等价验证路径，记录这一边界。

## 扩展命令只在适用宿主调用

以下命令 ID 来自同次官方声明核对；必须先确认本机版本仍提供对应命令，并在 VS Code 命令面板或该宿主允许的命令接口中使用。普通终端不能直接把命令 ID 当成可执行文件。

| 目的 | 命令 ID |
| --- | --- |
| 预览活动图 | `mermaidChart.preview` |
| 创建独立图文件 | `mermaidChart.createMermaidFile` |
| 修复图 | `mermaidChart.repairDiagram` |
| 改进图 | `mermaidChart.improveDiagram` |
| 从代码生成图 | `mermaidChart.generateDiagramFromCode` |
| 生成云架构图 | `mermaidChart.generateCloudDiagram` |
| 生成实体关系图 | `mermaidChart.generateERDiagram` |
| 生成 Docker 图 | `mermaidChart.generateDockerDiagram` |
| 打开 Copilot 对话 | `mermaidChart.openCopilotChat` |
| 登录与退出 | `mermaidChart.login` / `mermaidChart.logout` |
| 将本地图连接到云端图 | `mermaidChart.connectDiagramToMermaidChart` |
| 同步已连接图 | `mermaidChart.syncDiagramWithMermaid` |
| 审查同步变更 | `mermaidChart.reviewAppCommits` |
| 从来源重新生成 | `mermaidChart.regenerateDiagramWithMermaidAI` |
| 安装扩展自带 AI 说明 | `mermaidChart.installAiSkills` |

AI 修复、生成和云同步可能调用远端服务或消耗额度；先查实际功能提示和本次授权。只要求本地编辑图时，不因此自动连接云端、分享、同步、登录或购买服务。原记录列出的 `@mermaid-chart` 斜杠命令须以实际安装版本提供的菜单与帮助为准，不将历史名称固化为必需入口。

## 已连接的图先查来源所有权

Front Matter 中存在 `id` 只是一项线索，不足以证明任意 Markdown 都由 Mermaid Chart 管理；本仓库正文也有稳定文档 ID。结合扩展元数据、连接状态和实际维护流程确定图源。确属 Mermaid Chart GitHub Sync 管理的文件，应使用其审查、差异和接受流程，避免本地重生成覆盖来源。

普通教材内嵌 Mermaid 图由本仓库正文维护，不因为正文含有 `id` 就停止编辑。需要修改由外部同步负责的图时，先在其真实来源中落实授权修改，再检查同步结果与引用。

## 交付时报告什么

报告图源所在文档、检查过的渲染器和版本、实际目视结果以及未测试的目标环境。保留同一份可编辑图源，避免把“有截图”“扩展未报错”或“生成了 SVG”当作语义正确的全部依据。语法细节追溯 [Mermaid 官方文档](https://mermaid.js.org/)，扩展行为追溯 [官方仓库](https://github.com/Mermaid-Chart/vscode-mermaid-chart)。
