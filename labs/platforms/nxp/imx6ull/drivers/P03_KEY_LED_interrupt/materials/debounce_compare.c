/* 逻辑按键轨迹模型：只计算已给定的时间序列，不访问GPIO或模拟调度器。 */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

struct key_edge {
    unsigned int ms;
    bool pressed; /* 已经过有效极性翻译，1表示按下。 */
};

struct comparison {
    unsigned int guarded_edges;
    unsigned int stable_presses;
};

static struct comparison compare(const struct key_edge *edges, size_t count,
                                  unsigned int window_ms, unsigned int finish_ms)
{
    struct comparison result = {0, 0};
    unsigned int now, accepted_at = 0, deadline = 0;
    size_t next = 0;
    bool have_accepted = false, pending = false;
    bool sample = false, stable = false;

    for (now = 0; now <= finish_ms; ++now) {
        /* 同一时刻先处理边沿，再尝试采样，明确模型的边界次序。 */
        while (next < count && edges[next].ms == now) {
            sample = edges[next].pressed;
            if (sample && (!have_accepted || now - accepted_at >= window_ms)) {
                ++result.guarded_edges;
                accepted_at = now;
                have_accepted = true;
            }
            deadline = now + window_ms;
            pending = true;
            ++next;
        }
        if (pending && now >= deadline) {
            if (sample != stable) {
                stable = sample;
                if (stable)
                    ++result.stable_presses;
            }
            pending = false;
        }
    }
    return result;
}

static int check(const char *name, const struct key_edge *edges, size_t count,
                  unsigned int expected_guard, unsigned int expected_stable)
{
    struct comparison result = compare(edges, count, 30, 260);
    printf("%s: edge_guard=%u stable_press=%u\n", name,
           result.guarded_edges, result.stable_presses);
    return result.guarded_edges == expected_guard &&
           result.stable_presses == expected_stable;
}

int main(void)
{
    const struct key_edge short_pulse[] = {{10, true}, {15, false}};
    const struct key_edge press_release_bounce[] = {
        {10, true}, {14, false}, {20, true},
        {90, false}, {94, true}, {99, false}
    };
    const struct key_edge two_short_presses[] = {
        {10, true}, {20, false}, {30, true}, {40, false}
    };
    const struct key_edge two_long_presses[] = {
        {10, true}, {80, false}, {130, true}, {200, false}
    };

    if (!check("short pulse", short_pulse, 2, 1, 0) ||
        !check("press/release bounce", press_release_bounce, 6, 2, 1) ||
        !check("two short presses", two_short_presses, 4, 1, 0) ||
        !check("two long presses", two_long_presses, 4, 2, 2))
        return 1;
    return 0;
}
