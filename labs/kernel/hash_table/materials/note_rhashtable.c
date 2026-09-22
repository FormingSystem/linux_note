// SPDX-License-Identifier: GPL-2.0
/* 仅初始化任务使用业务对象；后台扩缩容仍可与它重叠。 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/rhashtable.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>

struct note_record {
    u32 key;
    u32 value;
    struct rhash_head node;
};
static struct rhashtable records;
static const struct rhashtable_params record_params = {
    .head_offset = offsetof(struct note_record, node),
    .key_offset = offsetof(struct note_record, key),
    .key_len = sizeof(u32),
    .nelem_hint = 3,
    .min_size = 4,
    .max_size = 64,
    .automatic_shrinking = true,
};
static int fail_at;
module_param(fail_at, int, 0444);
MODULE_PARM_DESC(fail_at, "在第1到3次分配前失败，0不注入");

/* 表拥有已成功插入对象；未发布的候选仍由添加路径负责释放。 */
static int add_record(u32 key, u32 value)
{
    struct note_record *candidate;
    int error;

    candidate = kzalloc(sizeof(*candidate), GFP_KERNEL);
    if (!candidate)
        return -ENOMEM;
    candidate->key = key;
    candidate->value = value;
    error = rhashtable_lookup_insert_fast(&records, &candidate->node,
                                         record_params);
    if (error)
        kfree(candidate);
    return error;
}

/* 查找在读侧内复制值，返回值不是可逃逸的借用指针。 */
static int read_record(u32 key, u32 *value)
{
    struct note_record *record;
    int error = -ENOENT;

    rcu_read_lock();
    record = rhashtable_lookup(&records, &key, record_params);
    if (record) {
        *value = record->value;
        error = 0;
    }
    rcu_read_unlock();
    return error;
}

static void free_record(void *object, void *argument)
{
    (void)argument;
    kfree(object);
}

static int __init note_rhashtable_init(void)
{
    const u32 keys[] = { 10, 18, 26 };
    struct note_record *removed;
    u32 value, key = 18;
    unsigned int index;
    int error;

    if (fail_at < 0 || fail_at > 3)
        return -EINVAL;
    error = rhashtable_init(&records, &record_params);
    if (error)
        return error;
    for (index = 0; index < ARRAY_SIZE(keys); ++index) {
        error = -ENOMEM;
        if ((unsigned int)fail_at == index + 1)
            goto destroy;
        error = add_record(keys[index], keys[index] * 10);
        if (error)
            goto destroy;
    }
    error = add_record(18, 999);
    if (error != -EEXIST) {
        error = error ? error : -EINVAL;
        goto destroy;
    }
    error = read_record(18, &value);
    if (error)
        goto destroy;
    if (value != 180) {
        error = -EINVAL;
        goto destroy;
    }

    rcu_read_lock();
    removed = rhashtable_lookup(&records, &key, record_params);
    error = removed ? rhashtable_remove_fast(&records, &removed->node,
                                             record_params) : -ENOENT;
    rcu_read_unlock();
    if (error)
        goto destroy;
    /* 本模块没有第二个业务删除者，移除后由当前路径独占最终回收责任。 */
    synchronize_rcu();
    kfree(removed);
    error = read_record(18, &value);
    if (error != -ENOENT) {
        error = error ? error : -EINVAL;
        goto destroy;
    }
    pr_info("note_rhashtable: duplicate=EEXIST value18=180 removed=ENOENT\n");
    return 0;
destroy:
    /* 没有外部读者或生产入口；函数会停止该表的后台扩缩容工作。 */
    rhashtable_free_and_destroy(&records, free_record, NULL);
    return error;
}

static void __exit note_rhashtable_exit(void)
{
    rhashtable_free_and_destroy(&records, free_record, NULL);
}
module_init(note_rhashtable_init);
module_exit(note_rhashtable_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("rhashtable参数、去重、复制读取与对象回收教学模块");
