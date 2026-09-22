/* 容量以条目计，观察输出成功后才提交位置，不模拟真实目录并发。 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static const char *const names[] = { "foo", "bar", "baz" };

static size_t emit_page(size_t *position, const char **output, size_t capacity)
{
    size_t count = 0;
    while (*position < sizeof(names) / sizeof(names[0]) && count < capacity) {
        output[count++] = names[*position];
        ++*position; /* 写入本次输出后才推进共享于各页的位置。 */
    }
    return count;
}

int main(void)
{
    const size_t capacities[] = { 1, 0, 1, 5 };
    const size_t expected_positions[] = { 1, 1, 2, 3 };
    const size_t expected_counts[] = { 1, 0, 1, 1 };
    size_t position = 0, page;

    for (page = 0; page < sizeof(capacities) / sizeof(capacities[0]); ++page) {
        const char *output[5];
        size_t count = emit_page(&position, output, capacities[page]);
        size_t index;
        assert(position == expected_positions[page]);
        assert(count == expected_counts[page]);
        printf("%zu [", position);
        for (index = 0; index < count; ++index)
            printf("%s%s", index ? ", " : "", output[index]);
        puts("]");
    }
    return 0;
}
