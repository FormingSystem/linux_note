#include <iostream>

struct bst_node {
	int key;
	bst_node *left;
	bst_node *right;
	bst_node *parent;
};

/*
 * 成品左旋（C++ 版）
 *
 * 参数：
 * root - 整棵树根指针的引用
 * x    - 当前左旋点
 */
void bst_left_rotate(bst_node *&root, bst_node *x)
{
	bst_node *y;

	if (!root || !x || !x->right)
		return;

	/* y 是上移节点 */
	y = x->right;

	/* 第一步：回接 T2 */
	x->right = y->left;
	if (y->left)
		y->left->parent = x;

	/* 第二步：让 y 接替 x 原来的位置 */
	y->parent = x->parent;

	if (!x->parent) {
		root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}

	/* 第三步：让 x 下沉 */
	y->left = x;
	x->parent = y;
}

void bst_right_rotate(bst_node *&root, bst_node *y)
{
	bst_node *x;

	if (!root || !y || !y->left)
		return;

	/* x 是上移节点 */
	x = y->left;

	/* 第一步：回接 T2 */
	y->left = x->right;
	if (x->right)
		x->right->parent = y;

	/* 第二步：让 x 接替 y 原来的位置 */
	x->parent = y->parent;

	if (!y->parent) {
		root = x;
	} else if (y == y->parent->left) {
		y->parent->left = x;
	} else {
		y->parent->right = x;
	}

	/* 第三步：让 y 下沉 */
	x->right = y;
	y->parent = x;
}

int main()
{
    // 自动对象持有节点，指针只描述连接，不拥有或转移内存。
    bst_node n5{5, nullptr, nullptr, nullptr};
    bst_node n12{12, nullptr, nullptr, nullptr};
    bst_node n18{18, nullptr, nullptr, nullptr};
    bst_node n15{15, &n12, &n18, nullptr};
    bst_node n10{10, &n5, &n15, nullptr};
    bst_node *root = &n10;
    bst_node *saved_root = root;
    n5.parent = &n10;
    n12.parent = &n15;
    n18.parent = &n15;
    n15.parent = &n10;

    bst_left_rotate(root, root);
    std::cout << "left: root=" << root->key
              << " saved=" << saved_root->key
              << " middle_parent=" << n12.parent->key << '\n';
    // 逆旋以新的局部根为参数，不能继续把旧根当作右旋点。
    bst_right_rotate(root, root);
    std::cout << "right: root=" << root->key
              << " restored=" << (root == saved_root)
              << " middle_parent=" << n12.parent->key << '\n';
    return 0;
}
