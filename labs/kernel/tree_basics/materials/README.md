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

组合旋转在 [P24](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P24_组合旋转与形状判断.md#24.1_章节内容说明)接着运行：[rotation_cases.c](rotation_cases.c)和[rotation_cases.cpp](rotation_cases.cpp)各自从四组三节点输入识别两级方向并重排；[rotation_internal_lr.c](rotation_internal_lr.c)保留内部子树案例，展示形状为 LR 并不代表该处已经违反 AVL 高度约束。三份程序仍独立编译，检查孩子非空不能代替节点存活、成员关系或平衡诊断。

[avl_height_demo.c](avl_height_demo.c)在 [P25 完整实验](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P25_AVL高度诊断与更新传播.md#25.23_运行完整高度维护程序)中增加缓存高度、插入和删除后的回溯、等高停止及多层修复。它由根负责动态节点回收，失败保留原树，main 的失败出口销毁已建成部分；本例是独占 AVL 模型，不是 Linux rbtree 的实现。

[lookup_paths.c](lookup_paths.c)在[P10 查找实验](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P10_Linux_6.12_内核_rbtree_查找与返回边界.md#10.2.10_用完整C程序观察相等节点和旧路径)中串行重放重复键左旋、旧根漏查与错误写序形成环。对象全程存活，坏树查询使用有限预算，防止实验挂起；它不执行真实线程竞争，也不把预算耗尽当成通用判环结果。

以上均为宿主内存模型，不使用 Linux API，不验证内核容器、并发或硬件；节点拓扑由程序构造，未声称能校验任意输入图。每份材料独立编译，按相应正文的 C11/C++17 命令运行。各批实际检查范围见[工作记录](../../../../governance/migration/repository_textbook_refactor.md#1.4_批次结果)。

[note_rbtree_insert.c](note_rbtree_insert.c)是另一类材料：实际调用 Linux rbtree 的内核模块，同目录 [Makefile](Makefile)分别构建插入、删除、遍历、替换四个模块。按[P26 五组插入](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P26_Linux红叶接入与插入修复.md#26.3.15_在内核模块中观察五组插入)准备匹配目标的内核构建环境；根与自动节点只在私有 run_case 中存活，无分配、外部注册或异步持有者。ARM 语法检查已完成，目标 Kbuild、装卸和实际日志仍待验证，不以宿主模型结果代替。


[note_rbtree_erase.c](note_rbtree_erase.c)对应[P11 取消请求](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P11_Linux_6.12_内核_rbtree_删除与缺黑修复.md#11.3.12_用完整模块观察取消请求)，用数组身份观察后继移位、黑叶修复以及删除与清标记的区别。ARM 语法和明确适配的宿主路径检查已完成，目标 Kbuild/装卸/日志未执行；没有分配、外部注册或共享读者。


[note_rbtree_walk.c](note_rbtree_walk.c)对应[P27 遍历与销毁](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P27_Linux有序遍历与整树销毁.md#27.2.9_运行完整遍历与销毁模块)：堆对象展示中序取消、后序回收及申请失败清理，自动对象重放 postorder 与 rb_erase 混用漏访。宿主适配和 ARM 语法已查，目标 Kbuild/装卸未执行。


[note_rbtree_replace.c](note_rbtree_replace.c)对应[P28 同键替换](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P28_Linux同键替换与旧对象退出.md#28.2.7_运行同键替换观察模块)：普通/cached 使用私有自动对象，RCU 使用单任务顺序重放和堆对象。检查旧字段、新 payload、缓存与退出后等待，不能作为并发或弱内存序证明。宿主适配和 ARM 语法已查，目标 Kbuild/装卸未执行。


[P29 旋转映射](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P29_普通旋转与Linux修复的完成边界.md#29.3_相同形状不等于相同中间状态)复用上述单旋程序、插入和删除模块：材料只能观察对应程序实际暴露的检查点，操作返回后的输出不能证明回调中间态或并发安全，无需拼接源码裁剪片段。


[tree234_search.c](tree234_search.c)在[P06 区间下行](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P06_2-3-4_树_从多路平衡到红黑树的结构桥梁.md#6.4.7_用完整C程序观察区间下行)使用同一十五键静态树，分别统计节点访问和参与比较的键数，以独立线性集合核对 0～80。它只接受已合法构造的只读树，不验证任意图、不执行插入删除，也不是页访问或内核性能测量。


[tree234_insert.cpp](tree234_insert.cpp)在[P30 插入实验](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P30_2-3-4树插入与分裂时机.md#30.3_运行完整的两种插入)中使用固定池和同一初始树，对比溢出后上推与下降前分裂。只读预检同时查重和计算新节点数，容量不足时保持原树；再观察新键本身被上推造成的不同合法形状。它是独占 C++17 内存模型，没有删除、动态扩容、内核 API 或共享读者。


[tree234_erase.cpp](tree234_erase.cpp)在[P31 预修复删除](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P31_2-3-4树预修复删除.md#31.3_运行完整预修复删除)中独立构建六组合法树，观察直接删除、左右借位、合并、前驱/后继与根收缩。固定池保有退休对象的存储，alive 与活动拓扑相对应；删除不申请资源，入口先查找以保证未命中不改形状。程序不含插入、池空槽复用或并发接口。


[tree234_erase_bottom.cpp](tree234_erase_bottom.cpp)在[P32 回溯删除](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P32_2-3-4树下溢回溯与根收缩.md#32.4_运行完整回溯删除)中允许暂时零键，并通过 deleted/underflow 返回修复结果；内部零键节点仍保留唯一孩子。六组独立场景包含两层合并与根收缩，池所有权沿用前例；未命中下降不写树，无须额外预查。它不提供并发、插入或节点槽复用。


[rbtree_height_balance.cpp](rbtree_height_balance.cpp)在[P33 黑高收支](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P33_从多路删除到红黑缺口.md#33.3_用黑高收支检查父层是否仍有缺口)中计算合并与远侄借位的局部高度。它只枚举 h=1～8 和父的两种颜色，检查算术推导；不执行指针旋转、不验证整棵红黑树或内核实现。


[page_index_model.cpp](page_index_model.cpp)在[P34 页请求模型](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P34_从多路节点到页级索引.md#34.3_运行页请求与未命中的计数模型)中读取固定根和三个叶页，分别统计逻辑请求与模型缓存未命中。物理块号只是标签，所有数据实际在宿主数组中；无设备 I/O、更新或缓存逐出。对范围/点查、热状态和根驻留的比较不能外推为真实数据库性能。

[vma_range_model.cpp](vma_range_model.cpp)在[P14 G/H 区间实例](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P14_Maple_Tree_与_VMA_管理.md#14.9.3_运行G与H的区间模型)中用 C++17 顺序容器观察精确查询、向后查询、相交、权限切分、取消映射和最低空洞。查询指针不能跨容器修改使用；不模拟内核的 VMA 合并、系统调用回滚、Maple 节点、RCU 或硬件性能。

[rbtree_invariants.cpp](rbtree_invariants.cpp)在[P07 性质实验](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P07_红黑树_把_2-3-4_树映射成二叉表示.md#7.3.9_让程序区分三种非法结构)中检查稳定对象组成的唯一键树，区分红红、黑高、祖先范围及重复节点，并报告到 NIL 的路径边数。它不执行修复，不识别悬空地址，不检查父指针或并发生命周期。

[rbtree_insert.cpp](rbtree_insert.cpp)在[P35 插入实验](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P35_红黑插入与红红冲突上推.md#35.3_运行完整插入程序)中完整实现唯一键的自底向上插入，以稳定地址容器拥有节点，观察左右镜像、内侧角色更新和多层染色上推。它是单线程教学树，不实现删除或 Linux 接口；重复键不改结构，分配成功后才写树槽。
