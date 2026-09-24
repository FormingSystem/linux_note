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

## 1.6\_图锁与检查入口的自保护

类登记依赖的图锁也是共享同步，但不能通过普通被检查锁再次递归进入自己。上游在同一文件定义架构锁`__lock`和拥有者指针`__owner`；另以每CPU的`lockdep_recursion`和当前任务同名字段控制不同重入路径。两个递归计数属于不同存储，不能混作一项。

```c
/* 仓库补充：这两项全局状态只服务检查器自身的串行化。 */
static arch_spinlock_t __lock = (arch_spinlock_t)__ARCH_SPIN_LOCK_UNLOCKED;
static struct task_struct *__owner;

/**
 * @brief 判断全局检查、每CPU递归和当前任务递归是否允许入口继续。
 * 仓库补充，非上游原文。
 */
static __always_inline bool lockdep_enabled(void)
{
	if (!debug_locks)
		return false;

	if (this_cpu_read(lockdep_recursion))
		return false;

	if (current->lockdep_recursion)
		return false;

	return true;
}

/**
 * @brief 先增加每CPU递归保护，再取得不经普通Lockdep注解的架构锁。
 * 仓库补充，非上游原文。
 */
static inline void lockdep_lock(void)
{
	DEBUG_LOCKS_WARN_ON(!irqs_disabled());

	__this_cpu_inc(lockdep_recursion);
	arch_spin_lock(&__lock);
	__owner = current;
}

/**
 * @brief 验证拥有者，清除记录后释放架构锁并退出每CPU递归保护。
 * 仓库补充，非上游原文。
 */
static inline void lockdep_unlock(void)
{
	DEBUG_LOCKS_WARN_ON(!irqs_disabled());

	if (debug_locks && DEBUG_LOCKS_WARN_ON(__owner != current))
		return;

	__owner = NULL;
	arch_spin_unlock(&__lock);
	__this_cpu_dec(lockdep_recursion);
}

/**
 * @brief 取得底层图锁后复查debug_locks，失败时先解锁再返回。
 * 仓库补充，非上游原文。
 */
static int graph_lock(void)
{
	lockdep_lock();
	/*
	 * Make sure that if another CPU detected a bug while
	 * walking the graph we dont change it (while the other
	 * CPU is busy printing out stuff with the graph lock
	 * dropped already)
	 */
	if (!debug_locks) {
		lockdep_unlock();
		return 0;
	}
	return 1;
}

/**
 * @brief 释放图锁，继承底层解锁的IRQ与拥有者要求。
 * 仓库补充，非上游原文。
 */
static inline void graph_unlock(void)
{
	lockdep_unlock();
}

/**
 * @brief 先关闭检查，再释放图锁，并返回是否由本次完成停检。
 * 仓库补充，非上游原文。
 */
static inline int debug_locks_off_graph_unlock(void)
{
	int ret = debug_locks_off();

	lockdep_unlock();

	return ret;
}
```

`lockdep_enabled()`先读全局`debug_locks`，再分别读本CPU和current的递归计数，任一条件不允许就返回false。它本身不加锁，也不改变功能锁；初始化非零子类的调用点用它拒绝不合适的递归入口。

`lockdep_lock()`要求调用方已关本地IRQ，然后增加本CPU递归计数，再通过`arch_spin_lock()`争用`__lock`，成功后才写`__owner=current`。关IRQ防止同CPU普通中断重入，架构锁串行化不同CPU，两者不能互换。架构锁的具体指令由目标架构实现，此处只使用其排他与释放契约，不复制架构代码。

`graph_lock()`取得锁后才复查全局生命状态，堵住“等待图锁期间另一CPU已经停检”的窗口。若关闭，它自行解锁并返回0，调用者此时不再拥有图锁；若返回1，调用者必须在成功或失败出口配对释放。容量失败使用`debug_locks_off_graph_unlock()`，因此不能再额外解锁一次。

解锁正常路径先清`__owner`，再释放架构锁，最后减少本CPU递归计数；拥有者不匹配且检查仍有效时会诊断并提前返回。这是内部不变量失败，不能把它当作普通可恢复返回值继续假设已经解锁。`DEBUG_LOCKS_WARN_ON`承担诊断，`unlikely`只表达分支预测提示，`__always_inline`/`inline`是编译器内联指示；都不是功能mutex的状态字段。

