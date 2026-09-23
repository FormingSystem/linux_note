#if defined(__GNUC__) || defined(__clang__)
#define LAB_READ_ONCE(x) (*(volatile __typeof__(x) *)&(x))
#define LAB_WRITE_ONCE(x, value) (*(volatile __typeof__(x) *)&(x) = (value))
#define LAB_BARRIER() __asm__ __volatile__("" : : : "memory")
#else
#error "本实验需要 GCC 或 Clang 的 __typeof__ 扩展"
#endif

int shared;

/* 普通表达式允许编译器合并两次读取。 */
int plain_sum(void)
{
    return shared + shared;
}

/* 两个 ONCE 表达式要求保留两个访问实例。 */
int once_sum(void)
{
    return LAB_READ_ONCE(shared) + LAB_READ_ONCE(shared);
}

/* 普通轮询可能只在进入循环前读取一次。 */
int plain_poll(void)
{
    while (shared == 0)
        ;

    return shared;
}

/* ONCE 轮询要求循环中重新读取共享值。 */
int once_poll(void)
{
    while (LAB_READ_ONCE(shared) == 0)
        ;

    return LAB_READ_ONCE(shared);
}

/* 编译器屏障也会改变普通轮询的优化条件，不等于CPU屏障。 */
int barrier_poll(void)
{
    while (shared == 0)
        LAB_BARRIER();
    return shared;
}

/* 前一个普通写可能被后一个覆盖，外部只留下最终值。 */
void plain_stores(void)
{
    shared = 1;
    shared = 2;
}

/* 两次局部volatile访问保留写入动作，不意味着读者必定观察到1。 */
void once_stores(void)
{
    LAB_WRITE_ONCE(shared, 1);
    LAB_WRITE_ONCE(shared, 2);
}
