#include <iostream>
#include <gtest/gtest.h>
#include "params.cpp"

class TestNetParams;

class ParamsTest : public testing::Test {
    public:
        Params params = TestNetParams::GetParams();
    protected:
        static void SetUpTestCase()  {
            std::cout << "Demo test begins" << std::endl;
        }
        static void TearDownTestCase() {
            std::cout << "Demo test ends" << std::endl;
        }
};


TEST_F(ParamsTest, Basic_Value_Check) {
    EXPECT_EQ(100, params.targetTPS);
}
