#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iostream>

/* 固定池拥有节点；alive 只表示节点是否仍属于树，不释放单个对象。 */
constexpr std::size_t pool_capacity = 64;
struct node_234 {
    unsigned int key_count = 0;
    bool leaf = true;
    bool alive = false;
    std::array<int, 3> keys{};
    std::array<node_234 *, 4> child{};
};
struct tree_234 {
    std::array<node_234, pool_capacity> pool{};
    std::size_t used = 0;
    node_234 *root = nullptr;
    tree_234() = default;
    tree_234(const tree_234 &) = delete;
    tree_234 &operator=(const tree_234 &) = delete;
};
struct erase_counts {
    unsigned int borrow_left = 0, borrow_right = 0, merge = 0;
    unsigned int predecessor = 0, underflow = 0, shrink = 0;
};

static unsigned int find_slot(const node_234 *node, int key)
{
    unsigned int i = 0;
    while (i < node->key_count && node->keys[i] < key)
        ++i;
    return i;
}

static void retire_node(node_234 *node)
{
    /* 全部入口和孩子已移交后清空；池的拥有关系不变。 */
    *node = node_234{};
}

static void borrow_left(node_234 *parent, unsigned int i)
{
    node_234 *target = parent->child[i];
    node_234 *left = parent->child[i - 1];
    assert(target->key_count == 0 && left->key_count >= 2);
    target->keys[0] = parent->keys[i - 1];
    parent->keys[i - 1] = left->keys[left->key_count - 1];
    if (!target->leaf) {
        target->child[1] = target->child[0];
        target->child[0] = left->child[left->key_count];
        left->child[left->key_count] = nullptr;
    }
    --left->key_count;
    ++target->key_count;
}

static void borrow_right(node_234 *parent, unsigned int i)
{
    node_234 *target = parent->child[i];
    node_234 *right = parent->child[i + 1];
    assert(target->key_count == 0 && right->key_count >= 2);
    target->keys[0] = parent->keys[i];
    parent->keys[i] = right->keys[0];
    if (!target->leaf) {
        target->child[1] = right->child[0];
        for (unsigned int j = 0; j < right->key_count; ++j)
            right->child[j] = right->child[j + 1];
        right->child[right->key_count] = nullptr;
    }
    for (unsigned int j = 0; j + 1 < right->key_count; ++j)
        right->keys[j] = right->keys[j + 1];
    --right->key_count;
    ++target->key_count;
}

/* 合并 child[i]、keys[i]、child[i+1]，返回仍存活的左节点。 */
static node_234 *merge_children(node_234 *parent, unsigned int i)
{
    node_234 *left = parent->child[i];
    node_234 *right = parent->child[i + 1];
    assert(left->key_count + right->key_count == 1);
    unsigned int left_count = left->key_count;
    left->keys[left_count] = parent->keys[i];
    for (unsigned int j = 0; j < right->key_count; ++j)
        left->keys[left_count + 1 + j] = right->keys[j];
    if (!left->leaf)
        for (unsigned int j = 0; j <= right->key_count; ++j)
            left->child[left_count + 1 + j] = right->child[j];
    left->key_count = 2;
    for (unsigned int j = i; j + 1 < parent->key_count; ++j)
        parent->keys[j] = parent->keys[j + 1];
    for (unsigned int j = i + 1; j < parent->key_count; ++j)
        parent->child[j] = parent->child[j + 1];
    parent->child[parent->key_count] = nullptr;
    --parent->key_count;
    retire_node(right);
    return left;
}

static int extreme_key(const node_234 *node, bool maximum)
{
    while (!node->leaf)
        node = node->child[maximum ? node->key_count : 0];
    return node->keys[maximum ? node->key_count - 1 : 0];
}

/* 下溢内部节点仍保留 child[0]，不能因零键就把它当空叶释放。 */
static void fix_child_underflow(node_234 *parent, unsigned int i, erase_counts &counts)
{
    assert(parent->child[i]->key_count == 0);
    ++counts.underflow;
    if (i > 0 && parent->child[i - 1]->key_count >= 2) {
        ++counts.borrow_left;
        borrow_left(parent, i);
    } else if (i < parent->key_count && parent->child[i + 1]->key_count >= 2) {
        ++counts.borrow_right;
        borrow_right(parent, i);
    } else {
        if (i == parent->key_count)
            --i;
        ++counts.merge;
        merge_children(parent, i);
    }
}

struct erase_result {
    bool deleted;
    bool underflow;
};

