#include <array>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>

struct tree_node {
    char value;
    tree_node *left = nullptr;  // 两条边借用地址，不负责释放节点
    tree_node *right = nullptr;
    explicit tree_node(char ch) : value(ch) {}
};

using node_owners = std::array<std::unique_ptr<tree_node>, 7>;

static node_owners build_demo_tree(int fail_at)
{
    node_owners nodes;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (fail_at == static_cast<int>(i + 1))
            throw std::bad_alloc{};
        nodes[i] = std::make_unique<tree_node>(static_cast<char>('A' + i));
    }
    for (std::size_t i = 0; i < 3; ++i) {
        nodes[i]->left = nodes[2 * i + 1].get();
        nodes[i]->right = nodes[2 * i + 2].get();
    }
    return nodes;
}

void postorder_traversal(const tree_node *root)
{
    if (!root)
        return;
    postorder_traversal(root->left);
    postorder_traversal(root->right);
    std::cout << root->value << ' ';
}

static int parse_fail_at(int argc, char **argv)
{
    char *end;
    long value;
    if (argc == 1)
        return 0;
    if (argc != 2 || argv[1][0] == '\0')
        return -1;
    errno = 0;
    value = std::strtol(argv[1], &end, 10);
    if (errno || *end != '\0' || value < 0 || value > 7)
        return -1;
    return (int)value;
}

int main(int argc, char **argv)
{
    int fail_at = parse_fail_at(argc, argv);
    if (fail_at < 0) {
        std::cerr << "用法: " << argv[0] << " [0..7]\n";
        return 2;
    }
    try {
        auto nodes = build_demo_tree(fail_at);
        std::cout << "后序遍历结果: ";
        postorder_traversal(nodes[0].get());
        std::cout << '\n';
        // 正常离开与异常展开都会销毁拥有者，不能再手工 delete 借用的边。
    } catch (const std::bad_alloc &) {
        std::cerr << "存储分配失败，已取得的资源由拥有者回收\n";
        return 1;
    }
    return 0;
}
