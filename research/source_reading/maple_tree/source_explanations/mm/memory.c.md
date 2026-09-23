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

上游相对位置 mm/memory.c，NXP 官方 Linux 6.12.20，固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob 525f96ad65b8d77fe9d1feb5c7db1dc70d39647f。以[总索引](../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.1_从地址查询进入版本证据)进入[撤销范围模块](../../navigation/P10_撤销范围与临时索引.md#10.2_两棵树沿S0到S5分工)。以下中文 Doxygen 是仓库补充，函数体为固定版本代码。

## 1.2\_解除映射与继续遍历

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

固定注释允许 USER_PGTABLES_CEILING 为零，此处 ceiling-1 的回绕不等于非法空 VMA 请求。函数还先解除反向映射/文件关系，再组织页表释放；不代表全部业务对象在这里被释放。mm_wr_locked 影响是否调用 vma_start_write，不替代外层完整锁协议。体系结构页表实现及真实回收未在本批运行。
