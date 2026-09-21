#include <stdio.h>

struct channel {
    unsigned int number;
    int busy;
};

/* 教学程序自定义状态值，不冒充系统 errno 编号。 */
enum lookup_status {
    LOOKUP_OK = 0,
    LOOKUP_BAD_NUMBER = -1,
    LOOKUP_BUSY = -2
};

static struct channel channels[] = {
    {0, 0}, {1, 0}, {2, 1}, {3, 0}
};

static int lookup_channel(unsigned int number, struct channel **out)
{
    /* 每次调用都先清除输出，失败时不留下上次的对象。 */
    *out = NULL;
    if (number >= sizeof(channels) / sizeof(channels[0]))
        return LOOKUP_BAD_NUMBER;
    if (channels[number].busy)
        return LOOKUP_BUSY;
    *out = &channels[number];
    return LOOKUP_OK;
}

int main(void)
{
    const unsigned int requests[] = {1, 9, 2};
    struct channel *selected = NULL;
    unsigned int index;

    for (index = 0; index < sizeof(requests) / sizeof(requests[0]); ++index) {
        int status = lookup_channel(requests[index], &selected);

        if (status != LOOKUP_OK) {
            printf("request=%u status=%d empty=%d\n",
                   requests[index], status, selected == NULL);
            continue;
        }
        printf("request=%u channel=%u\n", requests[index], selected->number);
    }
    return 0;
}
