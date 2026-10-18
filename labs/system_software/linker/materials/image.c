#include <stdint.h>

/* 这些声明只引用链接符号，没有在 C 文件里分配数组。 */
extern unsigned char __data_load[], __data_start[], __data_end[];
extern unsigned char __bss_start[], __bss_end[];

volatile uint32_t bias = 17;
volatile uint32_t counter;
const uint32_t scale[2] = {3, 5};

/* 故意按 20、10 的顺序声明，让链接排序的作用可见。 */
const uint32_t reg_b __attribute__((section(".registry.20"), used)) = 0x22222222;
const uint32_t reg_a __attribute__((section(".registry.10"), used)) = 0x11111111;

/* 此函数无调用者，供 --gc-sections 对照实验使用。 */
uint32_t unused_helper(void)
{
    return 99;
}

void runtime_start(void)
{
    uintptr_t src = (uintptr_t)__data_load;
    uintptr_t dst = (uintptr_t)__data_start;
    uintptr_t end = (uintptr_t)__data_end;
    /* 启动环境按 GCC/目标 ABI 的地址模型访问链接定义的区间。 */
    while (dst < end)
        *(unsigned char *)dst++ = *(const unsigned char *)src++;

    dst = (uintptr_t)__bss_start;
    end = (uintptr_t)__bss_end;
    while (dst < end)
        *(unsigned char *)dst++ = 0;

    /* 此时才使用已经初始化的可写全局对象。 */
    counter = bias + scale[counter & 1u];
    for (;;) { }
}
