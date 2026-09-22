/* 只模拟整数编码，不创建或解引用任何指针。 */
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    const int errors[] = { -4096, -4095, -517, -22, -1, 0, 22 };
    const unsigned int widths[] = { 32, 64 };
    size_t sample, index;

    for (sample = 0; sample < sizeof(widths) / sizeof(widths[0]); ++sample) {
        unsigned int width = widths[sample];
        /* 不执行 1ULL << 64，避免移位数等于类型宽度。 */
        uint64_t mask = width == 32 ? UINT32_MAX : UINT64_MAX;
        uint64_t threshold = (uint64_t)(int64_t)-4095 & mask;
        int error;

        printf("width=%u\n", width);
        for (index = 0; index < sizeof(errors) / sizeof(errors[0]); ++index) {
            uint64_t encoded = (uint64_t)(int64_t)errors[index] & mask;
            printf("input=%5d value=0x%0*" PRIx64 " error=%d\n",
                   errors[index], (int)(width / 4), encoded,
                   encoded >= threshold);
        }
        for (error = -4095; error < 0; ++error) {
            uint64_t encoded = (uint64_t)(int64_t)error & mask;
            /* 差值只有 0 到 4094，不把超范围无符号数强转成有符号数。 */
            int64_t decoded = -(int64_t)(mask - encoded) - 1;
            assert(encoded >= threshold);
            assert(decoded == error);
        }
    }
    return 0;
}
