#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

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
        assert(first.use_count() == 1 && borrowed->value == 42);

        auto second = first; // 拷贝管理型指针，共享同一份所有权记录。
        assert(first.use_count() == 2 && second.get() == borrowed);
        std::cout << "after_copy=" << first.use_count() << '\n';

        auto third = std::move(second); // 转移这份所有权，second 变空。
        assert(!second && third.use_count() == 2);
        first.reset(); // 归还 first 那份；third 仍保活对象。
        assert(third.use_count() == 1 && borrowed->value == 42);
        std::cout << "after_reset=" << third.use_count() << '\n';

        throw std::runtime_error("finish"); // 展开作用域时析构 third。
    } catch (const std::runtime_error &) {
        // 不再读取 borrowed；只观察对象之外的析构次数。
        assert(destroyed == 1);
        std::cout << "destroyed=" << destroyed << '\n';
    }
    assert(destroyed == 1);
}
