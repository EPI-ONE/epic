#include <gtest/gtest.h>

#include "test_env.h"

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new EpicTestEnvironment);
    return RUN_ALL_TESTS();
}
