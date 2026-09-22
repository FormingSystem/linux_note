#include <iostream>

struct bst_node {
	int key;
	bst_node *left;
	bst_node *right;
	bst_node *parent;
};

enum class bst_rotation_case {
	invalid = 0,
	ll,
	lr,
	rr,
	rl,
};

void bst_left_rotate(bst_node *&root, bst_node *x)
{
	bst_node *y;

	if (!root || !x || !x->right)
		return;

	y = x->right;

	x->right = y->left;
	if (y->left)
		y->left->parent = x;

	y->parent = x->parent;

	if (!x->parent) {
		root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}

	y->left = x;
	x->parent = y;
}

void bst_right_rotate(bst_node *&root, bst_node *y)
{
	bst_node *x;

	if (!root || !y || !y->left)
		return;

	x = y->left;

	y->left = x->right;
	if (x->right)
		x->right->parent = y;

	x->parent = y->parent;

	if (!y->parent) {
		root = x;
	} else if (y == y->parent->left) {
		y->parent->left = x;
	} else {
		y->parent->right = x;
	}

	x->right = y;
	y->parent = x;
}

bst_rotation_case
bst_identify_rotation_case(bst_node *g, bst_node *p, bst_node *n)
{
	if (!g || !p || !n)
		return bst_rotation_case::invalid;

	if (p == g->left) {
		if (n == p->left)
			return bst_rotation_case::ll;
		if (n == p->right)
			return bst_rotation_case::lr;
	}

	if (p == g->right) {
		if (n == p->right)
			return bst_rotation_case::rr;
		if (n == p->left)
			return bst_rotation_case::rl;
	}

	return bst_rotation_case::invalid;
}

void bst_rotate_by_case(bst_node *&root,
						   bst_node *g,
						   bst_rotation_case rot_case)
{
	if (!root || !g)
		return;

	switch (rot_case) {
	case bst_rotation_case::ll:
		/* 先核对两级方向，避免双旋只执行后一半。 */
		if (!g->left || !g->left->left)
			return;
		bst_right_rotate(root, g);
		break;
	case bst_rotation_case::lr:
		/* 先核对两级方向，避免双旋只执行后一半。 */
		if (!g->left || !g->left->right)
			return;
		bst_left_rotate(root, g->left);
		bst_right_rotate(root, g);
		break;
	case bst_rotation_case::rr:
		/* 先核对两级方向，避免双旋只执行后一半。 */
		if (!g->right || !g->right->right)
			return;
		bst_left_rotate(root, g);
		break;
	case bst_rotation_case::rl:
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
        bst_node nodes[3]{};
        bst_node *root = NULL;
        for (int i = 0; i < 3; ++i) {
            bst_node **slot = &root, *parent = NULL;
            nodes[i].key = orders[kind][i];
            while (*slot) {
                parent = *slot;
                slot = nodes[i].key < parent->key ? &parent->left : &parent->right;
            }
            nodes[i].parent = parent;
            *slot = &nodes[i];
        }
        bst_rotation_case rot_case = bst_identify_rotation_case(root, &nodes[1], &nodes[2]);
        bst_rotate_by_case(root, root, rot_case);
        std::cout << names[kind] << " root=" << root->key << " inorder="
                  << root->left->key << ',' << root->key << ','
                  << root->right->key << '\n';
    }
    return 0;
}
