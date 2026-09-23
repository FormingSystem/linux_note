/* C11顺序模型：不调用Linux devres、kref、锁或真实设备接口。 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct device_model {
    unsigned int refs;
    bool resources_live, released;
};
struct context {
    struct device_model *owner;
    unsigned int refs, active;
    bool closing, drained;
};
static struct context *registry;
static unsigned int context_releases;

static void device_put(struct device_model *dev)
{
    assert(dev->refs && !dev->released);
    if (--dev->refs == 0)
        dev->released = true; /* 外壳用栈存储，只模拟其逻辑回收。 */
}
static void context_get(struct context *ctx)
{
    assert(ctx && ctx->refs);
    ++ctx->refs;
}
static void context_put(struct context *ctx)
{
    assert(ctx && ctx->refs);
    if (--ctx->refs != 0)
        return;
    assert(ctx->closing && ctx->drained && !ctx->active);
    assert(registry != ctx && !ctx->owner->resources_live);
    device_put(ctx->owner); /* 先结束桥接责任，此后不再访问owner。 */
    ++context_releases;
    free(ctx);
}
static struct context *create_binding(struct device_model *dev, bool fail)
{
    if (fail)
        return NULL; /* 注入创建前失败，尚无桥接或登记责任。 */
    struct context *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;
    ctx->refs = 1; /* 绑定期份额：假定已经交给最后执行的清理记录。 */
    ctx->owner = dev;
    ++dev->refs;
    dev->resources_live = true;
    assert(!registry);
    context_get(ctx); /* 拥有型发布槽持有一份。 */
    registry = ctx;
    return ctx;
}
static struct context *open_session(void)
{
    if (!registry || registry->closing)
        return NULL;
    context_get(registry);
    return registry; /* 模拟一个独立打开文件实例的份额。 */
}
static bool begin_operation(struct context *ctx)
{
    assert(ctx && ctx->refs);
    if (ctx->closing)
        return false;
    assert(ctx->owner->resources_live);
    ++ctx->active;
    context_get(ctx);
    return true;
}
static void close_gate(struct context *ctx)
{
    assert(registry == ctx);
    ctx->closing = true;
    registry = NULL;
    context_put(ctx); /* 撤下发布槽的份额，绑定期份额仍在。 */
    ctx->drained = (ctx->active == 0);
}
static void finish_operation(struct context *ctx)
{
    assert(ctx->active && ctx->owner->resources_live);
    --ctx->active;
    if (ctx->closing && !ctx->active)
        ctx->drained = true; /* 只模拟汇聚条件，不实现通知和等待。 */
    context_put(ctx);
}
static void finish_binding(struct context *ctx)
{
    assert(ctx->closing && ctx->drained && !ctx->active);
    ctx->owner->resources_live = false; /* 模拟IRQ、时钟、映射等先退出。 */
    context_put(ctx); /* 最后才消费绑定期记录中的私有对象份额。 */
}
int main(void)
{
    for (unsigned int scenario = 0; scenario < 6; ++scenario) {
        struct device_model dev = { .refs = 1 };
        struct context *ctx, *file = NULL;
        unsigned int active = scenario == 4 ? 2U : scenario == 3 ? 1U : 0U;
        context_releases = 0;
        ctx = create_binding(&dev, scenario == 0);
        if (scenario == 0) {
            assert(!ctx && !registry && dev.refs == 1);
            device_put(&dev);
        } else {
            assert(ctx);
            if (scenario >= 2) {
                file = open_session();
                assert(file == ctx);
            }
            for (unsigned int i = 0; i < active; ++i)
                assert(begin_operation(file));
            bool old_live_snapshot = !ctx->closing;
            close_gate(ctx);
            assert(open_session() == NULL && !begin_operation(ctx));
            assert(ctx->drained == (active == 0));
            while (active) {
                finish_operation(ctx);
                --active;
                assert(ctx->drained == (active == 0));
            }
            finish_binding(ctx); /* 无旧文件时，ctx可在这里最终回收。 */
            ctx = NULL;
            device_put(&dev); /* 再模拟框架归还自己的设备份额。 */
            if (file) {
                assert(!dev.resources_live && !dev.released && dev.refs == 1);
                assert(context_releases == 0 && !begin_operation(file));
                if (scenario == 5)
                    assert(old_live_snapshot && file->closing);
                context_put(file); /* 旧文件只归还软件对象，不再操作硬件。 */
            }
            assert(context_releases == 1);
        }
        assert(!registry && dev.released && dev.refs == 0);
        printf("case %u: context_release=%u device_released=1\n",
               scenario, context_releases);
    }
    puts("6 binding/session traces passed; no Linux or concurrent execution");
    return 0;
}
