/* C11 单线程所有权模型：两个索引入口、一个借出者，不实现树或 RCU。 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct job {
    unsigned int id;
    unsigned int references;
};

static unsigned int destroyed;

static void job_get(struct job *item)
{
    assert(item && item->references > 0);
    ++item->references;
}

static void job_put(struct job *item)
{
    assert(item && item->references > 0);
    if (--item->references == 0) {
        ++destroyed;
        free(item);
    }
}

static struct job *lookup_get(struct job *index)
{
    /* 真实并发版本必须保护“找到入口至取得引用”的整个窗口。 */
    if (index)
        job_get(index);
    return index;
}

static void withdraw(struct job **index)
{
    struct job *old = *index;
    *index = NULL;                 /* 先撤入口，再放弃该入口持有的引用。 */
    if (old)
        job_put(old);
}

int main(void)
{
    struct job *item = malloc(sizeof *item);
    if (!item)
        return EXIT_FAILURE;
    *item = (struct job){ .id = 7, .references = 1 };
    puts("S0 private: creator owns 1 reference");

    job_get(item);
    struct job *by_time = item;
    job_get(item);
    struct job *by_id = item;
    job_put(item);                 /* 交出创建者引用，两个索引各保留一个。 */
    item = NULL;
    printf("S1 published: references=%u\n", by_time->references);

    struct job *reader = lookup_get(by_time);
    assert(reader && reader->id == 7);
    printf("S2 reader holds: references=%u\n", reader->references);
    withdraw(&by_time);
    assert(!by_time && by_id && destroyed == 0);
    printf("S3 first index closed: references=%u\n", reader->references);

    withdraw(&by_id);
    assert(!by_time && !by_id && lookup_get(by_time) == NULL);
    assert(reader->references == 1 && destroyed == 0);
    printf("S4 all entries closed: reader still uses id=%u\n", reader->id);
    job_put(reader);
    reader = NULL;                /* 最后放弃后不再读取已销毁对象。 */
    assert(destroyed == 1);
    printf("S5 final put: destroyed=%u\n", destroyed);
    return EXIT_SUCCESS;
}
