#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <list>
#include <optional>
#include <unordered_set>
#include <vector>

enum class color { red, black };

struct node {
    int key;
    color shade = color::red;
    node* parent = nullptr;
    node* left = nullptr;
    node* right = nullptr;
};

struct repair_counts {
    unsigned recolors = 0;  // 叔红分支次数，不是单个颜色写入次数
    unsigned inner = 0;     // 内侧转换次数
    unsigned outer = 0;     // 外侧收尾次数
};

class rb_tree {
    std::list<node> storage_;  // 拥有对象，增加元素不会搬移已有节点
    node* root_ = nullptr;
    repair_counts counts_;

    static bool is_red(const node* current) {
        return current != nullptr && current->shade == color::red;
    }

    void rotate_left(node* old_root) {
        node* new_root = old_root->right;
        assert(new_root != nullptr);
        old_root->right = new_root->left;
        if (new_root->left) new_root->left->parent = old_root;
        new_root->parent = old_root->parent;
        if (!old_root->parent) root_ = new_root;
        else if (old_root == old_root->parent->left)
            old_root->parent->left = new_root;
        else old_root->parent->right = new_root;
        new_root->left = old_root;
        old_root->parent = new_root;
    }

    void rotate_right(node* old_root) {
        node* new_root = old_root->left;
        assert(new_root != nullptr);
        old_root->left = new_root->right;
        if (new_root->right) new_root->right->parent = old_root;
        new_root->parent = old_root->parent;
        if (!old_root->parent) root_ = new_root;
        else if (old_root == old_root->parent->right)
            old_root->parent->right = new_root;
        else old_root->parent->left = new_root;
        new_root->right = old_root;
        old_root->parent = new_root;
    }

    void repair(node* current) {
        while (is_red(current->parent)) {
            node* parent = current->parent;
            node* grand = parent->parent;  // 黑根不可能是红父，祖父存在
            assert(grand != nullptr);
            if (parent == grand->left) {
                node* uncle = grand->right;
                if (is_red(uncle)) {
                    parent->shade = color::black;
                    uncle->shade = color::black;
                    grand->shade = color::red;
                    current = grand;  // 检查点换成原祖父，不再是新叶子
                    ++counts_.recolors;
                    continue;
                }
                if (current == parent->right) {
                    current = parent;  // 转换后原父成为新的下层红节点
                    rotate_left(current);
                    parent = current->parent;
                    ++counts_.inner;
                }
                parent->shade = color::black;
                grand->shade = color::red;
                rotate_right(grand);
                ++counts_.outer;
                break;
            } else {
                node* uncle = grand->left;
                if (is_red(uncle)) {
                    parent->shade = color::black;
                    uncle->shade = color::black;
                    grand->shade = color::red;
                    current = grand;
                    ++counts_.recolors;
                    continue;
                }
                if (current == parent->left) {
                    current = parent;
                    rotate_right(current);
                    parent = current->parent;
                    ++counts_.inner;
                }
                parent->shade = color::black;
                grand->shade = color::red;
                rotate_left(grand);
                ++counts_.outer;
                break;
            }
        }
        root_->shade = color::black;  // 空树首插或上推到根时统一收尾
    }

    static int inspect(const node* current, const node* parent,
                       std::optional<int> low, std::optional<int> high,
                       std::unordered_set<const node*>& seen) {
        if (!current) return 1;  // 内部计数包含当前黑节点和终点 NIL
        if (!seen.insert(current).second || current->parent != parent ||
            (low && current->key <= *low) || (high && current->key >= *high) ||
            (is_red(parent) && is_red(current))) return -1;
        const int left = inspect(current->left, current, low, current->key, seen);
        if (left < 0) return -1;
        const int right = inspect(current->right, current, current->key, high, seen);
        if (right < 0 || left != right) return -1;
        return left + (current->shade == color::black ? 1 : 0);
    }

public:
    rb_tree() = default;
    rb_tree(const rb_tree&) = delete;  // 默认复制会让指针仍指向另一棵树
    rb_tree& operator=(const rb_tree&) = delete;
    rb_tree(rb_tree&&) = delete;
    rb_tree& operator=(rb_tree&&) = delete;

    bool insert(int key) {
        counts_ = {};
        node* parent = nullptr;
        node** slot = &root_;
        while (*slot) {
            parent = *slot;
            if (key == parent->key) return false;  // 唯一键，重复不改结构
            slot = key < parent->key ? &parent->left : &parent->right;
        }
        storage_.push_back(node{key});  // 若分配抛异常，此时还没有改动树链接
        node* inserted = &storage_.back();
        inserted->parent = parent;
        *slot = inserted;
        repair(inserted);  // 此后只写指针/颜色，不再申请资源
        return true;
    }

    const node* root() const { return root_; }
    repair_counts counts() const { return counts_; }
    std::size_t size() const { return storage_.size(); }

    bool valid() const {
        if (!root_) return storage_.empty();
        if (root_->parent || is_red(root_)) return false;
        std::unordered_set<const node*> seen;
        return inspect(root_, nullptr, {}, {}, seen) >= 0 && seen.size() == size();
    }

    std::vector<int> keys() const {
        std::vector<int> result;
        auto visit = [&](auto&& self, const node* current) -> void {
            if (!current) return;
            self(self, current->left);
            result.push_back(current->key);
            self(self, current->right);
        };
        visit(visit, root_);
        return result;
    }
};

static void run_case(const char* name, std::initializer_list<int> input) {
    rb_tree tree;
    repair_counts total;
    for (int key : input) {
        const bool inserted = tree.insert(key);
        assert(inserted && tree.valid());
        const auto step = tree.counts();
        total.recolors += step.recolors;
        total.inner += step.inner;
        total.outer += step.outer;
    }
    std::cout << name << ": root=" << tree.root()->key
              << " recolors=" << total.recolors << " inner=" << total.inner
              << " outer=" << total.outer << " keys=";
    for (int key : tree.keys()) std::cout << ' ' << key;
    std::cout << '\n';
}

int main() {
    run_case("LL", {30, 20, 10});
    run_case("LR", {30, 10, 20});
    run_case("RR", {10, 20, 30});
    run_case("RL", {10, 30, 20});
    run_case("red uncle", {30, 20, 40, 10});
    run_case("propagate", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    return 0;
}
