---
id: tools.practice_tool.architecture
title: "回路架构设计索引"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_回路架构设计索引

## 1.1\_阅读入口

先阅读 [实现状态与版本边界](implementation_status.md)，确认当前版本已经完成、部分完成和尚未完成的能力；再按问题进入下列目标设计。设计目标不等于当前实现，任何完成度结论都以实现状态文档为准。

## 1.2\_目录职责

`architecture` 只保存会长期约束产品和工程实现的设计，不保存普通使用说明、临时调查、开发日志或故障排查记录。

```text
architecture/
├── README.md       # 架构索引、阅读顺序和文档所有权
├── implementation_status.md # 当前代码的完成度、验证和风险
├── product/        # 用户流程、训练模型和学习体验
├── engineering/    # 工程边界、数据安全和质量验收
└── decisions/      # 已接受架构决策及其代价
```

`product` 回答“用户看到什么、怎样操作、业务状态怎样推进”；`engineering` 回答“代码怎样分层、数据怎样持久化和保护、什么条件下才算交付完成”。同一结论只保留一个权威位置，其他文档通过链接引用。

## 1.3\_推荐阅读顺序

### 1.3.1\_产品设计

1. [产品导航与交互设计](product/navigation_and_interaction.md)：大厅、训练库、路由、训练步骤和常规操作。
2. [学习导引提炼标准](product/learning_guide_standard.md)：怎样把知识来源提炼成可学习、可核验和可追溯的导引。
3. [知识提炼、训练适配与 AI 治理](product/content_adaptation_and_ai_governance.md)：授权、提示注入、证据、审核和发布。
4. [训练会话状态与持久化](product/training_session_state_and_persistence.md)：状态机、解锁、自动保存、版本快照和恢复。
5. [复习调度与训练历史](product/review_scheduling_and_history.md)：本次表现、掌握状态、复习时间和历史比较。

### 1.3.2\_工程设计

1. [工程结构与模块边界](engineering/project_structure_and_module_boundaries.md)：Feature-first 目录、模块职责、数据所有权和依赖方向。
2. [导入导出与数据安全](engineering/import_export_and_data_safety.md)：包类型、冲突预览、知识源映射、隐私和回滚。
3. [本地服务安全与威胁模型](engineering/local_service_security_and_threat_model.md)：同源、令牌、规范路径、渲染和更新边界。
4. [无障碍、性能与产品验收标准](engineering/accessibility_performance_and_acceptance.md)：键盘、响应式、性能、可靠性和完成定义。

### 1.3.3\_架构决策

- [架构决策记录索引](decisions/README.md)

## 1.4\_文档所有权

| 设计问题 | 权威文档 |
| --- | --- |
| 当前完成度、验证结果和已知风险 | `implementation_status.md` |
| 大厅、主导航、页面和按钮语义 | `product/navigation_and_interaction.md` |
| 学习导引结构和提炼要求 | `product/learning_guide_standard.md` |
| AI 提炼、证据、审核和发布 | `product/content_adaptation_and_ai_governance.md` |
| 会话、步骤、保存和恢复 | `product/training_session_state_and_persistence.md` |
| 复习、掌握状态和历史 | `product/review_scheduling_and_history.md` |
| 源码目录、模块边界和依赖 | `engineering/project_structure_and_module_boundaries.md` |
| 导入、导出、删除和隐私 | `engineering/import_export_and_data_safety.md` |
| 本地服务、路径与内容渲染安全 | `engineering/local_service_security_and_threat_model.md` |
| 无障碍、性能和验收 | `engineering/accessibility_performance_and_acceptance.md` |
| 关键选择、替代方案和长期后果 | `decisions/` |

若一项需求同时影响多个领域，应在拥有核心业务规则的文档中定义，在其他文档中只记录接口和链接，避免分别维护两套状态定义。

## 1.5\_变更规则

- 稳定文档 ID 不因目录移动而改变。
- 新设计应先判断归属，不能直接继续在索引或 README 中扩写正文。
- 状态、字段和删除语义变化时，同步检查会话、导入导出和验收文档。
- 用户操作变化时，同步检查桌面端、移动端、键盘和异常路径。
- 文档路径变化后更新仓库 README、工具 README、`AGENTS.md` 和全部相对链接。
- 提交前检查旧路径残留、链接、元数据和 `git diff --check`。
