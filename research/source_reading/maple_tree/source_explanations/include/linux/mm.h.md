---
id: research.maple_tree.implementation.mm_header
title: "include/linux/mm.h VMA 点查询"
kind: source
status: evolving
domains:
  - linux
  - memory
---

# 第1章\_include/linux/mm.h\_VMA点查询

## 1.1\_点查询的证据位置

上游位置为 include/linux/mm.h，NXP Linux 6.12.20 固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob 8617adc6becd1f9325e7217b885c4cc4124c5cc3。保存的[原始文件](../../../../linux/include/linux/mm.h)不混入阅读注释；回到[模块导读](../../../navigation/P02_范围契约与查询入口.md#2.3_按封装协作而不是名字猜语义)或[总索引](../../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)可对照另外两种查询。

## 1.2\_vma\_lookup只查询当前地址

以下 Doxygen 和行内中文为 **仓库补充阅读说明**，不是上游注释。函数体保持固定版本语句。

```c
/**
 * @brief 仓库补充：只取 addr 所在范围的 VMA。
 * @param mm 已取得适当使用保护的地址空间。
 * @param addr 用户虚拟地址索引。
 * @return 覆盖该地址的 VMA；空洞返回 NULL。
 * @note 不获取长期 VMA 引用，不负责验证访问权限。
 */
static inline
struct vm_area_struct *vma_lookup(struct mm_struct *mm, unsigned long addr)
{
    /* 使用共享范围树做点查询；对象使用期限仍由调用方保证。 */
    return mtree_load(&mm->mm_mt, addr);
}
```

这里只有一层封装：mm_mt 选择地址空间的索引，addr 是索引值，mtree_load 决定当前位置的 entry。它不会自行转到后面的非空 VMA，也不修改索引。G 的右端排除，所以查其终点时若 H 正好从此开始，结果为 H；若后面是空洞，则为空。

调用者尚未因此证明读、写或执行访问被允许。函数返回后继续读 vm_flags 等属性，需要相应的对象和字段保护。不能因为没有显式 mmap 锁断言，就推导出“任意无锁访问均合法”。

## 1.3\_VMA游标失效调用暂停

```c
/**
 * @brief 仓库补充阅读说明：将 VMA 包装中的 mas 交给暂停接口。
 * @note 保留固定版本语句；同步、业务对象和资源期限按调用契约建立。
 */
static inline void vma_iter_invalidate(struct vma_iterator *vmi)
{
	mas_pause(&vmi->mas);
}
```

本封装没有获取或释放 mmap 锁，没有销毁游标或取得 VMA 引用。后续使用 mas_find 时，pause 状态会影响继续起点，不能只解释成保留原地址从根重找。见[暂停与继续](../../../source_explanations/lib/maple_tree.c.md#1.9_暂停继续与有界find)及[游标导读](../../../navigation/P06_操作游标与暂停继续.md#6.2_沿一次遍历追踪状态)。
