---
id: research.source_reading.rbtree.types_implementation
title: "rbtree_types.h 节点与根的存储实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_rbtree\_types.h节点与根的存储实现

本页对应 [include/linux/rbtree_types.h](../../../../linux/include/linux/rbtree_types.h)，采用[总索引固定提交](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.1_固定提交与阅读边界)。先由[节点状态导读](../../../navigation/P07_节点布局与编码状态导读.md#7.1_先识别三个存储对象)定位角色，再读下面的定义。中文 Doxygen 与行内说明均为仓库补充，不属于上游原文。

## 1.1\_rb\_node的三个字段与对齐

```c
/**
 * struct rb_node - 仓库阅读说明：嵌入业务对象的树链接。
 * @__rb_parent_color: 父地址与颜色的打包值，不能直接作为普通父指针。
 * @rb_right: 右孩子的嵌入节点地址；空子树为 NULL。
 * @rb_left: 左孩子的嵌入节点地址；空子树为 NULL。
 * aligned 属性提供本实现需要的节点对齐，业务不得用打包布局破坏它。
 */
struct rb_node {
	unsigned long  __rb_parent_color;
	struct rb_node *rb_right;
	struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));
```

实现原理：节点中没有业务指针或通用 key。树沿孩子字段前进，业务层再用嵌入成员地址恢复外围对象。父指针和颜色共用一个整型存储位置；四字节对齐的地址低两位为零，取父时可掩去低两位。原注释提到 CRIS（Linux 曾支持的一种处理器架构）的对齐需求，但这不是在本页重新验证 CRIS ABI 的证据。

本次 ARM 目标前端检查得到三个四字节字段，共十二字节、四字节对齐。不能由宿主 Windows 的类型大小推定 Linux 布局，更不能把 Linux 的 unsigned long 指针打包原样搬到指针更宽的 LLP64 宿主执行。正文的[整数编码实验](../../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#8.4.3_为什么颜色可以使用指针低位存储)特意不构造真实宿主指针。

可修改性：改变字段布局会影响所有嵌入对象，改变对齐或打包格式会同时影响取父、测色和更新。不是修改一个结构体以后只重编本文件就能成立的局部优化。状态写入路径见导读 T1～T4。

## 1.2\_rb\_root保存外部入口槽

```c
/**
 * struct rb_root - 仓库阅读说明：普通树的入口，不拥有业务对象。
 * @rb_node: 当前根节点地址，空树为 NULL。
 */
struct rb_root {
	struct rb_node *rb_node;
};
```

这个槽位于根对象中，与根节点内部的父色字段不在同一地址。根旋转后必须改外部入口；仅将新根的父设为空，不会让旧入口自动变化。复制根结构只复制指针值，重置也只写这个槽；不复制节点、不增加引用、不回收任何对象。该边界在[根值实验](../../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#2%29_观察根值复制与对象存活)可以直接观察。

## 1.3\_rb\_root\_cached增加一个最左入口

```c
/**
 * struct rb_root_cached - 仓库阅读说明：普通根及最左节点缓存。
 * @rb_root: 普通搜索入口。
 * @rb_leftmost: 按调用者比较关系位于最左端的节点，空树时为 NULL。
 * 两个入口必须由匹配的插入、删除和替换路径共同维护。
 */
struct rb_root_cached {
	struct rb_root rb_root;
	struct rb_node *rb_leftmost;
};
```

缓存减少重复寻找左端点的下行访问，但增加了一个入口及更新责任。它没有增加节点颜色、没有改变红黑修复，也不是增广子树摘要。固定源码的注释把“不额外内建最右缓存”解释为空间成本与潜在用户数量的取舍；并不禁止具体调用者自行维护最右端点。

普通根的赋值不会触发缓存同步。删除或替换最左对象时若只调用普通接口，缓存可能保留错误地址；可从[替换模块的附加入口](../../../navigation/P06_同键替换与旧对象退出导读.md#6.3_附加入口与回收条件)追踪已经落地的路径。

## 1.4\_两个空根初始化器

```c
/**
 * RB_ROOT - 仓库阅读说明：普通根的空值表达式。
 * 可用于相应 C 上下文中的初始化或赋值，不遍历或释放原节点。
 */
#define RB_ROOT (struct rb_root) { NULL, }
/**
 * RB_ROOT_CACHED - 仓库阅读说明：普通入口与最左缓存都为空的值。
 * 这是整个缓存根的赋值来源，不能仅对内部普通根赋值后期待联动。
 */
#define RB_ROOT_CACHED (struct rb_root_cached) { {NULL, }, NULL }
```

括号中的类型与后面的初始化列表组成 C 复合字面量，外层对象接收其结构体值。它不调用分配函数，不能建立对象所有权。固定 C 定义与 C++ 扩展的存储期规则不要混用；块内 C 示例及完整展开推导见[教材根初始化单元](../../../../../../knowledge/linux/data_structures/红黑树_rb-tree/P08_Linux_6.12_内核_rbtree_基础结构与工程模型.md#1%29_Tip_RB_ROOT_对象式宏_把结构体默认初始化封装成可复用模板)。

返回[节点状态导读](../../../navigation/P07_节点布局与编码状态导读.md#7.2_沿一个节点的成员周期读写字段)或[总索引](../../../navigation/P01_Linux_6.12_rbtree源码阅读索引.md#1.2_按问题选择源码入口)。