**修改约束：** 保留IRQ前提、递归计数配对、owner写入顺序与graph_lock返回值的锁所有权含义。用三条路径核对每次修改：正常登记后解锁，等待期间他CPU停检导致graph_lock返回0，当前CPU容量失败并停检解锁。不能因为一次初始化未告警就推断这三条都执行过。

## 1.7\_类查找与静态身份补全

登记先调用下面的查找函数；它返回NULL既可能表示尚未登记，也可能表示输入或检查条件错误。因此register_lock_class还要分别检查身份与全局生命状态，不能把每个NULL都当成可以直接分配新类。

```c
/**
 * @brief 识别内核、模块与允许阶段的初始化区静态对象地址。
 * 仓库补充，非上游原文。
 */
static int static_obj(const void *obj)
{
	unsigned long addr = (unsigned long) obj;

	if (is_kernel_core_data(addr))
		return 1;

	/*
	 * keys are allowed in the __ro_after_init section.
	 */
	if (is_kernel_rodata(addr))
		return 1;

	/*
	 * in initdata section and used during bootup only?
	 * NOTE: On some platforms the initdata section is
	 * outside of the _stext ... _end range.
	 */
	if (system_state < SYSTEM_FREEING_INITMEM &&
		init_section_contains((void *)addr, 1))
		return 1;

	/*
	 * in-kernel percpu var?
	 */
	if (is_kernel_percpu_address(addr))
		return 1;

	/*
	 * module static or percpu var?
	 */
	return is_module_address(addr) || is_module_percpu_address(addr);
}

/**
 * @brief 按子键地址查询全局类，并检查子类边界和IRQ前提。
 * 仓库补充，非上游原文。
 */
static noinstr struct lock_class *
look_up_lock_class(const struct lockdep_map *lock, unsigned int subclass)
{
	struct lockdep_subclass_key *key;
	struct hlist_head *hash_head;
	struct lock_class *class;

	if (unlikely(subclass >= MAX_LOCKDEP_SUBCLASSES)) {
		instrumentation_begin();
		debug_locks_off();
		nbcon_cpu_emergency_enter();
		printk(KERN_ERR
			"BUG: looking up invalid subclass: %u\n", subclass);
		printk(KERN_ERR
			"turning off the locking correctness validator.\n");
		dump_stack();
		nbcon_cpu_emergency_exit();
		instrumentation_end();
		return NULL;
	}

	/*
	 * If it is not initialised then it has never been locked,
	 * so it won't be present in the hash table.
	 */
	if (unlikely(!lock->key))
		return NULL;

	/*
	 * NOTE: the class-key must be unique. For dynamic locks, a static
	 * lock_class_key variable is passed in through the mutex_init()
	 * (or spin_lock_init()) call - which acts as the key. For static
	 * locks we use the lock object itself as the key.
	 */
	BUILD_BUG_ON(sizeof(struct lock_class_key) >
			sizeof(struct lockdep_map));

	key = lock->key->subkeys + subclass;

	hash_head = classhashentry(key);

	/*
	 * We do an RCU walk of the hash, see lockdep_free_key_range().
	 */
	if (DEBUG_LOCKS_WARN_ON(!irqs_disabled()))
		return NULL;

	hlist_for_each_entry_rcu_notrace(class, hash_head, hash_entry) {
		if (class->key == key) {
			/*
			 * Huh! same key, different name? Did someone trample
			 * on some memory? We're most confused.
			 */
			WARN_ONCE(class->name != lock->name &&
				  lock->key != &__lockdep_no_validate__,
				  "Looking for class \"%s\" with key %ps, but found a different class \"%s\" with the same key\n",
				  lock->name, lock->key, class->name);
			return class;
		}
	}

	return NULL;
}

/**
 * @brief 为缺省key的静态锁取得规范地址，拒绝未标注的非静态对象。
 * 仓库补充，非上游原文。
 */
static bool assign_lock_key(struct lockdep_map *lock)
{
	unsigned long can_addr, addr = (unsigned long)lock;

#ifdef __KERNEL__
	/*
	 * lockdep_free_key_range() assumes that struct lock_class_key
	 * objects do not overlap. Since we use the address of lock
	 * objects as class key for static objects, check whether the
	 * size of lock_class_key objects does not exceed the size of
	 * the smallest lock object.
	 */
	BUILD_BUG_ON(sizeof(struct lock_class_key) > sizeof(raw_spinlock_t));
#endif

	if (__is_kernel_percpu_address(addr, &can_addr))
		lock->key = (void *)can_addr;
	else if (__is_module_percpu_address(addr, &can_addr))
		lock->key = (void *)can_addr;
	else if (static_obj(lock))
		lock->key = (void *)lock;
	else {
		/* Debug-check: all keys must be persistent! */
		debug_locks_off();
		nbcon_cpu_emergency_enter();
		pr_err("INFO: trying to register non-static key.\n");
		pr_err("The code is fine but needs lockdep annotation, or maybe\n");
		pr_err("you didn't initialize this object before use?\n");
		pr_err("turning off the locking correctness validator.\n");
		dump_stack();
		nbcon_cpu_emergency_exit();
		return false;
	}

	return true;
}
```

