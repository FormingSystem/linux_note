#include <cassert>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <vector>

enum class color { red, black };

struct node {
    int key = 0;
    color shade = color::black;
    node* parent = nullptr;
    node* left = nullptr;
    node* right = nullptr;
    bool alive = true;
};

struct node_spec {
    int key;
    color shade;
    int left = -1;   // -1 表示 NIL，其余值是输入数组的下标
    int right = -1;
};

struct repair_counts {
    unsigned red_sibling = 0;
    unsigned merge = 0;
    unsigned near = 0;
    unsigned far = 0;
    unsigned absorbed = 0;
    unsigned rotations = 0;
};

class erase_tree {
    std::vector<node> storage_;  // 构造后不扩容，节点地址固定到树销毁
    node* root_ = nullptr;
    repair_counts counts_;

    static bool is_red(const node* current) {
        return current && current->shade == color::red;
    }

    void replace(node* old_node, node* replacement) {
        node* parent = old_node->parent;
        if (!parent) root_ = replacement;
        else if (old_node == parent->left) parent->left = replacement;
        else parent->right = replacement;
        if (replacement) replacement->parent = parent;
    }

    void rotate_left(node* old_root) {
        node* new_root = old_root->right;
        assert(new_root);
        old_root->right = new_root->left;
        if (new_root->left) new_root->left->parent = old_root;
        replace(old_root, new_root);
        new_root->left = old_root;
        old_root->parent = new_root;
        ++counts_.rotations;
    }

    void rotate_right(node* old_root) {
        node* new_root = old_root->left;
        assert(new_root);
        old_root->left = new_root->right;
        if (new_root->right) new_root->right->parent = old_root;
        replace(old_root, new_root);
        new_root->right = old_root;
        old_root->parent = new_root;
        ++counts_.rotations;
    }

    void repair(node* current, node* parent, bool on_left) {
        // current 可为空，父节点和方向由调用方显式保存
        while (current != root_ && !is_red(current)) {
            assert(parent);
            node* sibling = on_left ? parent->right : parent->left;
            assert(sibling);  // 合法树中缺黑侧比兄弟恰少一黑，兄弟不可能是 NIL
            if (is_red(sibling)) {
                sibling->shade = color::black;
                parent->shade = color::red;
                if (on_left) rotate_left(parent);
                else rotate_right(parent);
                sibling = on_left ? parent->right : parent->left;
                ++counts_.red_sibling;
            }
            if (!is_red(sibling->left) && !is_red(sibling->right)) {
                sibling->shade = color::red;
                current = parent;
                parent = current->parent;
                on_left = parent && current == parent->left;
                ++counts_.merge;
                continue;  // 红父在循环出口染黑，黑父继续携带缺口
            }
            node* far_child = on_left ? sibling->right : sibling->left;
            if (!is_red(far_child)) {
                node* near_child = on_left ? sibling->left : sibling->right;
                assert(is_red(near_child));
                near_child->shade = color::black;
                sibling->shade = color::red;
                if (on_left) rotate_right(sibling);
                else rotate_left(sibling);
                sibling = on_left ? parent->right : parent->left;
                ++counts_.near;
            }
            far_child = on_left ? sibling->right : sibling->left;
            assert(is_red(far_child));
            sibling->shade = parent->shade;
            parent->shade = color::black;
            far_child->shade = color::black;
            if (on_left) rotate_left(parent);
            else rotate_right(parent);
            ++counts_.far;
            return;
        }
        if (current) {
            if (is_red(current)) ++counts_.absorbed;
            current->shade = color::black;
        }
    }

