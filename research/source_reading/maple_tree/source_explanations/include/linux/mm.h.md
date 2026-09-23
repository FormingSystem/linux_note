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

## 1.4\_VMA查找复用高级游标

```c
/**
 * @brief 仓库补充阅读说明：把 VMA 半开上界转换为 Maple 包含式上界，复用 vmi 中的状态。
 * @note 保留固定语句，调用者仍负责输入、上下文和保护协议。
 */
static inline
struct vm_area_struct *vma_find(struct vma_iterator *vmi, unsigned long max)
{
	return mas_find(&vmi->mas, max - 1);
}
```

调用者须提供有效非零的半开上界；无符号 max=0 再减一会变成 ULONG_MAX，函数没有在此拒绝。复用状态不等于建立 VMA 生命周期保护。与高级接口分工见[资源导读](../../../navigation/P08_写入准备与资源清理.md#8.1_先决定由谁持有请求与锁)。

## 1.5\_VMA方向与范围遍历

```c
/**
 * @brief 仓库补充阅读说明：首次需要包含当前位置，使用 find 而非直接 next。
 * @note 原样保留固定版本代码；外围锁和对象寿命由调用者建立。
 */
static inline struct vm_area_struct *vma_next(struct vma_iterator *vmi)
{
	/*
	 * Uses mas_find() to get the first VMA when the iterator starts.
	 * Calling mas_next() could skip the first entry.
	 */
	return mas_find(&vmi->mas, ULONG_MAX);
}
```

```c
/**
 * @brief 仓库补充阅读说明：推进到下一个范围槽，可以观察空范围，NULL 不独立证明越界。
 * @note 原样保留固定版本代码；外围锁和对象寿命由调用者建立。
 */
static inline
struct vm_area_struct *vma_iter_next_range(struct vma_iterator *vmi)
{
	return mas_next_range(&vmi->mas, ULONG_MAX);
}
```

```c
/**
 * @brief 仓库补充阅读说明：向前读取，以零为下界。
 * @note 原样保留固定版本代码；外围锁和对象寿命由调用者建立。
 */
static inline struct vm_area_struct *vma_prev(struct vma_iterator *vmi)
{
	return mas_prev(&vmi->mas, 0);
}
```

S2 包装选择方向和窗口，底层按状态推进位置。它们没有替调用者建立锁或对象引用；窗口翻译沿[既有 vma_find](mm.h.md#1.4_VMA查找复用高级游标)。

## 1.6\_VMA写入请求与资源退出

```c
/**
 * @brief 仓库补充阅读说明：依已有位置背景设置半开范围的闭区间形式并写 NULL，以状态映射错误。
 * @note 原样保留固定版本代码；外围锁和对象寿命由调用者建立。
 */
static inline int vma_iter_clear_gfp(struct vma_iterator *vmi,
			unsigned long start, unsigned long end, gfp_t gfp)
{
	__mas_set_range(&vmi->mas, start, end - 1);
	mas_store_gfp(&vmi->mas, NULL, gfp);
	if (unlikely(mas_is_err(&vmi->mas)))
		return -ENOMEM;

	return 0;
}
```

```c
/**
 * @brief 仓库补充阅读说明：范围来自 VMA 字段，使用 mas_store 并检查状态，而非以旧 entry 判成功。
 * @note 原样保留固定版本代码；外围锁和对象寿命由调用者建立。
 */
static inline int vma_iter_bulk_store(struct vma_iterator *vmi,
				      struct vm_area_struct *vma)
{
	vmi->mas.index = vma->vm_start;
	vmi->mas.last = vma->vm_end - 1;
	mas_store(&vmi->mas, vma);
	if (unlikely(mas_is_err(&vmi->mas)))
		return -ENOMEM;

	return 0;
}
```

```c
/**
 * @brief 仓库补充阅读说明：只释放当前状态资源，批量状态的后续责任沿 mas_destroy。
 * @note 原样保留固定版本代码；外围锁和对象寿命由调用者建立。
 */
static inline void vma_iter_free(struct vma_iterator *vmi)
{
	mas_destroy(&vmi->mas);
}
```

```c
/**
 * @brief 仓库补充阅读说明：重设查询起点，不更新共享映射。
 * @note 原样保留固定版本代码；外围锁和对象寿命由调用者建立。
 */
static inline void vma_iter_set(struct vma_iterator *vmi, unsigned long addr)
{
	mas_set(&vmi->mas, addr);
}
```

clear_gfp 与 bulk_store 不原样转发所有底层 errno，而将检测到的状态错误表示为 -ENOMEM。__mas_set_range 与直接赋值也不能视作任意位置上的普通写操作；调用者须满足状态、批量资源、锁与范围前提。start<end、end 的表示及生成它的加法溢出应在进入这些短封装前建立。S4 暂停见[既有 invalidate](mm.h.md#1.3_VMA游标失效调用暂停)，不是重复新增一份实现。回到[适配模块](../../../navigation/P09_VMA游标与边界适配.md#9.3_转发与边界检查各有责任)。
