// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/maple_tree.h>
#include <linux/errno.h>
#include <linux/limits.h>

/* 载荷保持到模块退出；私有树没有并发读者或发布入口。 */
struct basic_item { int id; };
static struct basic_item basic_items[] = {{1}, {2}, {3}, {4}};

static int expect_entry(void *entry, unsigned int item)
{
    void *expected = item ? &basic_items[item - 1] : NULL;

    /* 比较地址即可验证映射，不需要解引用不符合预期的返回值。 */
    return entry == expected ? 0 : -EINVAL;
}

static int __init note_maple_basic_init(void)
{
    struct maple_tree tree;
    unsigned long index;
    void *entry;
    int ret;

    mt_init(&tree);
    ret = mtree_store_range(&tree, 100, 199, &basic_items[0], GFP_KERNEL);
    if (ret)
        goto destroy;
    ret = mtree_store_range(&tree, 150, 249, &basic_items[1], GFP_KERNEL);
    if (ret)
        goto destroy;
    ret = expect_entry(mtree_load(&tree, 125), 1);
    if (!ret)
        ret = expect_entry(mtree_load(&tree, 175), 2);
    if (!ret)
        ret = expect_entry(mtree_load(&tree, 225), 2);
    if (ret)
        goto destroy;
    pr_info("maple_basic overwrite: A[100,149] B[150,249]\n");

    /* insert 的契约是整段空闲；即使只有一部分重叠也不能覆盖。 */
    ret = mtree_insert_range(&tree, 240, 299, &basic_items[2], GFP_KERNEL);
    if (ret != -EEXIST) {
        ret = -EINVAL;
        goto destroy;
    }
    ret = expect_entry(mtree_load(&tree, 249), 2);
    if (!ret)
        ret = expect_entry(mtree_load(&tree, 250), 0);
    if (ret)
        goto destroy;
    ret = mtree_store_range(&tree, 300, 349, &basic_items[2], GFP_KERNEL);
    if (ret)
        goto destroy;
    ret = mtree_store_range(&tree, 2, 1, &basic_items[3], GFP_KERNEL);
    if (ret != -EINVAL) {
        ret = -EINVAL;
        goto destroy;
    }

    /* max 限制搜索位置，不把命中条目的右端裁剪到 max。 */
    index = 0;
    entry = mt_find(&tree, &index, 119);
    ret = expect_entry(entry, 1);
    if (ret || index != 150) {
        ret = -EINVAL;
        goto destroy;
    }
    index = 250;
    entry = mt_find(&tree, &index, 349);
    ret = expect_entry(entry, 3);
    if (ret || index != 350) {
        ret = -EINVAL;
        goto destroy;
    }
    index = 400;
    entry = mt_find(&tree, &index, 399);
    if (entry || index != 400) {
        ret = -EINVAL;
        goto destroy;
    }
    pr_info("maple_basic find: bounded cursor=150, gap cursor=350\n");

    /* erase 删除当前命中的整段 B；局部清空则明确写入 NULL。 */
    ret = expect_entry(mtree_erase(&tree, 175), 2);
    if (!ret)
        ret = expect_entry(mtree_load(&tree, 150), 0);
    if (!ret)
        ret = expect_entry(mtree_load(&tree, 249), 0);
    if (ret)
        goto destroy;
    ret = mtree_store_range(&tree, 110, 119, NULL, GFP_KERNEL);
    if (ret)
        goto destroy;
    ret = expect_entry(mtree_load(&tree, 109), 1);
    if (!ret)
        ret = expect_entry(mtree_load(&tree, 115), 0);
    if (!ret)
        ret = expect_entry(mtree_load(&tree, 120), 1);
    if (ret)
        goto destroy;
    pr_info("maple_basic clear: A[100,109] hole[110,119] A[120,149]\n");

    ret = mtree_store(&tree, ULONG_MAX, &basic_items[3], GFP_KERNEL);
    if (ret)
        goto destroy;
    index = ULONG_MAX;
    entry = mt_find(&tree, &index, ULONG_MAX);
    ret = expect_entry(entry, 4);
    if (ret || index || mt_find_after(&tree, &index, ULONG_MAX)) {
        ret = -EINVAL;
        goto destroy;
    }
    pr_info("maple_basic end: wrapped cursor=0, find_after stopped\n");
destroy:
    /* 内部节点与载荷不是同一所有权；destroy 不释放静态载荷。 */
    mtree_destroy(&tree);
    return ret;
}

static void __exit note_maple_basic_exit(void)
{
    /* 私有树已在初始化结束前清理。 */
}

module_init(note_maple_basic_init);
module_exit(note_maple_basic_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Private Maple normal API contracts");
