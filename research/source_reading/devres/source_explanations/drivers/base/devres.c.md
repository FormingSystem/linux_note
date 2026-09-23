---
id: research.source_reading.devres.impl.devres
title: "devres核心登记与分组实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_devres核心登记与分组实现

上游位置`drivers/base/devres.c`，NXP Linux 6.12.20固定dfaf2136提交；[完整原文](../../../../linux/drivers/base/devres.c)。先读[模块状态导读](../../../navigation/P02_记录与分组清理导读.md#2.1_从记录地址追踪S0到S4)，再沿下面同一组S0～S4看具体函数。下列Doxygen阅读说明均为仓库补充，不是上游注释；源码仅抽取对应完整函数，在函数入口补中文阅读注释，未重写算法。

## 1.1\_普通action登记成功才转交责任

仓库补充Doxygen阅读说明：

```c
/**
 * @brief S0到S1：记录分配失败不执行action；成功写入函数与数据再登记。
 * @note 调用者须保证设备、组ID与回调参数期限；链锁不代替业务同步。
 */
```

```c
int __devm_add_action(struct device *dev, void (*action)(void *), void *data, const char *name)
{
	/* 仓库阅读注释：S0到S1：记录分配失败不执行action；成功写入函数与数据再登记。 */
	struct action_devres *devres;

	devres = __devres_alloc_node(devm_action_release, sizeof(struct action_devres),
				     GFP_KERNEL, NUMA_NO_NODE, name);
	if (!devres)
		return -ENOMEM;

	devres->data = data;
	devres->action = action;

	devres_add(dev, devres);
	return 0;
}
```

S0到S1：记录分配失败不执行action；成功写入函数与数据再登记。 具体状态读写者与范围比较回看模块导读；不要把函数名当作资源状态变化的全部证据。

## 1.2\_devres\_open\_group登记开始标记

仓库补充Doxygen阅读说明：

```c
/**
 * @brief S1：分配两标记的组结构，生成或保存ID，只登记开始标记并返回ID。
 * @note 调用者须保证设备、组ID与回调参数期限；链锁不代替业务同步。
 */
```

```c
void *devres_open_group(struct device *dev, void *id, gfp_t gfp)
{
	/* 仓库阅读注释：S1：分配两标记的组结构，生成或保存ID，只登记开始标记并返回ID。 */
	struct devres_group *grp;
	unsigned long flags;

	grp = kmalloc(sizeof(*grp), gfp);
	if (unlikely(!grp))
		return NULL;

	grp->node[0].release = &group_open_release;
	grp->node[1].release = &group_close_release;
	INIT_LIST_HEAD(&grp->node[0].entry);
	INIT_LIST_HEAD(&grp->node[1].entry);
	set_node_dbginfo(&grp->node[0], "grp<", 0);
	set_node_dbginfo(&grp->node[1], "grp>", 0);
	grp->id = grp;
	if (id)
		grp->id = id;
	grp->color = 0;

	spin_lock_irqsave(&dev->devres_lock, flags);
	add_dr(dev, &grp->node[0]);
	spin_unlock_irqrestore(&dev->devres_lock, flags);
	return grp->id;
}
```

S1：分配两标记的组结构，生成或保存ID，只登记开始标记并返回ID。 具体状态读写者与范围比较回看模块导读；不要把函数名当作资源状态变化的全部证据。

## 1.3\_devres\_close\_group限定范围

仓库补充Doxygen阅读说明：

```c
/**
 * @brief S1：找到有效未关闭组后登记结束标记，不调用资源回调。
 * @note 调用者须保证设备、组ID与回调参数期限；链锁不代替业务同步。
 */
```

```c
void devres_close_group(struct device *dev, void *id)
{
	/* 仓库阅读注释：S1：找到有效未关闭组后登记结束标记，不调用资源回调。 */
	struct devres_group *grp;
	unsigned long flags;

	spin_lock_irqsave(&dev->devres_lock, flags);

	grp = find_group(dev, id);
	if (grp)
		add_dr(dev, &grp->node[1]);
	else
		WARN_ON(1);

	spin_unlock_irqrestore(&dev->devres_lock, flags);
}
```

S1：找到有效未关闭组后登记结束标记，不调用资源回调。 具体状态读写者与范围比较回看模块导读；不要把函数名当作资源状态变化的全部证据。

## 1.4\_devres\_remove\_group只拿走标记

仓库补充Doxygen阅读说明：

```c
/**
 * @brief 保留资源责任：锁内摘掉两标记，锁外只释放组结构，不处理普通资源。
 * @note 调用者须保证设备、组ID与回调参数期限；链锁不代替业务同步。
 */
```

```c
void devres_remove_group(struct device *dev, void *id)
{
	/* 仓库阅读注释：保留资源责任：锁内摘掉两标记，锁外只释放组结构，不处理普通资源。 */
	struct devres_group *grp;
	unsigned long flags;

	spin_lock_irqsave(&dev->devres_lock, flags);

	grp = find_group(dev, id);
	if (grp) {
		list_del_init(&grp->node[0].entry);
		list_del_init(&grp->node[1].entry);
		devres_log(dev, &grp->node[0], "REM");
	} else
		WARN_ON(1);

	spin_unlock_irqrestore(&dev->devres_lock, flags);

	kfree(grp);
}
```

保留资源责任：锁内摘掉两标记，锁外只释放组结构，不处理普通资源。 这使组ID失效，但普通记录仍在设备链中；成功创建后可以撤掉阶段标记，不能用它完成失败回滚。

## 1.5\_devres\_release\_group摘取后回调

仓库补充Doxygen阅读说明：

```c
/**
 * @brief S3到S4：按组结束标记或设备链尾选择范围，摘到todo后锁外清理。
 * @note 调用者须保证设备、组ID与回调参数期限；链锁不代替业务同步。
 */
```

```c
int devres_release_group(struct device *dev, void *id)
{
	/* 仓库阅读注释：S3到S4：按组结束标记或设备链尾选择范围，摘到todo后锁外清理。 */
	struct devres_group *grp;
	unsigned long flags;
	LIST_HEAD(todo);
	int cnt = 0;

	spin_lock_irqsave(&dev->devres_lock, flags);

	grp = find_group(dev, id);
	if (grp) {
		struct list_head *first = &grp->node[0].entry;
		struct list_head *end = &dev->devres_head;

		if (!list_empty(&grp->node[1].entry))
			end = grp->node[1].entry.next;

		cnt = remove_nodes(dev, first, end, &todo);
		spin_unlock_irqrestore(&dev->devres_lock, flags);

		release_nodes(dev, &todo);
	} else {
		WARN_ON(1);
		spin_unlock_irqrestore(&dev->devres_lock, flags);
	}

	return cnt;
}
```

S3到S4：按组结束标记或设备链尾选择范围，摘到todo后锁外清理。 remove_nodes负责普通记录与合法嵌套组的选择，不在本节复制另一份算法；它返回非组资源数，函数返回该计数。

## 1.6\_devres\_release\_all交出待清理记录

仓库补充Doxygen阅读说明：

```c
/**
 * @brief S3到S4：所有已选记录从设备链移到本次todo，锁外完成回调。
 * @note 调用者须保证设备、组ID与回调参数期限；链锁不代替业务同步。
 */
```

```c
int devres_release_all(struct device *dev)
{
	/* 仓库阅读注释：S3到S4：所有已选记录从设备链移到本次todo，锁外完成回调。 */
	unsigned long flags;
	LIST_HEAD(todo);
	int cnt;

	/* Looks like an uninitialized device structure */
	if (WARN_ON(dev->devres_head.next == NULL))
		return -ENODEV;

	/* Nothing to release if list is empty */
	if (list_empty(&dev->devres_head))
		return 0;

	spin_lock_irqsave(&dev->devres_lock, flags);
	cnt = remove_nodes(dev, dev->devres_head.next, &dev->devres_head, &todo);
	spin_unlock_irqrestore(&dev->devres_lock, flags);

	release_nodes(dev, &todo);
	return cnt;
}
```

S3到S4：所有已选记录从设备链移到本次todo，锁外完成回调。 具体状态读写者与范围比较回看模块导读；不要把函数名当作资源状态变化的全部证据。

## 1.7\_release\_nodes执行实际清理

仓库补充Doxygen阅读说明：

```c
/**
 * @brief S4：逆序调用每项release，再释放记录；action的包装release再调用action(data)。
 * @note 调用者须保证设备、组ID与回调参数期限；链锁不代替业务同步。
 */
```

```c
static void release_nodes(struct device *dev, struct list_head *todo)
{
	/* 仓库阅读注释：S4：逆序调用每项release，再释放记录；action的包装release再调用action(data)。 */
	struct devres *dr, *tmp;

	/* Release.  Note that both devres and devres_group are
	 * handled as devres in the following loop.  This is safe.
	 */
	list_for_each_entry_safe_reverse(dr, tmp, todo, node.entry) {
		devres_log(dev, &dr->node, "REL");
		dr->node.release(dev, dr->data);
		kfree(dr);
	}
}
```

S4：逆序调用每项release，再释放记录；action的包装release再调用action(data)。 这里由当前调用者直接执行，无额外后台通知。账本锁已经在调用者中释放，不表示当前执行上下文一定允许任意睡眠。

返回[模块导读](../../../navigation/P02_记录与分组清理导读.md#2.2_同一周期内比较分组范围)或[总索引](../../../navigation/P01_Linux_6.12_devres源码阅读索引.md#1.2_按问题进入实现)。
