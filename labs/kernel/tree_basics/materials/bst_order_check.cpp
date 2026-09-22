#include <cassert>
#include <climits>
#include <iostream>

struct bst_node {
    int key;
    const bst_node *left;
    const bst_node *right;
};

/* prev 记录本次中序遍历中刚访问的节点，不是跨次缓存。 */
static bool inorder_check(const bst_node *root, const bst_node *&prev)
{
    if (!root)
        return true;
    if (!inorder_check(root->left, prev))
        return false;
    if (prev && prev->key >= root->key)
        return false;
    prev = root;
    return inorder_check(root->right, prev);
}

static bool bst_order_valid(const bst_node *root)
{
    const bst_node *prev = nullptr;
    return inorder_check(root, prev);
}

int main()
{
    bst_node nodes[] = {
        {8, nullptr, nullptr}, {3, nullptr, nullptr},
        {10, nullptr, nullptr}, {9, nullptr, nullptr}
    };
    nodes[0].left = &nodes[1];
    nodes[0].right = &nodes[2];
    nodes[1].right = &nodes[3];
    assert(!bst_order_valid(nodes));  // 9 超过祖先 8 的上界
    nodes[3].key = 6;
    assert(bst_order_valid(nodes));
    assert(bst_order_valid(nodes));   // 再次检查，prev 必须重新置空
    nodes[3].key = 8;
    assert(!bst_order_valid(nodes));  // 本例不允许重复键
    assert(bst_order_valid(nullptr));
    bst_node extreme[] = {
        {INT_MIN, nullptr, nullptr}, {INT_MAX, nullptr, nullptr}
    };
    extreme[0].right = &extreme[1];
    assert(bst_order_valid(extreme)); // 不采用 key +/- 1，极值不溢出
    std::cout << "祖先边界、重复键、空树、极值和重复检查通过\n";
}
