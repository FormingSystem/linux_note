#include <stdbool.h>
#include <stdio.h>

enum event_id { write_data, write_flag, read_flag, read_data, event_count };

static bool has_cycle(bool release, bool acquire)
{
    bool reach[event_count][event_count] = {{false}};
    // 固定候选：读flag来自写1，读data来自初始0；全部为标记访问。
    reach[write_flag][read_flag] = true; // 跨线程读取来源rfe。
    if (release) {
        reach[write_data][write_flag] = true; // po-rel进入ppo。
        // fr(data) -> release -> rfe(flag)构成prop的同线程回边。
        reach[read_data][read_flag] = true;
    }
    if (acquire)
        reach[read_flag][read_data] = true; // acq-po进入ppo。

    // 求可达闭包；从任一事件重新到达自身就发现环。
    for (int mid = 0; mid < event_count; ++mid)
        for (int from = 0; from < event_count; ++from)
            for (int to = 0; to < event_count; ++to)
                reach[from][to] = reach[from][to] ||
                    (reach[from][mid] && reach[mid][to]);
    for (int event = 0; event < event_count; ++event)
        if (reach[event][event])
            return true;
    return false;
}

int main(void)
{
    for (int release = 0; release <= 1; ++release) {
        for (int acquire = 0; acquire <= 1; ++acquire) {
            const bool cycle = has_cycle(release != 0, acquire != 0);
            printf("release=%d acquire=%d hb_cycle=%d\n",
                   release, acquire, cycle);
            if (cycle != (release != 0 && acquire != 0))
                return 1;
        }
    }
    // 仅核对正文给定的四节点子图，不解析litmus/cat，不判断完整LKMM。
    return 0;
}
