// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

enum request_phase { REQUEST_NEW, REQUEST_QUEUED, REQUEST_RUNNING };
struct queued_request {
    int result;
    enum request_phase phase;
    struct kref ref;
    struct work_struct work;
};
struct request_queue {
    struct mutex lock;
    struct queued_request *slot; /* 非空槽拥有一份；只在此队列处理这些请求。 */
};
static struct request_queue inbox;
static struct workqueue_struct *execution_queue;
static unsigned int release_calls; /* 演示每处理完一个对象才开始下一个。 */

static void request_release(struct kref *ref)
{
    struct queued_request *request = container_of(ref, struct queued_request, ref);
    ++release_calls;
    kfree(request);
}

static void request_put(struct queued_request *request)
{
    kref_put(&request->ref, request_release);
}

static void request_worker(struct work_struct *work)
{
    struct queued_request *request = container_of(work, struct queued_request, work);
    request->result = 42;
    request_put(request); /* 归还经队列、消费者转来的同一份。 */
}

static struct queued_request *request_create(void)
{
    struct queued_request *request = kzalloc(sizeof(*request), GFP_KERNEL);
    if (!request)
        return NULL;
    request->phase = REQUEST_NEW;
    kref_init(&request->ref);
    INIT_WORK(&request->work, request_worker);
    return request;
}

/* 成功接管参数所代表的一份；失败不消费。参数必须有效且拥有一份。 */
static int enqueue_take(struct request_queue *queue, struct queued_request *request)
{
    int result = 0;
    mutex_lock(&queue->lock);
    if (queue->slot || request->phase != REQUEST_NEW)
        result = -EBUSY;
    else {
        request->phase = REQUEST_QUEUED;
        queue->slot = request;
    }
    mutex_unlock(&queue->lock);
    return result;
}

/* 两种返回都保留调用者原份额；成功额外保留队列份额，失败退回预留。 */
static int enqueue_ref(struct request_queue *queue, struct queued_request *request)
{
    int result;
    kref_get(&request->ref);
    result = enqueue_take(queue, request);
    if (result)
        request_put(request);
    return result;
}

/* 返回非空时把槽拥有的一份交给消费者，计数不变。 */
static struct queued_request *dequeue_take(struct request_queue *queue)
{
    struct queued_request *request;
    mutex_lock(&queue->lock);
    request = queue->slot;
    if (request) {
        queue->slot = NULL;
        request->phase = REQUEST_RUNNING;
    }
    mutex_unlock(&queue->lock);
    return request;
}

/* 本例每请求只投递一次；成功消费当前份额，拒绝时仍由消费者持有。 */
static int execute_take(struct queued_request *request)
{
    return queue_work(execution_queue, &request->work) ? 0 : -EIO;
}

static int run_one(bool shared)
{
    struct queued_request *producer = request_create(), *consumer;
    int result;
    if (!producer)
        return -ENOMEM;
    result = shared ? enqueue_ref(&inbox, producer) : enqueue_take(&inbox, producer);
    if (result) {
        request_put(producer);
        return result;
    }
    if (!shared)
        producer = NULL; /* 只清本地变量，不再通过对象地址访问。 */
    consumer = dequeue_take(&inbox); /* 本例无其他消费者，成功发布保证非空。 */
    result = execute_take(consumer);
    if (result)
        request_put(consumer); /* 拒绝，消费者仍负责队列转来的一份。 */
    consumer = NULL;
    flush_workqueue(execution_queue); /* 无重排，等待执行后才观察或进入下一轮。 */
    if (producer) {
        pr_info("note_queue: shared result=%d\n", producer->result);
        request_put(producer);
    }
    return result;
}

static int __init note_queue_init(void)
{
    int result;
    mutex_init(&inbox.lock);
    inbox.slot = NULL;
    execution_queue = alloc_ordered_workqueue("note_queue", 0);
    if (!execution_queue)
        return -ENOMEM;
    result = run_one(false);
    if (!result)
        result = run_one(true);
    destroy_workqueue(execution_queue); /* 初始化内收束两轮，不导出并发入口。 */
    return result;
}

static void __exit note_queue_exit(void)
{
    pr_info("note_queue: release=%u\n", release_calls);
}

module_init(note_queue_init);
module_exit(note_queue_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("同一请求的队列转交与共享引用对照实验");