`static_obj()`判定的是地址归属，不是承诺对象永不释放。普通内核数据、只读数据、per-CPU区以及模块静态区各有各的存续协议；初始化区只有在`system_state < SYSTEM_FREEING_INITMEM`时可通过对应检查。模块卸载仍需走key/类清理，不能因为一个地址今天被识别为静态就永久保存引用。各地址分类辅助函数由内核/模块/per-CPU设施提供，本节使用其地址归属契约，不展开架构地址布局。

`look_up_lock_class()`先限制subclass，再处理尚无key的情形。哈希桶由子键地址计算，桶内还比较完整子键指针，哈希碰撞不等于同类。遍历使用RCU的notrace入口并要求本地IRQ关闭；它不取得图锁，删除端必须与这种读者协调。`noinstr`及instrumentation_begin/end用于控制插桩区域，不能把它们当成可以随意删除的装饰。名称不一致会告警，但找到的类仍按key返回，名称不是分类键。

`assign_lock_key()`只处理未显式提供key的对象。per-CPU实例先转换为去掉CPU偏移的规范地址，从而让各CPU副本共享逻辑身份；其他认可的静态map用自身地址。普通动态对象不能走这个兜底，否则每个分配地址都会意外变成不同类，释放重用还会使身份失真。失败路径停检、输出初始化/注解提示并返回false，调用者必须停止该次登记。

两个BUILD_BUG_ON检查的是编译期尺寸关系，防止用对象地址充当key时潜在范围重叠假设失效；它们不在运行时给对象分配额外内存。WARN_ONCE及printk/dump_stack属于诊断，KERN_ERR/KERN_CONT标明日志级别或续行，nbcon配对限定紧急输出区。修改地址分类或身份兜底时，必须同时复核动态key注册/注销和模块释放协议，不能只让一个新地址通过static_obj。

## 1.8\_动态key的登记与撤销

普通动态锁对象通常共用调用点静态key；只有key本身也动态分配时，才需要下面的登记协议。动态key哈希保存的是允许作为身份的地址，并不代表当前持有某把功能锁。调用方仍须在撤销之前阻止新的业务使用，并保证没有仍依赖该身份的活动对象。

