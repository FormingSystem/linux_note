// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct note_request {
    int value; /* 将 ref 放在非首成员位置，回调必须按成员偏移还原。 */
    struct kref ref;
    struct work_struct work;
};

static struct workqueue_struct *note_wq;

static void note_release(struct kref *ref)
{
    struct note_request *request = container_of(ref, struct note_request, ref);
    pr_info("note_kref: release\n");
    kfree(request);
}

static void note_worker(struct work_struct *work)
{
    struct note_request *request = container_of(work, struct note_request, work);
    pr_info("note_kref: value=%d\n", request->value);
    kref_put(&request->ref, note_release);
    /* 归还后不再访问 request，包括嵌入的 work。 */
}

static int __init note_init(void)
{
    struct note_request *request;

    note_wq = alloc_ordered_workqueue("note_kref", 0);
    if (!note_wq)
        return -ENOMEM;
    request = kzalloc(sizeof(*request), GFP_KERNEL);
    if (!request) {
        destroy_workqueue(note_wq);
        return -ENOMEM;
    }

    kref_init(&request->ref); /* 初始引用属于创建者。 */
    request->value = 42;
    INIT_WORK(&request->work, note_worker);
    kref_get(&request->ref); /* 在发布前为一次 worker 执行预留引用。 */
    if (!queue_work(note_wq, &request->work)) {
        /* 本例全新且仅提交一次；拒绝分支防御性归还两份责任。 */
        kref_put(&request->ref, note_release);
        kref_put(&request->ref, note_release);
        destroy_workqueue(note_wq);
        return -EIO;
    }
    kref_put(&request->ref, note_release); /* 创建者结束，之后不再碰对象。 */
    return 0;
}

static void __exit note_exit(void)
{
    /* 队列只由本模块提交一次，无重排；等回调退出后才卸载代码。 */
    destroy_workqueue(note_wq);
}

module_init(note_init);
module_exit(note_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("一次工作交付的引用责任实验");
