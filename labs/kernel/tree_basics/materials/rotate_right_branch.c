#include <stdio.h>
#include <stdlib.h>

struct bst_node {
	int key;
	struct bst_node *left;
	struct bst_node *right;
	struct bst_node *parent;
};

void bst_right_rotate(struct bst_node **root, struct bst_node *y)
{
	struct bst_node *x;

	if (!root || !*root || !y || !y->left)
		return;

	x = y->left;

	y->left = x->right;
	if (x->right)
		x->right->parent = y;

	x->parent = y->parent;

	if (!y->parent)
		*root = x;
	else if (y == y->parent->left)
		y->parent->left = x;
	else
		y->parent->right = x;

	x->right = y;
	y->parent = x;
}

void inorder_print(const struct bst_node *root)
{
	if (!root)
		return;

	inorder_print(root->left);
	printf("%d ", root->key);
	inorder_print(root->right);
}

static void print_node(const char *name, const struct bst_node *node)
{
	if (!node) {
		printf("%s: NULL\n", name);
		return;
	}

	printf("%s: key=%d", name, node->key);
	if (node->parent)
		printf(", parent=%d\n", node->parent->key);
	else
		printf(", parent=NULL\n");
}

int main(void)
{
	/*
	 * 构造：
	 *           20
	 *          /  \
	 *        15    30
	 *       /  \
	 *     10   18
	 *    /  \
	 *   5   12
	 */
	struct bst_node n5  = { 5,  NULL, NULL, NULL };
	struct bst_node n12 = { 12, NULL, NULL, NULL };
	struct bst_node n10 = { 10, &n5, &n12, NULL };
	struct bst_node n18 = { 18, NULL, NULL, NULL };
	struct bst_node n15 = { 15, &n10, &n18, NULL };
	struct bst_node n30 = { 30, NULL, NULL, NULL };
	struct bst_node n20 = { 20, &n15, &n30, NULL };

	n5.parent = &n10;
	n12.parent = &n10;
	n10.parent = &n15;
	n18.parent = &n15;
	n15.parent = &n20;
	n30.parent = &n20;

	{
		struct bst_node *root = &n20;

		printf("右旋前中序: ");
		inorder_print(root);
		printf("\n");

		/* 对内部节点 15 右旋 */
		bst_right_rotate(&root, &n15);

		printf("右旋后中序: ");
		inorder_print(root);
		printf("\n");

		print_node("root", root);
		print_node("root->left", root->left);
		print_node("root->left->left", root->left->left);
		print_node("root->left->right", root->left->right);
	}

	return 0;
}
