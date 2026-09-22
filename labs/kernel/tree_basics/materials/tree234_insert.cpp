#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>

/* 节点池拥有所有节点；树中的指针只借用池内地址。 */
constexpr std::size_t pool_capacity = 64;

struct node_234 {
    unsigned int key_count = 0;
    bool leaf = true;
    std::array<int, 4> keys{};               // 第四槽只供自底向上的临时溢出
    std::array<node_234 *, 5> child{};       // 内部溢出时临时有五个孩子
};

struct tree_234 {
    std::array<node_234, pool_capacity> pool{};
    std::size_t used = 0;
    std::size_t limit = pool_capacity;
    node_234 *root = nullptr;

    tree_234() = default;
    tree_234(const tree_234 &) = delete;    // 复制对象会让内部指针仍指向旧池
    tree_234 &operator=(const tree_234 &) = delete;
};

enum class insert_result { inserted, duplicate, no_capacity };
enum class insert_mode { bottom_up, top_down };

static node_234 *take_node(tree_234 &tree, bool leaf)
{
    /* 只能在入口已经完成容量预检后调用。 */
    assert(tree.used < tree.limit && tree.limit <= pool_capacity);
    node_234 *node = &tree.pool[tree.used++];
    *node = node_234{};
    node->leaf = leaf;
    return node;
}

static unsigned int find_slot(const node_234 *node, int key)
{
    unsigned int i = 0;
    while (i < node->key_count && node->keys[i] < key)
        ++i;
    return i;
}

struct insert_plan {
    bool duplicate = false;
    std::size_t new_nodes = 0;
};

static insert_plan plan_insert(const tree_234 &tree, int key, insert_mode mode)
{
    if (!tree.root)
        return {false, 1};
    std::array<const node_234 *, pool_capacity> path{};
    std::size_t depth = 0, full_count = 0;
    const node_234 *node = tree.root;
    while (node) {
        assert(depth < path.size());
        path[depth++] = node;
        unsigned int i = find_slot(node, key);
        if (i < node->key_count && node->keys[i] == key)
            return {true, 0};
        if (node->key_count == 3)
            ++full_count;
        node = node->leaf ? nullptr : node->child[i];
    }
    if (mode == insert_mode::top_down) {
        /* 原路径上的每个满节点都会预分裂；满根还需一个新根。 */
        return {false, full_count + (tree.root->key_count == 3 ? 1U : 0U)};
    }
    std::size_t splits = 0;
    while (depth && path[depth - 1]->key_count == 3) {
        ++splits;
        --depth;
    }
    /* 只有从叶到根都满，传播才会穿过根并新增一层。 */
    return {false, splits + (depth == 0 ? 1U : 0U)};
}

static void insert_key(node_234 *node, unsigned int index, int key)
{
    assert(node->key_count < 4 && index <= node->key_count);
    for (unsigned int j = node->key_count; j > index; --j)
        node->keys[j] = node->keys[j - 1];
    node->keys[index] = key;
    ++node->key_count;
}

static void attach_right(node_234 *parent, unsigned int index,
                         int key, node_234 *right)
{
    unsigned int old_count = parent->key_count;
    for (unsigned int j = old_count + 1; j > index + 1; --j)
        parent->child[j] = parent->child[j - 1];
    parent->child[index + 1] = right;
    insert_key(parent, index, key);
}

struct split_result {
    bool did_split = false;
    int promote_key = 0;
    node_234 *right_node = nullptr;
};

static split_result split_overflow(tree_234 &tree, node_234 *node)
{
    assert(node->key_count == 4);
    int promote = node->keys[2];             // 固定提升上中位数
    node_234 *right = take_node(tree, node->leaf);
    right->key_count = 1;
    right->keys[0] = node->keys[3];
    if (!node->leaf) {
        right->child[0] = node->child[3];
        right->child[1] = node->child[4];
        node->child[3] = node->child[4] = nullptr;
    }
    node->key_count = 2;                     // 原对象继续拥有左半部分
    return {true, promote, right};
}

static split_result insert_bottom(tree_234 &tree, node_234 *node, int key)
{
    unsigned int i = find_slot(node, key);
    if (node->leaf)
        insert_key(node, i, key);
    else {
        split_result child_result = insert_bottom(tree, node->child[i], key);
        if (!child_result.did_split)
            return {};
        attach_right(node, i, child_result.promote_key, child_result.right_node);
    }
    return node->key_count == 4 ? split_overflow(tree, node) : split_result{};
}

