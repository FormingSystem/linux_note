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

	if (!x->parent)
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

int main(void)
{
	/*
	 * 构造：
	 *        10
	 *       /  \
	 *      5    15
	 *          /  \
	 *         12   18
	 */
	struct bst_node n5  = { 5,  NULL, NULL, NULL };
	struct bst_node n12 = { 12, NULL, NULL, NULL };
	struct bst_node n18 = { 18, NULL, NULL, NULL };
	struct bst_node n15 = { 15, &n12, &n18, NULL };
	struct bst_node n10 = { 10, &n5,  &n15, NULL };

	struct bst_node *root = &n10;

	n5.parent = &n10;
	n12.parent = &n15;
	n18.parent = &n15;
	n15.parent = &n10;

	printf("左旋前中序: ");
	inorder_print(root);
	printf("\n");

	bst_left_rotate(&root, &n10);

	printf("左旋后中序: ");
	inorder_print(root);
	printf("\n");

	printf("左旋后根节点: %d\n", root->key);

	return 0;
}
