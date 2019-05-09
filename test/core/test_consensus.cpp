#include <gtest/gtest.h>
#include <random>

#include "utxo.h"
#include "chain.h"

typedef Tasm::Listing Listing;

std::shared_ptr<Block> BlockFactory(uint32_t _time) {
    auto pb = std::make_shared<Block>();
    pb->SetDifficultyTarget(params.maxTarget.GetCompact());
    pb->SetTime(_time);
    pb->Solve();
    return pb;
}

class TestConsensus : public testing::Test {};

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

TEST_F(TestConsensus, SyntaxChecking) {
    BlockNet b = GENESIS;
    EXPECT_TRUE(b.Verify());

    // Create a random block with bad difficulty target
    uint256 rand256;
    rand256.randomize();
    uint256 zeros;
    BlockNet block = Block(1, rand256, zeros, rand256, time(nullptr), 1, 1);
    EXPECT_FALSE(block.Verify());
}

TEST_F(TestConsensus, BlockDagOptimalEncodingSize) {
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

TEST_F(TestConsensus, BlockNetOptimalEncodingSize) {
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

TEST_F(TestConsensus, UTXO) {
    BlockNet b  = FakeBlock(1, 67, true);
    UTXO utxo   = UTXO(b.GetTransaction()->GetOutput(66), 66);
    uint256 key = utxo.GetKey();

    arith_uint256 bHash = UintToArith256(b.GetHash());
    arith_uint256 index = arith_uint256("0x4200000000000000000000000000000000000000000000000000000000");
    EXPECT_EQ(ArithToUint256(bHash ^ index), key);
}

TEST_F(TestConsensus, MilestoneDifficultyUpdate) {
    std::array<std::shared_ptr<Milestone>, 100> arrayMs;
    arrayMs[0] = std::make_shared<Milestone>();
    ASSERT_EQ(0, arrayMs[0]->height);

    uint32_t simulatedTime = 1556784921L; // arbitrarily take a starting point
    std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> distribution(1,30);

    arrayMs[1] = std::make_shared<Milestone>(BlockFactory(simulatedTime), arrayMs[0]);

    size_t LOOPS = 100;
    for (int i=2; i< LOOPS; i++) {
        simulatedTime += distribution(generator);
        arrayMs[i] = std::make_shared<Milestone>(BlockFactory(simulatedTime), arrayMs[i-1]);
        ASSERT_EQ(i, arrayMs[i]->height);
        if (((i+1) % params.timeInterval) == 0) {
            ASSERT_NE(arrayMs[i-1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);
            ASSERT_NE(arrayMs[i-1]->milestoneTarget, arrayMs[i]->milestoneTarget);
            ASSERT_NE(arrayMs[i-1]->blockTarget, arrayMs[i]->blockTarget);
        } else if ( ((i+1) % params.timeInterval) != 1){
            ASSERT_EQ(arrayMs[i-1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);
        }
        ASSERT_NE(0, arrayMs[i-1]->hashRate);
        ASSERT_LE(arrayMs[i-1]->chainwork, arrayMs[i]->chainwork);
    }
}

TEST_F(TestConsensus, Chain) {
    Chain chain1{};
    Chain chain2(false);
    ASSERT_EQ(chain1.GetChainHead()->height, chain2.GetChainHead()->height);
}
