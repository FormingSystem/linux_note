---
id: tools.practice_tool.architecture.decision.0014
title: "ADR-0014：采用冲突检查与句柄相对安全替换"
kind: reference
status: maintained
domains:
  - tools
---

# 第1章\_ADR-0014\_采用冲突检查与句柄相对安全替换

## 1.1\_状态

`accepted`

本决策于 2026-08-03 在 D1-SAVE 实现前评审中接受。它收窄 ADR-0007 中尚未落实的“safe replace 或 guarded write”：D1-SAVE 只开放能够以根句柄为锚点完成的安全替换，不在恢复备份和启动恢复尚未完成时实现可能留下半写正文的 guarded in-place。

## 1.2\_问题

D1B/D1C 已经让 CodeMirror 成为内存草稿和撤销历史的唯一所有者，但 `Ctrl+S` 仍没有写入接口。直接增加 `write_file(path, content)` 会同时破坏四条已经接受的边界：Renderer 不得提交路径；保存必须拒绝外部冲突；文件夹后代必须从根句柄一次解析；发起写入不能被显示成成功。

既有设计还存在三个缺口：

- D1-SAVE 排在 D2 恢复备份与 Hot Exit 之前。如果此时实现截断后原地写入，崩溃或磁盘满可能留下半个文件，而当前版本没有可兑现的启动恢复闭环。
- CodeMirror `Text` 以内存 LF 表示行。LF 和 CRLF 可以按已读基线确定性序列化，mixed 换行却不能在任意编辑后自动恢复原来的逐行分隔符；静默选择一种格式会把格式变化伪装成普通保存。
- Windows `ReplaceFileW` 能合并多类属性和 ACL，但它接收路径并重新解析。把根句柄验证后的路径交给该接口，会重新引入 ADR-0013 已经删除的中间 junction/reparse 交换窗口。

## 1.3\_协议与会话决策

- Native 协议升级为版本 `4`，服务版本升级为 `0.4.0`，不解析版本 `3`。新增 `documents.save` 与 Native `workspace.save_document`；Renderer 仍只提交 `workspace_id + document_id`，Main 注入 `window_session_id`。
- 保存源码使用复合帧附件 `markdown_source_utf8`，控制区不复制或 Base64 编码正文。打开响应继续使用 `markdown_utf8`。只有 `workspace.save_document` 请求允许前一种附件，只有成功的 `workspace.open_document` 响应允许后一种附件；其他方向或方法一律拒绝。
- Main 待写队列预算提高到至少容纳一个 1 MiB 控制区加一个 5 MiB 正文附件；64 个待处理请求上限不变。队列按实际帧字节计量，不能因单个合法最大保存请求超过旧 2 MiB 预算而假失败。
- 保存请求字段为 `workspace_id`、`document_id`、`expected_file_version_token`、`expected_content_hash`、`editor_revision` 和 `line_ending_policy`。`line_ending_policy` 只能是 `preserve`、`normalize_lf` 或 `normalize_crlf`；编码和 BOM 由 Native 已读基线持有，Renderer 不能重定义。
- `file_version_token` 是一次基线租约。每次成功读取或保存都旋转；token、摘要、窗口、工作区和文档任一不匹配都返回 `DOCUMENT_CONFLICT`，不能用旧 token 重放覆盖新版本。
- CodeMirror 在发起保存时保留该修订对应的不可变 `Text`。成功响应只把这一份 `Text` 推进为新基线；若响应前又发生编辑，修订号继续单调递增且文档保持 Dirty。React 只保存元数据和派生保存状态，不复制逐键正文，也不通过重建 EditorView 丢失撤销历史、选择或滚动。
- D1-SAVE 同时增加文档级 `documents.close`，只有 Clean 标签可以直接释放；Dirty 标签仍需保存、明确放弃或取消。释放撤销 Native 文档能力和 Renderer 中对应的编辑器、预览及标签状态，不提高 32/64 上限规避生命周期。

## 1.4\_格式决策

