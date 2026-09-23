#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

struct node {
    int key;
    bool red = false;
    const node* left = nullptr;
    const node* right = nullptr;
};

struct group {
    std::vector<int> keys;
    std::vector<std::unique_ptr<group>> children;
};

// 输入契约：有限、无共享节点、唯一键、满足全部红黑性质的稳定树。
// 此转换器不是验证器，不接受环、悬空地址或尚未修复的中间状态。
static std::unique_ptr<group> collapse(const node* root);

static void collect_member(const node* current, group& output) {
    if (current->left && current->left->red)
        collect_member(current->left, output);   // 红孩子并入当前逻辑节点
    else output.children.push_back(collapse(current->left));
    output.keys.push_back(current->key);
    if (current->right && current->right->red)
        collect_member(current->right, output);
    else output.children.push_back(collapse(current->right));
}

static std::unique_ptr<group> collapse(const node* root) {
    if (!root) return nullptr;  // NIL 是区间的空边界，不是一个逻辑数据节点
    assert(!root->red);
    auto output = std::make_unique<group>();
    collect_member(root, *output);
    assert(output->keys.size() >= 1 && output->keys.size() <= 3);
    assert(output->children.size() == output->keys.size() + 1);
    return output;
}

static void print_group(const group* current) {
    if (!current) { std::cout << '.'; return; }
    std::cout << '[';
    for (std::size_t i = 0; i < current->keys.size(); ++i) {
        if (i) std::cout << '|';
        std::cout << current->keys[i];
    }
    std::cout << ']';
    bool internal = false;
    for (const auto& child : current->children) internal = internal || bool(child);
    if (!internal) return;  // 叶子的空区间不重复打印
    std::cout << '(';
    for (std::size_t i = 0; i < current->children.size(); ++i) {
        if (i) std::cout << ',';
        print_group(current->children[i].get());
    }
    std::cout << ')';
}

static void show(const char* label, const node* root) {
    auto logical = collapse(root);
    std::cout << label << ": ";
    print_group(logical.get());
    std::cout << '\n';
}

int main() {
    // 同样三个区间，分别接到两种合法的 3-node 二叉表示。
    node a{5}, b{15}, c{30};
    node ten_left{10,true,&a,&b};
    node twenty_left{20,false,&ten_left,&c};
    show("left red", &twenty_left);

    node twenty_right{20,true,&b,&c};
    node ten_right{10,false,&a,&twenty_right};
    show("right red", &ten_right);

    // 四个孩子必须按键区间归属，不能沿用上一棵树的三个槽。
    node q0{5}, q1{15}, q2{25}, q3{35};
    node low{10,true,&q0,&q1}, high{30,true,&q2,&q3};
    node middle{20,false,&low,&high};
    show("four node", &middle);
    show("empty", nullptr);
    return 0;
}
