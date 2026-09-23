#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

enum work_state {
    IDLE,       /* 尚未预留 */
    RESERVED,   /* 已预留，尚未交付 */
    PENDING,    /* 接收成功，等待执行 */
    RUNNING,    /* 执行者正在使用 */
    DONE,       /* 执行结束，已归还工作份额 */
    CANCELED,   /* 待执行实例被取消，取消者接管归还 */
    REJECTED   /* 提交拒绝，预留已经收回 */
};
struct ledger {
    unsigned int refs;
    bool creator;
    bool ticket;
    enum work_state state;
    unsigned int runs;
    unsigned int releases;
};

/* 外部观察账本，不是真实 work_struct，也不分配或释放业务对象。 */
static void check(const struct ledger *book)
{
    assert(book->refs == (book->creator ? 1u : 0u) + (book->ticket ? 1u : 0u));
    assert(book->releases == (book->refs ? 0u : 1u));
}

static void put_one(struct ledger *book, bool *owner)
{
    assert(*owner && book->refs);
    *owner = false;
    if (--book->refs == 0)
        ++book->releases;
    check(book);
}

static void reserve(struct ledger *book)
{
    assert(book->creator && !book->ticket && book->state == IDLE);
    book->ticket = true;
    ++book->refs;
    book->state = RESERVED;
    check(book);
}

static void submit(struct ledger *book, bool accept)
{
    assert(book->state == RESERVED && book->ticket);
    book->state = accept ? PENDING : REJECTED;
    if (!accept)
        put_one(book, &book->ticket); /* 提交者收回本次未交出的预留。 */
}

static void start_work(struct ledger *book)
{
    assert(book->state == PENDING && book->ticket);
    book->state = RUNNING;
    ++book->runs;
}

static void finish_work(struct ledger *book)
{
    assert(book->state == RUNNING);
    book->state = DONE;
    put_one(book, &book->ticket); /* 执行者归还这一实例的责任。 */
}

static bool cancel_sync_model(struct ledger *book)
{
    assert(book->creator); /* 管理者在取消过程中保留自己的份额。 */
    if (book->state == PENDING) {
        book->state = CANCELED;
        return true; /* 待执行实例被取消；函数本身不代替调用者 put。 */
    }
    if (book->state == RUNNING)
        finish_work(book); /* 显式安排执行者结束，代替真实等待。 */
    return false;
}

int main(void)
{
    for (unsigned int path = 0; path < 6; ++path) {
        struct ledger book = { .refs = 1, .creator = true, .state = IDLE };
        reserve(&book);
        submit(&book, path != 0);
        bool canceled = false;
        if (path == 4) {
            put_one(&book, &book.creator); /* 创建者先退出，之后不再取消。 */
            start_work(&book);
            finish_work(&book);
        } else if (path == 5) {
            start_work(&book);
            finish_work(&book);
            put_one(&book, &book.creator);
        } else {
            if (path == 2 || path == 3)
                start_work(&book);
            if (path == 2)
                finish_work(&book);
            canceled = cancel_sync_model(&book);
            if (canceled)
                put_one(&book, &book.ticket); /* 只接管明确取消的那一实例。 */
            put_one(&book, &book.creator);
        }
        assert(canceled == (path == 1));
        assert(book.runs == (path >= 2 ? 1u : 0u));
        assert(book.releases == 1 && book.refs == 0);
        printf("path=%u canceled=%u runs=%u releases=%u\n",
               path, canceled ? 1u : 0u, book.runs, book.releases);
    }
    return 0;
}
