/* C11顺序解释器：两份单槽待写状态，不是LKMM或处理器模拟器。 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

struct model {
    unsigned int pc[2], memory[2], observed[2];
    bool pending[2];
};
struct results {
    unsigned int paths, mask, counts[4];
};

static void explore(struct model state, bool fence, struct results *result)
{
    if (state.pc[0] == 2 && state.pc[1] == 2) {
        unsigned int pair = 2 * state.observed[0] + state.observed[1];
        ++result->paths;
        ++result->counts[pair];
        result->mask |= 1U << pair;
        return; /* 读取结束以后才传播的写不再改变已记录的结果。 */
    }
    for (unsigned int cpu = 0; cpu < 2; ++cpu) {
        if (state.pc[cpu] == 0) {
            struct model next = state;
            next.pending[cpu] = true; /* 自己的写先进入本地待写槽。 */
            next.pc[cpu] = 1;
            explore(next, fence, result);
        } else if (state.pc[cpu] == 1 && (!fence || !state.pending[cpu])) {
            struct model next = state;
            next.observed[cpu] = next.memory[1 - cpu];
            next.pc[cpu] = 2;
            explore(next, fence, result);
        }
        if (state.pending[cpu]) {
            struct model next = state;
            next.memory[cpu] = 1; /* 独立传播事件使另一角色能看见写。 */
            next.pending[cpu] = false;
            explore(next, fence, result);
        }
    }
}

int main(void)
{
    struct model initial = {0};
    struct results plain = {0}, fenced = {0};
    explore(initial, false, &plain);
    explore(initial, true, &fenced);
    assert(plain.mask == 15U && fenced.mask == 14U);
    for (unsigned int pair = 0; pair < 4; ++pair)
        printf("%u/%u: plain=%s fenced=%s\n", pair / 2, pair % 2,
               plain.counts[pair] ? "possible" : "absent",
               fenced.counts[pair] ? "possible" : "absent");
    printf("model paths: plain=%u fenced=%u; not a hardware result\n",
           plain.paths, fenced.paths);
    return 0;
}
