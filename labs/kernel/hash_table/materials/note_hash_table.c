// SPDX-License-Identifier: GPL-2.0
/* 私有固定桶实验：没有外部入口，初始化与退出串行，业务对象由本模块独占。 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/string.h>

struct note_task {
    u32 id;
    char name[32];
    struct hlist_node node;
};
static DEFINE_HASHTABLE(task_table, 3);
static int fail_at;
module_param(fail_at, int, 0444);
MODULE_PARM_DESC(fail_at, "在第1到3次添加前失败，0不注入");

static struct note_task *find_task(u32 id)
{
    struct note_task *task;

    hash_for_each_possible(task_table, task, node, id)
        if (task->id == id)
            return task;
    return NULL;
}
static int add_task(u32 id, const char *name)
{
    struct note_task *task;

    if (find_task(id))
        return -EEXIST;
    task = kzalloc(sizeof(*task), GFP_KERNEL);
    if (!task)
        return -ENOMEM;
    task->id = id;
    if (strscpy(task->name, name, sizeof(task->name)) < 0) {
        kfree(task);
        return -E2BIG;
    }
    INIT_HLIST_NODE(&task->node);
    hash_add(task_table, &task->node, task->id);
    return 0;
}
static int rename_task(u32 id, const char *name)
{
    struct note_task *task = find_task(id);
    char candidate[32];

    if (!task)
        return -ENOENT;
    /* 先在私有缓冲区验证；过长输入不破坏已发布的旧名字。 */
    if (strscpy(candidate, name, sizeof(candidate)) < 0)
        return -E2BIG;
    strscpy(task->name, candidate, sizeof(task->name));
    return 0;
}
static int remove_task(u32 id)
{
    struct note_task *task = find_task(id);

    if (!task)
        return -ENOENT;
    hash_del(&task->node);
    kfree(task); /* 没有并发借用者，摘除后由唯一拥有者直接释放。 */
    return 0;
}
static void clear_tasks(void)
{
    struct note_task *task;
    struct hlist_node *next;
    unsigned int bucket;

    hash_for_each_safe(task_table, bucket, next, task, node) {
        hash_del(&task->node);
        kfree(task);
    }
}
static int __init note_hash_table_init(void)
{
    const u32 ids[] = { 10, 18, 26 };
    const char *const names[] = { "task10", "task18", "task26" };
    struct note_task *task;
    unsigned int index;
    int error;

    if (fail_at < 0 || fail_at > 3)
        return -EINVAL;
    for (index = 0; index < ARRAY_SIZE(ids); ++index) {
        error = -ENOMEM;
        if ((unsigned int)fail_at == index + 1)
            goto fail;
        error = add_task(ids[index], names[index]);
        if (error)
            goto fail;
    }
    error = add_task(18, "duplicate");
    if (error != -EEXIST) {
        error = error ? error : -EINVAL;
        goto fail;
    }
    error = rename_task(18, "renamed18");
    if (error)
        goto fail;
    task = find_task(18);
    if (!task || strcmp(task->name, "renamed18")) {
        error = -EINVAL;
        goto fail;
    }
    pr_info("note_hash_table: id=%u name=%s buckets=%zu\n",
            task->id, task->name, HASH_SIZE(task_table));
    error = remove_task(18);
    if (error)
        goto fail;
    if (find_task(18)) {
        error = -EINVAL;
        goto fail;
    }
    pr_info("note_hash_table: duplicate=EEXIST removed18=absent\n");
    return 0;
fail:
    clear_tasks();
    return error;
}
static void __exit note_hash_table_exit(void)
{
    clear_tasks();
}
module_init(note_hash_table_init);
module_exit(note_hash_table_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("私有固定桶的增删查改与失败回滚教学模块");
