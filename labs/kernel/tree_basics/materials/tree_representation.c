/* 普通树实验：名字借用字符串常量，节点内存由当前程序独占。 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct tree_node {
    const char *name;
    struct tree_node *first_child;
    struct tree_node *next_sibling;
};

static struct tree_node *tree_node_create(const char *name)
{
    struct tree_node *node = malloc(sizeof(*node));
    if (!node)
        return NULL;
    node->name = name;
    node->first_child = NULL;
    node->next_sibling = NULL;
    return node;
}

/* 仅接受尚未属于任何树的独立新节点；调用者保证没有环和重复挂接。 */
static void tree_add_child(struct tree_node *parent, struct tree_node *child)
{
    struct tree_node **slot = &parent->first_child;
    while (*slot)
        slot = &(*slot)->next_sibling;
    *slot = child;
}

static void tree_print_preorder(const struct tree_node *node, int depth)
{
    const struct tree_node *child;
    int index;

    if (!node)
        return;
    for (index = 0; index < depth; ++index)
        printf("  ");
    printf("%s\n", node->name);
    for (child = node->first_child; child; child = child->next_sibling)
        tree_print_preorder(child, depth + 1);
}

static size_t tree_count_nodes(const struct tree_node *node)
{
    const struct tree_node *child;
    size_t count = node ? 1 : 0;

    if (node)
        for (child = node->first_child; child; child = child->next_sibling)
            count += tree_count_nodes(child);
    return count;
}

/* 高度按边数计：空树为 -1，叶子为 0。 */
static int tree_height(const struct tree_node *node)
{
    const struct tree_node *child;
    int highest = -1;

    if (!node)
        return -1;
    for (child = node->first_child; child; child = child->next_sibling) {
        int child_height = tree_height(child);
        if (child_height > highest)
            highest = child_height;
    }
    return highest + 1;
}

static size_t tree_count_leaves(const struct tree_node *node)
{
    const struct tree_node *child;
    size_t count = 0;

    if (!node)
        return 0;
    if (!node->first_child)
        return 1;
    for (child = node->first_child; child; child = child->next_sibling)
        count += tree_count_leaves(child);
    return count;
}

static void tree_destroy(struct tree_node *node)
{
    struct tree_node *child;

    if (!node)
        return;
    child = node->first_child;
    while (child) {
        /* 下一兄弟保存在将释放对象中，必须先取出。 */
        struct tree_node *next = child->next_sibling;
        tree_destroy(child);
        child = next;
    }
    free(node);
}

int main(int argc, char **argv)
{
    const char *const names[] = { "/", "home", "etc", "usr", "ssh" };
    struct tree_node *nodes[5] = { NULL };
    size_t index, created = 0;
    long fail_at = 0;

    if (argc > 2) {
        fprintf(stderr, "用法：tree_representation [0..5]\n");
        return 2;
    }
    if (argc == 2) {
        char *end;
        errno = 0;
        fail_at = strtol(argv[1], &end, 10);
        if (errno || end == argv[1] || *end || fail_at < 0 || fail_at > 5) {
            fprintf(stderr, "失败序号必须为 0..5\n");
            return 2;
        }
    }
    /* 分配阶段没有建立连接；失败时逐一释放私有对象即可。 */
    for (index = 0; index < 5; ++index) {
        if (fail_at != (long)index + 1)
            nodes[index] = tree_node_create(names[index]);
        if (!nodes[index]) {
            fprintf(stderr, "第 %zu 次分配失败；回收 %zu 个私有节点\n",
                    index + 1, created);
            while (created)
                free(nodes[--created]);
            return 1;
        }
        ++created;
    }

    tree_add_child(nodes[0], nodes[1]);
    tree_add_child(nodes[0], nodes[2]);
    tree_add_child(nodes[0], nodes[3]);
    tree_add_child(nodes[2], nodes[4]);
    tree_print_preorder(nodes[0], 0);
    printf("nodes=%zu height=%d leaves=%zu\n", tree_count_nodes(nodes[0]),
           tree_height(nodes[0]), tree_count_leaves(nodes[0]));
    tree_destroy(nodes[0]); /* 成树后按唯一根回收，不再逐项 free 数组。 */
    return 0;
}
