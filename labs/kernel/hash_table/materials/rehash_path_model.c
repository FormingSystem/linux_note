/* 确定性串行模型：用独立链尾对象模拟桶身份，不模拟指针位或真实并发。 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

struct node {
    int key;
    bool terminal;
    struct node *next;
};
struct table {
    struct node *head;
    struct node *end;
    struct table *future;
};
static struct node old_end = { 0, true, NULL };
static struct node new_end = { 0, true, NULL };
static struct node a = { 10, false, NULL };
static struct node b = { 18, false, NULL };
static struct node c = { 26, false, NULL };
static struct table old_table = { NULL, &old_end, NULL };
static struct table new_table = { NULL, &new_end, NULL };
static unsigned int retries;

/* 新表已通过 future 可发现。先发布尾节点到新链，再绕过旧入口。 */
static void move_tail(void)
{
    struct node **slot = &old_table.head;
    struct node *item = *slot;
    assert(old_table.future == &new_table && !item->terminal);
    while (!item->next->terminal) {
        slot = &item->next;
        item = item->next;
    }
    struct node *old_next = item->next;
    item->next = new_table.head;
    new_table.head = item;
    *slot = old_next;
}

/* hook 在读者已经拿到旧尾 C 后插入一次迁移动作。 */
static struct node *lookup(struct table *table, int key, bool hook)
{
    while (table) {
        struct node *cursor;
        do {
            cursor = table->head;
            while (!cursor->terminal) {
                if (hook && cursor == &c) {
                    move_tail();
                    hook = false;
                }
                if (cursor->key == key)
                    return cursor;
                cursor = cursor->next;
            }
            if (cursor != table->end)
                ++retries;
        } while (cursor != table->end);
        table = table->future;
    }
    return NULL;
}

int main(void)
{
    a.next = &b;
    b.next = &c;
    c.next = &old_end;
    old_table.head = &a;
    new_table.head = &new_end;
    old_table.future = &new_table;

    assert(lookup(&old_table, 99, true) == NULL);
    assert(retries == 1); /* 走到新桶的尾标记，必须重扫旧桶。 */
    assert(b.next == &old_end && new_table.head == &c);
    assert(lookup(&old_table, 26, false) == &c);
    printf("wrong_end_retries=%u moved_key=%d\n", retries, c.key);

    move_tail();
    move_tail();
    assert(old_table.head == &old_end);
    assert(lookup(&new_table, 10, false) == &a);
    assert(lookup(&new_table, 18, false) == &b);
    assert(lookup(&new_table, 26, false) == &c);
    assert(lookup(&new_table, 99, false) == NULL);
    puts("迁移后对象地址不变，三个键都可查到");
    return 0;
}
