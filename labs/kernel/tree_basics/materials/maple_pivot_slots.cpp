// SPDX-License-Identifier: MIT
// 有限地址域的范围分区模型；不调用 Maple API，也不模拟真实节点分裂。
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <vector>

using address = std::uint64_t;
struct region { address start, end; char owner; };
struct slot { address first, last; char entry; };

class partition {
    address domain_end;
    std::vector<slot> slots;
public:
    // 输入须已按起点排序、不重叠且落在 [0,end)；entry '-' 表示空洞。
    partition(address end, const std::vector<region> &regions) : domain_end(end) {
        assert(end != 0);
        address next = 0;
        for (const auto &item : regions) {
            assert(next <= item.start && item.start < item.end && item.end <= end);
            assert(item.owner != '-');
            if (next < item.start)
                slots.push_back({next, item.start - 1, '-'});
            slots.push_back({item.start, item.end - 1, item.owner});
            next = item.end;
        }
        if (next < end)
            slots.push_back({next, end - 1, '-'});
    }

    std::optional<char> lookup(address index) const {
        if (index >= domain_end)
            return std::nullopt;
        // 等于包含式上界时留在同号槽，而不是进入右边槽。
        auto found = std::lower_bound(slots.begin(), slots.end(), index,
            [](const slot &part, address key) { return part.last < key; });
        return found->entry;
    }

    address max_gap() const {
        address maximum = 0;
        for (const auto &part : slots)
            if (part.entry == '-')
                maximum = std::max(maximum, part.last - part.first + 1);
        return maximum;
    }

    std::optional<address> first_fit(address low, address high, address length) const {
        if (length == 0 || low >= high || high > domain_end)
            return std::nullopt;
        for (const auto &part : slots) {
            if (part.entry != '-')
                continue;
            address begin = std::max(low, part.first);
            address end = std::min(high, part.last + 1);
            // 先求交集、再做差，避免用 begin + length 判定造成溢出。
            if (begin < end && length <= end - begin)
                return begin;
        }
        return std::nullopt;
    }

    void print() const {
        for (std::size_t i = 0; i < slots.size(); ++i) {
            const auto &part = slots[i];
            std::cout << std::dec << i << ' ' << part.entry << " [0x"
                      << std::hex << part.first << ",0x" << part.last << "]\n";
        }
    }
};

int main() {
    const std::vector<region> regions{
        {0x400000,0x452000,'A'}, {0x600000,0x610000,'B'},
        {0x800000,0xa80000,'C'}, {0x4000000,0x4800000,'D'},
        {0x7f1000000000,0x7f1000200000,'E'},
        {0x7f1000600000,0x7f1000800000,'F'},
        {0x7fff00000000,0x7fff00021000,'G'}
    };
    partition tree(0x800000000000, regions);
    tree.print();
    for (const auto &item : regions) {
        assert(tree.lookup(item.start) == item.owner);
        assert(tree.lookup(item.end - 1) == item.owner);
        assert(tree.lookup(item.end) == '-');
    }
    assert(!tree.lookup(0x800000000000));
    auto fit = tree.first_fit(0x7f1000200000,0x7f1000600000,0x300000);
    assert(fit && *fit == 0x7f1000200000);
    auto clipped = tree.first_fit(0x7f1000400000,0x7f1000600000,0x300000);
    assert(!clipped);
    std::cout << "E-F gap: 0x400000; fit: 0x" << std::hex << *fit
              << "; clipped window: no fit\n";
    return 0;
}
