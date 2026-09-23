#include <atomic>
#include <barrier>
#include <cassert>
#include <exception>
#include <iostream>
#include <latch>
#include <memory>
#include <mutex>
#include <thread>

enum class update_mode { locked, atomic_rmw, split_atomic };
struct counters {
    std::mutex lock;
    unsigned int plain = 0;
    std::atomic<unsigned int> atomic{0};
};

static void run_case(update_mode mode, const char *name)
{
    constexpr unsigned int rounds = 1000;
    auto state = std::make_shared<counters>();
    std::latch start_gate(1);
    std::barrier rendezvous(2);
    std::atomic<bool> canceled{false};
    std::thread workers[2];
    auto work = [&, state] {
        start_gate.wait();
        if (canceled.load())
            return;
        for (unsigned int step = 0; step < rounds; ++step) {
            if (mode == update_mode::locked) {
                std::lock_guard guard(state->lock);
                ++state->plain;
            } else if (mode == update_mode::atomic_rmw) {
                state->atomic.fetch_add(1, std::memory_order_relaxed);
            } else {
                // 单次访问均为原子，但读和写不是一个不可分割的更新。
                unsigned int old = state->atomic.load(std::memory_order_relaxed);
                rendezvous.arrive_and_wait(); // 两人都读完同一旧值才开始写。
                state->atomic.store(old + 1, std::memory_order_relaxed);
                rendezvous.arrive_and_wait(); // 两次写都完成后才进入下一轮。
            }
        }
    };
    try {
        for (auto &worker : workers)
            worker = std::thread(work);
    } catch (...) {
        // 创建第二个线程失败时，先取消并放行已创建者，避免它等不到同伴。
        canceled.store(true);
        start_gate.count_down();
        for (auto &worker : workers)
            if (worker.joinable())
                worker.join();
        throw;
    }
    start_gate.count_down();
    for (auto &worker : workers)
        worker.join();
    unsigned int actual = mode == update_mode::locked ? state->plain : state->atomic.load();
    unsigned int expected = mode == update_mode::split_atomic ? rounds : 2 * rounds;
    assert(actual == expected);
    std::cout << name << ": updates=" << 2 * rounds << " value=" << actual << '\n';
    // 管理者和线程的shared_ptr保住外壳，字段正确性仍由上述更新协议负责。
}

int main()
{
    try {
        run_case(update_mode::locked, "mutex");
        run_case(update_mode::atomic_rmw, "atomic_rmw");
        run_case(update_mode::split_atomic, "split_atomic");
    } catch (const std::exception &error) {
        std::cerr << "experiment failed: " << error.what() << '\n';
        return 1;
    }
}
