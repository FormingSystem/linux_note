/* 用确定顺序重放两次插入，不制造真实 C 数据竞争。 */
#include <assert.h>
#include <stdio.h>

struct node {
    struct node *next;
    struct node *prev;
};

static void finish_insert(struct node *head, struct node *item,
                          struct node *old_tail)
{
    item->prev = old_tail;
    item->next = head;
    old_tail->next = item;
    head->prev = item;
}

int main(void)
{
    struct node head = { &head, &head };
    struct node node_a = { NULL, NULL };
    struct node node_b = { NULL, NULL };
    /* 两个调用者在任何修改开始前都读取了同一个旧队尾。 */
    struct node *old_tail_a = head.prev;
    struct node *old_tail_b = head.prev;

    finish_insert(&head, &node_a, old_tail_a);
    finish_insert(&head, &node_b, old_tail_b);
    assert(head.next == &node_b && head.prev == &node_b);
    assert(node_b.next == &head && node_b.prev == &head);
    assert(node_a.next == &head && node_a.prev == &head);
    puts("从 head 可达的是 B，A 的对象仍存在但成员关系已丢失");
    return 0;
}
