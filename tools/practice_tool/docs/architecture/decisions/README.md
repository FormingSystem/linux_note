---
id: tools.practice_tool.architecture.decisions
title: "回路架构决策记录索引"
kind: reference
status: evolving
domains:
  - tools
---

# 第1章\_回路架构决策记录索引

## 1.1\_用途

ADR 记录已经做出的重要架构选择、被放弃的替代方案和长期后果，不复制产品或工程设计正文。设计细节变化时先修改权威设计；只有决策本身改变时才新增 ADR 取代旧决策。

状态使用：

```text
proposed
accepted
superseded
rejected
```

已接受 ADR 不原地改写决策结论。新决策通过 `supersedes` 指向被取代记录。

## 1.2\_当前决策

1. [0001：采用 Feature-first 工程组织](0001-feature-first-architecture.md)
2. [0002：采用本地优先持久化](0002-local-first-persistence.md)
3. [0003：训练会话冻结不可变计划快照](0003-immutable-training-plan-snapshot.md)
4. [0004：AI 只生成需要人工审核的草稿](0004-ai-draft-requires-review.md)