    static int inspect(const node* current, const node* parent,
                       std::optional<int> low, std::optional<int> high,
                       std::unordered_set<const node*>& seen) {
        if (!current) return 1;
        if (!seen.insert(current).second || !current->alive ||
            current->parent != parent ||
            (current->shade != color::red && current->shade != color::black) ||
            (low && current->key <= *low) || (high && current->key >= *high) ||
            (is_red(parent) && is_red(current))) return -1;
        const int left = inspect(current->left, current, low, current->key, seen);
        if (left < 0) return -1;
        const int right = inspect(current->right, current, current->key, high, seen);
        if (right < 0 || right != left) return -1;
        return left + (current->shade == color::black ? 1 : 0);
    }

public:
    explicit erase_tree(const std::vector<node_spec>& input) : storage_(input.size()) {
        for (std::size_t i = 0; i < input.size(); ++i) {
            storage_[i].key = input[i].key;
            storage_[i].shade = input[i].shade;
        }
        for (std::size_t i = 0; i < input.size(); ++i) {
            auto attach = [&](int index, node*& slot) {
                if (index == -1) return;
                if (index < 0 || static_cast<std::size_t>(index) >= input.size())
                    throw std::invalid_argument("child index");
                node* child = &storage_[static_cast<std::size_t>(index)];
                if (child->parent) throw std::invalid_argument("shared child");
                slot = child;
                child->parent = &storage_[i];
            };
            attach(input[i].left, storage_[i].left);
            attach(input[i].right, storage_[i].right);
        }
        root_ = storage_.empty() ? nullptr : &storage_[0];
        if (!valid()) throw std::invalid_argument("invalid red-black fixture");
    }
    erase_tree(const erase_tree&) = delete;
    erase_tree& operator=(const erase_tree&) = delete;
    erase_tree(erase_tree&&) = delete;
    erase_tree& operator=(erase_tree&&) = delete;

    const node* root() const { return root_; }
    repair_counts counts() const { return counts_; }

    const node* find(int key) const {
        const node* current = root_;
        while (current && key != current->key)
            current = key < current->key ? current->left : current->right;
        return current;
    }

    bool valid() const {
        std::size_t live = 0;
        for (const auto& object : storage_) live += object.alive;
        if (!root_) return live == 0;
        if (root_->parent || is_red(root_)) return false;
        std::unordered_set<const node*> seen;
        return inspect(root_, nullptr, {}, {}, seen) >= 0 && seen.size() == live;
    }

    bool erase(int key) {
        counts_ = {};
        node* target = root_;
        while (target && key != target->key)
            target = key < target->key ? target->left : target->right;
        if (!target) return false;
        node* removed_from = target;  // 记录抽走黑贡献的原位置，不等同于退出对象
        color removed_color = target->shade;
        node* current = nullptr;
        node* parent = nullptr;
        bool on_left = false;
        if (!target->left || !target->right) {
            current = target->left ? target->left : target->right;
            parent = target->parent;
            on_left = parent && target == parent->left;
            replace(target, current);
        } else {
            removed_from = target->right;
            while (removed_from->left) removed_from = removed_from->left;
            removed_color = removed_from->shade;  // 必须在继承目标颜色以前保存
            current = removed_from->right;
            if (removed_from->parent == target) {
                parent = removed_from;
                on_left = false;
                if (current) current->parent = removed_from;
            } else {
                parent = removed_from->parent;
                on_left = true;  // 非直接后继是原父的左孩子
                replace(removed_from, current);
                removed_from->right = target->right;
                removed_from->right->parent = removed_from;
            }
            replace(target, removed_from);
            removed_from->left = target->left;
            removed_from->left->parent = removed_from;
            removed_from->shade = target->shade;
        }
        target->alive = false;  // 只摘除目标对象，不复制键，不立即释放池内存
        target->parent = target->left = target->right = nullptr;
        if (removed_color == color::black) repair(current, parent, on_left);
        return true;
    }
};

static void run_case(const char* name, const std::vector<node_spec>& input, int key) {
    erase_tree tree(input);
    const node* target = tree.find(key);
    const bool erased = tree.erase(key);
    assert(erased && tree.valid() && !target->alive);
    const auto count = tree.counts();
    std::cout << name << ": root=" << (tree.root() ? tree.root()->key : -1)
              << " red=" << count.red_sibling << " merge=" << count.merge
              << " near=" << count.near << " far=" << count.far
              << " absorbed=" << count.absorbed << " rotations=" << count.rotations << '\n';
}

int main() {
    using c = color;
    run_case("merge", {{20,c::black,1,2},{10,c::black},{30,c::black}}, 10);
    run_case("far", {{20,c::black,1,2},{10,c::black},{30,c::black,-1,3},{40,c::red}}, 10);
    run_case("near", {{20,c::black,1,2},{10,c::black},{40,c::black,3},{30,c::red}}, 10);
    run_case("red sibling", {{20,c::black,1,2},{10,c::black},{40,c::red,3,4},
                            {30,c::black},{50,c::black}}, 10);
    run_case("three rotations", {{20,c::black,1,2},{10,c::black},{50,c::red,3,5},
                                {40,c::black,4},{30,c::red},{60,c::black}}, 10);
    run_case("red replacement", {{20,c::black,1},{10,c::red}}, 20);
    return 0;
}
