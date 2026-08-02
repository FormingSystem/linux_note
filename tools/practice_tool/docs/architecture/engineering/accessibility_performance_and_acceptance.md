---
id: tools.practice_tool.architecture.accessibility_performance_acceptance
title: "无障碍、性能与产品验收标准"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_无障碍、性能与产品验收标准

## 1.1\_适用范围

本文是 Electron 文件/文件夹 Markdown 工作台的交付门槛，不是当前完成声明。所有性能结果使用安装后的生产包，并记录操作系统、CPU、内存、磁盘、Electron/Chromium/Node 版本、文件规模和 P50/P95。

## 1.2\_键盘与焦点

- 仅用键盘可以完成新建、打开文件、打开文件夹、资源树导航、编辑、保存、预览、搜索、关闭和冲突处理。
- 焦点进入 CodeMirror 后可以用明确命令离开，不能形成键盘陷阱。
- 对话框初始焦点、取消、确认和关闭后焦点恢复可预测。
- 资源树、标签、命令面板、诊断和大纲使用正确复合控件语义。
- 不仅靠颜色表达 Dirty、失败、冲突、链接错误和当前选择。
- 源码—预览同步、动画和 Mermaid 平移可关闭或遵循减少运动偏好。

## 1.3\_屏幕阅读器与视觉

- 保存失败、外部冲突和恢复结果通过克制的 `aria-live` 通知，同时保留可聚焦状态详情。
- 编辑器的行列、诊断和折叠语义使用 CodeMirror 可访问能力并经过 NVDA 验证。
- 预览标题层级、表格、列表、引用、代码和图片替代文本保持语义。
- Mermaid 至少提供可访问名称、源码入口和错误文本；图形不能成为获取信息的唯一方式。
- 亮色、暗色和高对比主题均满足 WCAG 2.2 AA 的对比要求。
- 200% 缩放时关键命令不被裁切，窄屏可切换单视图。

## 1.4\_性能场景

统一夹具：

| 场景 | 数据规模 |
| --- | --- |
| 普通文档 | 200 KB，含表格、代码和图片链接 |
| 大文档 | 1 MB，约 20000 行 |
| 极限文档 | 5 MB，预览允许降级 |
| 复杂块 | 20 个 Mermaid、20 个公式块、50 个高亮代码块 |
| 普通目录 | 1000 项、200 个 Markdown |
| 大目录 | 10000 项、2000 个 Markdown、含排除目录 |

首版目标：

- 冷启动到可输入 P95 不高于 3 秒。
- 1 MB 文档普通输入事务 P95 不高于 16 ms，不因预览或备份阻塞。
- 停止输入后，1 MB 普通 Markdown 预览 P95 在 300 ms 内更新；复杂块可以继续异步。
- 5 MB 文档仍可编辑，输入事务 P95 不高于 50 ms；系统可以自动暂停复杂预览并解释。
- 大目录打开后 2 秒内可浏览首层；完整索引在后台进行并可取消。
- 工作区文件树不把所有正文送入 Renderer；关闭文档后释放编辑器与复杂块缓存。

这些值在首个桌面 spike 中实测；不满足时先优化或缩小功能，不通过调高警告阈值伪造通过。

## 1.5\_存储与写放大

连续输入 60 秒的测试必须证明：

- 默认手动保存下源文件写入次数为 0。
- 每次按键只更新内存，没有逐键 AppData 写入。
- 恢复备份遵守空闲约 2 秒和最大约 30 秒的合并策略，内容摘要相同不重复写。
- 预览解析不写磁盘。
- 本地历史只在源文件成功保存后生成，并受合并窗口和空间上限约束。

测试同时记录正文大小、编辑次数、备份写入次数与总写入字节，防止实现虽然“不是每键一次”却反复重写多个无界副本。

## 1.6\_文件与冲突可靠性

必须覆盖：

- UTF-8、UTF-8 BOM、CRLF、LF、无效编码和无尾换行。
- 只读、权限变化、磁盘满、目标占用、目录不可用和中途取消。
- 保存期间继续编辑，旧保存结果不能把新修订标为 Clean。
- watcher 丢失、重复、乱序和应用从挂起恢复。
- 外部编辑、外部 rename/delete、相同 mtime 不同内容。
- symlink、hard link、junction、reparse point、UNC、bind mount 和文件身份替换。
- safe replace 各断点故障后原文件、临时文件、恢复备份和 UI 状态一致。

不能实现完美跨进程原子 compare-and-swap 的文件系统竞态必须在测试报告中保留限制说明，依靠复检、失败关闭、备份和历史降低损失。

## 1.7\_Markdown\_与安全

- CommonMark/GFM、Front Matter、代码、Mermaid、KaTeX、Callout、Wiki 链接和脚注具有固定 fixture。
- CodeMirror 与 Unified 对语法范围和源码位置的一致性有回归测试。
- 原始 HTML、URL、SVG、Mermaid 和公式恶意输入经过 sanitizer 与 CSP 测试。
- Preview Frame 没有 Preload/IPC，消息协议拒绝未知事件、过期修订、越界源码位置和未分类 URL。
- 远程图片在默认设置下不发出网络请求。
- 一个复杂块失败不阻止正文；旧修订预览有明确标识。
- 本地资源 token 越界、过期、跨窗口复用和文件身份变化全部拒绝。

## 1.8\_桌面安全

- 生产窗口断言 sandbox、context isolation、禁用 Node integration 和启用 webSecurity。
- CSP、导航、新窗口、权限请求和 IPC sender 使用自动化负面测试。
- `file://`、本地 HTTP、任意通道、任意路径和任意外部 URL 接口不存在。
- 单文件不能枚举父目录，文件夹不能越过真实根。
- 工作区脚本、设置、插件和 CSS 不自动加载。
- 安装包、更新清单与依赖供应链经过签名和审计。

## 1.9\_平台矩阵

Windows 10/11 x64 与 Ubuntu 22.04 x64 都在干净环境执行：

1. 安装、启动、更新、回滚与卸载。
2. 新建、打开单文件、打开文件夹、最近打开与 Hot Exit。
3. 编辑、预览、手动保存、可选自动保存、外部冲突和恢复。
4. 新建、重命名、回收站、资源插入和链接更新。
5. 平台链接、权限、占用、监听与路径边界故障注入。
6. 清缓存、清状态和卸载不删除用户文件、备份或历史的错误范围。

## 1.10\_发布门槛

- ADR-0006～0008 被接受，ADR-0009 保持有效。
- Electron 安全 spike、打包 spike、文件保存 spike 和 1 MB/5 MB 预览 spike 通过。
- 单文件和单文件夹纵向 E2E 在两个平台通过。
- 无 P0/P1 数据损坏、安全或键盘阻塞问题。
- `npm run check:data` 只用于旧 `0.1.0` 退役前验证；新目标建立独立 `lint / typecheck / unit / integration / e2e / security` 门禁。
- 实现状态文档逐项给出证据，不能把构建通过描述成完整交互、安全或发布验收。

## 1.11\_相关设计

- [文件与文件夹工作区设计](../product/file_and_folder_workspace.md)
- [Markdown 编辑与实时预览设计](../product/markdown_editing_and_live_preview.md)
- [桌面运行时与文档服务设计](desktop_runtime_and_document_services.md)
- [桌面运行时安全与威胁模型](desktop_runtime_security_and_threat_model.md)
- [工作区文件操作与数据安全](workspace_file_operations_and_data_safety.md)
- [实现状态与版本边界](../implementation_status.md)
