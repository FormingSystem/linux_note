// C++17单生产者/单消费者：固定轮数单槽交还协议，不是Linux环形队列。
#include <atomic>
#include <exception>
#include <iostream>
#include <thread>

struct shared_slot {
    unsigned int value = 0, check = 0;
    std::atomic<bool> ready{false};
};

int main()
{
    constexpr unsigned int rounds = 10000, mask = 0x5a5a;
    shared_slot slot;
    unsigned int errors = 0;
    try {
        std::thread producer([&slot] {
            for (unsigned int item = 1; item <= rounds; ++item) {
                while (slot.ready.load(std::memory_order_acquire))
                    std::this_thread::yield();
                slot.value = item;
                slot.check = item ^ mask;
                slot.ready.store(true, std::memory_order_release);
            }
        });
        for (unsigned int expected = 1; expected <= rounds; ++expected) {
            while (!slot.ready.load(std::memory_order_acquire))
                std::this_thread::yield();
            // 在归还以前完成所有普通字段读取，只保留本地副本。
            unsigned int value = slot.value, check = slot.check;
            slot.ready.store(false, std::memory_order_release);
            if (value != expected || check != (value ^ mask))
                ++errors;
        }
        producer.join(); // 两路都结束后，栈上的slot才可销毁。
    } catch (const std::exception &error) {
        std::cerr << "slot experiment failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << rounds << " publications consumed; errors=" << errors << '\n';
    return errors ? 1 : 0;
}
