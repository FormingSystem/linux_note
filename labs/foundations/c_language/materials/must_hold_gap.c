#ifdef __CHECKER__
#define must_hold(x) __attribute__((context(x, 1, 1)))
#else
#define must_hold(x)
#endif

extern void commit_locked(int *lock) must_hold(lock);

void missing_lock(int *lock);
void missing_lock(int *lock)
{
    /* 违背调用契约，但这次调用的静态净增量为0。 */
    commit_locked(lock);
}
