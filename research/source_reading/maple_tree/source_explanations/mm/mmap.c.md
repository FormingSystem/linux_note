---
id: research.maple_tree.implementation.mmap_queries
title: "mm/mmap.c VMA 范围查询"
kind: source
status: evolving
domains:
  - linux
  - memory
---

# 第1章\_mm/mmap.c\_VMA范围查询

## 1.1\_三个封装共享什么

上游位置 mm/mmap.c，NXP Linux 6.12.20 固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob 6183805f6f9e6ef1a6d3204834ff1c367d0376b1。查看[原始文件](../../../linux/mm/mmap.c)、[模块导读](../../navigation/P02_范围契约与查询入口.md#2.3_按封装协作而不是名字猜语义)和[总索引](../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)。

本页仅展开 VMA 层的小封装；mt_find 与游标内部的节点行走由 Maple 核心执行。下面 Doxygen 与行内中文均为 **仓库补充阅读说明**，函数体保持固定版本语句，省略原文件中不影响函数体的导出声明。

## 1.2\_find\_vma与上界

```c
/**
 * @brief 仓库补充：查当前地址，空洞则继续找其后的第一个 VMA。
 * @param mm 调用方已经按约定持有 mmap 锁的地址空间。
 * @param addr 搜索起点。
 * @return 当前或后续 VMA；不存在则为 NULL。
 * @note 锁断言不替调用者加锁。
 */
struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr)
{
    unsigned long index = addr; /* 本次调用拥有的可更新游标值。 */

    mmap_assert_locked(mm);
    return mt_find(&mm->mm_mt, &index, ULONG_MAX);
}
```

mt_find 从 index 所在范围向后找非空 entry，上界是 ULONG_MAX。若地址命中，返回该 VMA；若在 G 前的空洞中，返回 G。因此返回非空不等于 addr 已有映射，调用方仍需核对边界。index 是栈上局部变量，其改变不会修改传入地址，也不修改共享树。

## 1.3\_find\_vma\_intersection翻译排除式终点

```c
/**
 * @brief 仓库补充：查与半开范围相交的第一个 VMA。
 * @param mm 按约定持有 mmap 锁的地址空间。
 * @param start_addr 包含的起点。
 * @param end_addr 排除的终点；调用方保证 start_addr < end_addr。
 * @return 首个相交 VMA；没有则为 NULL。
 */
struct vm_area_struct *find_vma_intersection(struct mm_struct *mm,
                         unsigned long start_addr, unsigned long end_addr)
{
    unsigned long index = start_addr;

    mmap_assert_locked(mm);
    /* Maple 搜索上界包含，因此由合法半开终点减一。 */
    return mt_find(&mm->mm_mt, &index, end_addr - 1);
}
```

这里没有添加运行时的空区间检查。start_addr < end_addr 是接口前提；若擅自传 end_addr=0，再做无符号减一会得到最大值，不能期待它替你报告“空范围”。与前一函数相比，变化只在搜索终点；它不返回所有相交 VMA，也不裁剪或修改 VMA。

## 1.4\_find\_vma\_prev保持两个结果

这里的 VMA_ITERATOR 是声明并初始化局部游标的宏；后续 load、prev、next 都围绕该游标工作。先把命中结果另存到 vma，再移动游标，就不会把“原查询返回谁”与“前驱是谁”混成一个结果。

```c
/**
 * @brief 仓库补充：保留当前/下一个 VMA，并通过 pprev 给出前驱。
 * @param mm 受调用方外部 mmap 锁保护的地址空间。
 * @param addr 查询地址。
 * @param pprev 可写的前驱输出地址；无前驱时写入 NULL。
 * @return 当前地址对应或其后的 VMA；无后续对象时为 NULL。
 */
struct vm_area_struct *
find_vma_prev(struct mm_struct *mm, unsigned long addr,
             struct vm_area_struct **pprev)
{
    struct vm_area_struct *vma;
    VMA_ITERATOR(vmi, mm, addr); /* 局部操作状态，不是共享树节点。 */

    vma = vma_iter_load(&vmi);  /* 先保存当前位置的结果。 */
    *pprev = vma_prev(&vmi);    /* 游标移动后，原结果仍在 vma 中。 */
    if (!vma)
        vma = vma_next(&vmi);  /* 空洞情形继续定位后续对象。 */
    return vma;
}
```

VMA_ITERATOR 初始化局部游标并绑定 mm_mt；vma_iter_load 读当前位置，vma_prev 取得前一个 entry，vma_next 在原处没有对象时取得后续对象。这组游标操作可读取树而不改变共享映射集合。无后续 VMA 时返回 NULL 仍可能有非空 pprev；调用方必须分别解释两个结果。

上游注释明确说明这里依赖外部 mmap 锁，而没有另包一层 RCU 读锁。若只看到名字中的 prev 就丢弃函数返回值，会把“当前或后续”和“前驱”两类结果混淆。游标内部状态规则的进一步阅读仍由总索引关联 P15，本函数不能脱离其 API 契约自行改写调用顺序。
