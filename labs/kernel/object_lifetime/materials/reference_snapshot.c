#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* 顺序责任模型：refs 不是原子变量，不用于真实并发。 */
struct object {
    unsigned int refs;
    int value;
};
static unsigned int released;

static void put(struct object *obj)
{
    assert(obj->refs > 0);
    if (--obj->refs == 0) {
        ++released; /* 只在对象外保存回收记录。 */
        free(obj);
    }
}

static bool run(bool hold)
{
    struct object *obj = malloc(sizeof(*obj));
    if (!obj)
        return false;
    *obj = (struct object){ .refs = 1, .value = 42 };
    unsigned int before = released;

    /* 此时原持有者尚未退出，观察者暂时借用这个地址。 */
    if (hold)
        ++obj->refs; /* 在已有一份的保护下为观察者追加独立责任。 */
    unsigned int saved = obj->refs;

    put(obj); /* 原持有者先结束；之后不能再依赖它保活。 */
    bool original_released = released != before;
    assert(original_released == !hold);
    printf("hold=%u saved=%u original_released=%u\n",
           hold ? 1u : 0u, saved, original_released ? 1u : 0u);

    if (hold) {
        /* 能读字段的依据是尚未归还的观察者责任，不是 saved。 */
        assert(obj->refs == 1 && obj->value == 42);
        put(obj);
    }
    /* 无持有分支从第一次 put 后就不再读取、比较或传递旧 obj。 */
    assert(released == before + 1);
    return true;
}

int main(void)
{
    if (!run(false) || !run(true)) {
        fputs("allocation failed\n", stderr);
        return EXIT_FAILURE;
    }
    assert(released == 2);
    return EXIT_SUCCESS;
}
