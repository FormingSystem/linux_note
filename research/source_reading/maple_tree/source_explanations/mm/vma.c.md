---
id: research.maple_tree.implementation.vma_detach
title: "mm/vma.c撤销映射控制"
kind: source
status: evolving
domains:
  - linux
  - memory
---

# 第1章\_mm/vma.c撤销映射控制

## 1.1\_版本与阅读任务

撤销一段地址需要先隔离对象，再清主索引，最后清页表与对象资源。本页核对这条调用链的成功分界和错误出口，尤其区分地址键与临时集合的处理序号。

上游相对位置 mm/vma.c，NXP 官方 Linux 6.12.20，固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob c9ddc06b672a5235eb7365d2197856537ce6d143。以[总索引](../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.1_从地址查询进入版本证据)进入[撤销范围模块](../../navigation/P10_撤销范围与临时索引.md#10.2_两棵树沿S0到S5分工)。以下中文 Doxygen 是仓库补充，函数体为固定版本代码。

## 1.2\_对齐撤销建立临时树

本函数接到已经确定的撤销地址范围及受保护的 VMA 游标。正常查询需要尽早不再看见这段映射，后续清理却还需要这些对象，所以先用局部序号树收集入口，再清地址主树；清主树成功是进入完成阶段的分界。

```c
/**
 * @brief 仓库补充阅读说明：S0 建局部序号树，S1 收集，S2 清主树；成功后进入不可返回原状的完成阶段。
 * @note 固定语句完整保留；不将本控制层的核对称为全部辅助实现验证。
 */
int do_vmi_align_munmap(struct vma_iterator *vmi, struct vm_area_struct *vma,
		struct mm_struct *mm, unsigned long start, unsigned long end,
		struct list_head *uf, bool unlock)
{
	struct maple_tree mt_detach;
	MA_STATE(mas_detach, &mt_detach, 0, 0);
	mt_init_flags(&mt_detach, vmi->mas.tree->ma_flags & MT_FLAGS_LOCK_MASK);
	mt_on_stack(mt_detach);
	struct vma_munmap_struct vms;
	int error;

	init_vma_munmap(&vms, vmi, vma, start, end, uf, unlock);
	error = vms_gather_munmap_vmas(&vms, &mas_detach);
	if (error)
		goto gather_failed;

	error = vma_iter_clear_gfp(vmi, start, end, GFP_KERNEL);
	if (error)
		goto clear_tree_failed;

	/* Point of no return */
	vms_complete_munmap_vmas(&vms, &mas_detach);
	return 0;

clear_tree_failed:
	reattach_vmas(&mas_detach);
gather_failed:
	validate_mm(mm);
	return error;
}
```

临时树只继承锁模式相关位，不复制原地址空间的根或全部属性。主树清除失败时 reattach 处理已收集对象，调用方锁仍按错误契约保留。

## 1.3\_收集与边界拆分

失败原因在此分开：-ENOMEM 用于映射数量限制或资源不足，-EPERM 用于当前 VMA 不允许修改。VM_LOCKED 标记的页数进入 locked_vm 账本，VM_ACCOUNT 标记的页数进入 nr_accounted；这些统计不等于当前仍有多少个 VMA。CONFIG_DEBUG_VM_MAPLE_TREE 则控制额外的边界及集合一致性检查，BUG_ON 不参与正常的树更新。

请求可能只覆盖首尾 VMA 的一部分。必须先把边界拆到请求位置，才能把待退出对象放入临时集合；每项成功存入后再标记 detached 并累计统计，防止把仅准备到一半的集合当成完成任务。

```c
/**
 * @brief 仓库补充阅读说明：S1 按需拆首尾，序号存入临时树后标记 detached；主树尚未被整体清空。
 * @note 固定语句完整保留；不将本控制层的核对称为全部辅助实现验证。
 */
int vms_gather_munmap_vmas(struct vma_munmap_struct *vms,
		struct ma_state *mas_detach)
{
	struct vm_area_struct *next = NULL;
	int error;

	/*
	 * If we need to split any vma, do it now to save pain later.
	 * Does it split the first one?
	 */
	if (vms->start > vms->vma->vm_start) {

		/*
		 * Make sure that map_count on return from munmap() will
		 * not exceed its limit; but let map_count go just above
		 * its limit temporarily, to help free resources as expected.
		 */
		if (vms->end < vms->vma->vm_end &&
		    vms->vma->vm_mm->map_count >= sysctl_max_map_count) {
			error = -ENOMEM;
			goto map_count_exceeded;
		}

		/* Don't bother splitting the VMA if we can't unmap it anyway */
		if (!can_modify_vma(vms->vma)) {
			error = -EPERM;
			goto start_split_failed;
		}

		error = __split_vma(vms->vmi, vms->vma, vms->start, 1);
		if (error)
			goto start_split_failed;
	}
	vms->prev = vma_prev(vms->vmi);
	if (vms->prev)
		vms->unmap_start = vms->prev->vm_end;

	/*
	 * Detach a range of VMAs from the mm. Using next as a temp variable as
	 * it is always overwritten.
	 */
	for_each_vma_range(*(vms->vmi), next, vms->end) {
		long nrpages;

		if (!can_modify_vma(next)) {
			error = -EPERM;
			goto modify_vma_failed;
		}
		/* Does it split the end? */
		if (next->vm_end > vms->end) {
			error = __split_vma(vms->vmi, next, vms->end, 0);
			if (error)
				goto end_split_failed;
		}
		vma_start_write(next);
		mas_set(mas_detach, vms->vma_count++);
		error = mas_store_gfp(mas_detach, next, GFP_KERNEL);
		if (error)
			goto munmap_gather_failed;

		vma_mark_detached(next, true);
		nrpages = vma_pages(next);

		vms->nr_pages += nrpages;
		if (next->vm_flags & VM_LOCKED)
			vms->locked_vm += nrpages;

		if (next->vm_flags & VM_ACCOUNT)
			vms->nr_accounted += nrpages;

		if (is_exec_mapping(next->vm_flags))
			vms->exec_vm += nrpages;
		else if (is_stack_mapping(next->vm_flags))
			vms->stack_vm += nrpages;
		else if (is_data_mapping(next->vm_flags))
			vms->data_vm += nrpages;

		if (unlikely(vms->uf)) {
			/*
			 * If userfaultfd_unmap_prep returns an error the vmas
			 * will remain split, but userland will get a
			 * highly unexpected error anyway. This is no
			 * different than the case where the first of the two
			 * __split_vma fails, but we don't undo the first
			 * split, despite we could. This is unlikely enough
			 * failure that it's not worth optimizing it for.
			 */
			error = userfaultfd_unmap_prep(next, vms->start,
						       vms->end, vms->uf);
			if (error)
				goto userfaultfd_error;
		}
#ifdef CONFIG_DEBUG_VM_MAPLE_TREE
		BUG_ON(next->vm_start < vms->start);
		BUG_ON(next->vm_start > vms->end);
#endif
	}

	vms->next = vma_next(vms->vmi);
	if (vms->next)
		vms->unmap_end = vms->next->vm_start;

#if defined(CONFIG_DEBUG_VM_MAPLE_TREE)
	/* Make sure no VMAs are about to be lost. */
	{
		MA_STATE(test, mas_detach->tree, 0, 0);
		struct vm_area_struct *vma_mas, *vma_test;
		int test_count = 0;

		vma_iter_set(vms->vmi, vms->start);
		rcu_read_lock();
		vma_test = mas_find(&test, vms->vma_count - 1);
		for_each_vma_range(*(vms->vmi), vma_mas, vms->end) {
			BUG_ON(vma_mas != vma_test);
			test_count++;
			vma_test = mas_next(&test, vms->vma_count - 1);
		}
		rcu_read_unlock();
		BUG_ON(vms->vma_count != test_count);
	}
#endif

	while (vma_iter_addr(vms->vmi) > vms->start)
		vma_iter_prev_range(vms->vmi);

	vms->clear_ptes = true;
	return 0;

userfaultfd_error:
munmap_gather_failed:
end_split_failed:
modify_vma_failed:
	reattach_vmas(mas_detach);
start_split_failed:
map_count_exceeded:
	return error;
}
```

关键两句是 mas_set(mas_detach,vma_count++) 与随后的 store：树键是处理序号，VMA 字段才是实际地址。失败分支恢复分离标记，不保证此前 split 全部逆转；固定 userfaultfd 错误注释明确保留这项限制。DEBUG 比较只在相应配置发生，没有相应配置的运行记录便不能称为已执行。

## 1.4\_从序号一继续页表处理

首项 VMA 已经由 vms->vma 作为参数传给下层，两次遍历都从临时树的序号 1 找后续项。处理地址仍从 start/end 和 VMA 边界取得；序号只决定访问顺序，不能作为页表地址。

```c
/**
 * @brief 仓库补充阅读说明：S3 首项经 vms->vma 交付，临时游标两次从一开始，分别服务映射与页表处理。
 * @note 固定语句完整保留；不将本控制层的核对称为全部辅助实现验证。
 */
static inline void vms_clear_ptes(struct vma_munmap_struct *vms,
		    struct ma_state *mas_detach, bool mm_wr_locked)
{
	struct mmu_gather tlb;

	if (!vms->clear_ptes) /* Nothing to do */
		return;

	/*
	 * We can free page tables without write-locking mmap_lock because VMAs
	 * were isolated before we downgraded mmap_lock.
	 */
	mas_set(mas_detach, 1);
	lru_add_drain();
	tlb_gather_mmu(&tlb, vms->vma->vm_mm);
	update_hiwater_rss(vms->vma->vm_mm);
	unmap_vmas(&tlb, mas_detach, vms->vma, vms->start, vms->end,
		   vms->vma_count, mm_wr_locked);

	mas_set(mas_detach, 1);
	/* start and end may be different if there is no prev or next vma. */
	free_pgtables(&tlb, mas_detach, vms->vma, vms->unmap_start,
		      vms->unmap_end, mm_wr_locked);
	tlb_finish_mmu(&tlb);
	vms->clear_ptes = false;
}
```

tree_end=vma_count 是序号上界；start/end 和 unmap_start/unmap_end 是地址边界。函数结束清 clear_ptes，TLB 完成职责由 tlb_finish_mmu 承担；其体系结构底层不在本页展开。

## 1.5\_完成账本与对象退出

READ_ONCE/WRITE_ONCE 在这里约束对 total_vm 的单次访问形式，不能把整个读减写变成原子扣减；扣账仍依赖外层锁约定。VM_WARN_ON 检查统计是否自洽，也不负责修正账本。

清除主树之后还剩统计、页表和对象资源。这里先根据 unlock 选择是否把写锁降为读锁，再处理页表与 TLB，最后逐个退出对象并销毁临时索引；nr_pages 为零的早退必须按实际分支单独读。

```c
/**
 * @brief 仓库补充阅读说明：S3 按请求降锁，S4 从零处理全部对象，S5 清临时树。
 * @note 固定语句完整保留；不将本控制层的核对称为全部辅助实现验证。
 */
void vms_complete_munmap_vmas(struct vma_munmap_struct *vms,
		struct ma_state *mas_detach)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm;

	mm = current->mm;
	mm->map_count -= vms->vma_count;
	mm->locked_vm -= vms->locked_vm;
	if (vms->unlock)
		mmap_write_downgrade(mm);

	if (!vms->nr_pages)
		return;

	vms_clear_ptes(vms, mas_detach, !vms->unlock);
	/* Update high watermark before we lower total_vm */
	update_hiwater_vm(mm);
	/* Stat accounting */
	WRITE_ONCE(mm->total_vm, READ_ONCE(mm->total_vm) - vms->nr_pages);
	/* Paranoid bookkeeping */
	VM_WARN_ON(vms->exec_vm > mm->exec_vm);
	VM_WARN_ON(vms->stack_vm > mm->stack_vm);
	VM_WARN_ON(vms->data_vm > mm->data_vm);
	mm->exec_vm -= vms->exec_vm;
	mm->stack_vm -= vms->stack_vm;
	mm->data_vm -= vms->data_vm;

	/* Remove and clean up vmas */
	mas_set(mas_detach, 0);
	mas_for_each(mas_detach, vma, ULONG_MAX)
		remove_vma(vma, /* unreachable = */ false);

	vm_unacct_memory(vms->nr_accounted);
	validate_mm(mm);
	if (vms->unlock)
		mmap_read_unlock(mm);

	__mt_destroy(mas_detach->tree);
}
```

本函数使用 current->mm，并依赖当前 MM 操作的调用约定，不能提取成任意地址空间的通用 helper。nr_pages 的早退分支原样保留；P42 周期展示已有映射的正常任务，未将所有空任务分支推断成相同资源过程。remove_vma 的对象退出与 __mt_destroy 的临时索引退出分开。

修改时沿[主树与临时树关系图](../../navigation/P10_撤销范围与临时索引.md#10.1_状态变量的类型不能告诉我们树键是什么)和 [S0～S5 时序](../../navigation/P10_撤销范围与临时索引.md#10.2_两棵树沿S0到S5分工)核对：收集失败的 reattach 与清主树失败的 reattach 不能遗漏或重复，S2 成功后不能继续走假定主树未清除的恢复。两次 mas_set(...,1) 与末次 mas_set(...,0) 分别配对首项参数和完整对象遍历。应覆盖首尾拆分、权限拒绝、资源失败、userfaultfd 失败、DEBUG 配置、unlock 两分支和零页数早退；区间模型只证明地址分区，不能替代这些内核路径的目标验证。