static void split_child(tree_234 &tree, node_234 *parent, unsigned int i)
{
    node_234 *full = parent->child[i];
    assert(parent->key_count < 3 && full->key_count == 3);
    node_234 *right = take_node(tree, full->leaf);
    int promote = full->keys[1];
    right->key_count = 1;
    right->keys[0] = full->keys[2];
    if (!full->leaf) {
        right->child[0] = full->child[2];
        right->child[1] = full->child[3];
        full->child[2] = full->child[3] = nullptr;
    }
    full->key_count = 1;
    attach_right(parent, i, promote, right);
}

static void insert_non_full(tree_234 &tree, node_234 *node, int key)
{
    while (!node->leaf) {
        unsigned int i = find_slot(node, key);
        if (node->child[i]->key_count == 3) {
            split_child(tree, node, i);
            if (key > node->keys[i])
                ++i;
        }
        node = node->child[i];
    }
    insert_key(node, find_slot(node, key), key);
}

static insert_result insert(tree_234 &tree, int key, insert_mode mode)
{
    assert(tree.used <= tree.limit && tree.limit <= pool_capacity);
    insert_plan plan = plan_insert(tree, key, mode);
    if (plan.duplicate)
        return insert_result::duplicate;
    if (plan.new_nodes > tree.limit - tree.used)
        return insert_result::no_capacity;
    std::size_t before = tree.used;
    if (!tree.root) {
        tree.root = take_node(tree, true);
        insert_key(tree.root, 0, key);
    } else if (mode == insert_mode::bottom_up) {
        split_result result = insert_bottom(tree, tree.root, key);
        if (result.did_split) {
            node_234 *new_root = take_node(tree, false);
            new_root->keys[0] = result.promote_key;
            new_root->key_count = 1;
            new_root->child[0] = tree.root;
            new_root->child[1] = result.right_node;
            tree.root = new_root;
        }
    } else {
        if (tree.root->key_count == 3) {
            node_234 *new_root = take_node(tree, false);
            new_root->child[0] = tree.root;
            split_child(tree, new_root, 0);
            tree.root = new_root;
        }
        insert_non_full(tree, tree.root, key);
    }
    assert(tree.used - before == plan.new_nodes);
    return insert_result::inserted;
}

static void print_tree(const node_234 *node, unsigned int depth = 0)
{
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

/* 按正文初始图构造，避免两种算法的早期历史造成不同初始树。 */
static void build_example(tree_234 &tree)
{
    assert(!tree.root && tree.used == 0 && tree.limit >= 5);
    static const int leaf_keys[4][3] = {{10, 20, 0}, {50, 60, 70},
                                      {90, 100, 0}, {130, 140, 0}};
    tree.root = take_node(tree, false);
    tree.root->key_count = 3;
    tree.root->keys = {40, 80, 120, 0};
    for (unsigned int i = 0; i < 4; ++i) {
        node_234 *leaf = take_node(tree, true);
        leaf->key_count = i == 1 ? 3 : 2;
        for (unsigned int j = 0; j < leaf->key_count; ++j)
            leaf->keys[j] = leaf_keys[i][j];
        tree.root->child[i] = leaf;
    }
}

int main()
{
    for (insert_mode mode : {insert_mode::bottom_up, insert_mode::top_down}) {
        tree_234 tree;
        build_example(tree);
        insert_plan plan = plan_insert(tree, 55, mode);
        assert(plan.new_nodes == 3);
        tree.limit = tree.used + plan.new_nodes - 1;
        if (insert(tree, 55, mode) != insert_result::no_capacity)
            return 1;
        assert(tree.used == 5 && tree.root->key_count == 3);
        tree.limit = pool_capacity;
        if (insert(tree, 55, mode) != insert_result::inserted ||
            insert(tree, 55, mode) != insert_result::duplicate)
            return 1;
        std::cout << (mode == insert_mode::bottom_up ? "bottom_up" : "top_down")
                  << " nodes=" << tree.used << "\n";
        print_tree(tree.root);
        tree_234 promotion;
        for (int key : {10, 20, 30, 25}) {
            if (insert(promotion, key, mode) != insert_result::inserted)
                return 1;
        }
        std::cout << "promotion root_key=" << promotion.root->keys[0] << "\n";
    }
    return 0;
}
