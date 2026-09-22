---
id: labs.kernel.hash_table.materials
title: "哈希桶与位宽实验材料"
kind: lab
status: evolving
domains: [linux, c_language]
---

# 第1章\_哈希桶与位宽实验材料

[哈希教材](../../../../knowledge/linux/data_structures/哈希表_Hash_Table/大纲.md)就地提供完整 C11 程序、编译命令、预测和练习，本目录保存相同程序便于取用；hlist 的语句表达式程序使用 GNU C11，内核模块使用对应版本的 Kbuild。

| 程序 | 观察问题 |
| --- | --- |
| [bucket_model.c](bucket_model.c) | 同桶冲突、完整键比较、重复键、摘除与重新分桶；对象由调用者持有 |
| [hash_bits.c](hash_bits.c) | 固定宽度乘法、对齐输入、取高位，以及 32/64 位通用算法的区别 |
| [hlist_model.c](hlist_model.c) | 真实指针入口槽、首中尾摘除、重新连接与提前保存游标 |
| [statement_expression.c](statement_expression.c) | GNU C 语句表达式的结果与一次求值 |
| [note_hlist_rcu.c](note_hlist_rcu.c)、[Makefile](Makefile) | 完整模块：保留旧节点前向路径、排队回收与卸载前等待回调，支持 fail_at=1/2/3 注入 |
| [rehash_path_model.c](rehash_path_model.c) | 串行安排旧读者跨进新链，按链尾身份重扫，并沿后继表查找 |
| [note_rhashtable.c](note_rhashtable.c) | 完整动态表接口模块：固定键去重、复制读取、单项移除和失败/退出销毁 |

动态迁移模型使用标准 C11，完整接口模块的命令和验收见[P08](../../../../knowledge/linux/data_structures/哈希表_Hash_Table/P03_高级进阶与性能调优/P08_rhashtable接口与回收实验.md#8.3_预测日志并在匹配目标验证)。模型不模拟真实并发，接口输出也不证明已经触发实际扩容。

前面三份宿主模型采用 C11 编译器与标准库。断言承担自检，本实验不定义 NDEBUG。宿主程序不模拟内核并发、体系结构指令或真实性能。内核模块的构建、预期日志、失败恢复和证明边界在 P04 的实验小节；已完成 ARM 语法检查，尚未执行目标 Kbuild、装卸与并发验证。验证范围见[重构记录](../../../../governance/migration/repository_textbook_refactor.md#1.4.13_B03c节点入口槽与RCU旧路径)。
