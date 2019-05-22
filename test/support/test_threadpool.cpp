#include "spdlog.h"
#include "threadpool.h"
#include <functional>
#include <gtest/gtest.h>

class TestThreadPool : public testing::Test {
public:
    ThreadPool threadPool;
    size_t threadsize = 3;

    void f0() {
        spdlog::info("f0 is executed");
    }

    std::function<void()> f1 = []() { spdlog::info("f1 is executed"); };
    std::function<int()> f2  = []() {
        spdlog::info("f2 is executed");
        return 1;
    };

    struct Foo {
        void f3() {
            spdlog::info("f3 is executed");
        }
        static int f4() {
            return 3;
        }
    };

    class Bar {
    private:
        int value;

    public:
        Bar(int value_) : value(value_) {}
        int f4() {
            return value;
        }
    };

    void SetUp() {
        threadPool.SetThreadSize(threadsize);
        threadPool.Start();
    }

    void TearDown() {
        threadPool.Stop();
    }
};

int f6(int* a) {
    return ++(*a);
}


TEST_F(TestThreadPool, TestNormalFunction) {
    threadPool.Execute(std::bind(&TestThreadPool::f0, this));

    int a       = 2;
    auto result = threadPool.Submit(std::bind(f6, &a));
    EXPECT_EQ(result.get(), 3);
}

TEST_F(TestThreadPool, TestFunction) {
    threadPool.Execute(f1);
    auto result = threadPool.Submit(f2);
    EXPECT_EQ(result.get(), 1);
}

TEST_F(TestThreadPool, TestStructMemberFunction) {
    Foo foo;
    threadPool.Execute(std::bind(&Foo::f3, &foo));
    auto result = threadPool.Submit(Foo::f4);
    EXPECT_EQ(3, result.get());
}

TEST_F(TestThreadPool, TestClassMemberFunction) {
    Bar bar(2);
    auto result = threadPool.Submit(std::bind(&Bar::f4, &bar));
    EXPECT_EQ(result.get(), 2);
}

TEST_F(TestThreadPool, TestlambaFunction) {
    auto result = threadPool.Submit([]() { return "lambda function"; });
    EXPECT_EQ("lambda function", result.get());
}
