#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

using address = std::uint64_t;

struct region {
    address start;
    address end;
    unsigned permissions;
    char owner;
};

class range_map {
public:
    std::vector<region> regions;

    // 输入是有效、递增且不重叠的半开区间；空洞没有对象。
    explicit range_map(std::vector<region> input) : regions(std::move(input)) {
        for (std::size_t i = 0; i < regions.size(); ++i) {
            assert(regions[i].start < regions[i].end);
            assert(i == 0 || regions[i - 1].end <= regions[i].start);
        }
    }

    const region* lookup(address index) const {
        for (const auto& item : regions)
            if (item.start <= index && index < item.end)
                return &item;
        return nullptr;
    }

    const region* find(address index) const {
        for (const auto& item : regions)
            if (index < item.end)
                return &item;
        return nullptr;
    }

    const region* intersection(address start, address end) const {
        if (start >= end)
            return nullptr;
        const auto* item = find(start);
        return item && item->start < end ? item : nullptr;
    }

    // 仅寻找给定窗口内最低的连续空洞，不模拟对齐和栈保护间隔。
    std::optional<address> gap(address low, address high, address length) const {
        if (low >= high || length == 0 || length > high - low)
            return std::nullopt;
        address cursor = low;
        for (const auto& item : regions) {
            if (item.end <= cursor)
                continue;
            if (item.start >= high)
                break;
            if (item.start > cursor && length <= item.start - cursor)
                return cursor;
            cursor = std::max(cursor, item.end);
            if (cursor >= high)
                return std::nullopt;
        }
        return length <= high - cursor ? std::optional<address>(cursor)
                                       : std::nullopt;
    }

    // 修改已有映射；要求全部覆盖才修改，这是教学模型自己的失败契约。
    bool protect(address start, address end, unsigned permissions) {
        if (start >= end)
            return false;
        address cursor = start;
        for (const auto& item : regions) {
            if (item.end <= cursor)
                continue;
            if (item.start > cursor)
                return false;
            cursor = std::min(end, item.end);
            if (cursor == end)
                break;
        }
        if (cursor != end)
            return false;
        rewrite(start, end, permissions, false);
        return true;
    }

    void unmap(address start, address end) {
        if (start < end)
            rewrite(start, end, 0, true);
    }

private:
    void rewrite(address start, address end, unsigned permissions, bool erase) {
        std::vector<region> next;
        for (const auto& item : regions) {
            if (end <= item.start || item.end <= start) {
                next.push_back(item);
                continue;
            }
            if (item.start < start)
                next.push_back({item.start, start, item.permissions, item.owner});
            if (!erase)
                next.push_back({std::max(start, item.start), std::min(end, item.end),
                                permissions, item.owner});
            if (end < item.end)
                next.push_back({end, item.end, item.permissions, item.owner});
        }
        // 为了直接观察切分保留相邻片段，不模拟 Linux 的合并条件。
        regions.swap(next);
    }
};

static void show(const range_map& map) {
    for (const auto& item : map.regions)
        std::cout << item.owner << " [" << std::hex << item.start << ','
                  << item.end << ") permissions=" << item.permissions << '\n';
}

int main() {
    const std::vector<region> original{
        {0x50000000, 0x50010000, 1, 'G'},
        {0x50010000, 0x50030000, 3, 'H'}
    };
    range_map map(original);
    assert(map.lookup(0x50010000)->owner == 'H');
    assert(map.lookup(0x4fffffff) == nullptr);
    assert(map.find(0x4fffffff)->owner == 'G');
    assert(map.intersection(0x4fff0000, 0x50000000) == nullptr);
    assert(map.intersection(0x4fff0000, 0x50000001)->owner == 'G');
    const bool protected_all = map.protect(0x50008000, 0x50018000, 0);
    assert(protected_all);
    std::cout << "protect:\n";
    show(map);
    map = range_map(original);
    map.unmap(0x50008000, 0x50028000);
    std::cout << "unmap:\n";
    show(map);
    const auto vacant = map.gap(0x50000000, 0x50030000, 0x20000);
    assert(vacant && *vacant == 0x50008000);
    std::cout << "gap=" << std::hex << *vacant << '\n';
    return 0;
}
