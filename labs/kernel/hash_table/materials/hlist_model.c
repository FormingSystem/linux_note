/* 单线程教学模型：pprev 保存“通向本节点的指针槽地址”。 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

struct link {
    struct link *next;
    struct link **pprev;
};
struct bucket {
    struct link *first;
};
struct task {
    int id;
    struct link node; /* 故意放在业务字段之后，节点地址不等于对象起点。 */
};

static struct task *task_from_link(struct link *node)
{
    return (struct task *)((char *)node - offsetof(struct task, node));
}

static void add_head(struct bucket *head, struct link *node)
{
    struct link *first = head->first;
    assert(node->pprev == NULL);
    node->next = first;
    if (first)
        first->pprev = &node->next;
    head->first = node;
    node->pprev = &head->first;
}

static void remove_init(struct link *node)
{
    struct link *next;
    struct link **previous_slot;
    if (!node->pprev)
        return;
    next = node->next;
    previous_slot = node->pprev;
    *previous_slot = next;
    if (next)
        next->pprev = previous_slot;
    node->next = NULL;
    node->pprev = NULL;
}

static void show_and_check(const struct bucket *head)
{
    struct link *node = head->first;
    const struct link *previous = NULL;
    while (node) {
        assert(*node->pprev == node);
        if (previous)
            assert(node->pprev == &previous->next);
        else
            assert(node->pprev == &head->first);
        printf("%d ", task_from_link(node)->id);
        previous = node;
        node = node->next;
    }
    puts(node ? "异常" : "NULL");
}

int main(void)
{
    struct bucket head = { NULL };
    struct task first = { 10, { NULL, NULL } };
    struct task middle = { 18, { NULL, NULL } };
    struct task last = { 26, { NULL, NULL } };
    struct link *cursor, *next;

    add_head(&head, &first.node);
    add_head(&head, &middle.node);
    add_head(&head, &last.node);
    show_and_check(&head); /* 26 18 10 */
    assert(middle.node.pprev == &last.node.next);
    remove_init(&middle.node);
    show_and_check(&head); /* 26 10 */
    assert(first.node.pprev == &last.node.next);
    assert(middle.node.pprev == NULL && middle.node.next == NULL);
    remove_init(&last.node);
    show_and_check(&head); /* 10，首节点回指桶头 first 的地址。 */
    remove_init(&first.node);
    remove_init(&first.node); /* 本模型的 del_init 空节点检查允许重复调用。 */
    assert(head.first == NULL);
    add_head(&head, &middle.node);
    add_head(&head, &first.node);
    show_and_check(&head); /* 10 18 */
    /* 先保存 next，再摘当前项；未假设它还能从当前项重新读出。 */
    for (cursor = head.first; cursor; cursor = next) {
        next = cursor->next;
        remove_init(cursor);
    }
    assert(head.first == NULL);
    puts("首中尾摘除、重新加入与逐项清空通过");
    return 0;
}
