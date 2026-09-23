#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

struct closed_range { uint64_t first, last; };

/* 教学转换器主动检查输入；不声称实际 VMA 包装函数都执行此检查。 */
static bool to_closed(uint64_t start, uint64_t end, struct closed_range *out)
{
    if (start >= end)
        return false;
    *out = (struct closed_range){start, end - 1};
    return true;
}

static bool contains(struct closed_range range, uint64_t address)
{
    return range.first <= address && address <= range.last;
}

static void require(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "check failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    struct closed_range a, b;
    require(to_closed(0x1000, 0x3000, &a), "A input");
    require(to_closed(0x3000, 0x4000, &b), "B input");
    printf("A=[%" PRIx64 ",%" PRIx64 "] B=[%" PRIx64 ",%" PRIx64 "]\n",
           a.first, a.last, b.first, b.last);
    require(contains(a, 0x2fff) && !contains(a, 0x3000), "A excludes end");
    require(contains(b, 0x3000), "B owns boundary");
    struct closed_range wrong_a = {0x1000, 0x3000};
    require(contains(wrong_a, 0x3000) && contains(b, 0x3000), "wrong overlap");

    unsigned accepted = 0, rejected = 0, membership_checks = 0;
    for (uint64_t start = 0; start <= 32; ++start) {
        for (uint64_t end = 0; end <= 32; ++end) {
            struct closed_range result = {99, 100};
            bool ok = to_closed(start, end, &result);
            require(ok == (start < end), "validity");
            if (!ok) {
                require(result.first == 99 && result.last == 100, "failure unchanged");
                ++rejected;
                continue;
            }
            ++accepted;
            for (uint64_t address = 0; address <= 32; ++address) {
                require(contains(result, address) ==
                        (start <= address && address < end), "same membership");
                ++membership_checks;
            }
        }
    }
    /* 最大可表示 end 仍是排除式边界；本类型不能表示 2^64。 */
    require(to_closed(UINT64_MAX - 1, UINT64_MAX, &a), "high interval");
    require(a.last == UINT64_MAX - 1 && !contains(a, UINT64_MAX), "high boundary");
    uint64_t zero = 0;
    require(zero - 1 == UINT64_MAX, "unsigned wrap is not an empty interval");
    printf("accepted=%u rejected=%u membership_checks=%u; zero-1 wraps\n",
           accepted, rejected, membership_checks);
    return EXIT_SUCCESS;
}
