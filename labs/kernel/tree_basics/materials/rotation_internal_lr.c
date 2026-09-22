#include <stdio.h>
#include <stdlib.h>

struct bst_node {
	int key;
	struct bst_node *left;
	struct bst_node *right;
	struct bst_node *parent;
};

enum bst_rotation_case {
	BST_CASE_INVALID = 0,
	BST_CASE_LL,
	BST_CASE_LR,
	BST_CASE_RR,
	BST_CASE_RL,
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

	if (!x->parent) {
		*root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}

	y->left = x;
	x->parent = y;
}

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

	if (!y->parent) {
		*root = x;
	} else if (y == y->parent->left) {
		y->parent->left = x;
	} else {
		y->parent->right = x;
	}

	x->right = y;
	y->parent = x;
}

enum bst_rotation_case
bst_identify_rotation_case(struct bst_node *g,
						   struct bst_node *p,
						   struct bst_node *n)
{
	if (!g || !p || !n)
		return BST_CASE_INVALID;

	if (p == g->left) {
		if (n == p->left)
			return BST_CASE_LL;
		if (n == p->right)
			return BST_CASE_LR;
	}

	if (p == g->right) {
		if (n == p->right)
			return BST_CASE_RR;
		if (n == p->left)
			return BST_CASE_RL;
	}

	return BST_CASE_INVALID;
}

void bst_rotate_by_case(struct bst_node **root,
						   struct bst_node *g,
						   enum bst_rotation_case rot_case)
{
	if (!root || !*root || !g)
		return;

	switch (rot_case) {
	case BST_CASE_LL:
		/* 先核对两级方向，避免双旋只执行后一半。 */
		if (!g->left || !g->left->left)
			return;
		bst_right_rotate(root, g);
		break;
	case BST_CASE_LR:
		/* 先核对两级方向，避免双旋只执行后一半。 */
		if (!g->left || !g->left->right)
			return;
		bst_left_rotate(root, g->left);
		bst_right_rotate(root, g);
		break;
	case BST_CASE_RR:
		/* 先核对两级方向，避免双旋只执行后一半。 */
		if (!g->right || !g->right->right)
			return;
		bst_left_rotate(root, g);
		break;
	case BST_CASE_RL:
		/* 先核对两级方向，避免双旋只执行后一半。 */
		if (!g->right || !g->right->left)
			return;
		bst_right_rotate(root, g->right);
		bst_left_rotate(root, g);
		break;
	default:
		break;
	}
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
	 *            50
	 *           /  \
	 *         40    80
	 *        / \
	 *      20   45
	 *        \
	 *         30
	 *
	 * 当前选择的局部子树根：
	 * g = 40
	 * p = 20
	 * n = 30
	 *
	 * 这是一个内部子树上的 LR。
	 */
	struct bst_node n30 = { 30, NULL, NULL, NULL };
	struct bst_node n20 = { 20, NULL, &n30, NULL };
	struct bst_node n45 = { 45, NULL, NULL, NULL };
	struct bst_node n40 = { 40, &n20, &n45, NULL };
	struct bst_node n80 = { 80, NULL, NULL, NULL };
	struct bst_node n50 = { 50, &n40, &n80, NULL };

	struct bst_node *root = &n50;
	enum bst_rotation_case rot_case;

	n30.parent = &n20;
	n20.parent = &n40;
	n45.parent = &n40;
	n40.parent = &n50;
	n80.parent = &n50;

	printf("重排前中序: ");
	inorder_print(root);
	printf("\n");

	rot_case = bst_identify_rotation_case(&n40, &n20, &n30);
	printf("识别结果: %s\n",
		   rot_case == BST_CASE_LR ? "LR" : "INVALID");

	bst_rotate_by_case(&root, &n40, rot_case);

	printf("重排后中序: ");
	inorder_print(root);
	printf("\n");

	printf("整棵树根: %d\n", root->key);
	printf("左子树根: %d\n", root->left->key);

	return 0;
}
