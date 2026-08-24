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
| [Lockdep 总阅读索引](../navigation/P01_Linux_6.12_Lockdep源码导读.md#1.1_基线与阅读目标) | Linux 6.12.20 源码地图和建议顺序 |
| [身份与事件接入模块导读](../navigation/P02_Linux_6.12_Lockdep身份与事件接入模块导读.md#2.1_模块问题) | 实例、key、锁类和初始化调用链 |
| [稳定机制：锁实例、锁类、key 与 subclass](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P03_锁实例_锁类_key与subclass.md#3.1_从动态对象规模推导锁类) | 为什么必须分类以及错误分类后果 |

源码基线：NXP `linux-imx`，标签 `lf-6.12.20-2.0.0`，提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，Linux 6.12.20。下列 Doxygen 和中文行内注释均为 **仓库补充，非上游原文**；代码省略不影响本文所述控制流。

## 1.2\_lock\_class\_key与lockdep\_map身份结构

**上游相对位置：** [`include/linux/lockdep_types.h`](../../linux/include/linux/lockdep_types.h)

```c
/**
 * @brief 为一组逻辑同类锁提供稳定身份，并为 subclass 预留子键。
 *
 * 仓库补充，非上游原文。动态分配的 key 必须先登记，并在释放
 * key 内存以前注销；常规初始化宏通常使用调用点静态 key。
 */
struct lock_class_key {
	union {
		struct hlist_node hash_entry; /* 动态key登记时使用。 */
		struct lockdep_subclass_key subkeys[MAX_LOCKDEP_SUBCLASSES];
	};
};

/**
 * @brief 嵌入具体锁实例，把实例关联到锁类身份和等待类型。
 *
 * 仓库补充，非上游原文。map不是功能锁状态，不保存mutex owner。
 */
struct lockdep_map {
	struct lock_class_key *key;
	struct lock_class *class_cache[NR_LOCKDEP_CACHING_CLASSES];
	const char *name;
	u8 wait_type_outer; /* 外部上下文允许怎样取得本锁。 */
	u8 wait_type_inner; /* 持有本锁后向内层呈现什么等待约束。 */
	u8 lock_type;
#ifdef CONFIG_LOCK_STAT
	int cpu;
	unsigned long ip;
#endif
};
```

**实现原理：** `key + subclass` 决定全局锁类节点；`class_cache[]` 把某个实例最近使用的低编号 subclass 缓存下来，避免每次 acquire 都查全局 class hash。`name` 用于诊断，不是身份本身。等待类型参与上下文合法性检查，但不改变实际锁如何等待。

关闭 `CONFIG_LOCKDEP` 时，上游把 `lock_class_key` 和 `lockdep_map` 定义为空结构，说明检查身份能够从非调试构建中消失；功能锁本身的 owner、wait list 或架构锁字仍保留。

## 1.3\_lockdep\_init\_map\_type与关闭配置分支

**上游相对位置：** [`include/linux/lockdep.h`](../../linux/include/linux/lockdep.h)、[`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

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

	if (subclass) {
		unsigned long flags;

		raw_local_irq_save(flags);
		lockdep_recursion_inc();
		register_lock_class(lock, subclass, 1); /* 非零subclass提前注册。 */
		lockdep_recursion_finish();
		raw_local_irq_restore(flags);
	}
}
```

**状态副作用：** map 的类缓存被清空，名称、key、等待类型和锁类型被写入；非零 subclass 还会进入锁类登记。函数不会取得功能锁，也不会给 current 增加 held record。

`CONFIG_LOCKDEP=n` 时，同名宏只保留对 `name`/`key` 的无害引用以避免编译告警，不创建任何锁类。关闭分支见 [`include/linux/lockdep.h`](../../linux/include/linux/lockdep.h) 的 `!CONFIG_LOCKDEP` 区域。

## 1.4\_register\_lock\_class锁类注册

**上游相对位置：** [`kernel/locking/lockdep.c`](../../linux/kernel/locking/lockdep.c)

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

	class = look_up_lock_class(lock, subclass);
	if (likely(class))
		goto out_set_class_cache;

	if (!lock->key) {
		if (!assign_lock_key(lock)) /* 仅持久静态对象可自动用自身地址。 */
			return NULL;
	} else if (!static_obj(lock->key) && !is_dynamic_key(lock->key)) {
		return NULL;
	}

	key = lock->key->subkeys + subclass;
	hash_head = classhashentry(key);

	if (!graph_lock())
		return NULL;
	/* 持锁后再次查找，封闭与其他CPU并发登记的窗口。 */
	hlist_for_each_entry_rcu(class, hash_head, hash_entry) {
		if (class->key == key)
			goto out_unlock_set;
	}

	class = list_first_entry_or_null(&free_lock_classes,
					 typeof(*class), lock_entry);
	if (!class) {
		if (!debug_locks_off_graph_unlock())
			return NULL;
		print_lockdep_off("BUG: MAX_LOCKDEP_KEYS too low!");
		return NULL; /* 容量耗尽后关闭检查，避免半写图。 */
	}

	nr_lock_classes++;
	__set_bit(class - lock_classes, lock_classes_in_use);
	class->key = key;
	class->name = lock->name;
	class->subclass = subclass;
	class->wait_type_inner = lock->wait_type_inner;
	class->wait_type_outer = lock->wait_type_outer;
	class->lock_type = lock->lock_type;
	hlist_add_head_rcu(&class->hash_entry, hash_head);
	list_move_tail(&class->lock_entry, &all_lock_classes);
	/* 其余统计、缓存和解锁代码省略。 */
}
```

**实现原理：** 首次无锁快速查找减少重复登记；未命中后在 `graph_lock` 下双检，避免两个 CPU 为同一 key 创建两个类。`assign_lock_key()` 只接受内核/模块 per-CPU 的规范地址或其他静态对象；无法确认持久性的临时对象会关闭检查器并要求调用者补正确初始化/注解。

**配置与容量边界：** `MAX_LOCKDEP_KEYS` 是固定 class 槽位数。耗尽时不是静默忽略一个类，而是报告并使 `debug_locks` 失效。诊断见[成本、覆盖边界与工程选择](../../../../knowledge/linux/synchronization_and_asynchrony/synchronization/lockdep/P09_成本_覆盖边界与工程选择.md#9.3_固定容量为何是证明前提)。

## 1.5\_实现核对表

| 核对点 | 应见到的证据 |
| --- | --- |
| 动态实例共享逻辑类 | 同一初始化调用点传入同一静态 key |
| 静态实例延迟分配 key | `assign_lock_key()` 只接受持久静态地址 |
| subclass 成为独立图节点 | 使用 `key->subkeys + subclass` |
| 实例查询仍可精确匹配 | held record 另存具体 `lockdep_map *instance` |
| 容量失败可观察 | `MAX_LOCKDEP_KEYS` 告警与 `debug_locks` 停检 |

下一篇：[Lockdep 取得释放与持锁账本源码实现](P02_Linux_6.12_Lockdep取得释放与持锁账本源码实现.md#2.1_关联入口)。
