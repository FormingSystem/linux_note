// 只读视图：每次查询都有失败边界，不冻结设备拓扑。
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

static void describe(const fs::path &entry)
{
    std::error_code error;
    const fs::path location = fs::canonical(entry, error);
    if (error) {
        std::cout << entry.filename().string() << ": object unavailable: "
                  << error.message() << '\n';
        return;
    }
    std::cout << entry.filename().string() << "\n  object="
              << location.string() << '\n';
    const fs::path driver = fs::canonical(entry / "driver", error);
    if (error == std::errc::no_such_file_or_directory)
        std::cout << "  driver=(no resolvable driver link)\n";
    else if (error)
        std::cout << "  driver unavailable: " << error.message() << '\n';
    else
        std::cout << "  driver=" << driver.filename().string() << '\n';
}

int main(int argc, char **argv)
{
    if (argc > 2) {
        std::cerr << "用法: sysfs_view [设备目录]\n";
        return 1;
    }
    const fs::path root = argc == 2 ? argv[1] : "/sys/bus/platform/devices";
    try {
        if (!fs::is_directory(root)) {
            std::cerr << "不是设备目录: " << root.string() << '\n';
            return 1;
        }
        std::vector<fs::path> entries;
        for (const auto &entry : fs::directory_iterator(root))
            entries.push_back(entry.path());
        std::sort(entries.begin(), entries.end());
        if (entries.empty())
            std::cout << "当前目录没有设备；这不表示内核中没有其他总线。\n";
        const std::size_t count = std::min(entries.size(), std::size_t{5});
        for (std::size_t index = 0; index < count; ++index)
            describe(entries[index]);
    } catch (const fs::filesystem_error &error) {
        std::cerr << "目录观察失败: " << error.what() << '\n';
        return 1;
    }
}
