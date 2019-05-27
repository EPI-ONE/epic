#include <algorithm>
#include <gtest/gtest.h>
#include <random>

#include "block.h"
#include "caterpillar.h"
#include "chain.h"
#include "consensus.h"
#include "dag_manager.h"
#include "key.h"
#include "test_factory.h"
#include "utxo.h"

class TestConsensus : public testing::Test {
public:
    TestFactory fac;
};

TEST_F(TestConsensus, SyntaxChecking) {
    BlockNet b = GENESIS;
    EXPECT_TRUE(b.Verify());

    // Create a random block with bad difficulty target
    BlockNet block =
        BlockNet(1, fac.CreateRandomHash(), fac.CreateRandomHash(), fac.CreateRandomHash(), time(nullptr), 1, 1);
    EXPECT_FALSE(block.Verify());
}

TEST_F(TestConsensus, NodeRecordOptimalStorageEncodingSize) {
    NodeRecord bs = GENESIS_RECORD;
    EXPECT_EQ(VStream(bs).size(), bs.GetOptimalStorageSize());

    BlockNet b1 = fac.CreateBlockNet();
    NodeRecord bs1(b1);

    // test without a tx
    EXPECT_EQ(VStream(bs1).size(), bs1.GetOptimalStorageSize());

    // with a big-enough tx to test the variable-size ints (e.g., VarInt, CompactSize)
    Transaction tx;
    for (int i = 0; i < 512; ++i) {
        tx.AddInput(TxInput(Hash::GetZeroHash(), i, Tasm::Listing(VStream(i))));
        tx.AddOutput(TxOutput(i, Tasm::Listing(VStream(i))));
    }
    b1.AddTransaction(tx);
    NodeRecord bs2(b1);
    EXPECT_EQ(VStream(bs2).size(), bs2.GetOptimalStorageSize());
}

TEST_F(TestConsensus, BlockNetOptimalEncodingSize) {
    BlockNet b = GENESIS;
    EXPECT_EQ(VStream(b).size(), b.GetOptimalEncodingSize());

    BlockNet b1 = fac.CreateBlockNet();

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
    BlockNet b  = fac.CreateBlockNet(1, 67);
    UTXO utxo   = UTXO(b.GetTransaction()->GetOutputs()[66], 66);
    uint256 key = utxo.GetKey();

    arith_uint256 bHash = UintToArith256(b.GetHash());
    arith_uint256 index = arith_uint256("0x4200000000000000000000000000000000000000000000000000000000");
    EXPECT_EQ(ArithToUint256(bHash ^ index), key);
}

TEST_F(TestConsensus, MilestoneDifficultyUpdate) {
    std::array<std::shared_ptr<ChainState>, 100> arrayMs;
    arrayMs[0] = make_shared_ChainState();
    ASSERT_EQ(0, arrayMs[0]->height);

    size_t LOOPS = 100;
    for (size_t i = 1; i < LOOPS; i++) {
        arrayMs[i] = fac.CreateChainStatePtr(arrayMs[i - 1]);
        ASSERT_EQ(i, arrayMs[i]->height);

        if (((i + 1) % params.timeInterval) == 0) {
            ASSERT_NE(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);
            ASSERT_NE(arrayMs[i - 1]->milestoneTarget, arrayMs[i]->milestoneTarget);
            ASSERT_NE(arrayMs[i - 1]->blockTarget, arrayMs[i]->blockTarget);
        } else if (i > 1 && ((i + 1) % params.timeInterval) != 1) {
            ASSERT_EQ(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);
        }
        ASSERT_NE(0, arrayMs[i - 1]->hashRate);
        ASSERT_LE(arrayMs[i - 1]->chainwork, arrayMs[i]->chainwork);
    }
}

TEST_F(TestConsensus, Chain) {
    Chain chain1{};
    ASSERT_EQ(chain1.GetChainHead()->height, GENESIS_RECORD.snapshot->height);

    // construct the main chain and fork
    std::deque<ChainStatePtr> dqcs{make_shared_ChainState()};
    ConstBlockPtr forkblk;
    ChainStatePtr split;
    for (int i = 1; i < 10; i++) { // reach height 9
        dqcs.push_back(fac.CreateChainStatePtr(dqcs[i - 1]));
        if (i == 5) {
            // create a forked chain state at height 5
            auto blk = fac.CreateBlock();
            split    = dqcs[i];
            blk.SetMilestoneHash(split->GetMilestoneHash());
            blk.Solve();
            forkblk = std::make_shared<const BlockNet>(blk);
        }
    }
    Chain chain{dqcs, true}, fork{chain, forkblk};

    ASSERT_EQ(fork.GetChainHead()->height, 5);
    ASSERT_EQ(*split, *fork.GetChainHead());
}

TEST_F(TestConsensus, AddNewBlocks) {
    ///////////////////////////
    // Prepare for test data
    //
    std::size_t n = 100;
    std::vector<ConstBlockPtr> blocks;
    blocks.reserve(n);

    // Make the genesis first
    auto genesisPtr = std::make_shared<BlockNet>(GENESIS);
    blocks.emplace_back(genesisPtr);

    // Construct a fully connected and syntatical valid random graph
    ECC_Start();
    for (std::size_t i = 1; i < n; ++i) {
        BlockNet b = fac.CreateBlockNet(fac.GetRand() % 11 + 1, fac.GetRand() % 11 + 1);
        b.SetMilestoneHash(GENESIS.GetHash());
        b.SetPrevHash(blocks[rand() % i]->GetHash());
        b.SetTipHash(blocks[rand() % i]->GetHash());
        b.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());

        // Special transaction on the first registration block
        if (b.GetPrevHash() == GENESIS.GetHash()) {
            Transaction tx;
            tx.AddInput(TxInput(Hash::GetZeroHash(), UNCONNECTED));

            CKey seckey = CKey();
            seckey.MakeNewKey(true);
            CPubKey pubkey     = seckey.GetPubKey();
            uint160 pubkeyHash = Hash160<1>(pubkey.begin(), pubkey.end());
            VStream v(pubkeyHash);
            tx.AddOutput(TxOutput(ZERO_COIN, Tasm::Listing(v)));
            b.AddTransaction(tx);
        }
        b.Solve();

        blocks.emplace_back(std::make_shared<BlockNet>(b));
    }
    ECC_Stop();

    // Shuffle order of blocks to make some of them not solid
    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(blocks), std::end(blocks), rng);

    ///////////////////////////
    // Test starts here
    //
    std::string prefix = "test_consensus/";
    std::ostringstream os;
    os << time(nullptr);
    CAT = std::make_unique<Caterpillar>(prefix + os.str());

    // Initialize DB with genesis block
    CAT->StoreRecord(std::make_shared<NodeRecord>(GENESIS_RECORD));

    for (const auto& block : blocks) {
        CAT->AddNewBlock(block, nullptr);
    }

    CAT->Stop();
    DAG.Stop();

    for (const auto& blk : blocks) {
        auto bhash = blk->GetHash();
        EXPECT_TRUE(CAT->IsSolid(bhash));
        auto blkCache = CAT->GetBlockCache(bhash);
        EXPECT_TRUE(blkCache);
    }

    EXPECT_EQ(DAG.GetBestChain().GetPendingBlockCount(), blocks.size() - 1);

    CAT.reset();

    std::string cmd = "exec rm -r " + prefix;
    system(cmd.c_str());
}
