// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include "note_kref_file_protocol.h"

struct file_service {
    struct kref ref;
    struct mutex gate;
    bool closing;
    unsigned int requests;
};
static DEFINE_MUTEX(entry_lock);
static struct file_service *service_entry; /* 模块管理者持有初始份额。 */
static bool fail_open;
module_param(fail_open, bool, 0444);
MODULE_PARM_DESC(fail_open, "在open取得候选引用后模拟失败");
static atomic_t file_releases = ATOMIC_INIT(0);
static unsigned int object_releases;

static void service_release(struct kref *ref)
{
    struct file_service *service = container_of(ref, struct file_service, ref);
    ++object_releases;
    kfree(service);
}
static void service_put(struct file_service *service)
{
    kref_put(&service->ref, service_release);
}
static int service_open_file(struct inode *inode, struct file *file)
{
    struct file_service *service;
    int result = 0;
    (void)inode;
    /* misc入口原先放的是静态miscdevice地址，不是本服务的一份引用。 */
    file->private_data = NULL;
    mutex_lock(&entry_lock);
    service = service_entry;
    if (service)
        kref_get(&service->ref); /* 管理者份额在锁内保证地址和正计数。 */
    mutex_unlock(&entry_lock);
    if (!service)
        return -ENODEV;
    mutex_lock(&service->gate);
    if (service->closing)
        result = -ESHUTDOWN;
    else if (fail_open)
        result = -EIO;
    mutex_unlock(&service->gate);
    if (result) {
        service_put(service); /* open失败自行回滚，不等文件release替我们做。 */
        return result;
    }
    file->private_data = service; /* 向本次成功struct file交付候选份额。 */
    return 0;
}
static int service_release_file(struct inode *inode, struct file *file)
{
    struct file_service *service = file->private_data;
    (void)inode;
    file->private_data = NULL;
    atomic_inc(&file_releases);
    service_put(service); /* 一次成功打开实例的最终release，恰好归还一份。 */
    return 0;
}
static long service_ioctl(struct file *file, unsigned int command, unsigned long arg)
{
    struct file_service *service = file->private_data;
    long result = 0;
    (void)arg;
    mutex_lock(&service->gate);
    switch (command) {
    case NOTE_FILE_STEP:
        if (service->closing)
            result = -ESHUTDOWN;
        else
            ++service->requests; /* 检查与本例完整短操作处在同一锁窗口。 */
        break;
    case NOTE_FILE_CLOSE:
        service->closing = true; /* 关业务门，不消费文件或管理者份额。 */
        break;
    default:
        result = -ENOTTY;
        break;
    }
    mutex_unlock(&service->gate);
    return result;
}
static const struct file_operations service_fops = {
    .owner = THIS_MODULE,
    .open = service_open_file,
    .release = service_release_file,
    .unlocked_ioctl = service_ioctl,
    .compat_ioctl = service_ioctl, /* 命令没有指针或依赖字长的结构参数。 */
};
static struct miscdevice service_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "note_kref_file",
    .fops = &service_fops,
    .mode = 0600,
};
static int __init note_file_init(void)
{
    int result;
    struct file_service *service = kzalloc(sizeof(*service), GFP_KERNEL);
    if (!service)
        return -ENOMEM;
    kref_init(&service->ref);
    mutex_init(&service->gate);
    service_entry = service;
    result = misc_register(&service_device);
    if (result) {
        service_entry = NULL;
        service_put(service);
    }
    return result;
}
static void __exit note_file_exit(void)
{
    struct file_service *service;
    /* 正常模块卸载由fops.owner阻止存在打开文件时进入；不模拟热拔插。 */
    misc_deregister(&service_device);
    mutex_lock(&entry_lock);
    service = service_entry;
    service_entry = NULL;
    mutex_unlock(&entry_lock);
    pr_info("note_file: file_releases=%d requests=%u\n",
            atomic_read(&file_releases), service->requests);
    service_put(service);
    pr_info("note_file: object_releases=%u\n", object_releases);
}
module_init(note_file_init);
module_exit(note_file_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("文件实例持有对象份额与open失败回滚实验");
