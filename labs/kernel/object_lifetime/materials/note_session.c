// SPDX-License-Identifier: GPL-2.0
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

struct session_device {
    struct device dev;
    struct mutex lock; /* 同时保护关闭门和会话的完成数。 */
    bool closing;
};
struct note_session {
    struct kref ref;
    struct session_device *owner; /* 每个会话合计拥有设备的一份。 */
    unsigned int completed;
};
static unsigned int device_releases, session_releases;

static void session_device_release(struct device *dev)
{
    struct session_device *obj = container_of(dev, struct session_device, dev);
    ++device_releases;
    kfree(obj);
}

static void session_release(struct kref *ref)
{
    struct note_session *session = container_of(ref, struct note_session, ref);
    struct device *held = &session->owner->dev;
    ++session_releases;
    kfree(session);
    put_device(held); /* 会话消失后归还桥接份额；此后不再访问owner。 */
}

/* 调用者已有设备份额；成功交付会话初始份额，失败不交付任何引用。 */
static int session_open(struct session_device *owner, struct note_session **out)
{
    struct note_session *session;
    *out = NULL;
    session = kzalloc(sizeof(*session), GFP_KERNEL);
    if (!session)
        return -ENOMEM;
    mutex_lock(&owner->lock);
    if (owner->closing) {
        mutex_unlock(&owner->lock);
        kfree(session);
        return -ENODEV;
    }
    get_device(&owner->dev);
    session->owner = owner;
    kref_init(&session->ref);
    mutex_unlock(&owner->lock);
    *out = session;
    return 0;
}

/* 已持会话份额；本例只同步更新统计，不启动任何硬件或异步工作。 */
static int session_request(struct note_session *session)
{
    struct session_device *owner = session->owner;
    int result = 0;
    mutex_lock(&owner->lock);
    if (owner->closing)
        result = -ENODEV;
    else
        ++session->completed;
    mutex_unlock(&owner->lock);
    return result;
}

static unsigned int session_completed(struct note_session *session)
{
    struct session_device *owner = session->owner;
    unsigned int result;
    mutex_lock(&owner->lock);
    result = session->completed;
    mutex_unlock(&owner->lock);
    return result;
}

/* 唯一管理者只调用一次，并消费设备初始化份额。 */
static void session_device_stop(struct session_device *owner)
{
    mutex_lock(&owner->lock);
    owner->closing = true;
    mutex_unlock(&owner->lock);
    device_unregister(&owner->dev);
}

static int __init note_session_init(void)
{
    struct session_device *owner;
    struct note_session *session;
    int result;
    if (!IS_ENABLED(CONFIG_SYSFS) || IS_ENABLED(CONFIG_DEBUG_KOBJECT_RELEASE))
        return -EOPNOTSUPP; /* 同步示例不实现延迟设备清理的模块退出。 */
    owner = kzalloc(sizeof(*owner), GFP_KERNEL);
    if (!owner)
        return -ENOMEM;
    mutex_init(&owner->lock);
    device_initialize(&owner->dev);
    owner->dev.release = session_device_release;
    result = dev_set_name(&owner->dev, "note_session_lifetime");
    if (result)
        goto put_owner;
    result = device_add(&owner->dev);
    if (result)
        goto put_owner;
    result = session_open(owner, &session);
    if (result) {
        session_device_stop(owner);
        return result;
    }
    kref_get(&session->ref); /* 第二个会话拥有者，不额外取得设备份额。 */
    result = session_request(session);
    pr_info("note_session: before=%d completed=%u\n", result, session_completed(session));
    session_device_stop(owner);
    owner = NULL; /* 初始份额已被unregister消费。 */
    result = session_request(session);
    pr_info("note_session: after=%d completed=%u device_release=%u\n",
            result, session_completed(session), device_releases);
    kref_put(&session->ref, session_release);
    kref_put(&session->ref, session_release);
    return 0;
put_owner:
    put_device(&owner->dev);
    return result;
}

static void __exit note_session_exit(void)
{
    pr_info("note_session: session_release=%u device_release=%u\n",
            session_releases, device_releases);
}
module_init(note_session_init);
module_exit(note_session_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("独立会话引用与设备份额的单向连接");
