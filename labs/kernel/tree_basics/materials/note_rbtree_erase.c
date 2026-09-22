// SPDX-License-Identifier: GPL-2.0
/* 独占小树：观察真实 rb_erase，所有对象仅在 run_case 内存活。 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/errno.h>

struct note_item {
    int key;
    bool linked;
    struct rb_node rb;
};

static int insert_item(struct rb_root *root, struct note_item *item)
{
    struct rb_node **link = &root->rb_node;
    struct rb_node *parent = NULL;

    while (*link) {
        struct note_item *entry = rb_entry(*link, struct note_item, rb);
        parent = *link;
        if (item->key < entry->key)
            link = &parent->rb_left;
        else if (item->key > entry->key)
            link = &parent->rb_right;
        else
            return -EEXIST;
    }
    rb_link_node(&item->rb, parent, link);
    rb_insert_color(&item->rb, root);
    item->linked = true;
    return 0;
}

/* 中序观察同时核对原数组对象身份；不是任意坏指针检查器。 */
static int inspect_tree(const char *name, struct rb_root *root,
                        struct note_item *items, const int *keys,
                        unsigned int count, unsigned int expected)
{
    bool seen[8] = {false};
    struct rb_node *node;
    unsigned int visited = 0, i;
    int previous = 0;
    bool first = true;

    for (node = rb_first(root); node; node = rb_next(node)) {
        struct note_item *item;
        if (++visited > count)
            return -EINVAL;
        for (i = 0; i < count && node != &items[i].rb; ++i) {}
        if (i == count || seen[i] || !items[i].linked)
            return -EINVAL;
        seen[i] = true;
        item = &items[i];
        if (!first && item->key <= previous)
            return -EINVAL;
        first = false;
        previous = item->key;
        pr_info("erase %s live[%u]=%d color=%c\n", name, i, item->key,
                (node->__rb_parent_color & 1UL) ? 'B' : 'R');
    }
    for (i = 0; i < count; ++i)
        if (seen[i] != items[i].linked || items[i].key != keys[i])
            return -EINVAL;
    return visited == expected ? 0 : -EINVAL;
}

static int run_case(const char *name, const int *keys, const unsigned int *order,
                    unsigned int count)
{
    struct note_item items[8] = {0};
    struct rb_root root = RB_ROOT;
    unsigned int i;
    int error;

    if (!count || count > ARRAY_SIZE(items))
        return -EINVAL;
    for (i = 0; i < count; ++i) {
        items[i].key = keys[i];
        RB_CLEAR_NODE(&items[i].rb);
        error = insert_item(&root, &items[i]);
        if (error)
            return error;
    }
    error = inspect_tree(name, &root, items, keys, count, count);
    if (error)
        return error;
    for (i = 0; i < count; ++i) {
        struct note_item *victim;
        bool empty_after_erase;
        if (order[i] >= count || !items[order[i]].linked)
            return -EINVAL;
        victim = &items[order[i]];
        rb_erase(&victim->rb, &root);
        /* 本例对象仍存活，先观察删除并不自动设置游离标记。 */
        empty_after_erase = RB_EMPTY_NODE(&victim->rb);
        victim->linked = false;
        RB_CLEAR_NODE(&victim->rb);
        pr_info("erase %s removed=%d empty_before_clear=%d empty_after_clear=%d\n",
                name, victim->key, empty_after_erase, RB_EMPTY_NODE(&victim->rb));
        error = inspect_tree(name, &root, items, keys, count, count - i - 1);
        if (error)
            return error;
    }
    /* 无堆分配、外部发布或回调；成功和失败都只结束这个私有观察。 */
    return root.rb_node ? -EINVAL : 0;
}

static int __init note_init(void)
{
    static const int transplant[] = {20, 10, 30, 25};
    static const unsigned int transplant_order[] = {0, 1, 2, 3};
    static const int borrow[] = {20, 10, 30, 25, 40};
    static const unsigned int borrow_order[] = {1, 0, 2, 3, 4};
    int error;

    error = run_case("transplant", transplant, transplant_order,
                     ARRAY_SIZE(transplant));
    if (error)
        return error;
    return run_case("borrow", borrow, borrow_order, ARRAY_SIZE(borrow));
}

static void __exit note_exit(void)
{
    pr_info("erase observation unloaded\n");
}
module_init(note_init);
module_exit(note_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("私有红黑树删除与对象身份观察");
