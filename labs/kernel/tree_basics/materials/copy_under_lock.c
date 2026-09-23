// SPDX-License-Identifier: MIT
/* 用户态双线程寿命实验：只有一个入口槽，不实现树或内核自旋锁。 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct item { int value; };
struct registry {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    struct item *slot;
    unsigned int phase;
    int read_first;
};

static void require_ok(int code)
{
    if (code != 0) {
        fprintf(stderr, "pthread error: %d\n", code);
        exit(EXIT_FAILURE);
    }
}

/* 调用时持锁；等待会放锁，返回前重新持锁，醒来必须复查条件。 */
static void wait_phase(struct registry *registry, unsigned int expected)
{
    while (registry->phase < expected)
        require_ok(pthread_cond_wait(&registry->changed, &registry->lock));
}

static void *reader(void *argument)
{
    struct registry *registry = argument;
    int found = 0, copied = -1;
    require_ok(pthread_mutex_lock(&registry->lock));
    wait_phase(registry, registry->read_first ? 0 : 1);
    if (registry->slot) {
        copied = registry->slot->value; /* 对象仍受锁保护时只复制普通值。 */
        found = 1;
    }
    ++registry->phase;
    require_ok(pthread_cond_broadcast(&registry->changed));
    require_ok(pthread_mutex_unlock(&registry->lock));

    /* 等删除者实际 free 完毕，再使用副本；没有保留对象地址。 */
    require_ok(pthread_mutex_lock(&registry->lock));
    wait_phase(registry, 2);
    require_ok(pthread_mutex_unlock(&registry->lock));
    if (found)
        printf("read-first: copy=%d remains after free\n", copied);
    else
        printf("erase-first: absent, output=%d\n", copied);
    return NULL;
}

static void *eraser(void *argument)
{
    struct registry *registry = argument;
    struct item *removed;
    require_ok(pthread_mutex_lock(&registry->lock));
    wait_phase(registry, registry->read_first ? 1 : 0);
    removed = registry->slot;
    registry->slot = NULL; /* 关闭唯一入口，后来的读者只能看到空。 */
    require_ok(pthread_mutex_unlock(&registry->lock));
    free(removed);         /* 读者从不带走指针，此时没有剩余使用者。 */

    require_ok(pthread_mutex_lock(&registry->lock));
    ++registry->phase;     /* 在释放完成之后通知实验调度条件。 */
    require_ok(pthread_cond_broadcast(&registry->changed));
    require_ok(pthread_mutex_unlock(&registry->lock));
    return NULL;
}

int main(void)
{
    for (int read_first = 1; read_first >= 0; --read_first) {
        struct registry registry = {.phase=0, .read_first=read_first};
        pthread_t read_thread, erase_thread;
        registry.slot = malloc(sizeof *registry.slot);
        if (!registry.slot)
            return EXIT_FAILURE;
        registry.slot->value = 200;
        require_ok(pthread_mutex_init(&registry.lock, NULL));
        require_ok(pthread_cond_init(&registry.changed, NULL));
        require_ok(pthread_create(&read_thread, NULL, reader, &registry));
        require_ok(pthread_create(&erase_thread, NULL, eraser, &registry));
        require_ok(pthread_join(read_thread, NULL));
        require_ok(pthread_join(erase_thread, NULL));
        if (registry.slot || registry.phase != 2)
            return EXIT_FAILURE;
        require_ok(pthread_cond_destroy(&registry.changed));
        require_ok(pthread_mutex_destroy(&registry.lock));
    }
    return EXIT_SUCCESS;
}
