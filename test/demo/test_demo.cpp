#include <iostream>
#include <gtest/gtest.h>

class Demo : public testing::Test {
    protected:
        static void SetUpTestCase()  {
            std::cout << "Demo test begins" << std::endl;
        }
        static void TearDownTestCase() {
            std::cout << "Demo test ends" << std::endl;
        }
};

TEST_F(Demo, Basic_Value_Check) {
    long l1 = 30;
    long l2 = 30;
    EXPECT_EQ(l1, l2);
}

TEST_F(Demo, STRING_VALUE_CHECK) {
    std::string s1 = "hello world";
    std::string s2 = "helloworld";
    EXPECT_STRCASENE(s1.c_str(), s2.c_str());
}