- UTF-8 与 BOM 按读取基线保持。请求正文必须是严格 UTF-8、不得含 NUL，序列化后的 BOM 与换行转换结果仍不得超过 5 MiB。
- 基线为 `lf` 时 `preserve` 写 LF；基线为 `crlf` 时 `preserve` 写 CRLF；基线为 `none` 时新增换行采用 LF。
- 基线为 `mixed` 时 `preserve` 返回 `FORMAT_DECISION_REQUIRED`，磁盘不变。用户必须显式选择统一 LF 或统一 CRLF；UI 在写入前说明这是整文件格式变化。选择结果随成功响应成为新基线。
- 普通保存不执行格式化、标题编号、尾随空白清理或 Front Matter 改写。

## 1.5\_文件策略

D1-SAVE 只支持已打开的非链接、单硬链接、普通本地文件。只读文件、显式链接目标、`link_count > 1`、reparse point、跨卷对象、网络文件系统、特殊文件以及无法完整复制受支持元数据的对象返回稳定错误并保持 Dirty。它们后续只能在单独评审的“另存为”或可恢复 guarded write 中扩展；不得自动降级。

保存流程统一为：

1. 用窗口、工作区、文档 ID、token 和期望摘要取得 Native 文档能力。
2. 从根句柄一次解析完整组件链，打开同一目标句柄，执行有界读取并核对身份、摘要、链接数、只读和格式基线。
3. 在同一安全打开的父目录句柄下，以 CSPRNG 单组件名称排他创建临时普通文件；权限不宽于目标。
4. 写入完整序列化字节并刷新临时文件；复制平台允许且已验证的元数据。发现命名流、不可复制 ACL/xattr、压缩、加密或其他未登记语义时删除本操作临时文件并失败关闭。
5. 替换前再次从根打开目标并核对身份和摘要。普通文件系统不能提供强 compare-and-swap，但必须把可检测窗口缩到提交前，不能只比较 mtime。
6. Windows 使用安全父目录句柄作为 `RootDirectory`，通过 `FileRenameInformationEx`/等价 Native rename 把临时句柄替换为目标名称；Linux 使用同一父目录 fd 的 `renameat`。两端都禁止把重新拼接的绝对路径交给替换 API。
7. Linux 在 rename 前 `fsync` 临时文件，rename 后 `fsync` 父目录；Windows 在 rename 前刷新临时文件，并在替换后重新打开最终目标验证。系统 API 无法承诺所有硬件断电模型时必须在 D6 证据中保留边界，不能把“系统调用成功”等同于绝对断电事务。
8. 替换后从根重新打开目标，验证严格字节摘要、类型和新身份，更新文档能力、身份去重表、内容摘要、mtime、大小、换行基线并旋转 token。旧目录条目身份自然失效；已打开文档继续绑定新身份。

第 5 步是乐观冲突保护，不是对恶意同机进程的事务隔离。Windows/Linux 都没有“仅当目录项仍等于某文件身份才 rename”的跨进程 CAS；第二次验证与第 6 步之间仍有无法由当前原语彻底消除的极短窗口。当前实现用测试钩子在元数据复制后替换目标，证明紧邻提交的第二次根句柄验证会拒绝该竞态，但不把它夸大为数学上的原子 compare-and-swap。若威胁模型以后要求对抗精确竞态，必须先评审目录变更仲裁或具备 D2 恢复保证的句柄内 guarded write，不得再堆路径复检伪装成 CAS。

