#include <gtest/gtest.h>
#include <iostream>

#include "block.h"
#include "params.h"

class TestParams : public testing::Test {
public:
    const Params& params = GetParams();
};

TEST_F(TestParams, Basic_Value_Check) {
    EXPECT_EQ(100, params.targetTPS);
    EXPECT_EQ(1559859000L, GENESIS.GetTime());
    EXPECT_NE(0, params.initialMsTarget.GetCompact());
}

TEST_F(TestParams, Singleton_Check) {
    const Params& params2 = GetParams();
    EXPECT_EQ(&params, &params2);
}
