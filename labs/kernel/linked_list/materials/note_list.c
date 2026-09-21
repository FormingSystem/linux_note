// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/errno.h>

struct note_task {
    int number;
    struct list_head link;
};

static LIST_HEAD(note_ready);
static int fail_after = -1;
module_param(fail_after, int, 0444);
MODULE_PARM_DESC(fail_after, "成功分配多少个节点后模拟失败，-1 表示不注入");

static void note_free_all(struct list_head *head)
{
    struct note_task *task;
    struct note_task *next;

    list_for_each_entry_safe(task, next, head, link) {
        /* 先从容器摘除，再释放本例独占的宿主对象。 */
        list_del(&task->link);
        kfree(task);
    }
}

static void note_print(const char *phase)
{
    struct note_task *task;

    list_for_each_entry(task, &note_ready, link)
        pr_info("note_list %s: %d\n", phase, task->number);
}

static int __init note_list_init(void)
{
    LIST_HEAD(staging);
    struct note_task *task;
    struct note_task *next;
    int index;

    if (fail_after < -1 || fail_after > 2)
        return -EINVAL;
    for (index = 0; index < 3; ++index) {
        /* 在私有暂存链上构建完整批次，任一失败全部回滚。 */
        if (index == fail_after)
            goto no_memory;
        task = kmalloc(sizeof(*task), GFP_KERNEL);
        if (!task)
            goto no_memory;
        task->number = (index + 1) * 10;
        INIT_LIST_HEAD(&task->link);
        list_add_tail(&task->link, &staging);
    }
    list_splice_tail_init(&staging, &note_ready);
    note_print("ready");

    list_for_each_entry_safe(task, next, &note_ready, link) {
        if (task->number == 20) {
            list_del(&task->link);
            kfree(task);
        }
    }
    note_print("after_remove");
    return 0;

no_memory:
    note_free_all(&staging);
    return -ENOMEM;
}

static void __exit note_list_exit(void)
{
    note_free_all(&note_ready);
    pr_info("note_list empty=%d\n", list_empty(&note_ready));
}

module_init(note_list_init);
module_exit(note_list_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("链表批次构建、失败回滚与完整清理");
