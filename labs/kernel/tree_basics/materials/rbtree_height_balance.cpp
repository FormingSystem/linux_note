#include <cassert>
#include <iostream>

/* 只统计从当前局部入口到空边界的黑节点数，不执行指针旋转。 */
struct height_result {
    int left;
    int right;
    int expected;
    bool propagate;
};

static height_result merge_black_sibling(int h, bool parent_red)
{
    int parent_before = parent_red ? 0 : 1;
    int expected = parent_before + h + 1;
    // 兄弟由黑变红；红父可变黑，黑父则把缺口交给上层。
    int parent_after = 1;
    return {parent_after + h, parent_after + h, expected, !parent_red};
}

static height_result borrow_far_red(int h, bool parent_red)
{
    int old_parent_black = parent_red ? 0 : 1;
    int expected = old_parent_black + h + 1;
    // 旋转后兄弟继承旧父颜色，旧父和远侄都黑。
    int new_root_black = old_parent_black;
    int old_parent_subtree = 1 + h;
    int far_nephew_subtree = 1 + h;
    return {new_root_black + old_parent_subtree,
            new_root_black + far_nephew_subtree, expected, false};
}

int main()
{
    for (int h = 1; h <= 8; ++h) {
        for (bool parent_red : {false, true}) {
            auto merged = merge_black_sibling(h, parent_red);
            assert(merged.left == merged.right);
            assert(merged.expected - merged.left == (merged.propagate ? 1 : 0));
            auto borrowed = borrow_far_red(h, parent_red);
            assert(borrowed.left == borrowed.right);
            assert(borrowed.left == borrowed.expected && !borrowed.propagate);
        }
    }
    for (bool parent_red : {false, true}) {
        auto merged = merge_black_sibling(1, parent_red);
        auto borrowed = borrow_far_red(1, parent_red);
        std::cout << "parent=" << (parent_red ? "red" : "black")
                  << " merge_height=" << merged.left
                  << " expected=" << merged.expected
                  << " propagate=" << merged.propagate
                  << " borrow_height=" << borrowed.left << "\n";
    }
    return 0;
}
