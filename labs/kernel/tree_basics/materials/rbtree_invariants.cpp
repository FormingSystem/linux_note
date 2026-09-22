#include <algorithm>
#include <cassert>
#include <iostream>
#include <optional>
#include <unordered_set>

enum class color { red, black };

struct node {
    int key;
    color shade;
    node* left = nullptr;
    node* right = nullptr;
};

struct report {
    bool valid;
    unsigned nodes;
    unsigned height;
    unsigned shortest;
    unsigned black_height;
    const char* reason;
};

class checker {
    // 内部 b 计数包含当前节点和终点 NIL；空位置贡献一个黑色。
    struct partial {
        bool valid;
        unsigned nodes;
        unsigned height;
        unsigned shortest;
        unsigned b;
        const char* reason;
    };
    std::unordered_set<const node*> seen;

    static partial invalid(const char* reason) {
        return {false, 0, 0, 0, 0, reason};
    }

    partial walk(const node* current, std::optional<int> low,
                 std::optional<int> high, bool parent_red) {
        if (!current)
            return {true, 0, 0, 0, 1, "ok"};
        if (!seen.insert(current).second)
            return invalid("repeated-node");
        if (current->shade != color::red && current->shade != color::black)
            return invalid("color-domain");
        if ((low && current->key <= *low) || (high && current->key >= *high))
            return invalid("key-order");
        const bool red = current->shade == color::red;
        if (red && parent_red)
            return invalid("red-red");
        const auto left = walk(current->left, low, current->key, red);
        if (!left.valid)
            return left;
        const auto right = walk(current->right, current->key, high, red);
        if (!right.valid)
            return right;
        if (left.b != right.b)
            return invalid("black-height");
        return {true, 1 + left.nodes + right.nodes,
                1 + std::max(left.height, right.height),
                1 + std::min(left.shortest, right.shortest),
                left.b + (red ? 0U : 1U), "ok"};
    }

public:
    report inspect(const node* root) {
        seen.clear();
        // 空树按约定有效；根黑是本例采用的规范化条件。
        if (root && root->shade == color::red)
            return {false, 0, 0, 0, 0, "root-red"};
        const auto result = walk(root, std::nullopt, std::nullopt, false);
        if (!result.valid)
            return {false, 0, 0, 0, 0, result.reason};
        // 对非空黑根减去根自身，留下本章定义的 bh(root)。
        return {true, result.nodes, result.height, result.shortest,
                root ? result.b - 1 : 0, result.reason};
    }
};

static report show(checker& check, const char* name, const node* root) {
    const auto result = check.inspect(root);
    std::cout << name << ": " << result.reason;
    if (result.valid)
        std::cout << " nodes=" << result.nodes << " height=" << result.height
                  << " shortest=" << result.shortest
                  << " bh=" << result.black_height;
    std::cout << '\n';
    return result;
}

int main() {
    node a{20, color::black}, b{10, color::black}, c{30, color::red};
    node d{25, color::black}, e{40, color::black};
    a.left = &b;
    a.right = &c;
    c.left = &d;
    c.right = &e;
    checker check;
    const auto valid = show(check, "valid", &a);
    assert(valid.valid && valid.nodes == 5 && valid.height == 3 &&
           valid.shortest == 2 && valid.black_height == 2);
    d.shade = color::red;
    assert(!show(check, "red child", &a).valid);
    d.shade = color::black;
    b.shade = color::red;
    assert(!show(check, "unequal black paths", &a).valid);
    b.shade = color::black;
    d.key = 15;
    assert(!show(check, "ancestor bound", &a).valid);
    d.key = 25;
    const auto empty = show(check, "empty", nullptr);
    assert(empty.valid && empty.height == 0 && empty.black_height == 0);
    return 0;
}
