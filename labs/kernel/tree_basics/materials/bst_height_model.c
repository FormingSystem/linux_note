#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#define NODE_CAPACITY 31

struct bst_node {
    int key;
    struct bst_node *left;
    struct bst_node *right;
};

struct tree_model {
    struct bst_node nodes[NODE_CAPACITY];
    size_t used;
    struct bst_node *root;
    unsigned long insert_visits;
};

/* 返回 1 已插入、0 已存在、-1 池满；一次到达节点计一次访问。 */
static int insert_key(struct tree_model *tree, int key)
{
    struct bst_node **link = &tree->root;
    while (*link) {
        ++tree->insert_visits;
        if (key == (*link)->key)
            return 0;
        link = key < (*link)->key ? &(*link)->left : &(*link)->right;
    }
    if (tree->used == NODE_CAPACITY)
        return -1;
    struct bst_node *node = &tree->nodes[tree->used++];
    *node = (struct bst_node){key, NULL, NULL};
    *link = node;
    return 1;
}

static int insert_middle_first(struct tree_model *tree, int low, int high)
{
    if (low > high)
        return 1;
    int middle = low + (high - low) / 2;
    return insert_key(tree, middle) == 1 &&
           insert_middle_first(tree, low, middle - 1) &&
           insert_middle_first(tree, middle + 1, high);
}

static int tree_height(const struct bst_node *root)
{
    if (!root)
        return -1;
    int left = tree_height(root->left), right = tree_height(root->right);
    return (left > right ? left : right) + 1;
}

static const struct bst_node *find_key(const struct bst_node *root,
                                       int key, unsigned *visits)
{
    *visits = 0;
    while (root) {
        ++*visits;
        if (key == root->key)
            return root;
        root = key < root->key ? root->left : root->right;
    }
    return NULL;
}

static void compare_orders(int count)
{
    /* 池和入口共处同一对象，建好后不按值复制 tree_model。 */
    struct tree_model ordered = {0}, middle_first = {0};
    unsigned ordered_visits, middle_visits;
    for (int key = 1; key <= count; ++key) {
        int inserted = insert_key(&ordered, key);
        assert(inserted == 1);
    }
    int built = insert_middle_first(&middle_first, 1, count);
    assert(built);
    const struct bst_node *a = find_key(ordered.root, count, &ordered_visits);
    const struct bst_node *b = find_key(middle_first.root, count, &middle_visits);
    assert(a && b);
    printf("%d ordered %d %lu %u\n", count, tree_height(ordered.root),
           ordered.insert_visits, ordered_visits);
    printf("%d middle  %d %lu %u\n", count, tree_height(middle_first.root),
           middle_first.insert_visits, middle_visits);
}

int main(void)
{
    printf("nodes order height build_visits find_max_visits\n");
    compare_orders(7);
    compare_orders(15);
    compare_orders(31);
    return 0;
}
