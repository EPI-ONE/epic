#include <gtest/gtest.h>

#include "block.h"

class ConsensusTest : public testing::Test {};

TEST_F(ConsensusTest, SyntaxChecking) {
    Block b = GENESIS;
    EXPECT_TRUE(b.Verify());
}
