#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>

enum interference { NONE, DROP_LAST, ADD_OWNER };

/* refs 本身始终在有效存储中；这里只模拟零/正数，不模拟内核饱和。 */
static bool try_take(atomic_uint *refs, enum interference event,
                     unsigned int *attempts)
{
    unsigned int old = atomic_load_explicit(refs, memory_order_relaxed);
    *attempts = 0;
    while (old != 0) {
        assert(old < UINT_MAX);
        if (*attempts == 0 && event != NONE) {
            /* 在第一次观察与比较之间，显式安排另一条路径先改变计数。 */
            atomic_store_explicit(refs, event == DROP_LAST ? 0u : 2u,
                                  memory_order_relaxed);
        }
        ++*attempts;
        if (atomic_compare_exchange_strong_explicit(refs, &old, old + 1,
                memory_order_relaxed, memory_order_relaxed))
            return true;
        /* 比较失败已把 old 更新为当前值，下一轮必须重新检查它是否为零。 */
    }
    return false;
}

int main(void)
{
    const unsigned int initial[] = {0, 1, 1, 1};
    const enum interference events[] = {NONE, NONE, DROP_LAST, ADD_OWNER};
    const unsigned int expected_count[] = {0, 2, 0, 3};
    const unsigned int expected_attempts[] = {0, 1, 1, 2};
    const bool expected_result[] = {false, true, false, true};
    for (unsigned int path = 0; path < 4; ++path) {
        atomic_uint refs;
        atomic_init(&refs, initial[path]);
        unsigned int attempts;
        bool taken = try_take(&refs, events[path], &attempts);
        unsigned int count = atomic_load_explicit(&refs, memory_order_relaxed);
        assert(taken == expected_result[path]);
        assert(count == expected_count[path]);
        assert(attempts == expected_attempts[path]);
        printf("path=%u taken=%u count=%u attempts=%u\n",
               path, taken ? 1u : 0u, count, attempts);
    }
    return 0;
}
