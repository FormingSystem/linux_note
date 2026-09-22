#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct bst_node {
    int key;
    struct bst_node *left;
    struct bst_node *right;
};

/* 注入点仅用于复现本次构建失败；正常运行 fail_at 为零。 */
static int allocation_attempt, fail_at;

struct bst_node *bst_create_node(int key)
{
    struct bst_node *node;
    if (++allocation_attempt == fail_at)
        return NULL;
    node = malloc(sizeof(*node));
    if (node)
        *node = (struct bst_node){key, NULL, NULL};
    return node;
}

struct bst_node *bst_find(struct bst_node *root, int key)
{
    while (root) {
        if (key == root->key)
            return root;
        root = key < root->key ? root->left : root->right;
    }
    return NULL;
}

/* root 是有效根变量的地址；返回 1 成功、0 已存在、-1 无内存。 */
int bst_insert(struct bst_node **root, int key)
{
    struct bst_node **link = root;
    while (*link) {
        if (key < (*link)->key)
            link = &(*link)->left;
        else if (key > (*link)->key)
            link = &(*link)->right;
        else
            return 0;
    }
    struct bst_node *node = bst_create_node(key);
    if (!node)
        return -1;
    *link = node;
    return 1;
}

static void bst_inorder(const struct bst_node *root)
{
    if (!root)
        return;
    bst_inorder(root->left);
    printf("%d ", root->key);
    bst_inorder(root->right);
}

static void bst_destroy(struct bst_node *root)
{
    if (!root)
        return;
    bst_destroy(root->left);
    bst_destroy(root->right);
    free(root);
}

int main(int argc, char **argv)
{
    const int keys[] = {8, 3, 10, 1, 6, 14, 4, 7, 13};
    struct bst_node *root = NULL;
    if (argc > 2)
        return 2;
    if (argc == 2) {
        char *end;
        errno = 0;
        long value = strtol(argv[1], &end, 10);
        if (errno || end == argv[1] || *end || value < 0 || value > 9)
            return 2;
        fail_at = (int)value;
    }
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        if (bst_insert(&root, keys[i]) != 1) {
            fprintf(stderr, "构建失败，回收之前成功插入的节点\n");
            bst_destroy(root);
            return 1;
        }
    }
    printf("中序: ");
    bst_inorder(root);
    printf("\n重复插入 6: %d\n", bst_insert(&root, 6));
    struct bst_node *found = bst_find(root, 7);
    printf("查找 7: %d；查找 100: %s\n", found ? found->key : -1,
           bst_find(root, 100) ? "存在" : "不存在");
    bst_destroy(root);
    return 0;
}
