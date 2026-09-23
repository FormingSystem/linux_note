---
id: research.maple_tree.navigation.index
title: "Linux 6.12 Maple 范围源码阅读索引"
kind: source
status: evolving
domains:
  - linux
  - data_structures
  - memory
---

# 第1章\_Linux\_6.12\_Maple范围源码阅读索引

## 1.1\_从地址查询进入版本证据

给出地址 0x50010000，为什么 VMA 的排除式终点不属于前一段，而 Maple pivot 相等时又留在同号槽？给出空洞地址，为什么两个看似“查 VMA”的接口返回不同结果？这里围绕这些问题组织固定版本的阅读入口。

知识模型见 [P14 Maple 与 VMA](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P14_Maple_Tree_与_VMA_管理.md#14.1_一个地址为什么需要三种查询)。本索引负责版本、源码位置和模块关系；原始证据来自 NXP 官方 linux-imx 的 Linux 6.12.20，标签 lf-6.12.20-2.0.0，提交 dfaf2136deb2af2e60b994421281ba42f1c087e0。具体 blob 与配置界限见 [SOURCE_BASELINE](../../linux/SOURCE_BASELINE.md#1.28_Maple范围与VMA查询证据)。

## 1.2\_按读者问题进入证据

目前先沿[范围契约与查询入口](P02_范围契约与查询入口.md#2.1_同一地址可以提出不同问题)区分半开区间、闭区间与查询任务，再连接节点容量、调用方游标和返回指针的期限；随后用[树对象与模式](P03_树对象与模式选择.md#3.2_从未发布到受保护使用)追踪 R0～R3、初始化、实际持锁和内部节点退休。

| 阅读任务 | 唯一实现入口 |
| --- | --- |
| 共享树如何初始化、外部锁登记是否加锁 | [树根、模式与初始化](../source_explanations/include/linux/maple_tree.h.md#1.3_初始化先选择锁模式再建立空根)及[VMA 模式](../source_explanations/include/linux/mm_types.h.md#1.2_VMA树的三项模式) |
| RCU 模式怎样影响退休节点 | [mas_free](../source_explanations/lib/maple_tree.c.md#1.2_退休节点根据模式选择去向) |
| 当前地址没有对象时是否返回 NULL | [vma_lookup](../source_explanations/include/linux/mm.h.md#1.2_vma_lookup只查询当前地址) |
| 空洞之后继续找，范围上界在哪里 | [find_vma 与相交查询](../source_explanations/mm/mmap.c.md#1.2_find_vma与上界) |
| 同时带回前一个 VMA | [find_vma_prev](../source_explanations/mm/mmap.c.md#1.4_find_vma_prev保持两个结果) |

这些小封装没有完成 Maple 的节点搜索或分裂。完整 Maple API 与状态的进一步阅读由[已有 P15](../../../../knowledge/linux/data_structures/红黑树_rb-tree/P15_Linux_6.12_Maple_Tree_源码结构与_API_分层.md#15.1_本章涉及的源码文件)组织；各实现文件分别承担已经核对的查询封装、树根模式及节点分派，不把尚未展开的内层算法列成已完成模块。节点与完整游标路径仍在后续审查范围内。

## 1.3\_核对次序

先在 mm_types.h 找到共享 mm_mt 与 VMA 边界；再读 mm.h 中查询封装；最后读 mmap.c 的向后查询、相交查询和前驱组合。转到 maple_tree.h 时先核对构建条件与 pivot 的包含规则，不从结构体名字推断平台位宽。

本轮只读源码及运行独立 C++ 区间模型，没有编译或运行目标 Maple 实现，没有执行真实 mmap/mprotect/munmap。源码语义、宿主模型和目标行为是三种不同证据。
