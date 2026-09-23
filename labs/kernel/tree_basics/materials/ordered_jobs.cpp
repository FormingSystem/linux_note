#include <array>
#include <iostream>
#include <limits>
#include <set>

struct job {
    int deadline;
    int id;
};

struct by_deadline {
    bool operator()(const job *left, const job *right) const noexcept
    {
        // 到期时间相同时再比较编号，不用整数相减作比较。
        if (left->deadline != right->deadline)
            return left->deadline < right->deadline;
        return left->id < right->id;
    }
};

using job_index = std::set<job *, by_deadline>;

void print_jobs(const char *label, const job_index &index)
{
    std::cout << label;
    for (const job *item : index)
        std::cout << ' ' << item->deadline << ':' << item->id;
    std::cout << '\n';
}

int main()
{
    // 数组拥有对象，索引只保存指针；程序期间数组地址不变。
    std::array<job, 6> jobs{{{40, 4}, {10, 1}, {40, 2},
                            {70, 6}, {25, 3}, {55, 5}}};
    job_index index;
    for (job &item : jobs)
        index.insert(&item);
    print_jobs("ordered:", index);

    if (!index.empty())
        std::cout << "earliest: " << (*index.begin())->id << '\n';

    // 最小编号使同一到期时间的所有对象都不落在探针之前。
    job probe{35, std::numeric_limits<int>::min()};
    auto first = index.lower_bound(&probe);
    if (first != index.end())
        std::cout << "at least 35: " << (*first)->id << '\n';
    std::cout << "deadline 40:";
    probe.deadline = 40;
    for (auto it = index.lower_bound(&probe);
         it != index.end() && (*it)->deadline == 40; ++it)
        std::cout << ' ' << (*it)->id;
    std::cout << '\n';

    // 不同地址也可能有等价的排序键，地址不决定查重结果。
    job equivalent{40, 2};
    const auto result = index.insert(&equivalent);
    std::cout << "equivalent inserted: " << result.second << '\n';
    if (result.second)
        return 1;

    job *changed = &jobs[0];
    const auto old = index.find(changed);
    if (old == index.end())
        return 2;
    index.erase(old);             // 先撤销索引中的旧位置。
    changed->deadline = 5;        // 再修改业务对象，地址和编号保持不变。
    if (!index.insert(changed).second)
        return 3;
    print_jobs("rescheduled:", index);

    index.clear();                // 清索引不会销毁数组中的业务对象。
    std::cout << "after clear: index=" << index.size()
              << " objects=" << jobs.size()
              << " changed=" << changed->deadline << ':' << changed->id << '\n';
    return 0;
}
