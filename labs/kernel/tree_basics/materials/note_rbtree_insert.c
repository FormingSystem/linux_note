// SPDX-License-Identifier: GPL-2.0
/* 私有插入观察：所有节点与根仅在 run_case 内存活，不发布给外部读者。 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/errno.h>

struct note_item {
    int key;
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
    return 0;
}

/* 本次固定版本将最低位用于颜色：黑为 1，红为 0；只做观察。 */
static char node_color(const struct rb_node *node)
{
    return (node->__rb_parent_color & 1UL) ? 'B' : 'R';
}

static int run_case(const char *name, const int *keys, unsigned int count,
                    int expected_root)
{
    struct note_item items[4] = {0};
    struct rb_root root = RB_ROOT;
    struct rb_node *node;
    struct note_item *top;
    unsigned int i;
    int error;

    if (!count || count > ARRAY_SIZE(items))
        return -EINVAL;
    for (i = 0; i < count; ++i) {
        items[i].key = keys[i];
        error = insert_item(&root, &items[i]);
        if (error)
            return error;
    }
    top = rb_entry(root.rb_node, struct note_item, rb);
    if (top->key != expected_root || node_color(root.rb_node) != 'B')
        return -EINVAL;
    pr_info("note_rbtree_insert: %s root=%d\n", name, top->key);
    for (node = rb_first(&root); node; node = rb_next(node)) {
        struct note_item *item = rb_entry(node, struct note_item, rb);
        struct rb_node *parent = rb_parent(node);

        if (parent) {
            struct note_item *up = rb_entry(parent, struct note_item, rb);

            pr_info("note_rbtree_insert: key=%d parent=%d color=%c\n",
                    item->key, up->key, node_color(node));
        } else {
            pr_info("note_rbtree_insert: key=%d parent=none color=%c\n",
                    item->key, node_color(node));
        }
    }
    /* 根和节点都不逃逸；没有分配、注册、回调或等待释放的外部持有者。 */
    return 0;
}

static int __init note_rbtree_insert_init(void)
{
    static const int cases[5][4] = {
        {30, 20, 10, 0}, {30, 10, 20, 0},
        {10, 20, 30, 0}, {10, 30, 20, 0},
        {50, 30, 70, 20}
    };
    static const char *const names[5] = {"LL", "LR", "RR", "RL", "recolor"};
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(cases); ++i) {
        int error = run_case(names[i], cases[i], i == 4 ? 4 : 3,
                             i == 4 ? 50 : 20);

        if (error) {
            pr_err("note_rbtree_insert: %s failed: %d\n", names[i], error);
            return error;
        }
    }
    return 0;
}

static void __exit note_rbtree_insert_exit(void)
{
    pr_info("note_rbtree_insert: observation complete\n");
}
module_init(note_rbtree_insert_init);
module_exit(note_rbtree_insert_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("私有红黑树插入、父链与颜色观察");
