/* C11单线程交错解释器：不制造C数据竞争，也不实现Linux锁。 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

struct model {
    unsigned int value;
    unsigned int local[2];
    unsigned int step[2];
    int owner;
    char trace[5];
};
struct results {
    unsigned int paths, lost;
};

static void explore(struct model state, bool guarded,
                    struct results *results)
{
    unsigned int depth = state.step[0] + state.step[1];
    if (depth == 4) {
        ++results->paths;
        results->lost += state.value != 2;
        assert(state.owner == -1);
        printf("%s %s => %u\n", guarded ? "guarded" : "plain",
               state.trace, state.value);
        return;
    }
    for (int actor = 0; actor < 2; ++actor) {
        if (state.step[actor] == 2)
            continue;
        if (guarded && state.owner != -1 && state.owner != actor)
            continue; /* 解释器不让另一个角色进入该临界区。 */
        struct model next = state; /* 分支各持一份状态，避免互相污染。 */
        next.trace[depth] = (char)('A' + actor);
        next.trace[depth + 1] = '\0';
        if (next.step[actor] == 0) {
            if (guarded)
                next.owner = actor;
            next.local[actor] = next.value; /* 第一步：读到私有暂存。 */
        } else {
            next.value = next.local[actor] + 1; /* 第二步：计算并写回。 */
            if (guarded)
                next.owner = -1;
        }
        ++next.step[actor];
        explore(next, guarded, results);
    }
}

int main(void)
{
    struct model initial = { .owner = -1 };
    struct results plain = {0}, guarded = {0};
    explore(initial, false, &plain);
    explore(initial, true, &guarded);
    assert(plain.paths == 6 && plain.lost == 4);
    assert(guarded.paths == 2 && guarded.lost == 0);
    printf("plain: %u paths, %u lost; guarded: %u paths, %u lost\n",
           plain.paths, plain.lost, guarded.paths, guarded.lost);
    return 0;
}
