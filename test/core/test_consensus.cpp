#include <algorithm>
#include <gtest/gtest.h>
#include <random>

#include "caterpillar.h"
#include "key.h"
#include "test_factory.h"

class TestConsensus : public testing::Test {
public:
    TestFactory fac;
};

TEST_F(TestConsensus, SyntaxChecking) {
    Block b = GENESIS;
    EXPECT_TRUE(b.Verify());

    // Create a random block with bad difficulty target
    Block block =
        Block(GetParams().version, fac.CreateRandomHash(), fac.CreateRandomHash(), fac.CreateRandomHash(), time(nullptr), 1, 1);
    EXPECT_FALSE(block.Verify());
}

TEST_F(TestConsensus, NodeRecordOptimalStorageEncodingSize) {
    NodeRecord bs = GENESIS_RECORD;
    EXPECT_EQ(VStream(bs).size(), bs.GetOptimalStorageSize());

    Block b1 = fac.CreateBlock();
    Block b2{b1};
    NodeRecord bs1{std::move(b1)};

    // test without a tx
    EXPECT_EQ(VStream(bs1).size(), bs1.GetOptimalStorageSize());

    // with a big-enough tx to test the variable-size ints (e.g. VarInt, CompactSize)
    Transaction tx;
    for (int i = 0; i < 512; ++i) {
        tx.AddInput(TxInput(Hash::GetZeroHash(), i, Tasm::Listing(VStream(i))));
        tx.AddOutput(TxOutput(i, Tasm::Listing(VStream(i))));
    }
    b2.AddTransaction(tx);
    NodeRecord bs2{std::move(b2)};
    EXPECT_EQ(VStream(bs2).size(), bs2.GetOptimalStorageSize());
}

TEST_F(TestConsensus, BlockOptimalEncodingSize) {
    Block b = GENESIS;
    EXPECT_EQ(VStream(b).size(), b.GetOptimalEncodingSize());

    Block b1 = fac.CreateBlock();

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
    Block b  = fac.CreateBlock(1, 67);
    UTXO utxo   = UTXO(b.GetTransaction()->GetOutputs()[66], 66);
    uint256 key = utxo.GetKey();

    arith_uint256 bHash = UintToArith256(b.GetHash());
    arith_uint256 index = arith_uint256("0x4200000000000000000000000000000000000000000000000000000000");
    EXPECT_EQ(ArithToUint256(bHash ^ index), key);
}

TEST_F(TestConsensus, MilestoneDifficultyUpdate) {
    std::array<std::shared_ptr<ChainState>, 100> arrayMs;
    arrayMs[0] = GENESIS_RECORD.snapshot;
    ASSERT_EQ(0, arrayMs[0]->height);

    constexpr size_t LOOPS = 100;
    for (size_t i = 1; i < LOOPS; i++) {
        auto rec = fac.CreateConsecutiveRecordPtr();
        arrayMs[i] = fac.CreateChainStatePtr(arrayMs[i - 1], rec);
        ASSERT_EQ(i, arrayMs[i]->height);

        if (((i + 1) % GetParams().timeInterval) == 0) {
            ASSERT_NE(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);
            ASSERT_NE(arrayMs[i - 1]->milestoneTarget, arrayMs[i]->milestoneTarget);
            ASSERT_NE(arrayMs[i - 1]->blockTarget, arrayMs[i]->blockTarget);
        } else if (i > 1 && ((i + 1) % GetParams().timeInterval) != 1) {
            ASSERT_EQ(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);
        }
        ASSERT_NE(0, arrayMs[i - 1]->hashRate);
        ASSERT_LE(arrayMs[i - 1]->chainwork, arrayMs[i]->chainwork);
    }
}

TEST_F(TestConsensus, AddNewBlocks) {
    ///////////////////////////
    // Prepare for test data
    //
    std::size_t n = 100;
    std::vector<ConstBlockPtr> blocks;
    blocks.reserve(n);

    // Make the genesis first
    auto genesisPtr = std::make_shared<Block>(GENESIS);
    blocks.emplace_back(genesisPtr);

    // Construct a fully connected and syntatical valid random graph
    for (std::size_t i = 1; i < n; ++i) {
        Block b = fac.CreateBlock(fac.GetRand() % 11 + 1, fac.GetRand() % 11 + 1);
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

        while (CheckMsPOW(std::make_shared<const Block>(b), GENESIS_RECORD.snapshot)) {
            b.Solve();
        }

        blocks.emplace_back(std::make_shared<Block>(b));
    }

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
    DAG = std::make_unique<DAGManager>();

    // Initialize DB with genesis block
    CAT->StoreRecord(std::make_shared<NodeRecord>(GENESIS_RECORD));

    for (const auto& block : blocks) {
        CAT->AddNewBlock(block, nullptr);
    }

    CAT->Stop();
    DAG->Stop();

    for (const auto& blk : blocks) {
        auto bhash = blk->GetHash();
        EXPECT_TRUE(CAT->IsSolid(bhash));
        auto blkCache = CAT->GetBlockCache(bhash);
        EXPECT_TRUE(blkCache);
    }

    EXPECT_EQ(DAG->GetBestChain().GetPendingBlockCount(), blocks.size() - 1);

    CAT.reset();

    std::string cmd = "exec rm -r " + prefix;
    system(cmd.c_str());

    CAT.reset();
}
