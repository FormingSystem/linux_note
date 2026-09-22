#include <stdio.h>

/* 只保存本次观察所需的向下连接，不模拟 Linux 颜色或父指针。 */
struct lookup_node {
    int key;
    char tag;
    struct lookup_node *left;
    struct lookup_node *right;
};

static struct lookup_node *find_any(struct lookup_node *node, int key)
{
    while (node) {
        if (key < node->key)
            node = node->left;
        else if (key > node->key)
            node = node->right;
        else
            return node;
    }
    return NULL;
}

static struct lookup_node *find_first(struct lookup_node *node, int key)
{
    struct lookup_node *match = NULL;

    while (node) {
        if (key <= node->key) {
            if (key == node->key)
                match = node;
            node = node->left;
        } else {
            node = node->right;
        }
    }
    return match;
}

/* 调用者保证存在右孩子；三个普通写入只在这个串行模型中执行。 */
static void rotate_left(struct lookup_node **root)
{
    struct lookup_node *old = *root;
    struct lookup_node *up = old->right;
    struct lookup_node *middle = up->left;

    old->right = middle; /* 先撤去旧的向上路径。 */
    up->left = old;      /* 再建立反向父子连接。 */
    *root = up;
}

/* 只用于故意造环的实验：到达预算时停止，绝不挂住终端。 */
static struct lookup_node *find_bounded(struct lookup_node *node, int key,
                                        unsigned int budget, int *exhausted)
{
    *exhausted = 0;
    while (node && budget) {
        --budget;
        if (key == node->key)
            return node;
        node = key < node->key ? node->left : node->right;
    }
    *exhausted = node != NULL;
    return NULL;
}

int main(void)
{
    struct lookup_node a = {10, 'A', NULL, NULL};
    struct lookup_node b = {10, 'B', NULL, NULL};
    struct lookup_node c = {10, 'C', NULL, NULL};
    struct lookup_node x = {10, 'X', NULL, NULL};
    struct lookup_node middle = {15, 'M', NULL, NULL};
    struct lookup_node y = {20, 'Y', NULL, NULL};
    struct lookup_node *root = &a;
    struct lookup_node *saved;
    struct lookup_node *any;
    struct lookup_node *first;
    int exhausted;

    a.right = &b;
    b.right = &c;
    rotate_left(&root);
    any = find_any(root, 10);
    first = find_first(root, 10);
    printf("equal: root=%c left=%c any=%c first=%c\n",
           root->tag, root->left->tag,
           any ? any->tag : '-', first ? first->tag : '-');

    x.right = &y;
    y.left = &middle;
    root = &x;
    saved = root; /* 模拟读者已经取走旧根，随后写者完成旋转。 */
    rotate_left(&root);
    printf("stale: saved=%d current=%d miss=%d current_hit=%d\n",
           saved->key, root->key, find_any(saved, 20) == NULL,
           find_any(root, 20) == &y);

    /* 重置后故意先反接：X.right 仍是 Y，而 Y.left 已经变为 X。 */
    x.right = &y;
    y.left = &middle;
    root = &x;
    y.left = &x;
    (void)find_bounded(root, 15, 6, &exhausted);
    printf("bad_order: budget_exhausted=%d cycle=%d\n",
           exhausted, x.right == &y && y.left == &x);

    /* 补上撤边和根更新；所有对象在 main 返回前一直存活。 */
    x.right = &middle;
    root = &y;
    printf("repaired: middle_hit=%d\n", find_any(root, 15) == &middle);
    return 0;
}
