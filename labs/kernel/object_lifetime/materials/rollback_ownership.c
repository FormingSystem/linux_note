// SPDX-License-Identifier: GPL-2.0
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* 顺序教学模型：refs不是Linux原子kref，channel不是硬件句柄。 */
struct channel { char *buffer; };
struct request {
    unsigned int refs;
    char *buffer;
    struct channel *channel;
};
enum cleanup_policy { EXPLICIT_PARTS, FINAL_PARTS };
static struct request *registry;
static unsigned int objects, buffers, channels, releases;
static unsigned int tick, channel_end, buffer_end, object_end;

/* 检查始终执行，定义NDEBUG也不跳过责任核对。 */
static void require_model(bool condition)
{
    if (!condition) {
        fputs("rollback ownership invariant failed\n", stderr);
        exit(EXIT_FAILURE);
    }
}

static void dispose_parts(struct request *request)
{
    if (request->channel) {
        require_model(request->channel->buffer == request->buffer);
        free(request->channel);
        request->channel = NULL; /* 清空后不再把这项责任留给最终清理。 */
        --channels;
        channel_end = ++tick;
    }
    if (request->buffer) {
        free(request->buffer);
        request->buffer = NULL;
        --buffers;
        buffer_end = ++tick;
    }
}
static void request_get(struct request *request)
{
    require_model(request && request->refs);
    ++request->refs;
}
static void request_put(struct request *request)
{
    require_model(request && request->refs);
    if (--request->refs)
        return;
    require_model(registry != request); /* 发布份额必须先被真正撤下。 */
    dispose_parts(request);
    ++releases;
    --objects;
    object_end = ++tick;
    free(request);
}
static void publish(struct request *request)
{
    require_model(!registry && request->buffer && request->channel);
    request_get(request); /* 登记槽额外拥有一份，创建者仍保留原份额。 */
    registry = request;
}
static bool unpublish(struct request *request)
{
    /* 调用者始终有自己的有效份额；重复撤下不消费它。 */
    if (registry != request)
        return false;
    registry = NULL;
    request_put(request);
    return true;
}
static void finish_creator(struct request *request, enum cleanup_policy policy)
{
    unpublish(request);
    /* 本顺序模型没有查找者或异步借用者；现实系统必须另行证明排空。 */
    require_model(request->refs == 1);
    if (policy == EXPLICIT_PARTS)
        dispose_parts(request);
    request_put(request); /* 最后清理仍检查空字段，不会重复free。 */
}
static struct request *create_request(unsigned int fail_stage,
                                      enum cleanup_policy policy)
{
    struct request *request;
    if (fail_stage == 1)
        return NULL;
    request = calloc(1, sizeof(*request));
    if (!request)
        return NULL;
    ++objects;
    request->refs = 1;
    if (fail_stage == 2)
        goto fail;
    request->buffer = malloc(16);
    if (!request->buffer)
        goto fail;
    ++buffers;
    if (fail_stage == 3)
        goto fail;
    request->channel = malloc(sizeof(*request->channel));
    if (!request->channel)
        goto fail;
    request->channel->buffer = request->buffer;
    ++channels;
    if (fail_stage == 4)
        goto fail;
    publish(request);
    if (fail_stage == 5)
        goto fail; /* 槽已经可见，先撤下其份额，再归还创建者。 */
    return request;
fail:
    finish_creator(request, policy);
    return NULL; /* 失败没有向调用者交付可put的对象。 */
}
int main(void)
{
    for (unsigned int policy = EXPLICIT_PARTS; policy <= FINAL_PARTS; ++policy) {
        for (unsigned int stage = 0; stage <= 5; ++stage) {
            struct request *request;
            bool removed;
            require_model(!registry && !objects && !buffers && !channels);
            releases = tick = channel_end = buffer_end = object_end = 0;
            request = create_request(stage, (enum cleanup_policy)policy);
            if (stage == 0) {
                require_model(request && registry == request && request->refs == 2);
                removed = unpublish(request);
                require_model(removed);
                removed = unpublish(request);
                require_model(!removed && request->refs == 1);
                finish_creator(request, (enum cleanup_policy)policy);
            } else {
                require_model(!request);
            }
            require_model(!registry && !objects && !buffers && !channels);
            require_model(releases == (stage == 1 ? 0u : 1u));
            if (channel_end)
                require_model(buffer_end && channel_end < buffer_end);
            if (buffer_end)
                require_model(buffer_end < object_end);
            printf("policy=%u stage=%u releases=%u cleanup=%u/%u/%u\n",
                   policy, stage, releases, channel_end, buffer_end, object_end);
        }
    }
    puts("12 rollback paths passed");
    return 0;
}
