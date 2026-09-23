// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/maple_tree.h>
#include <linux/mutex.h>
#include <linux/errno.h>

struct prealloc_item { int id; };
static struct prealloc_item prealloc_items[] = {{1}, {2}};
static DEFINE_MUTEX(prealloc_lock);

static int __init note_maple_prealloc_init(void)
{
    struct maple_tree tree;
    MA_STATE(mas, &tree, 100, 199);
    int ret;

    /* 外部互斥锁允许当前进程上下文的分配路径睡眠。登记并不取得锁。 */
    mt_init_flags(&tree, MT_FLAGS_LOCK_EXTERN);
    mt_set_external_lock(&tree, &prealloc_lock);
    mutex_lock(&prealloc_lock);

    ret = mas_preallocate(&mas, &prealloc_items[0], GFP_KERNEL);
    if (ret)
        goto destroy_tree;
    /* 准备后取消：释放当前操作资源，树中尚未发布 A。 */
    mas_destroy(&mas);
    if (mtree_load(&tree, 100)) {
        ret = -EINVAL;
        goto destroy_tree;
    }
    pr_info("maple_prealloc cancel: tree still empty\n");

    mas_set_range(&mas, 100, 199);
    ret = mas_preallocate(&mas, &prealloc_items[0], GFP_KERNEL);
    if (ret)
        goto destroy_tree;
    /* 同一保护期、同一写入请求；中间没有解锁或修改其他树范围。 */
    mas_store_prealloc(&mas, &prealloc_items[0]);
    if (mtree_load(&tree, 100) != &prealloc_items[0] ||
        mtree_load(&tree, 199) != &prealloc_items[0]) {
        ret = -EINVAL;
        goto destroy_tree;
    }
    pr_info("maple_prealloc publish: A[100,199]\n");

    /* gfp 封装自行处理状态资源；外部锁仍由本调用者持有。 */
    mas_set_range(&mas, 150, 249);
    ret = mas_store_gfp(&mas, &prealloc_items[1], GFP_KERNEL);
    if (ret)
        goto destroy_tree;
    if (mtree_load(&tree, 125) != &prealloc_items[0] ||
        mtree_load(&tree, 175) != &prealloc_items[1]) {
        ret = -EINVAL;
        goto destroy_tree;
    }
    pr_info("maple_prealloc overwrite: A then B, external lock retained\n");
destroy_tree:
    /* 外部锁模式使用不自行取得 ma_lock 的销毁入口。 */
    __mt_destroy(&tree);
    mutex_unlock(&prealloc_lock);
    return ret;
}

static void __exit note_maple_prealloc_exit(void)
{
    /* 初始化结束前已撤销私有树，无异步参与者。 */
}

module_init(note_maple_prealloc_init);
module_exit(note_maple_prealloc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Private Maple allocation and external-lock protocol");
