#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

struct mapping_record {
    size_t original_count;
    size_t mapped_count;
    size_t dma_lengths[3];
};

static bool describe_hardware(const struct mapping_record *m,
                              size_t count, size_t *bytes)
{
    if (count != m->mapped_count)
        return false; /* 硬件只能消费映射层返回的有效段。 */
    *bytes = 0;
    for (size_t i = 0; i < count; ++i)
        *bytes += m->dma_lengths[i];
    return true;
}

static bool check_unmap(const struct mapping_record *m, size_t count)
{
    return count == m->original_count; /* 解除映射沿原输入身份。 */
}

int main(void)
{
    const size_t input_lengths[] = {1024, 1024, 512};
    const struct mapping_record m = {3, 2, {2048, 512, 0}};
    size_t original_bytes = 0, device_bytes = 0;
    for (size_t i = 0; i < m.original_count; ++i)
        original_bytes += input_lengths[i];
    if (!describe_hardware(&m, 2, &device_bytes) ||
        original_bytes != device_bytes || device_bytes != 2560 ||
        describe_hardware(&m, 3, &device_bytes) ||
        !check_unmap(&m, 3) || check_unmap(&m, 2)) {
        fputs("SG count contract failed\n", stderr);
        return 1; /* 检查失败不得继续打印成功，关闭断言也不能跳过检查。 */
    }
    puts("hardware=2 segments; unmap=3 entries; bytes=2560");
    return 0;
}