static erase_result erase_bottom_up(node_234 *node, int key, erase_counts &counts)
{
    unsigned int i = find_slot(node, key);
    bool found = i < node->key_count && node->keys[i] == key;
    if (node->leaf) {
        if (!found)
            return {false, false};
        for (unsigned int j = i; j + 1 < node->key_count; ++j)
            node->keys[j] = node->keys[j + 1];
        --node->key_count;
        return {true, node->key_count == 0};
    }
    if (found) {
        /* 本版本固定选前驱；修复发生在它真正删掉之后。 */
        ++counts.predecessor;
        key = extreme_key(node->child[i], true);
        node->keys[i] = key;
    }
    erase_result result = erase_bottom_up(node->child[i], key, counts);
    if (!result.deleted)
        return result;
    if (result.underflow)
        fix_child_underflow(node, i, counts);
    return {true, node->key_count == 0};
}

static bool erase(tree_234 &tree, int key, erase_counts &counts)
{
    if (!tree.root)
        return false;
    /* 下降只读，未命中原路返回，无须额外执行一次 contains。 */
    erase_result result = erase_bottom_up(tree.root, key, counts);
    if (!result.deleted)
        return false;
    if (tree.root->key_count == 0) {
        node_234 *old_root = tree.root;
        tree.root = old_root->leaf ? nullptr : old_root->child[0];
        retire_node(old_root);
        ++counts.shrink;
    }
    return true;
}

static node_234 *make_node(tree_234 &tree, std::initializer_list<int> keys)
{
    /* 仅用于本例受控构建；不是接收任意外部数据的解析接口。 */
    assert(tree.used < pool_capacity && keys.size() >= 1 && keys.size() <= 3);
    node_234 *node = &tree.pool[tree.used++];
    node->alive = true;
    for (int key : keys)
        node->keys[node->key_count++] = key;
    return node;
}

static void build_example(tree_234 &tree, unsigned int scenario)
{
    tree.root = make_node(tree, {40});
    tree.root->leaf = false;
    if (scenario == 4) {
        node_234 *left = make_node(tree, {20});
        node_234 *right = make_node(tree, {60});
        tree.root->child[0] = left;
        tree.root->child[1] = right;
        left->leaf = right->leaf = false;
        left->child[0] = make_node(tree, {10});
        left->child[1] = make_node(tree, {30});
        right->child[0] = make_node(tree, {50});
        right->child[1] = make_node(tree, {70});
    } else if (scenario == 0) {
        tree.root->child[0] = make_node(tree, {10, 20, 30});
        tree.root->child[1] = make_node(tree, {50, 60, 70});
    } else if (scenario == 1) {
        tree.root->child[0] = make_node(tree, {20});
        tree.root->child[1] = make_node(tree, {50, 60});
    } else if (scenario == 2) {
        tree.root->child[0] = make_node(tree, {20});
        tree.root->child[1] = make_node(tree, {60});
    } else {
        tree.root->child[0] = make_node(tree, {10, 20});
        tree.root->child[1] = make_node(tree, {50});
    }
}

static void print_tree(const node_234 *node, unsigned int depth = 0)
{
    if (!node) {
        std::cout << "(empty)\n";
        return;
    }
    for (unsigned int i = 0; i < depth; ++i)
        std::cout << "  ";
    std::cout << '[';
    for (unsigned int i = 0; i < node->key_count; ++i)
        std::cout << (i ? "|" : "") << node->keys[i];
    std::cout << "]\n";
    if (!node->leaf)
        for (unsigned int i = 0; i <= node->key_count; ++i)
            print_tree(node->child[i], depth + 1);
}

int main()
{
    for (unsigned int scenario = 0; scenario < 6; ++scenario) {
        tree_234 tree;
        unsigned int fixture = scenario == 3 ? 0 : (scenario == 5 ? 4 : scenario);
        if (scenario == 4)
            fixture = 3;
        build_example(tree, fixture);
        int key = scenario == 3 ? 40 : (scenario == 4 ? 50 : (scenario == 5 ? 10 : 20));
        erase_counts counts;
        if (erase(tree, 999, counts) || !erase(tree, key, counts))
            return 1;
        std::cout << "case=" << scenario << " erase=" << key
                  << " left=" << counts.borrow_left << " right=" << counts.borrow_right
                  << " merge=" << counts.merge << " pred=" << counts.predecessor
                  << " underflow=" << counts.underflow << " shrink=" << counts.shrink << "\n";
        print_tree(tree.root);
    }
    return 0;
}
