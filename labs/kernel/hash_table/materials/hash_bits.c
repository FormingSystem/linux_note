/* 固定宽度模型：观察桶分布，不把计数当作 CPU 性能测试。 */
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t multiply_32(uint32_t value)
{
    return value * UINT32_C(0x61C88647);
}

static uint32_t bucket_32(uint32_t value, unsigned int bits)
{
    assert(bits >= 1 && bits <= 32);
    return multiply_32(value) >> (32 - bits);
}

static uint32_t bucket_64(uint64_t value, unsigned int bits,
                          unsigned int word_bits)
{
    assert(bits >= 1 && bits <= 32);
    assert(word_bits == 32 || word_bits == 64);
    if (word_bits == 64)
        return (uint32_t)((value * UINT64_C(0x61C8864680B583EB)) >> (64 - bits));
    return bucket_32((uint32_t)value ^ multiply_32((uint32_t)(value >> 32)), bits);
}

static void print_counts(const char *name, const unsigned int counts[16])
{
    unsigned int index;
    printf("%s:", name);
    for (index = 0; index < 16; ++index)
        printf(" %u", counts[index]);
    putchar('\n');
}

int main(void)
{
    unsigned int low_counts[16] = { 0 };
    unsigned int high_counts[16] = { 0 };
    unsigned int index;
    uint64_t value = (UINT64_C(1) << 32) + 1;

    for (index = 0; index < 16; ++index) {
        uint32_t key = index * UINT32_C(16);
        ++low_counts[key & 15];
        ++high_counts[bucket_32(key, 4)];
    }
    print_counts("直接取低位", low_counts);
    print_counts("乘法取高位", high_counts);
    printf("截断为32位: %" PRIu32 "\n", bucket_32((uint32_t)value, 4));
    printf("完整64位/32位机器: %" PRIu32 "\n", bucket_64(value, 4, 32));
    printf("完整64位/64位机器: %" PRIu32 "\n", bucket_64(value, 4, 64));
    assert(bucket_32(123, 32) == multiply_32(123));
    return 0;
}
