#include <gtest/gtest.h>

#include "max_queue.h"

class TestMaxQueue : public testing::Test {};

TEST_F(TestMaxQueue, BasicFunctions) {
    int maxInt = rand() + 100;
    int size   = 1000000;

    // Prepare a vector of random integers less than maxInt
    std::vector<int> randomInts(size);
    for (int i = 0; i < size; ++i) {
        randomInts[i] = rand() % maxInt;
    }

    // Replace a random element in randomInts with maxInt
    randomInts[rand() % size] = maxInt;

    // Push elements into max_queue
    max_queue<int> q;
    q.reserve(size);
    for (int i = 0; i < size; ++i) {
        q.push(randomInts[i]);
    }

    EXPECT_EQ(q.size(), size);
    EXPECT_EQ(q.max(), maxInt);

    // Replace the first element by a bigger number
    int newMax = maxInt + 1;
    *q.begin() = newMax;
    q.update_max(q.begin());

    EXPECT_EQ(q.max(), newMax);

    // Erasing the max element not allowed
    q.erase(q.begin());
    EXPECT_EQ(q.size(), size);

    q.erase(q.begin() + 1);
    EXPECT_EQ(q.size(), size - 1);
}
