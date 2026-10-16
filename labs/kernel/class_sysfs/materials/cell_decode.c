/* 按字节组装一个大端 cell，不依赖宿主端序或地址对齐。 */
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    const uint8_t raw[4] = { 0x00, 0x00, 0x10, 0x00 };
    uint32_t value = ((uint32_t)raw[0] << 24) |
                     ((uint32_t)raw[1] << 16) |
                     ((uint32_t)raw[2] << 8) |
                     (uint32_t)raw[3];
    assert(value == UINT32_C(4096));
    printf("%" PRIu32 "\n", value);
    return 0;
}
