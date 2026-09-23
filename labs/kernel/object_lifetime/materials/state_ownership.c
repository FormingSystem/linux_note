// SPDX-License-Identifier: GPL-2.0
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* 顺序协议模型：每次函数调用代表一个已串行化步骤，不实现线程或Linux锁。 */
enum object_state { OBJECT_NEW, OBJECT_LIVE, OBJECT_CLOSING };
struct request {
    unsigned int refs, active, completed, notifications;
    enum object_state state;
    bool drained;
};
struct active_token { struct request *object; };
static struct request *registry; /* 拥有型单槽，非空时持有一份。 */
static unsigned int live_objects, releases;

static struct request *request_create(bool fail)
{
    if (fail)
        return NULL; /* 显式注入分配失败。 */
    struct request *obj = calloc(1, sizeof(*obj));
    if (!obj)
        return NULL;
    obj->refs = 1;
    obj->state = OBJECT_NEW;
    ++live_objects;
    return obj;
}
static void request_get(struct request *obj)
{
    assert(obj && obj->refs);
    ++obj->refs;
}
static void request_put(struct request *obj)
{
    assert(obj && obj->refs);
    if (--obj->refs)
        return;
    assert(registry != obj && obj->active == 0);
    --live_objects;
    ++releases;
    free(obj); /* NEW的初始化失败也允许直接最终回收。 */
}
static bool request_publish(struct request *obj)
{
    assert(obj && obj->refs);
    if (registry || obj->state != OBJECT_NEW)
        return false;
    request_get(obj);
    obj->state = OBJECT_LIVE;
    registry = obj;
    return true;
}
static struct request *request_lookup_get(void)
{
    if (!registry || registry->state != OBJECT_LIVE)
        return NULL;
    request_get(registry);
    return registry;
}
static bool request_unpublish(struct request *obj)
{
    if (registry != obj)
        return false;
    registry = NULL;
    request_put(obj); /* 只在实际移除时消费登记份额。 */
    return true;
}
static bool request_begin(struct request *obj, struct active_token *token)
{
    assert(obj && obj->refs && token->object == NULL);
    if (obj->state != OBJECT_LIVE)
        return false;
    ++obj->active;
    request_get(obj); /* 这次活动除登记外，还独立拥有存储份额。 */
    token->object = obj;
    return true;
}
static void report_drained(struct request *obj)
{
    if (obj->state == OBJECT_CLOSING && obj->active == 0 && !obj->drained) {
        obj->drained = true;
        ++obj->notifications; /* 模拟一次关闭事件发布，不是实际唤醒原语。 */
    }
}
static void request_close(struct request *obj)
{
    assert(obj && obj->refs);
    obj->state = OBJECT_CLOSING;
    report_drained(obj);
}
static void request_use(struct active_token *token)
{
    assert(token->object && token->object->active);
    ++token->object->completed; /* 关门前接纳的活动可以继续，关闭者须等它结束。 */
}
static void request_finish(struct active_token *token)
{
    struct request *obj = token->object;
    assert(obj && obj->active);
    token->object = NULL;
    --obj->active;
    report_drained(obj);
    request_put(obj); /* 此后不再访问obj；这是活动份额的归还。 */
}
static void reset_case(void)
{
    assert(!registry && !live_objects);
    releases = 0;
}
int main(void)
{
    struct request *owner, *reader, *other;
    struct active_token first = { NULL }, second = { NULL };

    reset_case(); /* 1：分配失败没有对象或责任。 */
    assert(request_create(true) == NULL && releases == 0);
    reset_case(); /* 2：未发布的NEW可以直接结束。 */
    owner = request_create(false); assert(owner);
    request_put(owner); assert(releases == 1);
    reset_case(); /* 3：完整关闭，旧活动结束才报告排空。 */
    owner = request_create(false); assert(owner && request_publish(owner));
    reader = request_lookup_get(); assert(reader == owner);
    assert(request_begin(reader, &first));
    request_close(owner); assert(!owner->drained && registry == owner);
    assert(request_lookup_get() == NULL && !request_begin(reader, &second));
    assert(request_unpublish(owner) && !request_unpublish(owner));
    request_use(&first); request_finish(&first);
    assert(owner->drained && owner->notifications == 1 && owner->refs == 2);
    request_put(reader); request_put(owner); assert(releases == 1);
    reset_case(); /* 4：只有旧引用、没有活动，关门立即报告排空。 */
    owner = request_create(false); assert(owner && request_publish(owner));
    reader = request_lookup_get(); request_close(owner);
    assert(owner->drained && !request_begin(reader, &first));
    request_unpublish(owner); request_put(owner);
    assert(releases == 0); request_put(reader); assert(releases == 1);
    reset_case(); /* 5：多活动汇聚，第一次结束不能提前完成。 */
    owner = request_create(false); assert(owner && request_publish(owner));
    assert(request_begin(owner, &first) && request_begin(owner, &second));
    request_close(owner); request_unpublish(owner); request_finish(&first);
    assert(!owner->drained && owner->active == 1);
    request_finish(&second); assert(owner->drained && owner->notifications == 1);
    request_put(owner);
    reset_case(); /* 6：重复关闭只发布一次事件。 */
    owner = request_create(false); assert(owner && request_publish(owner));
    request_close(owner); request_close(owner);
    assert(owner->notifications == 1); request_unpublish(owner); request_put(owner);
    reset_case(); /* 7：关闭后的NEW不再允许发布。 */
    owner = request_create(false); assert(owner); request_close(owner);
    assert(!request_publish(owner)); request_put(owner);
    reset_case(); /* 8：占用槽位时第二对象发布失败，不产生表份额。 */
    owner = request_create(false); other = request_create(false);
    assert(owner && other && request_publish(owner) && !request_publish(other));
    assert(other->refs == 1 && other->state == OBJECT_NEW); request_put(other);
    request_close(owner); request_unpublish(owner); request_put(owner); assert(releases == 2);
    reset_case(); /* 9：LIVE快照不能作为后续业务的进入票据。 */
    owner = request_create(false); assert(owner && request_publish(owner));
    bool was_live = owner->state == OBJECT_LIVE;
    request_close(owner); assert(was_live && owner->drained);
    assert(!request_begin(owner, &first));
    request_unpublish(owner); request_put(owner);
    reset_case(); /* 10：最后一个活动也可以成为最终回收者。 */
    owner = request_create(false); assert(owner && request_publish(owner));
    assert(request_begin(owner, &first)); request_close(owner); request_unpublish(owner);
    request_put(owner); assert(releases == 0);
    request_use(&first); request_finish(&first); assert(releases == 1 && !first.object);
    reset_case();
    puts("10 state/ownership paths passed; no threads or Linux synchronization simulated");
    return 0;
}
