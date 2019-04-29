#include <gtest/gtest.h>

#include "block.h"

class ConsensusTest : public testing::Test {};

TEST_F(ConsensusTest, SyntaxChecking) {
    Block b = GENESIS;
    EXPECT_TRUE(b.Verify());

    // Create a random block with bad difficulty target
    uint256 rand256;
    rand256.randomize();
    uint256 zeros;
    Block block = Block(BlockHeader(1, rand256, zeros, rand256, time(nullptr), 1, 1));
    EXPECT_FALSE(block.Verify());
}
