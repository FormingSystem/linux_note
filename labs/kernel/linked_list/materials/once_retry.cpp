// 教学模型：共同锁保护发布，智能指针保存使用期，不模拟内核内存序。
#include <cassert>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

static std::mutex init_lock;
static std::shared_ptr<const std::vector<int>> state;
static unsigned int attempts = 0;

static std::shared_ptr<const std::vector<int>> get_tasks()
{
    // 退出作用域时自动归还锁，抛出异常也走这条清理路径。
    std::lock_guard<std::mutex> guard(init_lock);
    if (!state) {
        ++attempts;
        auto candidate = std::make_shared<std::vector<int>>();
        candidate->push_back(10);
        if (attempts == 1)
            throw std::runtime_error("模拟构建失败");
        candidate->push_back(20);
        candidate->push_back(30);
        // 从此只发布 const 视图，调用者各自持有同一个对象的引用。
        state = candidate;
    }
    return state;
}

int main()
{
    bool failed = false;
    try {
        get_tasks();
    } catch (const std::runtime_error &) {
        failed = true;
    }
    assert(failed && !state && attempts == 1);

    auto first = std::async(std::launch::async, get_tasks);
    auto second = std::async(std::launch::async, get_tasks);
    auto result_a = first.get();
    auto result_b = second.get();
    // get 已等待两个调用结束，此后读取 attempts 不与构建者并发。
    assert(attempts == 2);
    assert(result_a.get() == result_b.get());
    assert((*result_a == std::vector<int>{10, 20, 30}));
    std::cout << "尝试次数: " << attempts << '\n';
    std::cout << "两个调用者共享完整结果:";
    for (int value : *result_a)
        std::cout << ' ' << value;
    std::cout << '\n';
}
