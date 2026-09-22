#include <stdio.h>
#include <stdlib.h>

struct bst_node {
	int key;
	struct bst_node *left;
	struct bst_node *right;
	struct bst_node *parent;
};

void bst_left_rotate(struct bst_node **root, struct bst_node *x)
{
	struct bst_node *y;

	if (!root || !*root || !x || !x->right)
		return;

	y = x->right;

	x->right = y->left;
	if (y->left)
		y->left->parent = x;

	y->parent = x->parent;

	if (!x->parent)	// 当x是根节点，旋转后，y是根节点
		*root = y;
	else if (x == x->parent->left)
		x->parent->left = y;
	else
		x->parent->right = y;

	y->left = x;
	x->parent = y;
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
	 *        10    30
	 *       /  \
	 *      5   15
	 *         /  \
	 *        12  18
	 */
	struct bst_node n5  = { 5,  NULL, NULL, NULL };
	struct bst_node n12 = { 12, NULL, NULL, NULL };
	struct bst_node n18 = { 18, NULL, NULL, NULL };
	struct bst_node n15 = { 15, &n12, &n18, NULL };
	struct bst_node n10 = { 10, &n5,  &n15, NULL };
	struct bst_node n30 = { 30, NULL, NULL, NULL };
	struct bst_node n20 = { 20, &n10, &n30, NULL };

	n5.parent = &n10;
	n12.parent = &n15;
	n18.parent = &n15;
	n15.parent = &n10;
	n10.parent = &n20;
	n30.parent = &n20;

	{
		struct bst_node *root = &n20;

		printf("左旋前中序: ");
		inorder_print(root);
		printf("\n");

		/* 对内部节点 10 左旋 */
		bst_left_rotate(&root, &n10);

		printf("左旋后中序: ");
		inorder_print(root);
		printf("\n");

		print_node("root", root);
		print_node("root->left", root->left);
		print_node("root->left->left", root->left->left);
		print_node("root->left->right", root->left->right);
	}

	return 0;
}
