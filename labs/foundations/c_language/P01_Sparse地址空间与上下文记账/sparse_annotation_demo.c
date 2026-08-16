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

struct record {
	int value;
};

static inline void
check_user_pointer(const volatile void __user *pointer)
{
	(void)pointer;
}

static void address_space_good(struct record __user *source)
{
	/* 空函数的形参类型迫使 Sparse 核对 source 的地址域。 */
	check_user_pointer(source);
}

#ifdef BAD_ADDRESS_SPACE
static int address_space_bad(struct record __user *source)
{
	/* 预期：不同地址域赋值和受限指针裸解引用诊断。 */
	struct record *plain = source;

	return source->value + plain->value;
}
#endif

static void fake_lock(int *lock) __acquires(lock)
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

#ifdef BAD_CONTEXT
static void context_bad_call(int *lock)
{
	/* 预期：未建立 context 就调用要求持有的函数。 */
	requires_lock(lock);
}

static void context_bad_exit(int *lock)
{
	/* 预期：函数返回时仍留下未清偿的 context。 */
	fake_lock(lock);
}

static void context_bad_release(int *lock)
{
	/* 预期：没有取得就执行释放。 */
	fake_unlock(lock);
}
#endif

void run_sparse_annotation_demo(struct record __user *source,
				int *lock, int try_succeeds)
{
	address_space_good(source);
	context_good(lock);
	conditional_context_good(lock, try_succeeds);
#ifdef BAD_ADDRESS_SPACE
	(void)address_space_bad(source);
#endif
#ifdef BAD_CONTEXT
	context_bad_call(lock);
	context_bad_exit(lock);
	context_bad_release(lock);
#endif
}