```c
/**
 * @brief 将动态key加入允许使用的身份哈希；拒绝静态地址与重复登记。
 * 仓库补充，非上游原文。
 */
void lockdep_register_key(struct lock_class_key *key)
{
	struct hlist_head *hash_head;
	struct lock_class_key *k;
	unsigned long flags;

	if (WARN_ON_ONCE(static_obj(key)))
		return;
	hash_head = keyhashentry(key);

	raw_local_irq_save(flags);
	if (!graph_lock())
		goto restore_irqs;
	hlist_for_each_entry_rcu(k, hash_head, hash_entry) {
		if (WARN_ON_ONCE(k == key))
			goto out_unlock;
	}
	hlist_add_head_rcu(&key->hash_entry, hash_head);
out_unlock:
	graph_unlock();
restore_irqs:
	raw_local_irq_restore(flags);
}

/**
 * @brief 检查动态key是否已登记；停检时避免遍历可能失效的哈希。
 * 仓库补充，非上游原文。
 */
static bool is_dynamic_key(const struct lock_class_key *key)
{
	struct hlist_head *hash_head;
	struct lock_class_key *k;
	bool found = false;

	if (WARN_ON_ONCE(static_obj(key)))
		return false;

	/*
	 * If lock debugging is disabled lock_keys_hash[] may contain
	 * pointers to memory that has already been freed. Avoid triggering
	 * a use-after-free in that case by returning early.
	 */
	if (!debug_locks)
		return true;

	hash_head = keyhashentry(key);

	rcu_read_lock();
	hlist_for_each_entry_rcu(k, hash_head, hash_entry) {
		if (k == key) {
			found = true;
			break;
		}
	}
	rcu_read_unlock();

	return found;
}

/**
 * @brief 移除动态key及相关类历史，并等待查key读者退出。
 * 仓库补充，非上游原文。
 */
void lockdep_unregister_key(struct lock_class_key *key)
{
	struct hlist_head *hash_head = keyhashentry(key);
	struct lock_class_key *k;
	struct pending_free *pf;
	unsigned long flags;
	bool found = false;
	bool need_callback = false;

	might_sleep();

	if (WARN_ON_ONCE(static_obj(key)))
		return;

	raw_local_irq_save(flags);
	lockdep_lock();

	hlist_for_each_entry_rcu(k, hash_head, hash_entry) {
		if (k == key) {
			hlist_del_rcu(&k->hash_entry);
			found = true;
			break;
		}
	}
	WARN_ON_ONCE(!found && debug_locks);
	if (found) {
		pf = get_pending_free();
		__lockdep_free_key_range(pf, key, 1);
		need_callback = prepare_call_rcu_zapped(pf);
	}
	lockdep_unlock();
	raw_local_irq_restore(flags);

	if (need_callback)
		call_rcu(&delayed_free.rcu_head, free_zapped_rcu);

	/* Wait until is_dynamic_key() has finished accessing k->hash_entry. */
	synchronize_rcu();
}
```

登记D0先排除静态对象，再关本地IRQ并取得图锁；D1在`lock_keys_hash`对应桶查重，使用`hlist_add_head_rcu()`发布节点。恢复IRQ发生在解锁之后，失败取得图锁也走恢复出口。返回类型是void，不是供业务判断功能锁能否使用的成功码。

查询D2用RCU读侧保护桶遍历，比较的是key指针。一个刻意的退化分支是`debug_locks=0`时直接返回true：上游注释指出停检后的哈希可能含已释放地址，因此不再遍历。这不是重新证明key有效；后续主路径仍受停检控制。把这个true用作业务对象仍存活的依据会跨越诊断边界。

撤销D3先通过`might_sleep()`表达上下文要求，然后使用底层`lockdep_lock()`，而不是停检后拒绝进入的`graph_lock()`。因此即使此前停检，它仍尝试移除已登记key。找到节点后用RCU删除，将相关类的清理交给待释放结构；未找到时只在检查有效条件下告警。退出共享临界区并恢复IRQ之后，按需排队类清理回调，最后D4调用`synchronize_rcu()`等待`is_dynamic_key()`中可能仍访问该哈希节点的读者。

```mermaid
sequenceDiagram
    autonumber
    participant O as key所有者
    participant H as 动态key哈希
    participant R as is_dynamic_key读者
    participant P as 待释放类历史
    O->>H: D0/D1 图锁内查重并发布key
    R->>H: D2 RCU保护下遍历
    O->>H: D3 底层图锁内删除节点
    O->>P: 标记相关类清理，必要时排队RCU回调
    O->>R: D4 synchronize_rcu等待旧查询退出
    R-->>O: 旧读侧结束，等待可以返回
    Note over O: 等待查key读者不等于排空所有业务使用
```

`get_pending_free()`选择本轮延迟清理的容器，`__lockdep_free_key_range()`处理关联类，`prepare_call_rcu_zapped()`决定是否需要排队，`free_zapped_rcu()`执行后续清理；这些是类池回收簇，不在本节复制其函数体。这里必须保留的契约是“哈希撤销在锁内、回调排队在恢复IRQ后、等待查key读者在返回前”。`call_rcu()`排队与`synchronize_rcu()`完成并非同一事件，不能把末尾等待描述成已经执行完所有类回调。

**修改约束与练习：** 为何注销不用graph_lock？停检后仍需清理登记，不能被普通新事件的生命状态门控直接拒绝。为何不能删掉末尾等待后马上释放key？已有RCU读者可能仍访问key内嵌的hash_entry。为何调用注销前还要排空业务？该函数不会替驱动停止新请求、取消工作或销毁功能锁。类池与回调回收的内部算法继续作为独立源码审查范围，本节不宣称已证明整个模块卸载协议。

