#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct avl_node {
    int key;
    int height; /* 缓存按边计算的高度，空孩子高度为 -1。 */
    struct avl_node *left, *right, *parent;
};

enum avl_rotation_case {
    AVL_CASE_INVALID, AVL_CASE_LL, AVL_CASE_LR, AVL_CASE_RR, AVL_CASE_RL
};

/* 仅供本程序观察本次改边代价，不参与平衡判断。 */
static unsigned avl_rotation_count;
static unsigned avl_repair_count;

static int avl_height(const struct avl_node *node)
{
    return node ? node->height : -1;
}

static void avl_update_height(struct avl_node *node)
{
    int left = avl_height(node->left);
    int right = avl_height(node->right);
    node->height = 1 + (left > right ? left : right);
}

static int avl_balance_factor(const struct avl_node *node)
{
    return avl_height(node->left) - avl_height(node->right);
}

static struct avl_node *avl_left_rotate(struct avl_node **root,
                                      struct avl_node *g)
{
    struct avl_node *up = g->right;
    g->right = up->left;
    if (up->left) up->left->parent = g;
    up->parent = g->parent;
    if (!g->parent) *root = up;
    else if (g == g->parent->left) g->parent->left = up;
    else g->parent->right = up;
    up->left = g;
    g->parent = up;
    /* 新根高度依赖下沉节点，不能反过来更新。 */
    avl_update_height(g);
    avl_update_height(up);
    ++avl_rotation_count;
    return up;
}

static struct avl_node *avl_right_rotate(struct avl_node **root,
                                       struct avl_node *g)
{
    struct avl_node *up = g->left;
    g->left = up->right;
    if (up->right) up->right->parent = g;
    up->parent = g->parent;
    if (!g->parent) *root = up;
    else if (g == g->parent->left) g->parent->left = up;
    else g->parent->right = up;
    up->right = g;
    g->parent = up;
    avl_update_height(g);
    avl_update_height(up);
    ++avl_rotation_count;
    return up;
}

static struct avl_node *avl_taller_child(struct avl_node *node)
{
    if (!node) return NULL;
    int left = avl_height(node->left), right = avl_height(node->right);
    if (left > right) return node->left;
    if (left < right) return node->right;
    /* p 等高时选相对 g 的外侧，使用单旋。 */
    if (node->parent && node == node->parent->right)
        return node->right ? node->right : node->left;
    return node->left ? node->left : node->right;
}

static enum avl_rotation_case
avl_identify_rotation_case(struct avl_node *g, struct avl_node *p,
                          struct avl_node *n)
{
    if (!g || !p || !n) return AVL_CASE_INVALID;
    if (p == g->left) {
        if (n == p->left) return AVL_CASE_LL;
        if (n == p->right) return AVL_CASE_LR;
    }
    if (p == g->right) {
        if (n == p->right) return AVL_CASE_RR;
        if (n == p->left) return AVL_CASE_RL;
    }
    return AVL_CASE_INVALID;
}

static struct avl_node *
avl_rebalance_at(struct avl_node **root, struct avl_node *g,
                 struct avl_node *p, enum avl_rotation_case rot_case)
{
    ++avl_repair_count;
    switch (rot_case) {
    case AVL_CASE_LL: return avl_right_rotate(root, g);
    case AVL_CASE_LR:
        avl_left_rotate(root, p);
        return avl_right_rotate(root, g);
    case AVL_CASE_RR: return avl_left_rotate(root, g);
    case AVL_CASE_RL:
        avl_right_rotate(root, p);
        return avl_left_rotate(root, g);
    default: return g;
    }
}

static void avl_fix_up(struct avl_node **root, struct avl_node *cur,
                       bool inserting)
{
    while (cur) {
        /* 进入本层前，孩子的结构与高度已经一致；cur 仍保存旧高度。 */
        int old_height = cur->height;
        avl_update_height(cur);
        int bf = avl_balance_factor(cur);
        if (bf < -1 || bf > 1) {
            struct avl_node *p = avl_taller_child(cur);
            struct avl_node *n = avl_taller_child(p);
            enum avl_rotation_case kind = avl_identify_rotation_case(cur, p, n);
            struct avl_node *new_subroot = avl_rebalance_at(root, cur, p, kind);
            /* 插入恢复旧高度；删除等高孩子分支也可能无需再传播。 */
            if (inserting || new_subroot->height == old_height) return;
            cur = new_subroot->parent;
        } else {
            if (cur->height == old_height) return;
            cur = cur->parent;
        }
    }
}

static struct avl_node *avl_find(struct avl_node *root, int key)
{
    while (root && root->key != key)
        root = key < root->key ? root->left : root->right;
    return root;
}

/* 返回 1 表示插入，0 表示重复，-1 表示无内存；失败不改变原树。 */
static int avl_insert(struct avl_node **root, int key)
{
    struct avl_node **slot = root, *parent = NULL;
    while (*slot) {
        parent = *slot;
        if (key == parent->key) return 0;
        slot = key < parent->key ? &parent->left : &parent->right;
    }
    struct avl_node *node = malloc(sizeof(*node));
    if (!node) return -1;
    node->key = key;
    node->height = 0;
    node->left = node->right = NULL;
    node->parent = parent;
    *slot = node; /* 私有初始化完成后才接入根或父槽。 */
    avl_fix_up(root, parent, true);
    return 1;
}

/* 返回是否移除了键；独占调用期间允许暂时存在两份后继键。 */
static bool avl_erase(struct avl_node **root, int key)
{
    struct avl_node *victim = avl_find(*root, key);
    if (!victim) return false;
    if (victim->left && victim->right) {
        struct avl_node *successor = victim->right;
        while (successor->left) successor = successor->left;
        victim->key = successor->key;
        victim = successor; /* 真正释放的对象是后继。 */
    }
    struct avl_node *parent = victim->parent;
    struct avl_node *child = victim->left ? victim->left : victim->right;
    if (child) child->parent = parent;
    if (!parent) *root = child;
    else if (victim == parent->left) parent->left = child;
    else parent->right = child;
    free(victim);
    avl_fix_up(root, parent, false); /* 从物理删除位置的父节点起步。 */
    return true;
}

static void avl_destroy(struct avl_node *node)
{
    if (!node) return;
    avl_destroy(node->left);
    avl_destroy(node->right);
    free(node);
}

static void avl_print(const struct avl_node *node)
{
    if (!node) return;
    avl_print(node->left);
    printf("%d ", node->key);
    avl_print(node->right);
}

int main(void)
{
    const int input[] = {50,30,70,20,40,60,80,10,25,35,45};
    const int removed[] = {80,70,60,10,25};
    struct avl_node *root = NULL;
    for (unsigned i=0; i<sizeof(input)/sizeof(input[0]); ++i) {
        if (avl_insert(&root, input[i]) < 0) {
            fputs("节点分配失败，回收已建成的树。\n", stderr);
            avl_destroy(root);
            return EXIT_FAILURE;
        }
    }
    printf("built height=%d root=%d\n", avl_height(root), root->key);
    for (unsigned i=0; i<sizeof(removed)/sizeof(removed[0]); ++i) {
        avl_rotation_count = avl_repair_count = 0;
        bool erased = avl_erase(&root, removed[i]);
        printf("erase=%d found=%d height=%d root=%d repairs=%u rotations=%u\n",
               removed[i], erased, avl_height(root), root->key,
               avl_repair_count, avl_rotation_count);
    }
    printf("inorder: ");
    avl_print(root);
    printf("\n");
    avl_destroy(root);
    return 0;
}
