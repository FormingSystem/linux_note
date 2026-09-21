---
id: labs.kernel.linked_list.materials
title: "链表教材实验材料"
kind: lab
status: evolving
domains: [linux, kernel]
---

# 第1章\_链表教材实验材料

[链表教材](../../../../knowledge/linux/data_structures/单链表_linked_list/大纲.md)提供完整程序、操作解释、预测和练习；这里保存相同材料，便于取用。

| 文件 | 对应内容 |
| --- | --- |
| [chain_model.c](chain_model.c) | P02 单线程宿主模型，观察摘链、重入和成员地址 |
| [once_retry.py](once_retry.py) | P04 私有候选、首次失败与两调用者重试，不模拟内核内存序 |
| [note_list.c](note_list.c)、[Makefile](Makefile) | P05 真实 Linux 接口、私有批次、可控分配失败和退出清理 |

C 模型使用 C11 编译器；Python 模型只用标准库。内核模块要求匹配运行内核的构建配置，固定证据为 NXP Linux 6.12.20。模块没有字符设备、异步使用者和硬件，不把它当成并发驱动模板。

宿主执行与 ARM 语法检查的结果及范围见[工作记录](../../../../governance/migration/repository_textbook_refactor.md#1.4.9_B03a链表拓扑与发布)。真实 Kbuild、MODPOST、模块装卸和内核动态检查器仍须在目标执行，不能以宿主通过替代。
