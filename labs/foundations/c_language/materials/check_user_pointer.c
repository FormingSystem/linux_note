struct record {
    unsigned int length;
};

#ifdef __CHECKER__
/* Sparse 分支：把 __user 保留为类型系统中的命名地址空间。 */
#define __user __attribute__((noderef, address_space(__user)))

/* 函数体为空，形参类型就是这个检查点的全部约束。 */
static inline void
__chk_user_ptr(const volatile void __user *ptr)
{
}
#else
/* 普通编译分支：不改变机器类型，也不生成运行时代码。 */
#define __user
#define __chk_user_ptr(ptr) ((void)0)
#endif

static void verify_record_sources(
    const struct record __user *user_record,
    const struct record *kernel_record)
{
    /* 正确：实参和形参都属于 __user 地址空间。 */
    __chk_user_ptr(user_record);

    /* 错误：普通内核指针不能传给要求 __user 的形参。 */
    __chk_user_ptr(kernel_record);
}
