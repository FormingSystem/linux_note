// SPDX-License-Identifier: GPL-2.0
/* 私有初始化实验：不发布外部入口；查询只复制值，移除交还独占对象。 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/errno.h>

struct demo_item {
    int key;
    int value;
    struct rb_node rb;
};

struct demo_tree {
    struct rb_root root;
    spinlock_t lock;
    unsigned int count;
};

/* 查找与插入共享同一排序规则，避免键相减的溢出问题。 */
static int compare_key(int key, const struct demo_item *item)
{
    return (key > item->key) - (key < item->key);
}

static struct demo_item *search_locked(struct demo_tree *tree, int key)
{
    struct rb_node *node = tree->root.rb_node;
    while (node) {
        struct demo_item *item = rb_entry(node, struct demo_item, rb);
        int cmp = compare_key(key, item);
        if (cmp < 0)
            node = node->rb_left;
        else if (cmp > 0)
            node = node->rb_right;
        else
            return item;
    }
    return NULL;
}

/* 调用者独占游离 item；成功移交给树，失败仍由调用者持有。 */
static int insert_item(struct demo_tree *tree, struct demo_item *item)
{
    struct rb_node **link = &tree->root.rb_node;
    struct rb_node *parent = NULL;
    int ret = 0;
    spin_lock(&tree->lock);
    if (!RB_EMPTY_NODE(&item->rb)) {
        ret = -EBUSY;
        goto out;
    }
    while (*link) {
        struct demo_item *entry = rb_entry(*link, struct demo_item, rb);
        int cmp = compare_key(item->key, entry);
        parent = *link;
        if (cmp < 0)
            link = &parent->rb_left;
        else if (cmp > 0)
            link = &parent->rb_right;
        else {
            ret = -EEXIST;
            goto out;
        }
    }
    rb_link_node(&item->rb, parent, link);
    rb_insert_color(&item->rb, &tree->root);
    ++tree->count;
out:
    spin_unlock(&tree->lock);
    return ret;
}

static int read_value(struct demo_tree *tree, int key, int *value)
{
    struct demo_item *item;
    int ret = -ENOENT;
    if (!value)
        return -EINVAL;
    spin_lock(&tree->lock);
    item = search_locked(tree, key);
    if (item) {
        *value = item->value;
        ret = 0;
    }
    spin_unlock(&tree->lock);
    return ret;
}

/* 本例没有外借指针、引用或 RCU 读者，可把树持有权移交给调用者。 */
static int remove_item(struct demo_tree *tree, int key, struct demo_item **removed)
{
    struct demo_item *item;
    if (!removed)
        return -EINVAL;
    *removed = NULL;
    spin_lock(&tree->lock);
    item = search_locked(tree, key);
    if (!item) {
        spin_unlock(&tree->lock);
        return -ENOENT;
    }
    rb_erase(&item->rb, &tree->root);
    RB_CLEAR_NODE(&item->rb);
    --tree->count;
    *removed = item;
    spin_unlock(&tree->lock);
    return 0;
}

static void destroy_tree(struct demo_tree *tree)
{
    for (;;) {
        struct rb_node *node;
        struct demo_item *item;
        spin_lock(&tree->lock);
        node = rb_first(&tree->root);
        if (!node) {
            spin_unlock(&tree->lock);
            break;
        }
        item = rb_entry(node, struct demo_item, rb);
        rb_erase(node, &tree->root);
        --tree->count;
        spin_unlock(&tree->lock);
        /* 已无使用者且不复用节点，无须为了即将释放再写游离标记。 */
        kfree(item);
    }
}

static int __init note_owner_init(void)
{
    const int keys[] = {20, 10, 30, 20};
    struct demo_tree tree;
    struct demo_item *item = NULL;
    int ret = 0, value = 0;
    unsigned int i;
    tree.root = RB_ROOT;
    tree.count = 0;
    spin_lock_init(&tree.lock);

    for (i = 0; i < ARRAY_SIZE(keys); ++i) {
        item = kmalloc(sizeof *item, GFP_KERNEL); /* 在自旋锁之外分配。 */
        if (!item) {
            ret = -ENOMEM;
            goto done;
        }
        item->key = keys[i];
        item->value = keys[i] * 10;
        RB_CLEAR_NODE(&item->rb);
        ret = insert_item(&tree, item);
        if (ret)
            kfree(item);         /* 失败未移交持有权；成功由整树清理回收。 */
        item = NULL;
        if (i == 3) {
            if (ret != -EEXIST) {
                ret = -EINVAL;
                goto done;
            }
            ret = 0;
        } else if (ret) {
            goto done;
        }
    }
    if (tree.count != 3 || read_value(&tree, 20, &value) || value != 200) {
        ret = -EINVAL;
        goto done;
    }
    pr_info("note_owner: duplicate rejected, count=3, value=200\n");
    ret = remove_item(&tree, 20, &item);
    if (ret)
        goto done;
    kfree(item);
    item = NULL;
    if (tree.count != 2 || read_value(&tree, 20, &value) != -ENOENT ||
        remove_item(&tree, 99, &item) != -ENOENT || item) {
        ret = -EINVAL;
        goto done;
    }
    pr_info("note_owner: remove 20, count=2, missing returns ENOENT\n");
done:
    destroy_tree(&tree);         /* 初始化失败也清理，不能等待模块退出。 */
    if (tree.count != 0)
        ret = -EINVAL;
    if (!ret)
        pr_info("note_owner: cleanup count=0\n");
    return ret;
}

static void __exit note_owner_exit(void)
{
    /* 实验在初始化返回前已撤销全部私有对象。 */
}
module_init(note_owner_init);
module_exit(note_owner_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Private rbtree ownership and copied-value exercise");
