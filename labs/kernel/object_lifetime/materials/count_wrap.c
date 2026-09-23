#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* 缩小到八位的串行教学模型，不是 Linux refcount_t 的实现。 */
struct guarded_count {
    uint8_t value;
    bool saturated;
};

static void add_guarded(struct guarded_count *count)
{
    if (count->saturated)
        return;
    if (count->value == 127) {
        count->saturated = true;
        count->value = 192; /* 教学标记，进入后不再作为真实责任数。 */
        return;
    }
    assert(count->value > 0);
    ++count->value;
}

static bool drop_guarded(struct guarded_count *count)
{
    if (count->saturated)
        return false;
    assert(count->value > 0);
    return --count->value == 0;
}

int main(void)
{
    unsigned int owners = 1;
    uint8_t wrapping = 1;
    struct guarded_count guarded = { .value = 1, .saturated = false };

    for (unsigned int i = 0; i < 256; ++i) {
        ++owners;
        wrapping = (uint8_t)(wrapping + 1u);
        add_guarded(&guarded);
    }
    --owners;
    wrapping = (uint8_t)(wrapping - 1u);
    bool guarded_release = drop_guarded(&guarded);
    assert(owners == 256 && wrapping == 0 && !guarded_release);
    printf("owners=%u wrapping=%u guarded_release=%d\n",
           owners, (unsigned int)wrapping, guarded_release);

    while (owners) {
        --owners;
        assert(!drop_guarded(&guarded));
    }
    assert(guarded.saturated && guarded.value == 192);
    puts("all owners left; saturated model still refuses release");
    return 0;
}