## 1.9\_双缓冲待回收批次与槽位归还

注销把身份从查询入口摘除以后，旧RCU读者仍可能拿着类或链地址。不能马上把固定数组槽交给新类，否则旧读者看到的同一地址会突然变成另一身份。这里用两份pending_free把“正在收集待回收对象”和“已经封闭、正在等宽限期”分开。

```c
/* 仓库补充：zapped记录类，位图记录待归还的链槽。 */
struct pending_free {
	struct list_head zapped;
	DECLARE_BITMAP(lock_chains_being_freed, MAX_LOCKDEP_CHAINS);
};

static struct delayed_free {
	struct rcu_head		rcu_head;
	int			index;
	int			scheduled;
	struct pending_free	pf[2];
} delayed_free;

/**
 * @brief 清除类的身份及后续字段，同时保留列表骨架。
 * 仓库补充，非上游原文。
 */
static void reinit_class(struct lock_class *class)
{
	WARN_ON_ONCE(!class->lock_entry.next);
	WARN_ON_ONCE(!list_empty(&class->locks_after));
	WARN_ON_ONCE(!list_empty(&class->locks_before));
	memset_startat(class, 0, key);
	WARN_ON_ONCE(!class->lock_entry.next);
	WARN_ON_ONCE(!list_empty(&class->locks_after));
	WARN_ON_ONCE(!list_empty(&class->locks_before));
}

/**
 * @brief 取得当前仍开放的待回收批次；调用者持有图锁。
 * 仓库补充，非上游原文。
 */
static struct pending_free *get_pending_free(void)
{
	return delayed_free.pf + delayed_free.index;
}

/**
 * @brief 封闭当前批次并切换收集槽，决定是否安排回调。
 * 仓库补充，非上游原文。
 */
static bool prepare_call_rcu_zapped(struct pending_free *pf)
{
	WARN_ON_ONCE(inside_selftest());

	if (list_empty(&pf->zapped))
		return false;

	if (delayed_free.scheduled)
		return false;

	delayed_free.scheduled = true;

	WARN_ON_ONCE(delayed_free.pf + delayed_free.index != pf);
	delayed_free.index ^= 1;

	return true;
}

/**
 * @brief 回调安全点归还类槽和链槽；调用者持有图锁。
 * 仓库补充，非上游原文。
 */
static void __free_zapped_classes(struct pending_free *pf)
{
	struct lock_class *class;

	check_data_structures();

	list_for_each_entry(class, &pf->zapped, lock_entry)
		reinit_class(class);

	list_splice_init(&pf->zapped, &free_lock_classes);

#ifdef CONFIG_PROVE_LOCKING
	bitmap_andnot(lock_chains_in_use, lock_chains_in_use,
		      pf->lock_chains_being_freed, ARRAY_SIZE(lock_chains));
	bitmap_clear(pf->lock_chains_being_freed, 0, ARRAY_SIZE(lock_chains));
#endif
}

/**
 * @brief 回收封闭批次，检查新批次并按需继续排队。
 * 仓库补充，非上游原文。
 */
static void free_zapped_rcu(struct rcu_head *ch)
{
	struct pending_free *pf;
	unsigned long flags;
	bool need_callback;

	if (WARN_ON_ONCE(ch != &delayed_free.rcu_head))
		return;

	raw_local_irq_save(flags);
	lockdep_lock();

	/* closed head */
	pf = delayed_free.pf + (delayed_free.index ^ 1);
	__free_zapped_classes(pf);
	delayed_free.scheduled = false;
	need_callback =
		prepare_call_rcu_zapped(delayed_free.pf + delayed_free.index);
	lockdep_unlock();
	raw_local_irq_restore(flags);

	/*
	* If there's pending free and its callback has not been scheduled,
	* queue an RCU callback.
	*/
	if (need_callback)
		call_rcu(&delayed_free.rcu_head, free_zapped_rcu);

}
```

批次F0由`delayed_free.index`指向开放的`pf[index]`，删除路径在图锁内把类放进它的zapped链表。F1中prepare函数发现非空且没有已安排回调，置scheduled并翻转index；旧批次随即封闭，新删除项进入另一槽。返回true的调用者必须在退出共享锁后真正调用call_rcu，不能只置位而漏掉排队。

