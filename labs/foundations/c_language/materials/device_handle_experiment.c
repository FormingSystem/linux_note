#ifdef __CHECKER__
#define __device_handle \
    __attribute__((noderef, address_space(__device_handle)))
#else
#define __device_handle
#endif

#ifdef ERASE_EXPECT_CONTRACT
#define __expected_device_handle
#else
#define __expected_device_handle __device_handle
#endif

struct device_slot {
    int value;
};

static struct device_slot __device_handle *device_handle_source;
static struct device_slot *plain_source;

static void expect_device_handle(
    struct device_slot __expected_device_handle *source)
{
    /* 空函数不访问设备，只让形参类型建立调用边界。 */
    (void)source;
}

int main(void)
{
    int result = 0;

#ifdef BAD_PLAIN_TO_HANDLE
    /* A/B两轮只比较这一处普通指针调用的形参契约。 */
    expect_device_handle(plain_source);
#else
    /* 正确基线：同域指针只进入同域形参。 */
    expect_device_handle(device_handle_source);
#endif

#ifdef BAD_HANDLE_TO_PLAIN
    /* 反例二：设备句柄降格为普通指针。 */
    struct device_slot *plain = device_handle_source;

    (void)plain;
#endif

#ifdef BAD_HANDLE_DEREF
    /* 反例三：绕过专用访问器直接解引用受限句柄。 */
    result += device_handle_source->value;
#endif

    return result;
}
