/* 教学模型：固定最多十六个桶，节点由调用者持有，不进行动态分配。 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAX_BUCKETS 16

struct entry {
    unsigned int key;
    const char *value;
    struct entry *next;
};

struct table {
    struct entry *buckets[MAX_BUCKETS];
    unsigned int bucket_count;
};

static struct entry *lookup(struct table *table, unsigned int key)
{
    struct entry *entry = table->buckets[key % table->bucket_count];
    while (entry) {
        if (entry->key == key)
            return entry;
        entry = entry->next;
    }
    return NULL;
}

static bool insert(struct table *table, struct entry *entry)
{
    unsigned int bucket = entry->key % table->bucket_count;
    if (lookup(table, entry->key))
        return false; /* 重复键不改变原记录，也不接入候选。 */
    entry->next = table->buckets[bucket];
    table->buckets[bucket] = entry;
    return true;
}

static struct entry *remove_key(struct table *table, unsigned int key)
{
    struct entry **slot = &table->buckets[key % table->bucket_count];
    while (*slot) {
        struct entry *entry = *slot;
        if (entry->key == key) {
            *slot = entry->next;
            entry->next = NULL;
            return entry; /* 只撤销成员资格，存储仍归调用者。 */
        }
        slot = &entry->next;
    }
    return NULL;
}

static bool rebuild(struct table *table, unsigned int bucket_count)
{
    struct entry *new_heads[MAX_BUCKETS] = { NULL };
    unsigned int index;
    if (bucket_count == 0 || bucket_count > MAX_BUCKETS)
        return false; /* 非法参数在修改任何连接之前拒绝。 */
    for (index = 0; index < table->bucket_count; ++index) {
        struct entry *entry = table->buckets[index];
        while (entry) {
            struct entry *next = entry->next;
            unsigned int bucket = entry->key % bucket_count;
            entry->next = new_heads[bucket];
            new_heads[bucket] = entry;
            entry = next;
        }
    }
    for (index = 0; index < MAX_BUCKETS; ++index)
        table->buckets[index] = new_heads[index];
    table->bucket_count = bucket_count;
    return true;
}

int main(void)
{
    struct table table = { .bucket_count = 8 };
    struct entry entries[] = {
        { 10, "任务10", NULL },
        { 18, "任务18", NULL },
        { 26, "任务26", NULL }
    };
    struct entry duplicate = { 18, "另一任务", NULL };
    struct entry *cursor;
    unsigned int index;

    for (index = 0; index < 3; ++index)
        assert(insert(&table, &entries[index]));
    printf("桶2:");
    for (cursor = table.buckets[2]; cursor; cursor = cursor->next)
        printf(" %u", cursor->key);
    putchar('\n');
    printf("查18: %s\n", lookup(&table, 18)->value);
    assert(lookup(&table, 34) == NULL);
    puts("查34: 未找到");
    assert(!insert(&table, &duplicate));
    puts("重复18: 已拒绝");
    assert(rebuild(&table, 16));
    for (index = 0; index < 3; ++index)
        assert(lookup(&table, entries[index].key) == &entries[index]);
    puts("扩容后: 三个对象仍可按键找到");
    assert(remove_key(&table, 18) == &entries[1]);
    assert(remove_key(&table, 18) == NULL);
    assert(strcmp(lookup(&table, 10)->value, "任务10") == 0);
    assert(lookup(&table, 26) == &entries[2]);
    puts("删18后: 未找到");
    assert(remove_key(&table, 10) == &entries[0]);
    assert(remove_key(&table, 26) == &entries[2]);
    for (index = 0; index < MAX_BUCKETS; ++index)
        assert(table.buckets[index] == NULL);
    return 0;
}
