// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/maple_tree.h>
#include <linux/errno.h>

/* 载荷在模块寿命内保持存活；实验不注册任何外部访问入口。 */
struct demo_item { int id; };
static struct demo_item items[] = {{1}, {2}, {3}, {4}};

static int expect_state(struct ma_state *mas, void *entry, int id,
                        unsigned long first, unsigned long last,
                        enum maple_status status, const char *stage)
{
    struct demo_item *item = entry;
    int actual = item ? item->id : 0;

    pr_info("maple_state %s: id=%d index=%lu last=%lu status=%u\n",
            stage, actual, mas->index, mas->last, mas->status);
    if (actual != id || mas->index != first || mas->last != last ||
        mas->status != status)
        return -EINVAL;
    return 0;
}

static int __init note_maple_state_init(void)
{
    struct maple_tree tree;
    MA_STATE(mas, &tree, 20, 20);
    void *entry;
    int ret;

    mt_init(&tree);
    ret = mtree_store_range(&tree, 20, 29, &items[0], GFP_KERNEL);
    if (ret)
        goto destroy;
    ret = mtree_store_range(&tree, 40, 49, &items[1], GFP_KERNEL);
    if (ret)
        goto destroy;
    ret = mtree_store_range(&tree, 60, 69, &items[2], GFP_KERNEL);
    if (ret)
        goto destroy;

    /* 高级遍历在锁内运行；暂停不执行解锁，仍由调用者安排。 */
    mtree_lock(&tree);
    entry = mas_find(&mas, 69);
    ret = expect_state(&mas, entry, 1, 20, 29, ma_active, "first");
    if (ret)
        goto unlock;
    mas_pause(&mas);
    ret = expect_state(&mas, NULL, 0, 20, 29, ma_pause, "pause");
    if (ret || mas.node) {
        ret = -EINVAL;
        goto unlock;
    }
    mtree_unlock(&tree);

    /* 普通写接口自行管理内部锁；不要在同一内部锁中再次调用。 */
    ret = mtree_store_range(&tree, 30, 39, &items[3], GFP_KERNEL);
    if (ret)
        goto destroy;
    mtree_lock(&tree);
    entry = mas_find(&mas, 69);
    ret = expect_state(&mas, entry, 4, 30, 39, ma_active, "resume");
    if (ret)
        goto unlock;

    /* reset 保留索引，不按 pause 的规则跳过刚才的范围。 */
    mas_reset(&mas);
    ret = expect_state(&mas, NULL, 0, 30, 39, ma_start, "reset");
    if (ret)
        goto unlock;
    entry = mas_find(&mas, 69);
    ret = expect_state(&mas, entry, 4, 30, 39, ma_active, "repeat");
    if (ret)
        goto unlock;

    mas_set(&mas, 50);
    entry = mas_find(&mas, 69);
    ret = expect_state(&mas, entry, 3, 60, 69, ma_active, "after_hole");
    if (ret)
        goto unlock;
    entry = mas_find(&mas, 69);
    ret = expect_state(&mas, entry, 0, 60, 69, ma_active, "bounded_end");
unlock:
    mtree_unlock(&tree);
destroy:
    /* 销毁的是内部节点，静态载荷不由 Maple 释放。 */
    mtree_destroy(&tree);
    if (!ret)
        pr_info("maple_state private sequence checked\n");
    return ret;
}

static void __exit note_maple_state_exit(void)
{
    /* 初始化实验结束前已清理私有树，无后台状态。 */
}

module_init(note_maple_state_init);
module_exit(note_maple_state_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Private Maple iterator state observation");
