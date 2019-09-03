// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "test_env.h"

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new EpicTestEnvironment);
    return RUN_ALL_TESTS();
}
