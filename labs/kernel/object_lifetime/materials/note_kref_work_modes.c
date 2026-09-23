// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct work_request {
    struct kref ref;
    struct work_struct work;
    int value;
};
static bool transfer;
module_param(transfer, bool, 0444);
MODULE_PARM_DESC(transfer, "false另取工作份额，true转交创建者份额");
static struct workqueue_struct *execution_queue;
static unsigned int release_calls;

static void request_release(struct kref *ref)
{
    struct work_request *request = container_of(ref, struct work_request, ref);
    ++release_calls;
    kfree(request);
}
static void request_put(struct work_request *request)
{
    kref_put(&request->ref, request_release);
}
static void request_worker(struct work_struct *work)
{
    struct work_request *request = container_of(work, struct work_request, work);
    pr_info("note_work_modes: value=%d\n", request->value);
    request_put(request); /* 只归还本次成功交付给worker的一份。 */
    /* 此后不能再访问request或嵌入其中的work。 */
}
static int __init note_modes_init(void)
{
    struct work_request *request;
    execution_queue = alloc_ordered_workqueue("note_modes", 0);
    if (!execution_queue)
        return -ENOMEM;
    request = kzalloc(sizeof(*request), GFP_KERNEL);
    if (!request) {
        destroy_workqueue(execution_queue);
        return -ENOMEM;
    }
    kref_init(&request->ref);
    INIT_WORK(&request->work, request_worker);
    request->value = 42;
    if (!transfer)
        kref_get(&request->ref); /* 分享模式先为worker准备独立一份。 */
    if (!queue_work(execution_queue, &request->work)) {
        /* 全新work仅投递一次；拒绝时没有向worker交付任何份额。 */
        if (!transfer)
            request_put(request); /* 退回分享模式的候选份额。 */
        request_put(request); /* 创建者仍需结算原有一份。 */
        destroy_workqueue(execution_queue);
        return -EIO;
    }
    if (!transfer)
        request_put(request); /* 分享成功后只结束创建者自己的责任。 */
    /* 转交成功时不再访问request，即使worker已经完成也没有问题。 */
    return 0;
}
static void __exit note_modes_exit(void)
{
    /* 无其他生产者或重排；等待唯一回调返回后才卸载代码。 */
    destroy_workqueue(execution_queue);
    pr_info("note_work_modes: transfer=%d releases=%u\n", transfer, release_calls);
}
module_init(note_modes_init);
module_exit(note_modes_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("同一工作请求的共享与转交对照");
