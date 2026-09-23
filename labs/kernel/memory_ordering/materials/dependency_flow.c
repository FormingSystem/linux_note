/* 只生成汇编观察数据流，不运行，不模拟Linux RCU或硬件弱序。 */
#define LAB_READ_ONCE(x) (*(volatile __typeof__(x) *)&(x))
#define LAB_WRITE_ONCE(x, value) (*(volatile __typeof__(x) *)&(x) = (value))

struct node { unsigned int value; };
struct node *shared_ptr;
struct node fixed_node;
unsigned int shared_index, values[4], copied_value;

/* 若执行，需要调用者先保证指针非空、对象存活；本实验只编译。 */
unsigned int dependent_pointer(void)
{
    struct node *p = LAB_READ_ONCE(shared_ptr);
    return LAB_READ_ONCE(p->value);
}

/* 数组地址确实需要首次读取的低两位。 */
unsigned int dependent_index(void)
{
    unsigned int index = LAB_READ_ONCE(shared_index) & 3U;
    return LAB_READ_ONCE(values[index]);
}

/* 代数抵消：读动作保留，但后续地址已不需要它的结果。 */
unsigned int cancelled_index(void)
{
    unsigned int index = LAB_READ_ONCE(shared_index);
    unsigned int offset = index ^ index;
    return LAB_READ_ONCE(values[offset]);
}

/* 真分支已知p等于固定地址，编译器可以用固定地址替代p。 */
unsigned int known_address(void)
{
    struct node *p = LAB_READ_ONCE(shared_ptr);
    if (p == &fixed_node)
        return LAB_READ_ONCE(p->value);
    return 0;
}

/* 写入的数值而非地址依赖第一次读取。 */
void data_copy(void)
{
    unsigned int value = LAB_READ_ONCE(shared_index);
    LAB_WRITE_ONCE(copied_value, value);
}
