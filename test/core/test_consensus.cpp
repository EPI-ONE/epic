#include <gtest/gtest.h>

#include "test-methods/block-factory.h"
#include "utxo.h"

class ConsensusTest : public testing::Test {};

BlockNet FakeBlock(int numTxInput = 0, int numTxOutput = 0, bool solve = false) {
    uint256 r1;
    r1.randomize();
    uint256 r2;
    r2.randomize();
    uint256 r3;
    r3.randomize();

    BlockNet b = Block(1, r1, r2, r3, time(nullptr), 0x1f00ffffL, 0);

    if (numTxInput || numTxOutput) {
        Transaction tx;
        int maxPos = rand() % 128;
        for (int i = 0; i < numTxInput; ++i) {
            uint256 inputH;
            inputH.randomize();
            tx.AddInput(TxInput(inputH, i % maxPos, Listing(std::vector<unsigned char>(i))));
        }

        for (int i = 0; i < numTxOutput; ++i) {
            tx.AddOutput(TxOutput(i, Listing(std::vector<unsigned char>(i))));
        }

        b.AddTransaction(tx);
    }

    if (solve) {
        b.Solve();
    }

    return b;
}

TEST_F(ConsensusTest, SyntaxChecking) {
    BlockNet b = GENESIS;
    EXPECT_TRUE(b.Verify());

    // Create a random block with bad difficulty target
    uint256 rand256;
    rand256.randomize();
    uint256 zeros;
    BlockNet block = Block(1, rand256, zeros, rand256, time(nullptr), 1, 1);
    EXPECT_FALSE(block.Verify());
}


TEST_F(ConsensusTest, BlockDagOptimalEncodingSize) {
    BlockDag b = GENESIS;
    EXPECT_EQ(VStream(b).size(), b.GetOptimalEncodingSize());

    BlockDag b1 = FakeBlock();

    // test without a tx
    EXPECT_EQ(VStream(b1).size(), b1.GetOptimalEncodingSize());

    // with a big-enough tx to test the variable-size ints (e.g., VarInt, CompactSize)
    Transaction tx;
    for (int i = 0; i < 512; ++i) {
        tx.AddInput(TxInput(Hash::GetZeroHash(), i, Tasm::Listing(VStream(i))));
        tx.AddOutput(TxOutput(i, Tasm::Listing(VStream(i))));
    }
    b1.AddTransaction(tx);
    EXPECT_EQ(VStream(b1).size(), b1.GetOptimalEncodingSize());
}

TEST_F(ConsensusTest, BlockNetOptimalEncodingSize) {
    BlockNet b = GENESIS;
    EXPECT_EQ(VStream(b).size(), b.GetOptimalEncodingSize());

    BlockNet b1 = FakeBlock();

    // test without a tx
    EXPECT_EQ(VStream(b1).size(), b1.GetOptimalEncodingSize());

    // with a big-enough tx to test the variable-size ints (e.g., VarInt, CompactSize)
    Transaction tx;
    for (int i = 0; i < 512; ++i) {
        tx.AddInput(TxInput(Hash::GetZeroHash(), i, Tasm::Listing(VStream(i))));
        tx.AddOutput(TxOutput(i, Tasm::Listing(VStream(i))));
    }
    b1.AddTransaction(tx);
    EXPECT_EQ(VStream(b1).size(), b1.GetOptimalEncodingSize());
}

TEST_F(ConsensusTest, UTXO) {
    BlockNet b  = FakeBlock(1, 67, true);
    UTXO utxo   = UTXO(b.GetTransaction()->GetOutput(66), 66);
    uint256 key = utxo.GetKey();

    arith_uint256 bHash = UintToArith256(b.GetHash());
    arith_uint256 index = arith_uint256("0x4200000000000000000000000000000000000000000000000000000000");
    EXPECT_EQ(ArithToUint256(bHash ^ index), key);
}
