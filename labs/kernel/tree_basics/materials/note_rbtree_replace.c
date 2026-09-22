// SPDX-License-Identifier: GPL-2.0
/* 私有替换观察：普通/cached 使用自动对象，RCU 场景只作单任务顺序重放。 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/errno.h>

struct replace_item {
    int key;
    int payload;
    struct rb_node rb;
};

static int insert_item(struct rb_root *root, struct replace_item *item)
{
    struct rb_node **slot = &root->rb_node;
    struct rb_node *parent = NULL;
    while (*slot) {
        struct replace_item *entry = rb_entry(*slot, struct replace_item, rb);
        parent = *slot;
        if (item->key < entry->key)
            slot = &parent->rb_left;
        else if (item->key > entry->key)
            slot = &parent->rb_right;
        else
            return -EEXIST;
    }
    rb_link_node(&item->rb, parent, slot);
    rb_insert_color(&item->rb, root);
    return 0;
}

static int run_private(bool cached)
{
    struct replace_item items[3] = {0};
    struct replace_item replacement = {0};
    struct rb_root_cached root = RB_ROOT_CACHED;
    struct rb_node saved_rb, *node;
    struct replace_item *victim;
    unsigned int i, visited = 0;
    int error;

    for (i = 0; i < ARRAY_SIZE(items); ++i) {
        items[i].key = (i + 1) * 10;
        items[i].payload = i + 1;
        error = insert_item(&root.rb_root, &items[i]);
        if (error)
            return error;
    }
    /* 根尚未发布：完成私有构建后一次性建立最左缓存。 */
    root.rb_leftmost = rb_first(&root.rb_root);
    victim = cached ? &items[0] : &items[1];
    replacement.key = victim->key;
    replacement.payload = 1000;
    saved_rb = victim->rb;

    if (cached)
        rb_replace_node_cached(&victim->rb, &replacement.rb, &root);
    else
        rb_replace_node(&victim->rb, &replacement.rb, &root.rb_root);
    if (replacement.payload != 1000 || victim->key != replacement.key ||
        victim->rb.__rb_parent_color != saved_rb.__rb_parent_color ||
        victim->rb.rb_left != saved_rb.rb_left || victim->rb.rb_right != saved_rb.rb_right)
        return -EINVAL;
    if (cached && root.rb_leftmost != &replacement.rb)
        return -EINVAL;
    if (!cached && root.rb_root.rb_node != &replacement.rb)
        return -EINVAL;
    for (node = rb_first(&root.rb_root); node; node = rb_next(node)) {
        struct replace_item *entry = rb_entry(node, struct replace_item, rb);
        if (++visited > ARRAY_SIZE(items) || node == &victim->rb)
            return -EINVAL;
        if ((node->rb_left && rb_parent(node->rb_left) != node) ||
            (node->rb_right && rb_parent(node->rb_right) != node))
            return -EINVAL;
        pr_info("replace %s key=%d payload=%d\n",
                cached ? "cached" : "plain", entry->key, entry->payload);
    }
    pr_info("replace old key=%d payload=%d empty_marker=%d\n",
            victim->key, victim->payload, RB_EMPTY_NODE(&victim->rb));
    return visited == ARRAY_SIZE(items) ? 0 : -EINVAL;
}

static int run_rcu_replay(void)
{
    struct replace_item *old, *new, *saved, *published;
    struct rb_root root = RB_ROOT;
    struct rb_node *node;
    int error = 0;

    old = kmalloc(sizeof(*old), GFP_KERNEL);
    if (!old)
        return -ENOMEM;
    new = kmalloc(sizeof(*new), GFP_KERNEL);
    if (!new) {
        kfree(old);
        return -ENOMEM;
    }
    old->key = new->key = 20;
    old->payload = 1;
    new->payload = 2;
    rb_link_node(&old->rb, NULL, &root.rb_node);
    rb_insert_color(&old->rb, &root);

    /* 本任务既保存旧视图又执行替换，无其他写者；不是并发压力测试。 */
    rcu_read_lock();
    node = rcu_dereference(root.rb_node);
    saved = rb_entry(node, struct replace_item, rb);
    rb_replace_node_rcu(&old->rb, &new->rb, &root);
    node = rcu_dereference(root.rb_node);
    published = rb_entry(node, struct replace_item, rb);
    if (saved != old || published != new || saved->payload != 1 || published->payload != 2)
        error = -EINVAL;
    pr_info("replace rcu saved_payload=%d published_payload=%d\n",
            saved->payload, published->payload);
    rcu_read_unlock();

    /* 私有根不再使用；等待必须在退出读侧后，其他持有权本例不存在。 */
    rb_erase(&new->rb, &root);
    synchronize_rcu();
    kfree(old);
    kfree(new);
    return error;
}

static int __init note_init(void)
{
    int error = run_private(false);
    if (error)
        return error;
    error = run_private(true);
    if (error)
        return error;
    return run_rcu_replay();
}
static void __exit note_exit(void)
{
    pr_info("replace observation unloaded\n");
}
module_init(note_init);
module_exit(note_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("同键替换与旧对象寿命观察");
