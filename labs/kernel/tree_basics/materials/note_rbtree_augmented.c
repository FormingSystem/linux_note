// SPDX-License-Identifier: GPL-2.0
/* 闭区间摘要：私有自动对象，不发布并发入口。 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rbtree_augmented.h>
#include <linux/errno.h>

struct range_item {
    unsigned long first;
    unsigned long last;
    unsigned long subtree_last;
    struct rb_node rb;
};

static unsigned long item_last(struct range_item *item)
{
    return item->last;
}

RB_DECLARE_CALLBACKS_MAX(static, range_callbacks, struct range_item, rb,
                        unsigned long, subtree_last, item_last)

static int insert_range(struct rb_root *root, struct range_item *item)
{
    struct rb_node **link = &root->rb_node;
    struct rb_node *parent = NULL;
    if (item->first > item->last)
        return -EINVAL;
    while (*link) {
        struct range_item *entry = rb_entry(*link, struct range_item, rb);
        parent = *link;
        if (item->first < entry->first)
            link = &parent->rb_left;
        else if (item->first > entry->first)
            link = &parent->rb_right;
        else
            return -EEXIST; /* 重复起点失败时尚未改任何祖先摘要。 */
    }
    item->subtree_last = item->last;
    rb_link_node(&item->rb, parent, link);
    range_callbacks.propagate(parent, NULL);
    rb_insert_augmented(&item->rb, root, &range_callbacks);
    return 0;
}

/* 独立递归读取原始 last，不拿缓存字段计算期望最大值。 */
static unsigned long inspect_summary(struct rb_node *node, bool *valid)
{
    struct range_item *item;
    unsigned long result, child;
    if (!node)
        return 0;
    item = rb_entry(node, struct range_item, rb);
    result = item->last;
    child = inspect_summary(node->rb_left, valid);
    if (child > result)
        result = child;
    child = inspect_summary(node->rb_right, valid);
    if (child > result)
        result = child;
    if (item->subtree_last != result)
        *valid = false;
    return result;
}

/* 返回任意覆盖 point 的闭区间；只能在本例独占且摘要正确时使用。 */
static struct range_item *find_point(struct rb_root *root, unsigned long point)
{
    struct rb_node *node = root->rb_node;
    while (node) {
        struct range_item *item = rb_entry(node, struct range_item, rb);
        if (node->rb_left) {
            struct range_item *left = rb_entry(node->rb_left, struct range_item, rb);
            if (left->subtree_last >= point) {
                node = node->rb_left;
                continue;
            }
        }
        if (item->first > point)
            return NULL;
        if (item->last >= point)
            return item;
        node = node->rb_right;
    }
    return NULL;
}

static int __init note_augmented_init(void)
{
    struct range_item items[] = {
        {.first=20,.last=21}, {.first=10,.last=12}, {.first=30,.last=80},
        {.first=25,.last=29}, {.first=35,.last=40}, {.first=5,.last=6}
    };
    struct rb_root root = RB_ROOT;
    struct range_item *found;
    bool valid = true;
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(items); ++i) {
        if (insert_range(&root, &items[i]))
            return -EINVAL;
        inspect_summary(root.rb_node, &valid);
        if (!valid)
            return -EINVAL;
    }
    found = find_point(&root, 72);
    if (found != &items[2])
        return -EINVAL;
    pr_info("note_augmented: max=80, point72 finds [30,80]\n");

    /* 排序起点不变，只改载荷；从该节点重算，不预先覆盖旧摘要。 */
    items[2].last = 33;
    range_callbacks.propagate(&items[2].rb, NULL);
    if (inspect_summary(root.rb_node, &valid) != 40 || !valid ||
        find_point(&root, 72))
        return -EINVAL;
    pr_info("note_augmented: payload shrinks, max=40, point72 absent\n");

    for (i = 0; i < ARRAY_SIZE(items); ++i) {
        rb_erase_augmented(&items[i].rb, &root, &range_callbacks);
        inspect_summary(root.rb_node, &valid);
        if (!valid)
            return -EINVAL;
    }
    if (root.rb_node)
        return -EINVAL;
    pr_info("note_augmented: all removals preserve summaries\n");
    return 0;
}

static void __exit note_augmented_exit(void)
{
    /* 自动存储对象已在初始化返回前结束，未向外部发布地址。 */
}
module_init(note_augmented_init);
module_exit(note_augmented_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Private augmented rbtree interval summary exercise");
