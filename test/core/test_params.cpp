#include "params.h"
#include <gtest/gtest.h>
#include <iostream>

class ParamsTest : public testing::Test {
public:
    const Params& params = TestNetParams::GetParams();

protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
};

TEST_F(ParamsTest, Basic_Value_Check) {
    EXPECT_EQ(100, params.targetTPS);
    Block genesis = params.GetGenesisBlock();
    EXPECT_EQ(1548078136L, genesis.GetTime());
}

TEST_F(ParamsTest, Singleton_Check) {
    const Params& params2 = TestNetParams::GetParams();
    EXPECT_EQ(&params, &params2);
}
