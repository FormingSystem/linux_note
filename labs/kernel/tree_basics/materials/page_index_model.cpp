#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>

/* 四个逻辑页驻留在宿主数组中；物理块号只是模型标签。 */
struct page {
    bool leaf;
    unsigned int count;
    std::array<int, 3> keys;
    std::array<unsigned int, 3> child;
    int next;
    unsigned int block;
};
static const std::array<page, 4> pages{{
    {false, 2, {30, 60, 0}, {1, 2, 3}, -1, 9},
    {true, 3, {10, 20, 25}, {}, 2, 1000},
    {true, 3, {30, 40, 50}, {}, 3, 8921},
    {true, 3, {60, 70, 80}, {}, -1, 331}
}};
struct page_cache {
    std::array<bool, 4> resident{};
    unsigned int requests = 0;
    unsigned int misses = 0;
    const page &fetch(unsigned int id) {
        assert(id < pages.size());
        ++requests;
        if (!resident[id]) {
            resident[id] = true;
            ++misses;
        }
        return pages[id];
    }
};
struct scan_result {
    std::array<int, 9> keys{};
    std::size_t count = 0;
};

static unsigned int locate_leaf(page_cache &cache, int key)
{
    const page &root = cache.fetch(0);
    unsigned int i = 0;
    // 内部键复制右侧最小边界，相等时必须向右。
    while (i < root.count && key >= root.keys[i])
        ++i;
    return root.child[i];
}
static bool point_lookup(page_cache &cache, int key)
{
    unsigned int id = locate_leaf(cache, key);
    const page &leaf = cache.fetch(id);
    for (unsigned int i = 0; i < leaf.count; ++i)
        if (leaf.keys[i] == key)
            return true;
    return false;
}
static scan_result range_scan(page_cache &cache, int low, int high)
{
    scan_result result;
    if (low > high)
        return result;
    int id = static_cast<int>(locate_leaf(cache, low));
    while (id >= 0) {
        const page &leaf = cache.fetch(static_cast<unsigned int>(id));
        for (unsigned int i = 0; i < leaf.count; ++i) {
            int key = leaf.keys[i];
            if (key > high)
                return result;
            if (key >= low) {
                assert(result.count < result.keys.size());
                result.keys[result.count++] = key;
            }
        }
        if (leaf.keys[leaf.count - 1] >= high)
            return result;
        id = leaf.next;                     // 跟随逻辑页号，不把物理块号加一
    }
    return result;
}
static void print_counts(const char *label, const page_cache &cache)
{
    std::cout << label << " requests=" << cache.requests
              << " misses=" << cache.misses << "\n";
}
int main()
{
    page_cache range_cache;
    auto result = range_scan(range_cache, 20, 70);
    for (std::size_t i = 0; i < result.count; ++i)
        std::cout << result.keys[i] << (i + 1 == result.count ? "\n" : " ");
    print_counts("range cold", range_cache);

    page_cache point_cache;
    for (int key : {20, 25, 30, 40, 50, 60, 70})
        if (!point_lookup(point_cache, key)) return 1;
    print_counts("points cold", point_cache);

    range_cache.requests = range_cache.misses = 0; // 保留已驻留页，开始下一轮
    range_scan(range_cache, 20, 70);
    print_counts("range warm", range_cache);
    page_cache root_cached;
    root_cached.resident[0] = true;
    range_scan(root_cached, 20, 70);
    print_counts("root cached", root_cached);

    const std::array<int, 9> expected{{10, 20, 25, 30, 40, 50, 60, 70, 80}};
    for (int low = 0; low <= 90; ++low)
        for (int high = 0; high <= 90; ++high) {
            page_cache cache;
            auto actual = range_scan(cache, low, high);
            std::size_t pos = 0;
            for (int key : expected)
                if (key >= low && key <= high) {
                    assert(pos < actual.count && actual.keys[pos] == key);
                    ++pos;
                }
            assert(pos == actual.count);
        }
    return 0;
}
