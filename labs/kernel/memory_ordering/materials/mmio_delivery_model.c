#include <stdbool.h>
#include <stdio.h>

struct device_model {
    bool pending; /* 桥中尚未送达的启动请求。 */
    bool running; /* 设备已收到命令，工作仍在进行。 */
    bool done;    /* 设备发布的业务完成状态。 */
};

static bool post_start(struct device_model *d)
{
    if (d->pending || d->running || d->done)
        return false; /* 本模型只允许从初始状态发出一次启动。 */
    d->pending = true;
    return true;
}

static void deliver(struct device_model *d)
{
    if (d->pending) {
        d->pending = false;
        d->running = true;
    }
}

static bool safe_status_read(struct device_model *d)
{
    deliver(d); /* 模型约定：读响应前先送达此前的写。 */
    return d->done;
}

static bool finish_work(struct device_model *d)
{
    if (!d->running)
        return false;
    d->running = false;
    d->done = true;
    return true;
}

int main(void)
{
    struct device_model d = {0};
    if (!post_start(&d) || !d.pending || d.running)
        goto failed;                       /* I0：写应已被桥接收。 */
    if (safe_status_read(&d) || d.pending || !d.running)
        goto failed;                       /* I1：送达，但尚未完成。 */
    if (!finish_work(&d))
        goto failed;                       /* I2：设备完成业务。 */
    if (!safe_status_read(&d))
        goto failed;                       /* I3：驱动取得完成证据。 */
    puts("accepted -> delivered -> completed -> observed");
    return 0;
failed:
    fputs("delivery/completion contract failed\n", stderr);
    return 1;
}
