#include <array>
#include <atomic>
#include <exception>
#include <iostream>
#include <thread>

constexpr int free_state = 0;
constexpr int owned_state = 1;
constexpr int worker_count = 4;
constexpr int rounds = 10000;

struct shared_resource {
    std::atomic<int> state{free_state};
    int value = 0;
    int check = 7;
};

bool try_acquire(shared_resource &resource)
{
    // 每次只允许 FREE -> OWNED；失败改写的期望值不能带入下一次尝试。
    int expected = free_state;
    return resource.state.compare_exchange_strong(
        expected, owned_state, std::memory_order_acquire,
        std::memory_order_relaxed);
}

int main()
{
    // 单线程反例只操作状态，不访问载荷，不制造数据竞争。
    std::atomic<int> probe{owned_state};
    int expected = free_state;
    const bool first = probe.compare_exchange_strong(
        expected, owned_state, std::memory_order_acquire,
        std::memory_order_relaxed);
    const int after_failure = expected;
    const bool false_grant = probe.compare_exchange_strong(
        expected, owned_state, std::memory_order_acquire,
        std::memory_order_relaxed);
    std::cout << "probe_first=" << first
              << " expected_after_failure=" << after_failure
              << " wrong_retry_success=" << false_grant << '\n';
    if (first || after_failure != owned_state || !false_grant)
        return 1;

    shared_resource resource;
    std::array<int, worker_count> errors{};
    std::array<std::thread, worker_count> workers;
    bool launch_failed = false;
    try {
        for (int id = 0; id < worker_count; ++id) {
            workers[id] = std::thread([&, id] {
                for (int n = 0; n < rounds; ++n) {
                    while (!try_acquire(resource))
                        std::this_thread::yield(); // 让出运行机会，不是同步边。
                    // 仅所有者访问这两个普通字段。
                    if (resource.check != resource.value * 3 + 7)
                        ++errors[id];
                    ++resource.value;
                    resource.check = resource.value * 3 + 7;
                    resource.state.store(free_state, std::memory_order_release);
                }
            });
        }
    } catch (const std::exception &error) {
        std::cerr << "thread launch failed: " << error.what() << '\n';
        launch_failed = true;
    }
    // 即使创建中途失败，也先收回已启动线程，之后才能销毁栈上资源。
    for (auto &worker : workers)
        if (worker.joinable())
            worker.join();
    if (launch_failed)
        return 2;

    int total_errors = 0;
    for (int count : errors)
        total_errors += count;
    std::cout << "updates=" << resource.value << " errors=" << total_errors
              << " check=" << resource.check << '\n';
    return resource.value == worker_count * rounds &&
                   resource.check == resource.value * 3 + 7 && total_errors == 0
               ? 0 : 1;
}
