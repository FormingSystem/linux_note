#include <stdio.h>

/* 模拟Linux非__CHECKER__分支提供的普通编译回退。 */
#ifndef __CHECKER__
#define __builtin_warning(x, y...) (1)
#endif

/*
 * Sparse能够理解检查参数；普通编译时，检查参数被上面的回退删除。
 * 最后一行才是真正的业务求值位置。
 */
#define CHECK_THEN_EVALUATE(expr) ({                            \
    int annotation_shape =                                    \
        __builtin_warning(!__builtin_safe_p(expr),             \
                          "实参可能带有副作用: " #expr);      \
    (void)annotation_shape;                                    \
    (expr);                                                    \
})

static unsigned int allocation_calls;

static unsigned int allocate_record_id(void)
{
    allocation_calls++;
    return allocation_calls;
}

int main(void)
{
    unsigned int base = 40;
    unsigned int plain_value;
    unsigned int allocated_id;

    /* 纯计算表达式：不修改程序状态。 */
    plain_value = CHECK_THEN_EVALUATE(base + 2);

    /* 函数调用带有副作用，但普通运行时仍只能调用一次。 */
    allocated_id = CHECK_THEN_EVALUATE(allocate_record_id());

    printf("plain=%u allocated=%u calls=%u\n",
           plain_value, allocated_id, allocation_calls);

    /* 用退出码验证结果和单次求值不变量。 */
    if (plain_value != 42 || allocated_id != 1 || allocation_calls != 1)
        return 1;

    return 0;
}
