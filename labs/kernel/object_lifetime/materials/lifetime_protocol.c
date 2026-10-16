#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* 模型检查始终求值，不由 NDEBUG 决定是否执行。 */
static void require_model(bool condition)
{
    if (!condition) {
        fputs("lifetime invariant failed\n", stderr);
        exit(EXIT_FAILURE);
    }
}

enum owner {
    OWNER_CREATOR = 1u,
    OWNER_CONTAINER = 2u,
    OWNER_READER = 4u
};

/* 观察者保存的协议模型，不是业务对象，更不是内核 kref 的字段。 */
struct model {
    bool storage_exists;
    bool visible;
    bool accepting;
    unsigned int owners;
    unsigned int releases;
};

static void take(struct model *m, unsigned int owner)
{
    require_model(m->storage_exists && m->owners && !(m->owners & owner));
    m->owners |= owner; /* 外部账本记录新增的一份责任。 */
}

static void drop(struct model *m, unsigned int owner)
{
    require_model(m->storage_exists && (m->owners & owner));
    m->owners &= ~owner;
    if (!m->owners) {
        require_model(!m->visible && !m->accepting);
        m->storage_exists = false;
        ++m->releases; /* 模型记录直接回收，没有真实 free。 */
    }
}

static bool lookup(struct model *m)
{
    if (!m->visible)
        return false;
    require_model(m->owners & OWNER_CONTAINER);
    take(m, OWNER_READER);
    return true;
}

static bool read_value(const struct model *m, int *value)
{
    require_model(m->storage_exists && (m->owners & OWNER_READER));
    if (!m->accepting)
        return false;
    *value = 42;
    return true;
}

static void close_entry(struct model *m)
{
    require_model(m->visible && (m->owners & OWNER_CONTAINER));
    m->visible = false;
    m->accepting = false;
    drop(m, OWNER_CONTAINER);
}

int main(void)
{
    for (unsigned int close_first = 0; close_first < 2; ++close_first) {
        /* 从已完成初始引用建立的 S0 开始，模型不模拟分配器。 */
        struct model m = { .storage_exists = true, .owners = OWNER_CREATOR };
        take(&m, OWNER_CONTAINER);
        m.visible = m.accepting = true;
        drop(&m, OWNER_CREATOR);
        bool found = lookup(&m);
        require_model(found);
        int value = -1;
        bool accepted = false;
        if (!close_first) {
            accepted = read_value(&m, &value);
            require_model(accepted && value == 42);
        }
        close_entry(&m);
        found = lookup(&m);
        require_model(!found);
        require_model(m.storage_exists && m.owners == OWNER_READER);
        accepted = read_value(&m, &value);
        require_model(!accepted); /* 有引用仍可能被业务拒绝。 */
        drop(&m, OWNER_READER);
        require_model(!m.storage_exists && m.releases == 1);
        printf("close_first=%u releases=%u\n", close_first, m.releases);
    }
    return 0;
}
