/* 单线程教学模型：一个平面分组、固定记录数组，不模拟内核链表或锁。 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

struct resource {
    char name;
    bool live;
};
struct record {
    struct resource *data;
    bool active;
};
struct ledger {
    struct record records[8];
    unsigned int count;
    unsigned int first;
    unsigned int end;
    bool group;
    bool closed;
    char released[9];
    unsigned int released_count;
};

static void cleanup(struct ledger *book, struct resource *item)
{
    assert(item->live && book->released_count < 8);
    item->live = false;
    book->released[book->released_count++] = item->name;
    book->released[book->released_count] = '\0';
}

static bool add_action(struct ledger *book, struct resource *item,
                       bool fail_record, bool reset_on_failure)
{
    assert(item->live);
    if (fail_record) {
        if (reset_on_failure)
            cleanup(book, item); /* 记录没建立，直接履行清理责任。 */
        return false;
    }
    assert(book->count < 8);
    book->records[book->count++] = (struct record){item, true};
    return true;
}

static void open_group(struct ledger *book)
{
    assert(!book->group); /* 本模型不支持嵌套；内核支持合法嵌套。 */
    book->first = book->count;
    book->group = true;
    book->closed = false;
}

static void close_group(struct ledger *book)
{
    assert(book->group && !book->closed);
    book->end = book->count;
    book->closed = true; /* 划定边界，不执行任何资源回调。 */
}

static void remove_group(struct ledger *book)
{
    assert(book->group);
    book->group = false; /* 仅删除分组资格，记录仍属于设备账本。 */
}

static void release_range(struct ledger *book, unsigned int first,
                          unsigned int end)
{
    while (end > first) {
        struct record *entry = &book->records[--end];
        if (entry->active) {
            entry->active = false; /* 先摘下，再执行清理。 */
            cleanup(book, entry->data);
        }
    }
}

static void release_group(struct ledger *book)
{
    assert(book->group);
    release_range(book, book->first, book->closed ? book->end : book->count);
    book->group = false;
}

static void release_all(struct ledger *book)
{
    release_range(book, 0, book->count);
    book->group = false;
}

static void group_case(unsigned int scene)
{
    struct ledger book = {0};
    struct resource a = {'A', true}, b = {'B', true}, c = {'C', true};
    assert(add_action(&book, &a, false, false));
    open_group(&book);
    assert(add_action(&book, &b, false, false));
    if (scene != 0)
        close_group(&book);
    assert(add_action(&book, &c, false, false));
    if (scene == 2)
        remove_group(&book);
    else if (scene != 3)
        release_group(&book);
    assert(strcmp(book.released, scene == 0 ? "CB" : scene == 1 ? "B" : "") == 0);
    assert(a.live && b.live == (scene >= 2) && c.live == (scene != 0));
    printf("group=%u before-detach=%s ", scene, book.released);
    release_all(&book);
    assert(!a.live && !b.live && !c.live);
    assert(strcmp(book.released, scene == 1 ? "BCA" : "CBA") == 0);
    printf("all=%s\n", book.released);
    release_all(&book); /* 已摘记录不能被第二次回收。 */
    assert(book.released_count == 3);
}

static void failure_case(bool reset_on_failure)
{
    struct ledger book = {0};
    struct resource a = {'A', true}, b = {'B', true};
    assert(add_action(&book, &a, false, false));
    assert(!add_action(&book, &b, true, reset_on_failure));
    assert(b.live == !reset_on_failure && book.count == 1);
    if (!reset_on_failure)
        cleanup(&book, &b); /* 普通add失败后责任仍由调用者承担。 */
    release_all(&book);
    assert(!a.live && !b.live && strcmp(book.released, "BA") == 0);
    printf("reset=%u registered=1 all=%s\n", reset_on_failure, book.released);
}

int main(void)
{
    for (unsigned int scene = 0; scene < 4; ++scene)
        group_case(scene);
    failure_case(false);
    failure_case(true);
    return 0;
}
