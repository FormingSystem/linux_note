// SPDX-License-Identifier: GPL-2.0
/* 私有遍历实验：正确销毁使用堆对象，错误混用只用自动对象。 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/errno.h>

struct walk_item {
    int key;
    unsigned int id;
    struct rb_node rb;
};

static int insert_item(struct rb_root *root, struct walk_item *item)
{
    struct rb_node **slot = &root->rb_node;
    struct rb_node *parent = NULL;

    while (*slot) {
        struct walk_item *other = rb_entry(*slot, struct walk_item, rb);
        parent = *slot;
        if (item->key < other->key)
            slot = &parent->rb_left;
        else if (item->key > other->key)
            slot = &parent->rb_right;
        else
            return -EEXIST;
    }
    rb_link_node(&item->rb, parent, slot);
    rb_insert_color(&item->rb, root);
    return 0;
}

/* 调用者已排除外部读者；不调用 rb_erase，也不再查找半销毁的树。 */
static unsigned int destroy_tree(struct rb_root *root)
{
    struct walk_item *pos, *next;
    unsigned int count = 0;

    rbtree_postorder_for_each_entry_safe(pos, next, root, rb) {
        pr_info("walk destroy id=%u key=%d\n", pos->id, pos->key);
        ++count;
        kfree(pos);
    }
    *root = RB_ROOT;
    return count;
}

static int build_tree(struct rb_root *root, const int *keys, unsigned int count)
{
    unsigned int i;
    int error;

    *root = RB_ROOT;
    for (i = 0; i < count; ++i) {
        struct walk_item *item = kmalloc(sizeof(*item), GFP_KERNEL);
        if (!item) {
            error = -ENOMEM;
            goto fail;
        }
        item->key = keys[i];
        item->id = i;
        error = insert_item(root, item);
        if (error) {
            kfree(item); /* 尚未入树的对象单独回收。 */
            goto fail;
        }
    }
    return 0;
fail:
    destroy_tree(root); /* 已入树部分统一回收，失败返回空根。 */
    return error;
}

static int show_bad_mix(void)
{
    const int keys[] = {10, 20, 30, 40};
    struct walk_item items[4] = {0};
    struct rb_root root = RB_ROOT;
    struct rb_node *node, *next;
    unsigned int i, visited = 0;
    int error;

    for (i = 0; i < ARRAY_SIZE(items); ++i) {
        items[i].key = keys[i];
        items[i].id = i;
        error = insert_item(&root, &items[i]);
        if (error)
            return error;
    }
    for (node = rb_first_postorder(&root); node; node = next) {
        if (++visited > ARRAY_SIZE(items))
            return -EINVAL;
        next = rb_next_postorder(node);
        pr_info("walk bad_mix erase=%d\n",
                rb_entry(node, struct walk_item, rb)->key);
        rb_erase(node, &root); /* 故意重排，仅观察漏访；不释放任何对象。 */
    }
    if (visited != 3 || root.rb_node != &items[1].rb)
        return -EINVAL;
    pr_info("walk bad_mix missed=%d\n", items[1].key);
    return 0;
}

static int __init note_init(void)
{
    const int keys[] = {10, 20, 30, 40};
    struct rb_root root = RB_ROOT;
    struct rb_node *node, *next;
    int error;

    error = build_tree(&root, keys, ARRAY_SIZE(keys));
    if (error)
        return error;
    for (node = rb_last(&root); node; node = rb_prev(node))
        pr_info("walk reverse key=%d\n",
                rb_entry(node, struct walk_item, rb)->key);
    /* 独占树，保存的下一对象全程存活；每次只删除当前对象。 */
    for (node = rb_first(&root); node; node = next) {
        struct walk_item *item = rb_entry(node, struct walk_item, rb);
        next = rb_next(node);
        rb_erase(node, &root);
        pr_info("walk inorder erase=%d\n", item->key);
        kfree(item);
    }
    if (root.rb_node) {
        destroy_tree(&root);
        return -EINVAL;
    }

    error = build_tree(&root, keys, ARRAY_SIZE(keys));
    if (error)
        return error;
    if (destroy_tree(&root) != ARRAY_SIZE(keys))
        return -EINVAL;
    return show_bad_mix();
}

static void __exit note_exit(void)
{
    pr_info("walk observation unloaded\n");
}
module_init(note_init);
module_exit(note_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("有序取消与后序整树销毁观察");