F2回调进入时先验证rcu_head地址，再关IRQ并取得底层图锁。`index ^ 1`选中封闭批次；F3逐类重置，并把链表整体接回free_lock_classes。在完整证明配置下，位图同时释放本批的链槽占用位，再清空待回收位图。memset_startat从key字段开始清零，因此lock_class字段次序是回收不变量，不能任意重排；前面的链表骨架需保留，前后依赖应已移除。

F4清scheduled后查看目前开放的批次。如果其中又积累了类，prepare再次翻转index，解锁恢复IRQ以后再排队下一轮。两槽不是最多只能回收两次，而是每次复用一个已经越过RCU边界的槽。scheduled是全局共享批次状态，只有在图锁保护下才能把收集方与回调方的操作串起来。

```mermaid
sequenceDiagram
    autonumber
    participant D as 删除路径
    participant A as pf[index]开放批次
    participant B as pf[index^1]封闭批次
    participant R as RCU回调
    participant F as 空闲类与链槽
    D->>A: F0 追加待回收类和链槽位
    D->>A: F1 scheduled置位并翻转index
    Note over A,B: 原开放槽成为封闭槽，后续删除写另一槽
    D->>R: 解锁后call_rcu
    R->>B: F2 宽限期后图锁内取封闭批次
    R->>F: F3 重置类并归还槽位
    R->>A: F4 清scheduled，检查下一批
    alt 下一批非空
        R->>R: 翻转index，解锁后再次call_rcu
    else 下一批为空
        Note over R: 不排队，等待以后删除触发
    end
```

`inside_selftest()`比较current与专用自测任务，本段WARN用于排除不适用的自测调用；`check_data_structures()`是内部一致性审计，不代替RCU寿命协议。链表拼接和位图运算只回收检查器存储，不释放业务对象。批次进入前的zap_class负责摘图与标记，仍需独立核对其边清理；本节证明的是“已加入批次的槽何时可以再用”。

**修改边界：** 同时检查空批次、回调已安排时新增删除、回调期间另一批非空及容量再利用；禁止在宽限期前归还封闭槽，禁止翻转index后把新删除继续写旧槽。测试需要真实并发与RCU推进，本仓库本次只核对源码和状态推演。

## 1.10\_摘除依赖与撤销相关链

前一节的延迟槽位归还以“类已经退出可查询历史”为前提。下面的四函数完成该前半程：按地址范围定位类，删除与它相连的依赖，把类移入待回收批次，并撤销包含它的链缓存。这里不是普通解锁，不能在每次release时调用，否则会抹去跨时间证据。

