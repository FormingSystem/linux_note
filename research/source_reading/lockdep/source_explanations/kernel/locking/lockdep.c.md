---
id: research.source_reading.lockdep.linux_6_12_identity_class_implementation
title: "Linux 6.12 Lockdep 身份与锁类源码实现"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
topics:
  - locking
  - lockdep
---

# 第1章\_Linux\_6.12\_Lockdep身份与锁类源码实现

## 1.1\_关联入口

| 入口 | 本文提供的实现证据 |
| --- | --- |
| [Lockdep 总阅读索引](../../../navigation/P01_Linux_6.12_Lockdep源码导读.md#1.1_基线与阅读目标) | Linux 6.12.20 源码地图和建议顺序 |
| [身份与事件接入模块导读](../../../navigation/P02_Linux_6.12_Lockdep身份与事件接入模块导读.md#2.1_模块问题) | 实例、key、锁类和初始化调用链 |
| [稳定机制：锁实例、锁类、key 与 subclass](../../../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P03_锁实例_锁类_key与subclass.md#3.1_从动态对象规模推导锁类) | 为什么必须分类以及错误分类后果 |

源码基线：NXP `linux-imx`，标签 `lf-6.12.20-2.0.0`，提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，Linux 6.12.20。下列 Doxygen 和中文行内注释均为 **仓库补充，非上游原文**；代码省略不影响本文所述控制流。

## 1.2\_身份类型入口

子键、key与map的唯一类型讲解位于[lockdep_types.h](../../include/linux/lockdep_types.h.md#1.2_lock_class_key与lockdep_map身份结构)。本文件继续解释上游kernel/locking/lockdep.c中的初始化与登记路径，不复制头文件类型。

## 1.3\_lockdep\_init\_map\_type与关闭配置分支

**上游相对位置：** [`include/linux/lockdep.h`](../../../../linux/include/linux/lockdep.h)、[`kernel/locking/lockdep.c`](../../../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 初始化一个锁实例到锁类的映射信息。
 *
 * 仓库补充，非上游原文。
 * @param lock     具体实例内嵌的dep_map。
 * @param name     诊断名称，不能为NULL。
 * @param key      持久静态key或已登记的动态key。
 * @param subclass 初始子类编号。
 * @param inner    向内层呈现的等待类型。
 * @param outer    外部允许的等待类型。
 * @param lock_type 锁类型标签。
 */
void lockdep_init_map_type(struct lockdep_map *lock, const char *name,
			   struct lock_class_key *key, int subclass,
			   u8 inner, u8 outer, u8 lock_type)
{
	int i;

	for (i = 0; i < NR_LOCKDEP_CACHING_CLASSES; i++)
		lock->class_cache[i] = NULL; /* 重新初始化必须清除旧类缓存。 */

#ifdef CONFIG_LOCK_STAT
	lock->cpu = raw_smp_processor_id(); /* 统计配置记录初始化CPU。 */
#endif

	if (DEBUG_LOCKS_WARN_ON(!name)) {
		lock->name = "NULL";
		return;
	}
	lock->name = name;
	lock->wait_type_outer = outer;
	lock->wait_type_inner = inner;
	lock->lock_type = lock_type;

	if (DEBUG_LOCKS_WARN_ON(!key))
		return;
	if (!static_obj(key) && !is_dynamic_key(key)) {
		if (debug_locks)
			printk(KERN_ERR "BUG: key %px has not been registered!\n", key);
		DEBUG_LOCKS_WARN_ON(1);
		return;
	}
	lock->key = key;

	if (unlikely(!debug_locks))
		return; /* 写入map不等于全局检查器仍能继续登记。 */

	if (subclass) {
		unsigned long flags;

		if (DEBUG_LOCKS_WARN_ON(!lockdep_enabled()))
			return;

		raw_local_irq_save(flags);
		lockdep_recursion_inc();
		register_lock_class(lock, subclass, 1); /* 非零subclass提前注册。 */
		lockdep_recursion_finish();
		raw_local_irq_restore(flags);
	}
}
```

**状态副作用：** map 的类缓存被清空，名称、key、等待类型和锁类型被写入；启用统计时还记录初始化CPU。非零 subclass 只有通过检查器有效性与入口检查后才进入锁类登记。函数不会取得功能锁，也不会给 current 增加 held record。

初始化不是事务式提交。先清缓存，再处理名称和类型，再验证key；某个后续检查失败并不会自动恢复前面已写字段。尤其在写入key以后，`debug_locks` 已关闭会直接返回，不能看到map字段有值就断言锁类已经登记。`subclass=0` 的通常路径不在这里强制注册；非零子类路径先要求 `lockdep_enabled()`，随后保存并关闭本地IRQ，增加检查递归计数，注册，再结束递归并恢复原IRQ状态。递归计数约束检查器自身的重入，不是功能mutex的嵌套次数。

修改这里的次序时，应同时检查名称为空、key为空、动态key未登记、检查器已停检、非零子类入口受抑制和正常初始化这些出口。`void`返回值不提供业务可用性判据；诊断失败也不能用来替代功能锁初始化协议。本段已恢复固定版本的全部可执行语句，省略的只是上游英文说明注释。

`CONFIG_LOCKDEP=n` 时，同名宏只保留对 `name`/`key` 的无害引用以避免编译告警，不创建任何锁类。关闭分支见 [`include/linux/lockdep.h`](../../../../linux/include/linux/lockdep.h) 的 `!CONFIG_LOCKDEP` 区域。

## 1.4\_register\_lock\_class锁类注册

**上游相对位置：** [`kernel/locking/lockdep.c`](../../../../linux/kernel/locking/lockdep.c)

```c
/**
 * @brief 查找或登记map在指定subclass下的全局锁类。
 *
 * 仓库补充，非上游原文。调用时本地IRQ已关闭；真正修改全局
 * class hash和类列表还要持有graph_lock。
 * @return 成功时返回锁类；身份非法、容量耗尽或检查器失效时返回NULL。
 */
static struct lock_class *
register_lock_class(struct lockdep_map *lock, unsigned int subclass, int force)
{
	struct lockdep_subclass_key *key;
	struct hlist_head *hash_head;
	struct lock_class *class;
	int idx;

	/* 仓库补充：入口要求本地中断已关闭。 */
	DEBUG_LOCKS_WARN_ON(!irqs_disabled());

	class = look_up_lock_class(lock, subclass);
	if (likely(class))
		goto out_set_class_cache;

	if (!lock->key) {
		if (!assign_lock_key(lock))
			return NULL;
	} else if (!static_obj(lock->key) && !is_dynamic_key(lock->key)) {
		return NULL;
	}

	key = lock->key->subkeys + subclass;
	hash_head = classhashentry(key);

	if (!graph_lock()) {
		return NULL;
	}
	/*
	 * We have to do the hash-walk again, to avoid races
	 * with another CPU:
	 */
	hlist_for_each_entry_rcu(class, hash_head, hash_entry) {
		if (class->key == key)
			goto out_unlock_set;
	}

	/* 仓库补充：首次使用时建立空闲类等全局结构。 */
	init_data_structures_once();

	/* Allocate a new lock class and add it to the hash. */
	class = list_first_entry_or_null(&free_lock_classes, typeof(*class),
					 lock_entry);
	if (!class) {
		if (!debug_locks_off_graph_unlock()) {
			return NULL;
		}

		nbcon_cpu_emergency_enter();
		print_lockdep_off("BUG: MAX_LOCKDEP_KEYS too low!");
		dump_stack();
		nbcon_cpu_emergency_exit();
		return NULL;
	}
	nr_lock_classes++;
	__set_bit(class - lock_classes, lock_classes_in_use);
	debug_atomic_inc(nr_unused_locks);
	class->key = key;
	class->name = lock->name;
	class->subclass = subclass;
	WARN_ON_ONCE(!list_empty(&class->locks_before));
	WARN_ON_ONCE(!list_empty(&class->locks_after));
	class->name_version = count_matching_names(class);
	class->wait_type_inner = lock->wait_type_inner;
	class->wait_type_outer = lock->wait_type_outer;
	class->lock_type = lock->lock_type;
	/*
	 * We use RCU's safe list-add method to make
	 * parallel walking of the hash-list safe:
	 */
	hlist_add_head_rcu(&class->hash_entry, hash_head);
	/*
	 * Remove the class from the free list and add it to the global list
	 * of classes.
	 */
	list_move_tail(&class->lock_entry, &all_lock_classes);
	idx = class - lock_classes;
	if (idx > max_lock_class_idx)
		max_lock_class_idx = idx;

	if (verbose(class)) {
		graph_unlock();

		nbcon_cpu_emergency_enter();
		printk("\nnew class %px: %s", class->key, class->name);
		if (class->name_version > 1)
			printk(KERN_CONT "#%d", class->name_version);
		printk(KERN_CONT "\n");
		dump_stack();
		nbcon_cpu_emergency_exit();

		if (!graph_lock()) {
			return NULL;
		}
	}
/* 仓库补充：锁内命中与新建类都从这里释放图锁。 */
out_unlock_set:
	graph_unlock();

/* 仓库补充：force可把非零子类写入默认缓存槽。 */
out_set_class_cache:
	if (!subclass || force)
		lock->class_cache[0] = class;
	else if (subclass < NR_LOCKDEP_CACHING_CLASSES)
		lock->class_cache[subclass] = class;

	/*
	 * Hash collision, did we smoke some? We found a class with a matching
	 * hash but the subclass -- which is hashed in -- didn't match.
	 */
	if (DEBUG_LOCKS_WARN_ON(class->subclass != subclass))
		return NULL;

	return class;
}

```

**实现原理：** 首次无锁快速查找减少重复登记；未命中后在 `graph_lock` 下双检，避免两个 CPU 为同一 key 创建两个类。`assign_lock_key()` 只接受内核/模块 per-CPU 的规范地址或其他静态对象；无法确认持久性的临时对象会关闭检查器并要求调用者补正确初始化/注解。

这段函数现保留完整上游控制流；不能把尾部解锁、缓存和返回统称为“统计代码”省略。按R0至R4追踪它：

| 阶段 | 入口与状态变化 | 谁继续读取 |
| --- | --- | --- |
| R0 | 当前CPU已关本地IRQ，查找现有类；命中直接转缓存出口 | 当前调用者 |
| R1 | 身份有效后取得图锁，在类哈希桶中复查 | 与其他CPU串行化的新类登记 |
| R2 | 初始化全局结构，从free_lock_classes取槽，设置key/名称/类型和占用位 | 类哈希与全局类列表的读者 |
| R3 | 用RCU链表操作发布哈希节点，把槽移到all_lock_classes，更新最大索引 | 后续取得路径及诊断统计 |
| R4 | 释放图锁，写实例class_cache，核对subclass并返回 | 本实例后续取得及当前调用者 |

R1的锁内命中直接进入R4，不重复分配。R2无空闲槽时，`debug_locks_off_graph_unlock()` 同时承担停检与释放图锁，随后输出容量故障，返回NULL。调用者不能把这个NULL解释成“锁类不存在但检查仍然完整”。R3的verbose分支是另一个容易漏看的出口：它先解锁再打印，打印后重新取得图锁；重新加锁失败也返回NULL，因此不能在抽取源码时删去它再声称控制流未变。

```mermaid
sequenceDiagram
    autonumber
    participant A as 登记CPU
    participant H as 全局类哈希与空闲槽
    participant B as 其他CPU
    participant M as 实例class_cache
    A->>H: R0 快速查找
    alt 已有类
        H-->>A: 返回类地址
    else 未命中
        B->>H: 可能先登记同一子键
        A->>H: R1 取得图锁后再次查找
        alt 仍未找到且有空闲槽
            A->>H: R2 初始化槽，R3发布类
        else 已有类
            H-->>A: 复用类，不再分配
        else 容量耗尽
            A->>H: 停检并解锁，返回NULL
        end
        A->>H: 成功路径R4释放图锁
    end
    A->>M: 成功路径R4写缓存并检查子类
```

缓存出口还解释了参数 `force`：subclass为0或force非零时写 `class_cache[0]`；其他可缓存的子类写其编号对应槽。不能将这里概括成“所有子类总写同编号槽”。`name_version` 用于区分同名类的诊断显示，`nr_lock_classes`、占用位及最大索引帮助管理全局槽位；它们不改变由子键决定的身份。`verbose`、`nbcon_cpu_emergency_enter/exit` 和打印调用服务诊断输出，其中重新加锁分支影响返回结果，所以源码保留原样。

**可修改性边界：** 改缓存策略时应同时核对force调用者、子类编号与查找端；改发布顺序时必须保持槽初始化先于哈希发布、空闲列表到全局列表的唯一归属以及全部错误出口解锁。验证应包含首次登记、已有类命中、两CPU竞争同一子键和容量失败；本节没有声称这些目标运行测试已经执行。

**配置与容量边界：** `MAX_LOCKDEP_KEYS` 是固定 class 槽位数。耗尽时不是静默忽略一个类，而是报告并使 `debug_locks` 失效。诊断见[成本、覆盖边界与工程选择](../../../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P09_成本_覆盖边界与工程选择.md#9.3_固定容量为何是证明前提)。

## 1.5\_实现核对表

| 核对点 | 应见到的证据 |
| --- | --- |
| 动态实例共享逻辑类 | 同一初始化调用点传入同一静态 key |
| 静态实例延迟分配 key | `assign_lock_key()` 只接受持久静态地址 |
| subclass 成为独立图节点 | 使用 `key->subkeys + subclass` |
| 实例查询仍可精确匹配 | held record 另存具体 `lockdep_map *instance` |
| 容量失败可观察 | `MAX_LOCKDEP_KEYS` 告警与 `debug_locks` 停检 |

下一篇：[Lockdep 取得释放与持锁账本源码实现](../../P02_Linux_6.12_Lockdep取得释放与持锁账本源码实现.md#2.1_关联入口)。
