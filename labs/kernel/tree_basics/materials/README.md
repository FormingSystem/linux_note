---
id: labs.kernel.tree_basics.materials
title: "普通树表示实验材料"
kind: lab
status: evolving
domains: [c_language, data_structures]
---

# 第1章\_普通树表示实验材料

[tree_representation.c](tree_representation.c)与[P16 正文](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P16_普通树的表示与构建实验.md#16.6_示例_普通树的一个简单链式实现)提供同一份完整 C11 程序。先读[P01](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P01_树的基本概念.md#1.2_什么是树)的逻辑关系，再预测五节点的深度、高度、叶子及各失败点的回收数。

程序先私有分配，全部成功后连接；失败清理独立节点，成功路径从唯一根递归释放。节点名字借用字符串常量，树节点地址由程序独占。命令参数 1～5 在对应分配前注入失败，0 不注入；真实分配失败仍走相同出口。

标准 C11 编译器即可运行，命令、输出与练习见[运行说明](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P16_普通树的表示与构建实验.md#16.7_运行预测与资源回收)。这是宿主内存模型，不使用 Linux API，不验证内核容器、并发或硬件；输入拓扑由程序构造，未声称能校验任意图。实际检查范围见[工作记录](../../../../governance/migration/repository_textbook_refactor.md#1.4.16_B03f树关系与表示实验)。
