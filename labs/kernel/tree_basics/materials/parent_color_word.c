#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* 抽象地址整数模型，不把整数转换成宿主指针。 */
static bool encode_parent(uint64_t parent, unsigned color, unsigned width,
                          uint64_t *word)
{
    const uint64_t limit = width == 32 ? UINT32_MAX : UINT64_MAX;
    if ((width != 32 && width != 64) || color > 1 ||
        parent > limit || (parent & UINT64_C(3)) != 0)
        return false;
    *word = parent + color;
    return true;
}

static uint64_t parent_part(uint64_t word)
{
    return word & ~UINT64_C(3);
}

static unsigned color_part(uint64_t word)
{
    return (unsigned)(word & UINT64_C(1));
}

int main(void)
{
    uint64_t word = 0;
    assert(encode_parent(UINT64_C(0x1000), 1, 32, &word));
    printf("black child: word=0x%" PRIx64 " parent=0x%" PRIx64
           " color=%u\n", word, parent_part(word), color_part(word));

    assert(encode_parent(0, 1, 32, &word));
    printf("black root: word=0x%" PRIx64 " parent=0x%" PRIx64 "\n",
           word, parent_part(word));

    /* 自指是完整字段相等的游离约定，不是额外的一位。 */
    const uint64_t self = UINT64_C(0x2000);
    word = self;
    printf("detached marker: self_match=%d low_color=%u\n",
           word == self, color_part(word));

    const uint64_t wide_parent = UINT64_C(0x100002000);
    const bool fit32 = encode_parent(wide_parent, 0, 32, &word);
    const bool fit64 = encode_parent(wide_parent, 0, 64, &word);
    printf("wide address: fit32=%d fit64=%d\n",
           fit32, fit64);
    printf("bad truncation loses address: %d\n",
           (uint64_t)(uint32_t)wide_parent != wide_parent);

    unsigned cases = 0;
    for (unsigned width = 32; width <= 64; width += 32) {
        for (uint64_t parent = 0; parent < 65536; parent += 4) {
            for (unsigned color = 0; color < 2; ++color) {
                assert(encode_parent(parent, color, width, &word));
                assert(parent_part(word) == parent && color_part(word) == color);
                ++cases;
            }
        }
        const uint64_t limit = width == 32 ? UINT32_MAX : UINT64_MAX;
        assert(encode_parent(limit - 3, 1, width, &word));
        assert(parent_part(word) == limit - 3);
    }
    assert(!encode_parent(UINT64_C(0x1001), 0, 32, &word));
    assert(!encode_parent(0, 2, 32, &word));
    assert(!encode_parent(0, 0, 16, &word));
    printf("round trips: %u\n", cases);
    return 0;
}
