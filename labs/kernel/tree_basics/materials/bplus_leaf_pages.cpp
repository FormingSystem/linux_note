// SPDX-License-Identifier: MIT
// 固定一层索引与最多九个叶页：观察等值路由、叶分裂和链式范围扫描。
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <vector>

enum class insert_result { inserted, duplicate, full };

class page_index {
    static constexpr unsigned leaf_capacity = 4;
    static constexpr unsigned max_leaves = 9;
    struct leaf_page {
        unsigned size = 0;
        std::array<int, leaf_capacity> keys{};
        int next = -1;
    };
    std::array<leaf_page, max_leaves> pages{};
    std::array<unsigned, max_leaves> children{};
    std::array<int, max_leaves - 1> separators{};
    unsigned child_count = 1;

    unsigned route(int key) const {
        unsigned slot = 0;
        // 分隔键等于右子树最小值；相等必须向右。
        while (slot + 1 < child_count && key >= separators[slot])
            ++slot;
        return slot;
    }

    void refresh_separators() {
        for (unsigned i = 1; i < child_count; ++i)
            separators[i - 1] = pages[children[i]].keys[0];
    }

public:
    static page_index example() {
        constexpr int keys[] = {5,9,12,18,21,27,33,37,42,48,
                                53,57,61,66,72,78,83,88,94,99};
        page_index tree;
        tree.child_count = 5;
        for (unsigned i = 0; i < 5; ++i) {
            tree.children[i] = i;
            tree.pages[i].size = leaf_capacity;
            tree.pages[i].next = i == 4 ? -1 : static_cast<int>(i + 1);
            std::copy_n(keys + i * leaf_capacity, leaf_capacity, tree.pages[i].keys.begin());
        }
        tree.refresh_separators();
        return tree;
    }

    unsigned leaf_id_for(int key) const { return children[route(key)]; }

    bool contains(int key) const {
        const auto &page = pages[leaf_id_for(key)];
        return std::binary_search(page.keys.begin(), page.keys.begin() + page.size, key);
    }

    std::vector<int> scan(int low, int high) const {
        std::vector<int> result;
        if (low > high)
            return result;
        int id = static_cast<int>(leaf_id_for(low));
        while (id != -1) {
            const auto &page = pages[static_cast<unsigned>(id)];
            for (unsigned i = 0; i < page.size; ++i) {
                if (page.keys[i] > high)
                    return result;
                if (page.keys[i] >= low)
                    result.push_back(page.keys[i]);
            }
            id = page.next;
        }
        return result;
    }

    insert_result insert(int key) {
        const unsigned slot = route(key), id = children[slot];
        auto &page = pages[id];
        if (contains(key))
            return insert_result::duplicate;
        // 固定根不能继续增加孩子时，先失败；尚未改任何页或分隔键。
        if (page.size == leaf_capacity && child_count == max_leaves)
            return insert_result::full;
        std::array<int, leaf_capacity + 1> pending{};
        unsigned position = 0;
        while (position < page.size && page.keys[position] < key) {
            pending[position] = page.keys[position];
            ++position;
        }
        pending[position] = key;
        for (unsigned i = position; i < page.size; ++i)
            pending[i + 1] = page.keys[i];
        const unsigned total = page.size + 1;
        if (total <= leaf_capacity) {
            std::copy_n(pending.begin(), total, page.keys.begin());
            page.size = total;
        } else {
            const unsigned fresh_id = child_count;
            auto &right = pages[fresh_id];
            page.size = 2;
            right.size = total - page.size;
            std::copy_n(pending.begin(), page.size, page.keys.begin());
            std::copy_n(pending.begin() + page.size, right.size, right.keys.begin());
            right.next = page.next;
            page.next = static_cast<int>(fresh_id);
            for (unsigned i = child_count; i > slot + 1; --i)
                children[i] = children[i - 1];
            children[slot + 1] = fresh_id;
            ++child_count;
        }
        refresh_separators();
        return insert_result::inserted;
    }

    bool valid() const {
        if (child_count == 0 || child_count > max_leaves)
            return false;
        std::array<bool, max_leaves> seen{};
        bool has_previous = false;
        int previous = 0;
        for (unsigned slot = 0; slot < child_count; ++slot) {
            unsigned id = children[slot];
            if (id >= child_count || seen[id])
                return false;
            seen[id] = true;
            const auto &page = pages[id];
            if (page.size > leaf_capacity || (child_count > 1 && page.size < 2))
                return false;
            if (slot && separators[slot - 1] != page.keys[0])
                return false;
            int next = slot + 1 < child_count ? static_cast<int>(children[slot + 1]) : -1;
            if (page.next != next)
                return false;
            for (unsigned i = 0; i < page.size; ++i) {
                if (has_previous && previous >= page.keys[i])
                    return false;
                previous = page.keys[i];
                has_previous = true;
            }
        }
        return true;
    }

    // 只导出逻辑状态供失败不改树检查，不比较带填充字节的对象内存。
    std::vector<int> snapshot() const {
        std::vector<int> result{static_cast<int>(child_count)};
        for (unsigned i = 0; i < max_leaves; ++i) {
            result.push_back(static_cast<int>(children[i]));
            result.push_back(static_cast<int>(pages[i].size));
            result.push_back(pages[i].next);
            result.insert(result.end(), pages[i].keys.begin(), pages[i].keys.end());
        }
        result.insert(result.end(), separators.begin(), separators.end());
        return result;
    }
};

static void show(const char *name, const std::vector<int> &keys) {
    std::cout << name << ':';
    for (int key : keys)
        std::cout << ' ' << key;
    std::cout << '\n';
}

int main() {
    auto tree = page_index::example();
    assert(tree.valid() && tree.contains(21) && tree.leaf_id_for(21) == 1);
    show("before", tree.scan(33,78));
    assert(tree.insert(55) == insert_result::inserted && tree.valid());
    assert(tree.contains(53) && tree.leaf_id_for(53) == 5);
    show("after", tree.scan(33,78));
    const auto saved = tree.snapshot();
    assert(tree.insert(55) == insert_result::duplicate && tree.snapshot() == saved);
    std::cout << "separator 53 routes to new leaf 5; duplicate leaves state unchanged\n";
    return 0;
}
