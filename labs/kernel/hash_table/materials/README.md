---
id: labs.kernel.hash_table.materials
title: "哈希桶与位宽实验材料"
kind: lab
status: evolving
domains: [linux, c_language]
---

# 第1章\_哈希桶与位宽实验材料

[哈希教材](../../../../knowledge/linux/data_structures/哈希表_Hash_Table/大纲.md)就地提供完整 C11 程序、编译命令、预测和练习，本目录保存相同程序便于取用。

| 程序 | 观察问题 |
| --- | --- |
| [bucket_model.c](bucket_model.c) | 同桶冲突、完整键比较、重复键、摘除与重新分桶；对象由调用者持有 |
| [hash_bits.c](hash_bits.c) | 固定宽度乘法、对齐输入、取高位，以及 32/64 位通用算法的区别 |

采用 C11 编译器与标准库。断言承担自检，本实验不定义 NDEBUG。它们是宿主模型，不需要装载模块，也不模拟内核并发、体系结构指令或真实性能。验证范围见[重构记录](../../../../governance/migration/repository_textbook_refactor.md#1.4.10_B03b哈希桶与计算契约)。
