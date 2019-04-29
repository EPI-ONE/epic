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

TEST_F(ConsensusTest, OptimalEncodingSize) {
    Block b             = GENESIS;
    size_t encoded_size = VStream(b).size();
    EXPECT_EQ(encoded_size, b.GetOptimalEncodingSize());

    Block b1 = Block(BlockHeader(1, Hash::GetZeroHash(), Hash::GetDoubleZeroHash(), Hash::GetZeroHash(), time(nullptr),
        EASIEST_COMP_DIFF_TARGET, 0));
    Transaction tx;
    // A big-enough tx to test the variable size ints.
    for (int i = 0; i < 512; ++i) {
        tx.AddInput(TxInput(Hash::GetZeroHash(), i, Script(std::vector<unsigned char>(i))));
        tx.AddOutput(TxOutput(i, Script(std::vector<unsigned char>(i))));
    }
    b1.AddTransaction(tx);
    size_t encoded_size1 = VStream(b1).size();
    EXPECT_EQ(encoded_size1, b1.GetOptimalEncodingSize());
}
