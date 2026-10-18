#include <stdbool.h>
#include <stdio.h>

struct policy_result {
    bool checked_input;
    bool ran_command;
};

static struct policy_result decide(int delay, const char *command,
                                   bool pending_key)
{
    struct policy_result result = {false, false};
    bool aborted = false;

    /* 只建模普通单键策略；特殊值在读取输入之前处理。 */
    if (command == NULL || delay == -1)
        return result;
    if (delay >= 0) {
        result.checked_input = true;
        aborted = pending_key;
    }
    result.ran_command = !aborted;
    return result;
}

int main(void)
{
    const int delays[] = {-2, -1, 0, 3};
    unsigned int i;

    for (i = 0; i < sizeof(delays) / sizeof(delays[0]); ++i) {
        struct policy_result r = decide(delays[i], "echo ready", true);
        printf("delay=%d input=%d command=%d\n", delays[i],
               r.checked_input, r.ran_command);
    }

    /* 未设置命令时，不能因为强制启动值而凭空产生命令。 */
    printf("missing=%d\n", decide(-2, NULL, false).ran_command);
    return 0;
}
