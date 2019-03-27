#include <gtest/gtest.h>
#include <iostream>
#include "params.h"

class TestNetParams;

class ParamsTest : public testing::Test {
   public:
    const Params& params = TestNetParams::GetParams();

   protected:
    static void SetUpTestCase() {
        std::cout << "Demo test begins" << std::endl;
    }
    static void TearDownTestCase() {
        std::cout << "Demo test ends" << std::endl;
    }
};

TEST_F(ParamsTest, Basic_Value_Check) {
    EXPECT_EQ(100, params.targetTPS);
    const Params& params2 = TestNetParams::GetParams();
    EXPECT_EQ(&params, &params2);
}
