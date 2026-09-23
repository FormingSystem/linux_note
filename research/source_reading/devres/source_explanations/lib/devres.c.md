---
id: research.source_reading.devres.impl.mapping
title: "devres映射包装与返回值实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_devres映射包装与返回值实现

上游位置`lib/devres.c`，NXP Linux 6.12.20固定dfaf2136提交；[完整原文](../../../linux/lib/devres.c)。从[资源族模块导读](../../navigation/P03_内存映射与中断资源导读.md#3.2_映射与区域占用是两条责任)理解记录和区域两条责任，再读下面裁剪出的完整函数。Doxygen阅读说明及中文入口注释均为仓库补充，不是上游注释。

## 1.1\_基础映射失败保持NULL

参数type是映射选择枚举：DEVM_IOREMAP、DEVM_IOREMAP_UC、DEVM_IOREMAP_WC、DEVM_IOREMAP_NP分别选择ioremap、ioremap_uc、ioremap_wc、ioremap_np入口。下段只比较选择和失败传播，底层是否支持某种映射另受架构约束。GFP_KERNEL是此处普通内核分配标志，dev_to_node提供设备关联的内存节点；ptr指向将来保存I/O地址的记录数据区，addr才是准备返回给调用者的映射地址。

```c
/**
 * @brief 先分配记录，再尝试所选映射；任一步失败都保持NULL返回。
 * @note 成功时登记iounmap责任；不承担物理区域独占申请。
 */
```

```c
static void __iomem *__devm_ioremap(struct device *dev, resource_size_t offset,
				    resource_size_t size,
				    enum devm_ioremap_type type)
{
	/* 仓库阅读注释：记录失败与映射失败均返回NULL。 */
	void __iomem **ptr, *addr = NULL;

	ptr = devres_alloc_node(devm_ioremap_release, sizeof(*ptr), GFP_KERNEL,
				dev_to_node(dev));
	if (!ptr)
		return NULL;

	switch (type) {
	case DEVM_IOREMAP:
		addr = ioremap(offset, size);
		break;
	case DEVM_IOREMAP_UC:
		addr = ioremap_uc(offset, size);
		break;
	case DEVM_IOREMAP_WC:
		addr = ioremap_wc(offset, size);
		break;
	case DEVM_IOREMAP_NP:
		addr = ioremap_np(offset, size);
		break;
	}

	if (addr) {
		*ptr = addr;
		devres_add(dev, ptr);
	} else
		devres_free(ptr);

	return addr;
}
```

公开devm_ioremap传DEVM_IOREMAP进入这里。ptr是记录数据区，成功后把实际I/O地址保存到其中；基础映射失败时只释放尚未登记的记录。返回值addr没有经过错误指针编码，调用者应先判NULL。回调devm_ioremap_release后来从记录中取回地址执行iounmap。

## 1.2\_资源包装将失败编码并撤回区域

res是资源描述，resource_type检查其是否为IORESOURCE_MEM内存区域；IORESOURCE_MEM_NONPOSTED是影响映射选择的标志。IOMEM_ERR_PTR把负错误值编码成带I/O地址类型注解的返回值，不能对该错误值执行寄存器访问。EINVAL、ENOMEM与EBUSY在这里分别承担无效资源、内存/映射失败及区间申请失败的错误分类。

```c
/**
 * @brief 检查资源，取得区间使用权后再建立映射，错误统一编码返回。
 * @note 映射失败撤回区间，但更早登记的名字分配仍由设备账本管理。
 */
```

```c
static void __iomem *
__devm_ioremap_resource(struct device *dev, const struct resource *res,
			enum devm_ioremap_type type)
{
	/* 仓库阅读注释：此包装的错误表示不同于基础映射。 */
	resource_size_t size;
	void __iomem *dest_ptr;
	char *pretty_name;
	int ret;

	BUG_ON(!dev);

	if (!res || resource_type(res) != IORESOURCE_MEM) {
		ret = dev_err_probe(dev, -EINVAL, "invalid resource %pR\n", res);
		return IOMEM_ERR_PTR(ret);
	}

	if (type == DEVM_IOREMAP && res->flags & IORESOURCE_MEM_NONPOSTED)
		type = DEVM_IOREMAP_NP;

	size = resource_size(res);

	if (res->name)
		pretty_name = devm_kasprintf(dev, GFP_KERNEL, "%s %s",
					     dev_name(dev), res->name);
	else
		pretty_name = devm_kstrdup(dev, dev_name(dev), GFP_KERNEL);
	if (!pretty_name) {
		ret = dev_err_probe(dev, -ENOMEM, "can't generate pretty name for resource %pR\n", res);
		return IOMEM_ERR_PTR(ret);
	}

	if (!devm_request_mem_region(dev, res->start, size, pretty_name)) {
		ret = dev_err_probe(dev, -EBUSY, "can't request region for resource %pR\n", res);
		return IOMEM_ERR_PTR(ret);
	}

	dest_ptr = __devm_ioremap(dev, res->start, size, type);
	if (!dest_ptr) {
		devm_release_mem_region(dev, res->start, size);
		ret = dev_err_probe(dev, -ENOMEM, "ioremap failed for resource %pR\n", res);
		return IOMEM_ERR_PTR(ret);
	}

	return dest_ptr;
}
```

资源类型无效、名字分配失败、区域申请失败和基础映射NULL分别进入明确错误返回。只有区域已申请而映射失败的分支调用devm_release_mem_region，不能对从未取得的区域进行回滚。成功分支保留区域和映射各自的托管记录。

IORESOURCE_MEM_NONPOSTED改变所选映射变体；是否支持所需页表属性与底层架构相关，本函数的选择逻辑不是硬件能力证明。调用者如何检查返回值见[API参考](../../../../../knowledge/linux/object_lifetime/devres/devres_API说明.md#2.3_I/O_资源与寄存器映射)，11条宿主包装检查的替身边界见[模块验证说明](../../navigation/P03_内存映射与中断资源导读.md#3.5_已执行验证与未覆盖范围)。返回[总索引](../../navigation/P01_Linux_6.12_devres源码阅读索引.md#1.2_按问题进入实现)。
