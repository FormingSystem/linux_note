/* 教学模型：只演示单线程拓扑，不是 Linux list.h 的替代品。 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

struct link {
    struct link *next;
    struct link *prev;
};
struct task {
    int number;
    struct link queue_link;
};

static void init_link(struct link *link)
{
    link->next = link;
    link->prev = link;
}

static void add_tail(struct link *node, struct link *head)
{
    struct link *tail = head->prev;

    node->prev = tail;
    node->next = head;
    tail->next = node;
    head->prev = node;
}

static void detach(struct link *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    init_link(node);
}

static struct task *task_from_link(struct link *link)
{
    return (struct task *)((char *)link - offsetof(struct task, queue_link));
}

static void print_queue(struct link *head)
{
    struct link *cursor;

    for (cursor = head->next; cursor != head; cursor = cursor->next) {
        assert(cursor->next->prev == cursor);
        assert(cursor->prev->next == cursor);
        printf("%d ", task_from_link(cursor)->number);
    }
    putchar('\n');
}

int main(void)
{
    struct link ready;
    struct task tasks[3] = {{.number = 10}, {.number = 20}, {.number = 30}};
    size_t index;

    init_link(&ready);
    assert(ready.next == &ready && ready.prev == &ready);
    for (index = 0; index < 3; ++index) {
        init_link(&tasks[index].queue_link);
        add_tail(&tasks[index].queue_link, &ready);
    }
    print_queue(&ready);
    detach(&tasks[1].queue_link);
    print_queue(&ready);
    assert(tasks[1].number == 20); /* 摘链没有结束对象寿命。 */
    add_tail(&tasks[1].queue_link, &ready);
    print_queue(&ready);
    for (index = 0; index < 3; ++index)
        detach(&tasks[index].queue_link);
    assert(ready.next == &ready && ready.prev == &ready);
    return 0;
}
