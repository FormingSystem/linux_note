---
id: labs.kernel.tree_basics.materials
title: "普通树与二叉遍历实验材料"
kind: lab
status: evolving
domains: [c_language, data_structures]
---

# 第1章\_普通树与二叉遍历实验材料

[tree_representation.c](tree_representation.c)与[P16 正文](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P16_普通树的表示与构建实验.md#16.6_示例_普通树的一个简单链式实现)提供同一份完整 C11 程序。先读[P01](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P01_树的基本概念.md#1.2_什么是树)的逻辑关系，再预测五节点的深度、高度、叶子及各失败点的回收数。

程序先私有分配，全部成功后连接；失败清理独立节点，成功路径从唯一根递归释放。节点名字借用字符串常量，树节点地址由程序独占。命令参数 1～5 在对应分配前注入失败，0 不注入；真实分配失败仍走相同出口。

四种二叉遍历使用同一 A～G 树，每种提供独立 C11 与 C++17 程序；材料和正文的完整代码一致，不要求把多篇程序拼接才能运行。

| 单元 | C 程序 | C++ 程序 | 正文 |
| --- | --- | --- | --- |
| 前序 | [preorder_demo.c](preorder_demo.c) | [preorder_demo.cpp](preorder_demo.cpp) | [P17](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P17_前序遍历与先根处理.md) |
| 中序 | [inorder_demo.c](inorder_demo.c) | [inorder_demo.cpp](inorder_demo.cpp) | [P18](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P18_中序遍历与有序性前提.md) |
| 后序 | [postorder_demo.c](postorder_demo.c) | [postorder_demo.cpp](postorder_demo.cpp) | [P19](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P19_后序遍历与子树完成.md) |
| 层序 | [levelorder_demo.c](levelorder_demo.c) | [levelorder_demo.cpp](levelorder_demo.cpp) | [P20](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P20_层序遍历与队列边界.md) |

八份程序均接受 0～7 的构建失败参数，省略参数表示正常运行。C 由根负责已连接树的释放，构建失败回收独立节点；C++ 的唯一拥有者数组负责释放，左右边只借用地址。层序 C 版检查调用者队列容量，C++ 队列仍可能抛出分配异常。实际命令和过程解释在各篇正文，先手算再运行。

[binary_queries.c](binary_queries.c)是[P21 的完整查询实验](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P21_递归状态与二叉树基本查询.md#21.8_让查询接受一个反例)，用非 BST 的四节点反例检查高度、计数、叶子和普通查找。它使用自动数组，无须动态释放。

标准 C11 编译器即可运行，命令、输出与练习见[运行说明](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P16_普通树的表示与构建实验.md#16.7_运行预测与资源回收)。这是宿主内存模型，不使用 Linux API，不验证内核容器、并发或硬件；输入拓扑由程序构造，未声称能校验任意图。实际检查范围见[工作记录](../../../../governance/migration/repository_textbook_refactor.md#1.4.16_B03f树关系与表示实验)。
