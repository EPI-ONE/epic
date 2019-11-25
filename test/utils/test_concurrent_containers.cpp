// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "blocking_queue.h"
#include "concurrent_container.h"
#include "threadpool.h"

class TestConcurrentContainers : public testing::Test {
protected:
    ThreadPool threadPool;
    int testSize;

    void SetUp() {
        testSize = 10000;
        threadPool.SetThreadSize(4);
        threadPool.Start();
    }

    void TearDown() {}
};

TEST_F(TestConcurrentContainers, HashMap) {
    ConcurrentHashMap<int, int> m;
    for (int i = 0; i < testSize; ++i) {
        threadPool.Execute([i, &m]() {
            auto key    = i;
            auto result = m.insert_or_assign(key, std::move(key));
            ASSERT_TRUE(result.second);
        });
    }

    while (!threadPool.IsIdle()) {
        usleep(100000);
    }

    for (int i = 0; i < testSize; ++i) {
        threadPool.Execute([i, &m]() { m.erase(i); });
    }

    while (!threadPool.IsIdle()) {
        usleep(100000);
    }

    threadPool.Stop();

    ASSERT_TRUE(m.empty());
}

TEST_F(TestConcurrentContainers, HashSet) {
    ConcurrentHashSet<int> s;
    for (int i = 0; i < testSize; ++i) {
        threadPool.Execute([i, &s]() {
            auto result = s.insert(i);
            ASSERT_TRUE(result.second);
        });
    }

    while (!threadPool.IsIdle()) {
        usleep(100000);
    }

    for (int i = 0; i < testSize; ++i) {
        threadPool.Execute([i, &s]() { s.erase(i); });
    }

    while (!threadPool.IsIdle()) {
        usleep(100000);
    }

    threadPool.Stop();

    ASSERT_TRUE(s.empty());
}
