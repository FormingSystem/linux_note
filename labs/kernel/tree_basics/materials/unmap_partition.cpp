#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

using unmap_address = std::uint64_t;
struct unmap_region { unmap_address start, end; char owner; };
struct unmap_plan { std::vector<unmap_region> keep, detached; };

// 只建区间分区计划；不模拟内核 VMA 对象、锁、页表或释放动作。
static unmap_plan partition(const std::vector<unmap_region>& input,
                            unmap_address start, unmap_address end)
{
    if (start >= end)
        throw std::invalid_argument("empty or reversed request");
    unmap_plan plan;
    unmap_address previous_end = 0;
    for (const auto& item : input) {
        if (item.start >= item.end || item.start < previous_end)
            throw std::invalid_argument("invalid input regions");
        previous_end = item.end;
        if (item.end <= start || item.start >= end) {
            plan.keep.push_back(item);
            continue;
        }
        if (item.start < start)
            plan.keep.push_back({item.start, start, item.owner});
        const auto first = item.start > start ? item.start : start;
        const auto last = item.end < end ? item.end : end;
        plan.detached.push_back({first, last, item.owner});
        if (item.end > end)
            plan.keep.push_back({end, item.end, item.owner});
    }
    return plan;
}

static void print_region(const unmap_region& item)
{
    std::cout << item.owner << " [" << std::hex << item.start << ','
              << item.end << ")\n";
}

int main()
{
    std::vector<unmap_region> live{
        {0x7f1000000000, 0x7f1000200000, 'E'},
        {0x7f1000200000, 0x7f1000240000, 'F'},
        {0x7f1000600000, 0x7f1000800000, 'G'}
    };
    const auto plan = partition(live, 0x7f1000100000, 0x7f1000700000);
    // detached 的下标是处理序号，元素中的 start/end 才是地址。
    std::cout << "prepared: live_count=" << live.size() << '\n';
    for (std::size_t i = 0; i < plan.detached.size(); ++i) {
        std::cout << "ordinal=" << std::dec << i << ' ';
        print_region(plan.detached[i]);
    }
    live = plan.keep;
    std::cout << "published survivors:\n";
    for (const auto& item : live)
        print_region(item);
    // 在模型中仍可读处理计划；这不是内核对象已被引用或释放的证据。
    std::cout << "pending cleanup=" << std::dec << plan.detached.size() << '\n';
    return 0;
}
