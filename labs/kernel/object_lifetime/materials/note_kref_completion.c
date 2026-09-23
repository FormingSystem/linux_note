// SPDX-License-Identifier: GPL-2.0
#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct handoff_request {
    int result;
    struct kref ref;
    struct work_struct work;
    struct completion done;
};

static unsigned int release_calls;

static void handoff_release(struct kref *ref)
{
    struct handoff_request *request = container_of(ref, struct handoff_request, ref);
    ++release_calls; /* 最后归还安排在等待者，观察量位于对象之外。 */
    kfree(request);
}

static void handoff_worker(struct work_struct *work)
{
    struct handoff_request *request = container_of(work, struct handoff_request, work);
    request->result = 42;
    complete(&request->done); /* 发布结果事件，不交还对象引用。 */
    kref_put(&request->ref, handoff_release);
}

static int __init note_handoff_init(void)
{
    struct workqueue_struct *queue;
    struct handoff_request *request;
    unsigned long remaining;
    bool canceled;

    queue = alloc_ordered_workqueue("note_handoff", 0);
    if (!queue)
        return -ENOMEM;
    request = kzalloc(sizeof(*request), GFP_KERNEL);
    if (!request) {
        destroy_workqueue(queue);
        return -ENOMEM;
    }
    kref_init(&request->ref); /* 初始份额属于等待者。 */
    init_completion(&request->done);
    INIT_WORK(&request->work, handoff_worker);
    kref_get(&request->ref); /* 唯一工作实例在发布前预留一份。 */
    if (!queue_work(queue, &request->work)) {
        kref_put(&request->ref, handoff_release); /* 退回未接收的预留。 */
        kref_put(&request->ref, handoff_release); /* 结束等待者责任。 */
        destroy_workqueue(queue);
        return -EIO;
    }

    remaining = wait_for_completion_timeout(&request->done, 1);
    /* 一次投递，无重新排队；等待事件成功也不等于 worker 已返回。 */
    canceled = cancel_work_sync(&request->work);
    if (canceled)
        kref_put(&request->ref, handoff_release); /* 接管未执行实例的一份。 */
    destroy_workqueue(queue); /* 代码卸载前已无 worker 执行。 */
    pr_info("note_handoff: completed_in_time=%d canceled=%d result=%d\n",
            remaining != 0, canceled, request->result);
    kref_put(&request->ref, handoff_release); /* 最后放弃等待者的一份。 */
    return 0;
}

static void __exit note_handoff_exit(void)
{
    pr_info("note_handoff: release=%u\n", release_calls);
}

module_init(note_handoff_init);
module_exit(note_handoff_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("完成事件与异步对象引用分别退出的完整实验");
