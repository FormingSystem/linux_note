---
id: research.maple_tree.implementation.memory_unmap
title: "mm/memory.c映射与页表清理"
kind: source
status: evolving
domains:
  - linux
  - memory
---

# 第1章\_mm/memory.c映射与页表清理

## 1.1\_版本与阅读任务

主索引已经撤下待处理 VMA，页表和相关映射却还没有因此完成退出。本页核对下层如何接收首项、继续遍历临时集合，并按真实地址组织解除映射和页表释放。

上游相对位置 mm/memory.c，NXP 官方 Linux 6.12.20，固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob 525f96ad65b8d77fe9d1feb5c7db1dc70d39647f。以[总索引](../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.1_从地址查询进入版本证据)进入[撤销范围模块](../../navigation/P10_撤销范围与临时索引.md#10.2_两棵树沿S0到S5分工)。以下中文 Doxygen 是仓库补充，函数体为固定版本代码。

## 1.2\_解除映射与继续遍历

details 是本次解除映射的局部参数：even_cows 要求也处理私有写时复制页，ZAP_FLAG_DROP_MARKER 要求丢弃页表项的特殊标记，ZAP_FLAG_UNMAP 表示最终解除映射，供 hugetlb 路径使用。MMU_NOTIFY_UNMAP 是通知事件类型，使其他维护映射的参与者围绕这一地址范围失效；它不等于本处理器的 TLB 刷新动作。

临时集合已经给出了首项对象，但下层还需要知道处理哪段虚拟地址，以及去哪个索引中找下一项。参数因此同时存在地址上界和树搜索上界。先沿传入的 vma 处理首项，再用 mas 继续，避免误把序号当地址。

```c
/**
 * @brief 仓库补充阅读说明：先处理传入首项，再按 mas 的树和 tree_end 找后续；处理地址来自 VMA 与参数。
 * @note 固定语句完整保留；不将本控制层的核对称为全部辅助实现验证。
 */
void unmap_vmas(struct mmu_gather *tlb, struct ma_state *mas,
		struct vm_area_struct *vma, unsigned long start_addr,
		unsigned long end_addr, unsigned long tree_end,
		bool mm_wr_locked)
{
	struct mmu_notifier_range range;
	struct zap_details details = {
		.zap_flags = ZAP_FLAG_DROP_MARKER | ZAP_FLAG_UNMAP,
		/* Careful - we need to zap private pages too! */
		.even_cows = true,
	};

	mmu_notifier_range_init(&range, MMU_NOTIFY_UNMAP, 0, vma->vm_mm,
				start_addr, end_addr);
	mmu_notifier_invalidate_range_start(&range);
	do {
		unsigned long start = start_addr;
		unsigned long end = end_addr;
		hugetlb_zap_begin(vma, &start, &end);
		unmap_single_vma(tlb, vma, start, end, &details,
				 mm_wr_locked);
		hugetlb_zap_end(vma, &details);
		vma = mas_find(mas, tree_end - 1);
	} while (vma && likely(!xa_is_zero(vma)));
	mmu_notifier_invalidate_range_end(&range);
}
```

S3 调用方可传临时序号树，也可传地址空间树。tree_end 属于搜索索引域，start_addr/end_addr 是待解除映射地址；不要因同为 unsigned long 就交换。通知开始/结束和 hugetlb 分支原样保留，未把 notifier 当成 TLB 刷新的同义词。

## 1.3\_释放页表与上界哨兵

PMD_SIZE 是当前体系结构中页表中间目录一级所覆盖的地址跨度，参与判断是否合并处理邻近 VMA；本页不把它固定写成某个字节常量。USER_PGTABLES_CEILING 则给出用户页表清理上界，零值按下方源码注释作为特殊边界解释。

解除叶映射之后，还要回收不再需要的页表结构；相邻仍有效的映射会限制能够释放的范围。函数先解除 VMA 的反向映射和文件关联，再按相邻 VMA 与 floor/ceiling 组织释放；把这些步骤缩成“删掉树节点”会漏掉实际资源责任。

```c
/**
 * @brief 仓库补充阅读说明：按 VMA 边界释放页表，处理邻近合并与特殊页；ceiling 为零在此有显式约定。
 * @note 固定语句完整保留；不将本控制层的核对称为全部辅助实现验证。
 */
void free_pgtables(struct mmu_gather *tlb, struct ma_state *mas,
		   struct vm_area_struct *vma, unsigned long floor,
		   unsigned long ceiling, bool mm_wr_locked)
{
	struct unlink_vma_file_batch vb;

	do {
		unsigned long addr = vma->vm_start;
		struct vm_area_struct *next;

		/*
		 * Note: USER_PGTABLES_CEILING may be passed as ceiling and may
		 * be 0.  This will underflow and is okay.
		 */
		next = mas_find(mas, ceiling - 1);
		if (unlikely(xa_is_zero(next)))
			next = NULL;

		/*
		 * Hide vma from rmap and truncate_pagecache before freeing
		 * pgtables
		 */
		if (mm_wr_locked)
			vma_start_write(vma);
		unlink_anon_vmas(vma);

		if (is_vm_hugetlb_page(vma)) {
			unlink_file_vma(vma);
			hugetlb_free_pgd_range(tlb, addr, vma->vm_end,
				floor, next ? next->vm_start : ceiling);
		} else {
			unlink_file_vma_batch_init(&vb);
			unlink_file_vma_batch_add(&vb, vma);

			/*
			 * Optimization: gather nearby vmas into one call down
			 */
			while (next && next->vm_start <= vma->vm_end + PMD_SIZE
			       && !is_vm_hugetlb_page(next)) {
				vma = next;
				next = mas_find(mas, ceiling - 1);
				if (unlikely(xa_is_zero(next)))
					next = NULL;
				if (mm_wr_locked)
					vma_start_write(vma);
				unlink_anon_vmas(vma);
				unlink_file_vma_batch_add(&vb, vma);
			}
			unlink_file_vma_batch_final(&vb);
			free_pgd_range(tlb, addr, vma->vm_end,
				floor, next ? next->vm_start : ceiling);
		}
		vma = next;
	} while (vma);
}
```

固定注释允许 USER_PGTABLES_CEILING 为零，此处 ceiling-1 的回绕不等于非法空 VMA 请求。函数还先解除反向映射/文件关系，再组织页表释放；不代表全部业务对象在这里被释放。mm_wr_locked 影响是否调用 vma_start_write，不替代外层完整锁协议。这里没有体系结构页表实现及真实回收的运行证据。

两函数都位于[撤销周期 S3 的页表处理箭头](../../navigation/P10_撤销范围与临时索引.md#10.2_两棵树沿S0到S5分工)，共享的首项参数和私有 mas 由调用者安排。修改遍历时需保留首项一次处理、后续上界单位、特殊零 entry 过滤和通知开始/结束配对；修改页表释放时还要核对 hugetlb、相邻区域合并及 ceiling=0 哨兵。页表具体层级和刷新实现属于体系结构分支，控制层的静态核对不能证明这些底层已运行。
