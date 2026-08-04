---
id: tools.practice_tool.architecture.decision.0011
title: "ADR-0011：分离有界控制区与正文附件"
kind: reference
status: maintained
domains:
  - tools
---

# 第1章\_ADR-0011\_分离有界控制区与正文附件

## 1.1\_状态

`accepted`

本决策于 2026-08-03 接受。它取代 ADR-0010 中只适用于 D1A 的单行 JSON 帧细节，不改变 C++20 Native Service、独立进程或窗口能力所有权。

## 1.2\_问题

D1A 只传输小型元数据，1 MiB 单行 JSON 足以形成严格控制面。D1B 需要在 Main 与 Native Service 之间传输最多 5 MiB 的 Markdown 正文；把正文塞进 JSON 会突破控制帧预算、引入 Base64 放大，并让正文与方法参数共享解析和日志风险。Windows 文本模式还可能改写原始换行，因此不能直接在原 `iostream` 文本帧后拼接字节。

## 1.3\_决策

协议版本升级为 `3`，不读取或生成版本 `2`。每个 stdio 消息使用 16 字节大端固定头：

```text
0..3    magic = "LOOP"
4       frame_version = 1
5       flags，bit 0 表示存在正文附件
6..7    reserved = 0
8..11   control_length
12..15  body_length
```

- 控制区是最多 1 MiB 的严格 UTF-8 JSON；正文附件最多 5 MiB，可以为空但必须由 body 标志与描述符明确区分。
- 控制 envelope 固定包含 `body` 字段。无附件时为 `null`；有附件时只包含 `kind`、`byte_length` 和 `sha256`，正文不进入 JSON、日志或错误对象。
- `kind` 在 D1B 只允许 `markdown_utf8`。接收方同时校验头部长度、描述符、SHA-256、方法是否允许附件和正文 UTF-8；不匹配即失败关闭。
- TypeScript 与 C++ 解码器先验证魔数、帧版本、标志、保留位和长度上限，再分配缓冲区。截断、超限、未知标志和异常结束不能产生部分业务请求。
- Native Service 使用 libuv 管道读取标准输入并写标准输出，绕开 Windows C 运行时文本转换；Main 使用 Node Buffer，并在写队列中处理背压和顺序。
- Main 同时最多保留 64 个待处理请求，Native 同时最多保留 64 个待写响应；两端待写预算均为 8 MiB。ADR-0014 增加保存请求正文后，Main 预算也必须容纳一个 1 MiB 控制区加 5 MiB 正文附件的合法最大帧，不能让队列预算反向收窄协议上限。Main 对超时方法保留最多 64 个、30 秒的有界 tombstone，使迟到正文仍按原方法完成附件校验后丢弃；未知请求 ID 不重新进入业务层。超预算或异常退出拒绝全部待处理请求，子进程代次阻止旧退出事件污染新会话。

## 1.4\_能力与正文读取

Renderer 仍不能连接 Native Service。Main 只代理固定 `documents.open` 用例，把 Native 正文附件严格解码为 Renderer 的 `document_snapshot.content`；不把附件保存到文件、状态或日志。

文件夹条目只是短期枚举能力。打开 Markdown 条目时 Native Service 按当前句柄身份签发独立 `document_id`，同一窗口工作区按文件身份去重；目录刷新重签 `entry_id` 不撤销已打开文档。单窗口 Native 文档能力上限为 64，Renderer 标签上限为 32；工作区或窗口关闭时整体撤销。ADR-0014 已按本决策预留要求增加标签级 `documents.close`，关闭文件夹标签会撤销独立文档能力，不通过提高上限规避生命周期。

本 ADR 最初建立的协议 v3 只允许成功的 `workspace.open_document` 响应携带 `markdown_utf8`；ADR-0014 已干净升级到协议 v4，并增加只允许 `workspace.save_document` 请求携带的 `markdown_source_utf8`，不保留 v3 兼容解析。空 Markdown 仍设置附件存在标志，长度为 `0`，摘要为 SHA-256 空值。`content_hash` 覆盖包含 BOM 的磁盘原始字节，打开附件摘要覆盖剥离 BOM 后的编辑正文；每次成功读取或保存都签发新的 `file_version_token`。

## 1.5\_取舍

- 复合帧解析和背压队列比逐行 JSON 更复杂，但控制面继续保持小型、可审计，正文没有 Base64 放大。
- 当前仍在单个服务进程内顺序处理请求；后续索引或保存并发必须在不改变能力所有权的前提下增加取消与调度，不能恢复通用路径或共享内存接口。
- 协议尚未发布给外部调用方，因此干净替换版本 `2`，不维护双解析器、协商降级或旧服务启动路径。
