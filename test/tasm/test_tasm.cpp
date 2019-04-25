#include <gtest/gtest.h>
#include "tasm/tasm.hpp"

class TestTasm: public testing::Test {};

TEST_F(TestTasm, simple_listing) {
    tasm t(functors);
    tasm::listing l({ RET }, { 0 });
    EXPECT_TRUE(t.exec_listing(l));
}
