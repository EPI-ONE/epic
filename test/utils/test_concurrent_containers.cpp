#include <gtest/gtest.h>

#include "concurrent_container.h"
#include "threadpool.h"

class TestConcurrentContainers : public testing::Test {
protected:
    ThreadPool threadPool;
    int testSize;
    ConcurrentHashMap<int, int> m;
    ConcurrentHashSet<int> s;

    void SetUp() {
        testSize = 10000;
        threadPool.SetThreadSize(4);
        threadPool.Start();
    }

    void TearDown() {}
};

TEST_F(TestConcurrentContainers, HashMap) {
    for (int i = 0; i < testSize; ++i) {
        threadPool.Execute([i, this]() {
            auto key    = i;
            auto result = m.insert_or_assign(key, std::move(key));
            ASSERT_TRUE(result.second);
        });
    }

    for (int i = 0; i < testSize; ++i) {
        threadPool.Execute([i, this]() { m.erase(i); });
    }

    while (threadPool.GetTaskSize() > 0) {
        std::this_thread::yield();
    }

    threadPool.Stop();

    ASSERT_EQ(m.size(), 0);
}

TEST_F(TestConcurrentContainers, HashSet) {
    for (int i = 0; i < testSize; ++i) {
        threadPool.Execute([i, this]() {
            auto result = s.insert(i);
            ASSERT_TRUE(result.second);
        });
    }

    for (int i = 0; i < testSize; ++i) {
        threadPool.Execute([i, this]() { s.erase(i); });
    }

    while (threadPool.GetTaskSize() > 0) {
        std::this_thread::yield();
    }

    threadPool.Stop();

    ASSERT_EQ(s.size(), 0);
}
