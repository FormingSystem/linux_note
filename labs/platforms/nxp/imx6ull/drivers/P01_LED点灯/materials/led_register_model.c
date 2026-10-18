/* 寄存器数值模型：只操作宿主变量，不访问真实硬件地址。 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LED_MASK (UINT32_C(1) << 3)
#define OTHER_MASK (UINT32_C(1) << 6)

/* 一次写入是一条完整命令；只接受on/off，可带一个结尾换行。 */
static int parse_command(const unsigned char *bytes, size_t count, int *on)
{
    if (!count)
        return 0; /* 空写入没有动作，不把空字符串当成二进制零。 */
    if (count > 4)
        return -1;
    if (bytes[count - 1] == '\n')
        --count;
    if (count == 2 && !memcmp(bytes, "on", 2)) {
        *on = 1;
        return 1;
    }
    if (count == 3 && !memcmp(bytes, "off", 3)) {
        *on = 0;
        return 1;
    }
    return -1; /* 完整匹配；onward或带NUL的输入不会截断成on。 */
}

static uint32_t apply_output(uint32_t output_latch, int on)
{
    /* 假设本模型中bit 3已配置为普通GPIO输出，LED为低有效。 */
    return on ? output_latch & ~LED_MASK : output_latch | LED_MASK;
}

static int write_model(uint32_t *output_latch,
                       const unsigned char *bytes, size_t count)
{
    int on = 0;
    int result = parse_command(bytes, count, &on);

    if (result > 0)
        *output_latch = apply_output(*output_latch, on);
    return result;
}

int main(void)
{
    uint32_t latch = UINT32_C(0xa8);
    uint32_t first_snapshot;
    uint32_t second_snapshot;
    uint32_t shared;
    const unsigned char invalid[] = {'o', 'n', 0};

    if (write_model(&latch, (const unsigned char *)"on\n", 3) != 1 ||
        latch != UINT32_C(0xa0))
        return 1;
    printf("on:  latch=%02" PRIx32 " pin=low\n", latch);
    if (write_model(&latch, (const unsigned char *)"off", 3) != 1 ||
        latch != UINT32_C(0xa8))
        return 1;
    printf("off: latch=%02" PRIx32 " pin=high\n", latch);
    if (write_model(&latch, invalid, sizeof(invalid)) != -1 ||
        write_model(&latch, (const unsigned char *)"onward", 6) != -1 ||
        write_model(&latch, (const unsigned char *)"", 0) != 0 ||
        latch != UINT32_C(0xa8))
        return 1;
    puts("invalid/empty: unchanged");

    /* 两个写者先取相同快照，再分别置位；严格按顺序模拟交错。 */
    shared = 0;
    first_snapshot = shared;
    second_snapshot = shared;
    shared = first_snapshot | LED_MASK;
    shared = second_snapshot | OTHER_MASK;
    printf("stale snapshots: %02" PRIx32 "\n", shared);
    if (shared != OTHER_MASK)
        return 1;

    /* 统一所有者串行读改写，第二次使用第一次提交后的状态。 */
    shared = 0;
    shared |= LED_MASK;
    shared |= OTHER_MASK;
    printf("serialized updates: %02" PRIx32 "\n", shared);
    return shared == (LED_MASK | OTHER_MASK) ? 0 : 1;
}