Windows 的公共 `ReplaceFileW` 文档说明它可以合并原文件的多类属性和 ACL，但也说明结果采用替换文件的新文件 ID；本决策不用它承担授权，因为它没有根句柄相对重解析约束。Windows handle-relative rename 前必须通过句柄复制 DACL/owner/group 与已登记基本属性，并拒绝无法复制的命名流或特殊属性。Linux 必须复制 uid/gid、mode 与全部可枚举 xattr；任一复制失败都不能继续 rename。参考：[ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)、[FILE_RENAME_INFORMATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information)、[rename(2)](https://man7.org/linux/man-pages/man2/renameat.2.html)、[fsync(2)](https://man7.org/linux/man-pages/man2/fsync.2.html)。

## 1.6\_失败与不确定结果

| 阶段 | 错误 | 磁盘与会话结果 |
| --- | --- | --- |
| token、身份或摘要不匹配 | `DOCUMENT_CONFLICT` | 不写临时文件或删除临时文件；保持 Dirty |
| mixed 未选择格式 | `FORMAT_DECISION_REQUIRED` | 不写磁盘；等待用户选择 |
| 链接、硬链接或元数据不受支持 | `UNSAFE_FILE_METADATA` | 不替换；提供另存为作为后续能力 |
| 权限、占用、空间或刷新失败且尚未 rename | 对应稳定 I/O 错误 | 原文件不变；只清理可证明属于本次操作的临时文件 |
| rename 已成功且最终摘要等于请求 | 成功 | 更新基线和 token，即使旧句柄仍由其他进程持有 |
| rename 后无法确认最终对象 | `SAVE_OUTCOME_UNKNOWN` | 不宣称成功；冻结该文档保存并要求重新读取/比较 |
| 保存响应到达时 Renderer 已有新修订 | 成功保存旧修订 | 新 token 成为磁盘基线，当前编辑器仍 Dirty |

临时文件名不得进入 Renderer、日志或协议。进程崩溃可能留下带固定产品前缀但高熵后缀的孤立临时文件；D2/D6 只能在重新验证普通文件、前缀、年龄、目录能力和所有权后清理，不能使用目录通配递归删除。

## 1.7\_明确不属于本切片

- 不实现 guarded in-place、强制覆盖、自动合并或“仍然保存”。
- 不实现自动保存、恢复备份、Hot Exit、启动恢复或本地历史；它们仍属于 D2。
- 不实现另存为、新建、重命名、移动、删除、资源复制或链接批量更新。
- 不为协议版本 `3`、旧 `Ctrl+S` 提示、Node 文件备用实现或浏览器 IndexedDB 建立兼容层。

## 1.8\_验收门禁

- 协议测试覆盖保存附件方向、摘要、空正文、5 MiB、CRLF 扩张超限、过期 token、未知字段与协议版本 `3` 拒绝。
- Native 测试覆盖 LF/CRLF/BOM/none、mixed 选择、无编辑保存、保存期间新修订、读后外部替换、写前与 rename 前冲突、磁盘满/短写/flush/rename/最终验证故障、临时文件清理、权限、占用、硬链接、symlink/junction、父目录移出、同名替换、跨窗、伪造与关闭撤权。
- Windows 自动化必须证明替换 API只接收安全父目录句柄和单组件目标名，并验证 DACL、基本属性、命名流拒绝及新 128 位文件身份。Linux 必须证明 `openat2` 父目录、`renameat`、文件与目录 `fsync`、mode/uid/gid/xattr、bind mount 和 rename 竞态。
- TypeScript 测试覆盖严格 IPC、Main 窗口绑定、5 MiB 主进程写队列、CodeMirror 保存快照、保存中继续编辑、成功后完整撤销仍以新基线判定、冲突/失败常驻状态和 Dirty 关闭保护。
- Electron smoke 必须真实编辑 fixture、`Ctrl+S`、确认磁盘摘要变化与成功响应；随后制造外部修改并确认第二次保存冲突且外部正文未被覆盖。绝对路径与正文不得进入日志或 Renderer 以外的状态副本。
- 双平台故障门禁未完成前 D1-SAVE 保持 `IN_PROGRESS`；不能仅凭正常路径 smoke 标为 `COMPLETE`。

## 1.9\_评审结论

第一轮评审拒绝了三种看似省事但会破坏既有架构的做法：在 D2 前用 guarded in-place、对 mixed 静默选换行、在 Windows 把已授权对象重新交给 `ReplaceFileW` 路径解析。第二轮按故障点检查后补充了保存后身份迁移、响应落后于编辑修订、rename 后结果不确定、临时文件所有权和正文请求队列预算。修订后的方案只扩大一个可验证的安全替换切片，没有用兼容层或虚假恢复承诺掩盖未完成能力，允许进入实现。

## 1.10\_相关决策

- [ADR-0007：采用磁盘正文与分层保存](0007-disk-markdown-and-desktop-persistence.md)
- [ADR-0011：采用有界控制区与正文附件复合帧](0011-bounded-control-and-body-frames.md)
- [ADR-0013：采用句柄相对文件系统能力解析](0013-handle-relative-filesystem-capability-resolution.md)
