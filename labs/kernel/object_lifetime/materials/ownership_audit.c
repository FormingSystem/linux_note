// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

enum event_kind {
    CREATE, SHARE_WORK, TRANSFER_WORK, DROP_CREATOR, DROP_WORK, USE_WORK
};
enum verdict { BALANCED, INVALID_OWNER, LEFTOVER, INCOMPLETE, STILL_OPEN };

/* 离线责任账本，不实现内核计数，也不实际释放或访问悬空内存。 */
static enum verdict audit(const enum event_kind *events, size_t count,
                          bool complete, bool closed)
{
    unsigned int creator = 0, worker = 0;
    bool created = false;
    if (!complete)
        return INCOMPLETE; /* 记录缺失时不能把缺少get判为真实少get。 */
    for (size_t i = 0; i < count; ++i) {
        switch (events[i]) {
        case CREATE:
            if (created)
                return INVALID_OWNER;
            created = true;
            creator = 1;
            break;
        case SHARE_WORK:
            if (!creator)
                return INVALID_OWNER;
            ++worker; /* 约定：创建者持份额时为工作者新取一份。 */
            break;
        case TRANSFER_WORK:
            if (!creator)
                return INVALID_OWNER;
            --creator;
            ++worker; /* 转交不增加总份额。 */
            break;
        case DROP_CREATOR:
            if (!creator)
                return INVALID_OWNER;
            --creator;
            break;
        case DROP_WORK:
            if (!worker)
                return INVALID_OWNER;
            --worker;
            break;
        case USE_WORK:
            if (!worker)
                return INVALID_OWNER;
            break;
        default:
            return INCOMPLETE;
        }
    }
    if (!created)
        return INCOMPLETE;
    if (!closed)
        return STILL_OPEN; /* 业务未结束时，有份额不等于泄漏。 */
    return creator || worker ? LEFTOVER : BALANCED;
}

int main(void)
{
    static const enum event_kind shared[] = {
        CREATE, SHARE_WORK, DROP_CREATOR, USE_WORK, DROP_WORK
    };
    static const enum event_kind missing_get[] = {
        CREATE, DROP_CREATOR, USE_WORK
    };
    static const enum event_kind duplicate_put[] = {
        CREATE, TRANSFER_WORK, DROP_CREATOR, USE_WORK, DROP_WORK
    };
    static const enum event_kind missing_put[] = {
        CREATE, SHARE_WORK, DROP_CREATOR, USE_WORK
    };
    static const struct {
        const char *name;
        const enum event_kind *events;
        size_t count;
        bool complete, closed;
        enum verdict expected;
    } cases[] = {
        {"shared", shared, 5, true, true, BALANCED},
        {"missing_get", missing_get, 3, true, true, INVALID_OWNER},
        {"transfer_then_put", duplicate_put, 5, true, true, INVALID_OWNER},
        {"missing_put", missing_put, 4, true, true, LEFTOVER},
        {"dropped_records", shared, 5, false, true, INCOMPLETE},
        {"active_worker", missing_put, 4, true, false, STILL_OPEN}
    };
    static const char *const names[] = {
        "balanced", "invalid_owner", "leftover", "incomplete", "still_open"
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        enum verdict result = audit(cases[i].events, cases[i].count,
                                    cases[i].complete, cases[i].closed);
        assert(result == cases[i].expected);
        printf("%s: %s\n", cases[i].name, names[result]);
    }
    return 0;
}
