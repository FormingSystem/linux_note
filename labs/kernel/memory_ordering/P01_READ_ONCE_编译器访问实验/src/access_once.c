#if defined(__GNUC__) || defined(__clang__)
#define LAB_READ_ONCE(x) (*(volatile __typeof__(x) *)&(x))
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
