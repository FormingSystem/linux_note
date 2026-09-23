// C++17发布实验：使用标准原子，不替代Linux内核原语或LKMM验证。
#include <atomic>
#include <exception>
#include <iostream>
#include <thread>

struct shared_record {
    int payload = 0;
    std::atomic<bool> ready{false};
};

int main()
{
    try {
        for (int trial = 0; trial < 100; ++trial) {
            shared_record record; // 每轮新对象，不重置仍被使用的发布位。
            const int expected = 42 + trial;
            std::thread producer([&record, expected] {
                record.payload = expected;
                record.ready.store(true, std::memory_order_release);
            });
            while (!record.ready.load(std::memory_order_acquire))
                std::this_thread::yield(); // 降低忙等侵占，不承担数据同步。
            const int observed = record.payload; // 在join以前读取载荷。
            producer.join(); // 结束线程寿命，然后本轮对象才可离开作用域。
            if (observed != expected) {
                std::cerr << "unexpected payload\n";
                return 1;
            }
        }
        std::cout << "100 one-shot publications passed\n";
    } catch (const std::exception &error) {
        std::cerr << "thread experiment failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
