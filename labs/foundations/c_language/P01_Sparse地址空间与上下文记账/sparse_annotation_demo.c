// SPDX-License-Identifier: GPL-2.0-only

/*
 * 本文件是独立 Sparse 实验，不实现真实用户访问和真实锁。
 * 所有 fake_* 函数只用于观察分析器类型与 context 账本。
 */

#ifdef __CHECKER__
#define __user __attribute__((noderef, address_space(1)))
#define __iomem __attribute__((noderef, address_space(2)))
#define __must_hold(x) __attribute__((context(x, 1, 1)))
#define __acquires(x) __attribute__((context(x, 0, 1)))
#define __releases(x) __attribute__((context(x, 1, 0)))
#define __acquire(x) __context__(x, 1)
#define __release(x) __context__(x, -1)
#define __cond_lock(x, condition) \
	((condition) ? ({ __acquire(x); 1; }) : 0)
#else
#define __user
#define __iomem
#define __must_hold(x)
#define __acquires(x)
#define __releases(x)
#define __acquire(x) ((void)0)
#define __release(x) ((void)0)
#define __cond_lock(x, condition) (condition)
#endif

#ifdef ERASE_BOUNDARY_CONTRACT
#define __boundary_user
#else
#define __boundary_user __user
#endif

#ifdef ERASE_LOCK_WRAPPER_CONTRACT
#define __fake_lock_acquires(x)
#else
#define __fake_lock_acquires(x) __acquires(x)
#endif

struct record {
	int value;
};

void run_sparse_annotation_demo(struct record __user *source,
				int *lock, int try_succeeds);

static inline void
check_user_pointer(const volatile void __user *pointer)
{
	(void)pointer;
}

static inline void
check_boundary_pointer(const volatile void __boundary_user *pointer)
{
	/* 形参可以为空函数，但类型仍会在调用点触发实参与形参检查。 */
	(void)pointer;
}

static void address_space_good(struct record __user *source)
{
	/* 空函数的形参类型迫使 Sparse 核对 source 的地址域。 */
	check_user_pointer(source);
}

#ifdef BAD_ADDRESS_BOUNDARY
static void address_space_bad_boundary(void)
{
	static struct record kernel_record;

	/* 预期：普通内核指针不能冒充用户地址空间指针。 */
	check_boundary_pointer(&kernel_record);
}
#endif

#ifdef BAD_ADDRESS_ASSIGNMENT
static void address_space_bad_assignment(struct record __user *source)
{
	/* 预期：不同地址域赋值诊断；这里不再混入裸解引用。 */
	struct record *plain = source;

	(void)plain;
}
#endif

#ifdef BAD_NODEREF
static int address_space_bad_dereference(struct record __user *source)
{
	/* 预期：noderef指针不能被普通C解引用表达式直接访问。 */
	return source->value;
}
#endif

static void fake_lock(int *lock) __fake_lock_acquires(lock)
{
	/* 只修改 Sparse 账本，不取得真实锁。 */
	(void)lock;
	__acquire(lock);
}

static void fake_unlock(int *lock) __releases(lock)
{
	/* 只修改 Sparse 账本，不释放真实锁。 */
	(void)lock;
	__release(lock);
}

static void requires_lock(int *lock) __must_hold(lock)
{
	(void)lock;
}

static void context_good(int *lock)
{
	fake_lock(lock);
	requires_lock(lock);
	fake_unlock(lock);
}

static void conditional_context_good(int *lock, int succeeds)
{
	if (__cond_lock(lock, succeeds)) {
		requires_lock(lock);
		fake_unlock(lock);
	}
}

#ifdef BAD_CONTEXT_CALL
static void context_bad_call(int *lock)
{
	/* 预期：未建立 context 就调用要求持有的函数。 */
	requires_lock(lock);
}
#endif

#ifdef BAD_CONTEXT_EXIT
static void context_bad_exit(int *lock)
{
	/* 预期：函数返回时仍留下未清偿的 context。 */
	fake_lock(lock);
}
#endif

#ifdef BAD_CONTEXT_RELEASE
static void context_bad_release(int *lock)
{
	/* 预期：没有取得就执行释放。 */
	fake_unlock(lock);
}
#endif

#ifdef BAD_CONDITIONAL_CONTEXT
static void conditional_context_bad(int *lock, int succeeds)
{
	/* 错误：在知道trylock结果以前就无条件登记“已经取得”。 */
	__acquire(lock);
	if (succeeds) {
		requires_lock(lock);
		fake_unlock(lock);
	}
}
#endif

void run_sparse_annotation_demo(struct record __user *source,
				int *lock, int try_succeeds)
{
	address_space_good(source);
	context_good(lock);
	conditional_context_good(lock, try_succeeds);
#ifdef BAD_ADDRESS_BOUNDARY
	address_space_bad_boundary();
#endif
#ifdef BAD_ADDRESS_ASSIGNMENT
	address_space_bad_assignment(source);
#endif
#ifdef BAD_NODEREF
	(void)address_space_bad_dereference(source);
#endif
#ifdef BAD_CONTEXT_CALL
	context_bad_call(lock);
#endif
#ifdef BAD_CONTEXT_EXIT
	context_bad_exit(lock);
#endif
#ifdef BAD_CONTEXT_RELEASE
	context_bad_release(lock);
#endif
#ifdef BAD_CONDITIONAL_CONTEXT
	conditional_context_bad(lock, try_succeeds);
#endif
}
