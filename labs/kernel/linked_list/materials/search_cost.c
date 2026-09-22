/* 计数真正的逐项比较，不把操作次数解释成执行时间。 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

int main(void)
{
    const size_t sizes[] = { 8, 32, 128 };
    size_t sample;

    for (sample = 0; sample < sizeof(sizes) / sizeof(sizes[0]); ++sample) {
        size_t size = sizes[sample];
        size_t key, candidate, comparisons = 0;
        for (key = 0; key < size; ++key) {
            for (candidate = 0; candidate < size; ++candidate) {
                ++comparisons;
                if (candidate == key)
                    break;
            }
        }
        assert(comparisons == size * (size + 1) / 2);
        printf("%zu %zu\n", size, comparisons);
    }
    return 0;
}
