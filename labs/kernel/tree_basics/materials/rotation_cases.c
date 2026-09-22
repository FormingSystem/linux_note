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

int main(void)
{
    /* 四种插入方向各形成三节点链，局部高度由 2 降为 1。 */
    const int orders[4][3] = {{30,20,10}, {30,10,20}, {10,20,30}, {10,30,20}};
    const char *names[4] = {"LL", "LR", "RR", "RL"};
    for (int kind = 0; kind < 4; ++kind) {
        struct bst_node nodes[3] = {0};
        struct bst_node *root = NULL;
        for (int i = 0; i < 3; ++i) {
            struct bst_node **slot = &root, *parent = NULL;
            nodes[i].key = orders[kind][i];
            while (*slot) {
                parent = *slot;
                slot = nodes[i].key < parent->key ? &parent->left : &parent->right;
            }
            nodes[i].parent = parent;
            *slot = &nodes[i];
        }
        enum bst_rotation_case rot_case = bst_identify_rotation_case(root, &nodes[1], &nodes[2]);
        bst_rotate_by_case(&root, root, rot_case);
        printf("%s root=%d inorder=%d,%d,%d\n", names[kind], root->key,
               root->left->key, root->key, root->right->key);
    }
    return 0;
}
