#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct tree_node {
    char value;
    struct tree_node *left;
    struct tree_node *right;
};

void preorder_traversal(const struct tree_node *root)
{
    if (!root)
        return;
    printf("%c ", root->value);
    preorder_traversal(root->left);
    preorder_traversal(root->right);
}

static void destroy_tree(struct tree_node *root)
{
    if (!root)
        return;
    destroy_tree(root->left);
    destroy_tree(root->right);
    free(root);
}

static struct tree_node *build_demo_tree(int fail_at)
{
    struct tree_node *nodes[7] = {0};
    size_t made = 0;
    /* 全部分配成功前不连接，回滚只释放已经取得的独立节点。 */
    for (; made < 7; ++made) {
        if (fail_at == (int)made + 1)
            goto fail;
        nodes[made] = malloc(sizeof(*nodes[made]));
        if (!nodes[made])
            goto fail;
        *nodes[made] = (struct tree_node){(char)('A' + made), NULL, NULL};
    }
    for (size_t i = 0; i < 3; ++i) {
        nodes[i]->left = nodes[2 * i + 1];
        nodes[i]->right = nodes[2 * i + 2];
    }
    return nodes[0];
fail:
    while (made)
        free(nodes[--made]);
    return NULL;
}

static int parse_fail_at(int argc, char **argv)
{
    char *end;
    long value;
    if (argc == 1)
        return 0;
    if (argc != 2 || argv[1][0] == '\0')
        return -1;
    errno = 0;
    value = strtol(argv[1], &end, 10);
    if (errno || *end != '\0' || value < 0 || value > 7)
        return -1;
    return (int)value;
}

int main(int argc, char **argv)
{
    int fail_at = parse_fail_at(argc, argv);
    struct tree_node *root;
    if (fail_at < 0) {
        fprintf(stderr, "用法: %s [0..7]\n", argv[0]);
        return 2;
    }
    root = build_demo_tree(fail_at);
    if (!root) {
        fprintf(stderr, "构建失败，已释放先前节点\n");
        return 1;
    }
    printf("前序遍历结果: ");
    preorder_traversal(root);
    printf("\n");
    destroy_tree(root);
    return 0;
}
