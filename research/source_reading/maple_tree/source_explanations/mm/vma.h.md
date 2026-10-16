---
id: research.maple_tree.implementation.vma_detach_helpers
title: "mm/vma.h撤销任务初始化与恢复"
kind: source
status: evolving
domains:
  - linux
  - memory
---

# 第1章\_mm/vma.h撤销任务初始化与恢复

## 1.1\_版本与阅读任务

撤销任务既要记住请求地址，又要累计已收集对象及待扣账数量；失败恢复还需要知道哪些对象已经标记分离。本页核对局部任务如何初始化，以及恢复实际撤销了哪些状态。

上游相对位置 mm/vma.h，NXP 官方 Linux 6.12.20，固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob d58068c0ff2eaa38161c5bac2f27ee145ec1a2f6。以[总索引](../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.1_从地址查询进入版本证据)进入[撤销范围模块](../../navigation/P10_撤销范围与临时索引.md#10.2_两棵树沿S0到S5分工)。以下中文 Doxygen 是仓库补充，函数体为固定版本代码。

## 1.2\_初始化撤销任务

FIRST_USER_ADDRESS 与 USER_PGTABLES_CEILING 提供页表清理边界的初值，后续再由相邻 VMA 收紧；它们不是本次请求的 start/end。CONFIG_MMU 是采用内存管理单元的构建分支，不能把此分支上下文直接套到不使用 MMU 的实现。

本定义位于固定头文件的 CONFIG_MMU 条件内；当前 MMU 路径的上下文不能自动外推到 NOMMU。

```c
/**
 * @brief 仓库补充阅读说明：S0 分开请求地址、对象计数、账本、清理边界与任务标志。
 * @note 固定语句完整保留；不将本控制层的核对称为全部辅助实现验证。
 */
static inline void init_vma_munmap(struct vma_munmap_struct *vms,
		struct vma_iterator *vmi, struct vm_area_struct *vma,
		unsigned long start, unsigned long end, struct list_head *uf,
		bool unlock)
{
	vms->vmi = vmi;
	vms->vma = vma;
	if (vma) {
		vms->start = start;
		vms->end = end;
	} else {
		vms->start = vms->end = 0;
	}
	vms->unlock = unlock;
	vms->uf = uf;
	vms->vma_count = 0;
	vms->nr_pages = vms->locked_vm = vms->nr_accounted = 0;
	vms->exec_vm = vms->stack_vm = vms->data_vm = 0;
	vms->unmap_start = FIRST_USER_ADDRESS;
	vms->unmap_end = USER_PGTABLES_CEILING;
	vms->clear_ptes = false;
}
```

计数从零开始，unmap_start/end 的初值允许后续根据相邻 VMA 收紧页表释放边界；它们不是临时树序号。

## 1.3\_恢复分离标记而非逆转全部拆分

收集阶段可能已经把一部分对象标成 detached，但主地址索引还没有被整体清空。恢复需要取消这些标记并退出临时集合；它不需要凭序号重新建立地址映射，也不承诺把已经完成的边界拆分全部合回去。

```c
/**
 * @brief 仓库补充阅读说明：遍历已收集对象，取消 detached 标记，再销毁临时树。
 * @note 固定语句完整保留；不将本控制层的核对称为全部辅助实现验证。
 */
static inline void reattach_vmas(struct ma_state *mas_detach)
{
	struct vm_area_struct *vma;

	mas_set(mas_detach, 0);
	mas_for_each(mas_detach, vma, ULONG_MAX)
		vma_mark_detached(vma, false);

	__mt_destroy(mas_detach->tree);
}
```

此函数没有重新插入一套地址范围，也没有调用逆向合并。它用于主索引整体清除尚未成功的相应恢复路径；不能用函数名的 reattach 宣称全部对象、边界和页表回到最初形状。

初始化属于[撤销周期 S0](../../navigation/P10_撤销范围与临时索引.md#10.2_两棵树沿S0到S5分工)向局部 vms 写入的箭头，reattach 属于 S2 成功之前的错误分支。新增统计字段必须同时补初始化、成功累计和完成阶段扣账；改变恢复路径必须验证“只遍历已成功存入的对象、清标记后销毁临时树”，不可把它调用到已经清除主索引的状态上。CONFIG_MMU 分支与对象寿命前提都应随调用者一起验证。
