#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

/* 检查不随 NDEBUG 消失，也不借业务异常掩盖失败。 */
static void require_model(bool condition)
{
    if (!condition) {
        std::cerr << "ownership invariant failed\n";
        std::exit(EXIT_FAILURE);
    }
}

static unsigned int destroyed;

struct record {
    int value = 42;
    ~record() { ++destroyed; } // 完成记录不在待销毁对象里面。
};

int main()
{
    try {
        auto first = std::make_shared<record>();
        record *borrowed = first.get(); // 借用裸指针，不增加所有者。
        require_model(first.use_count() == 1 && borrowed->value == 42);

        auto second = first; // 拷贝管理型指针，共享同一份所有权记录。
        require_model(first.use_count() == 2 && second.get() == borrowed);
        std::cout << "after_copy=" << first.use_count() << '\n';

        auto third = std::move(second); // 转移这份所有权，second 变空。
        require_model(!second && third.use_count() == 2);
        first.reset(); // 归还 first 那份；third 仍保活对象。
        require_model(third.use_count() == 1 && borrowed->value == 42);
        std::cout << "after_reset=" << third.use_count() << '\n';

        throw std::runtime_error("finish"); // 展开作用域时析构 third。
    } catch (const std::runtime_error &) {
        // 不再读取 borrowed；只观察对象之外的析构次数。
        require_model(destroyed == 1);
        std::cout << "destroyed=" << destroyed << '\n';
    }
    require_model(destroyed == 1);
}