```c
/**
 * @brief 若链包含目标类，则撤销整条链并登记待回收链槽。
 * 仓库补充，非上游原文。
 */
static void remove_class_from_lock_chain(struct pending_free *pf,
					 struct lock_chain *chain,
					 struct lock_class *class)
{
#ifdef CONFIG_PROVE_LOCKING
	int i;

	for (i = chain->base; i < chain->base + chain->depth; i++) {
		if (chain_hlock_class_idx(chain_hlocks[i]) != class - lock_classes)
			continue;
		/*
		 * Each lock class occurs at most once in a lock chain so once
		 * we found a match we can break out of this loop.
		 */
		goto free_lock_chain;
	}
	/* Since the chain has not been modified, return. */
	return;

free_lock_chain:
	free_chain_hlocks(chain->base, chain->depth);
	/* Overwrite the chain key for concurrent RCU readers. */
	WRITE_ONCE(chain->chain_key, INITIAL_CHAIN_KEY);
	dec_chains(chain->irq_context);

	/*
	 * Note: calling hlist_del_rcu() from inside a
	 * hlist_for_each_entry_rcu() loop is safe.
	 */
	hlist_del_rcu(&chain->entry);
	__set_bit(chain - lock_chains, pf->lock_chains_being_freed);
	nr_zapped_lock_chains++;
#endif
}

/**
 * @brief 遍历链哈希，把目标类的所有链引用交给单链处理。
 * 仓库补充，非上游原文。
 */
static void remove_class_from_lock_chains(struct pending_free *pf,
					  struct lock_class *class)
{
	struct lock_chain *chain;
	struct hlist_head *head;
	int i;

	for (i = 0; i < ARRAY_SIZE(chainhash_table); i++) {
		head = chainhash_table + i;
		hlist_for_each_entry_rcu(chain, head, entry) {
			remove_class_from_lock_chain(pf, chain, class);
		}
	}
}

/**
 * @brief 摘除类相关依赖与哈希身份，加入待回收批次。
 * 仓库补充，非上游原文。
 */
static void zap_class(struct pending_free *pf, struct lock_class *class)
{
	struct lock_list *entry;
	int i;

	WARN_ON_ONCE(!class->key);

	/*
	 * Remove all dependencies this lock is
	 * involved in:
	 */
	for_each_set_bit(i, list_entries_in_use, ARRAY_SIZE(list_entries)) {
		entry = list_entries + i;
		if (entry->class != class && entry->links_to != class)
			continue;
		__clear_bit(i, list_entries_in_use);
		nr_list_entries--;
		list_del_rcu(&entry->entry);
	}
	if (list_empty(&class->locks_after) &&
	    list_empty(&class->locks_before)) {
		list_move_tail(&class->lock_entry, &pf->zapped);
		hlist_del_rcu(&class->hash_entry);
		WRITE_ONCE(class->key, NULL);
		WRITE_ONCE(class->name, NULL);
		nr_lock_classes--;
		__clear_bit(class - lock_classes, lock_classes_in_use);
		if (class - lock_classes == max_lock_class_idx)
			max_lock_class_idx--;
	} else {
		WARN_ONCE(true, "%s() failed for class %s\n", __func__,
			  class->name);
	}

	remove_class_from_lock_chains(pf, class);
	nr_zapped_classes++;
}

/**
 * @brief 按key或名称地址范围查找并摘除类；调用者持有图锁。
 * 仓库补充，非上游原文。
 */
static void __lockdep_free_key_range(struct pending_free *pf, void *start,
				     unsigned long size)
{
	struct lock_class *class;
	struct hlist_head *head;
	int i;

	/* Unhash all classes that were created by a module. */
	for (i = 0; i < CLASSHASH_SIZE; i++) {
		head = classhash_table + i;
		hlist_for_each_entry_rcu(class, head, hash_entry) {
			if (!within(class->key, start, size) &&
			    !within(class->name, start, size))
				continue;
			zap_class(pf, class);
		}
	}
}
```

按Z0至Z3读一次注销。Z0的范围函数扫描classhash_table，只有key或name位于半开区间`[start,start+size)`才进入zap；`within()`只是这个地址范围判断。它不是用名称字符串内容决定归属。动态key注销给出相应地址范围，模块清理则有自己的范围输入，不能把两类调用者混成同一种对象销毁。

Z1在图锁内遍历依赖池占用位，凡是`entry->class`或`entry->links_to`指向目标类，就清占用位、减计数并用RCU链表删除。Z2只有确认前后依赖列表均为空，才把类移到`pf->zapped`，从类哈希摘除，并将key/name置NULL；这一步清类占用统计，却还没把类槽接回free_lock_classes。若列表未清空会报告内部不一致，不能默认为已完成正常摘除。

Z3扫描所有链，每条链通过其base/depth区间中的类索引判断是否引用目标类。命中后撤销整条缓存链，不能把中间元素简单删除后沿用旧chain_key。先释放链元素存储，再用WRITE_ONCE把chain_key改为INITIAL_CHAIN_KEY，使并发读取能观察到失效标志；随后撤销链哈希节点并在本批位图登记链槽。真正清除链槽占用位发生在上一节宽限期后的批次回收。

这里存在不同回收时点：依赖池占用位、链元素区间、类空闲列表与链槽占用位不是同一个池，不能把“RCU延迟回收”粗略解释成所有资源同时归还。`free_chain_hlocks()`处理链元素分配器，`dec_chains()`更新相应IRQ上下文计数；这两个内部管理操作不销毁业务锁。结构修改必须分别核对每种读者使用哪些字段，不能把类槽延迟保证套用到所有辅助数组。

**修改约束：** 保留图锁前提、双端依赖匹配、哈希删除和失效标记顺序，以及链槽先进入待回收位图再归还的关系。复核三个输入：没有匹配类、目标类关联多条链、多个待回收类共享同一条链。最后一种情形中，已经从链哈希摘除的链不应再次由后续扫描重复回收。此处仍是源码推演，不是已执行的并发测试。

下一篇：[Lockdep 取得释放与持锁账本源码实现](../../P02_Linux_6.12_Lockdep取得释放与持锁账本源码实现.md#2.1_关联入口)。
