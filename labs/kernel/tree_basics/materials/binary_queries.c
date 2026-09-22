#include <assert.h>
#include <stdio.h>

struct binary_node {
    int value;
    struct binary_node *left;
    struct binary_node *right;
};

static int binary_tree_height(struct binary_node *node)
{
	int left_h;
	int right_h;

	if (!node)
		return -1;

	left_h = binary_tree_height(node->left);
	right_h = binary_tree_height(node->right);

	return (left_h > right_h ? left_h : right_h) + 1;
}

static int binary_tree_count_nodes(struct binary_node *node)
{
	if (!node)
		return 0;

	return binary_tree_count_nodes(node->left) +
	       binary_tree_count_nodes(node->right) + 1;
}

static int binary_tree_is_leaf(struct binary_node *node)
{
	if (!node)
		return 0;

	return !node->left && !node->right;
}

static struct binary_node *
binary_tree_find(struct binary_node *node, int target)
{
	struct binary_node *found;

	if (!node)
		return NULL;

	if (node->value == target)
		return node;

	found = binary_tree_find(node->left, target);
	if (found)
		return found;

	return binary_tree_find(node->right, target);
}

int main(void)
{
    /* 故意不是搜索树：较大的 9 放在根的左边。 */
    struct binary_node nodes[4] = {
        {4, NULL, NULL}, {9, NULL, NULL},
        {2, NULL, NULL}, {7, NULL, NULL}
    };
    struct binary_node *root = &nodes[0];
    root->left = &nodes[1];
    root->right = &nodes[2];
    nodes[1].right = &nodes[3];
    assert(binary_tree_height(NULL) == -1);
    assert(binary_tree_count_nodes(NULL) == 0);
    assert(!binary_tree_is_leaf(NULL));
    assert(binary_tree_find(NULL, 9) == NULL);
    assert(binary_tree_height(root) == 2);
    assert(binary_tree_count_nodes(root) == 4);
    assert(!binary_tree_is_leaf(root));
    assert(binary_tree_is_leaf(&nodes[2]));
    assert(binary_tree_find(root, 9) == &nodes[1]);
    assert(binary_tree_find(root, 8) == NULL);
    printf("height=%d nodes=%d found=%d\n",
           binary_tree_height(root), binary_tree_count_nodes(root),
           binary_tree_find(root, 9)->value);
    /* 节点属于自动数组，不能调用 free 或 delete。 */
    return 0;
}
