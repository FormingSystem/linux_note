---
id: labs.kernel.tree_basics.materials
title: "树结构实验材料"
kind: lab
status: evolving
domains: [c_language, data_structures]
---

# 第1章\_树结构实验材料

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

增加排序约束后，沿下列三份完整材料继续观察：

| 材料 | 读者任务 | 正文 |
| --- | --- | --- |
| [bst_insert_demo.c](bst_insert_demo.c) | 查找、查重与失败保持原树；参数 1～9 在第几次申请前失败 | [P03](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P03_二叉搜索树_BST.md#3.7_运行查找与插入) |
| [bst_erase_demo.cpp](bst_erase_demo.cpp) | 三类删除、返回新根、后继右孩子回接与地址身份 | [P22](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P22_BST删除与子树回接.md#22.12_运行后再追一个地址) |
| [bst_order_check.cpp](bst_order_check.cpp) | 祖先上界、重复键、整数极值及每次验证的状态重置 | [P23](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P23_BST验证与高度边界.md#23.6_运行全子树边界反例) |

这三份材料独立编译，分别使用 C11、C++17、C++17；不要把各自节点结构和 main 拼接到同一翻译单元。它们都处理独占的小树，不是并发容器或 Linux rbtree 的替身。

[bst_height_model.c](bst_height_model.c)接入[P04 退化观察](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P04_为什么_BST_会退化.md#4.2.4_用节点访问次数观察退化)：在 31 节点固定池内，用相同键集合比较升序和已知中位数顺序，输出树高、构建访问和查找最大键的访问次数。它不测运行时间，也没有实现动态平衡；内部指针指向池中的对象，建好后不能按值复制模型。

旋转沿用自动节点，不分配或释放对象。四份 C11 程序分别观察整树根和内部子树根；C++17 程序观察根引用和逆旋，命令与完整讲解位于正文：

| 材料 | 观察责任 |
| --- | --- |
| [rotate_left_root.c](rotate_left_root.c) | [左旋根入口](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P05_旋转的作用与局部重排.md#5.3.7_C_成品示例_左旋整棵树根) |
| [rotate_left_branch.c](rotate_left_branch.c) | [左旋内部父槽](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P05_旋转的作用与局部重排.md#5.3.8_C_成品示例_左旋子树根节点) |
| [rotate_right_root.c](rotate_right_root.c) | [右旋根入口](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P05_旋转的作用与局部重排.md#5.4.7_C_成品示例_右旋整棵树根) |
| [rotate_right_branch.c](rotate_right_branch.c) | [右旋内部父槽](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P05_旋转的作用与局部重排.md#5.4.8_C_成品示例_右旋子树根节点) |
| [rotation_pair.cpp](rotation_pair.cpp) | [旧根地址与互逆动作](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P05_旋转的作用与局部重排.md#5.4.11_用根引用运行一对互逆动作) |

以上均为宿主内存模型，不使用 Linux API，不验证内核容器、并发或硬件；节点拓扑由程序构造，未声称能校验任意输入图。每份材料独立编译，按相应正文的 C11/C++17 命令运行。各批实际检查范围见[工作记录](../../../../governance/migration/repository_textbook_refactor.md#1.4_批次结果)。
