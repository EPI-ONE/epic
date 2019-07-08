#include <algorithm>
#include <gtest/gtest.h>
#include <random>

#include "caterpillar.h"
#include "key.h"
#include "test_env.h"

class TestConsensus : public testing::Test {
public:
    TestFactory fac;
    static std::string prefix;

    static void SetUpTestCase() {
        prefix = "test_consensus/";
    }

    void SetUp() {
        std::ostringstream os;
        os << time(nullptr) + fac.GetRand() % 100;
        file::SetDataDirPrefix(prefix + os.str());
        CAT = std::make_unique<Caterpillar>(prefix + os.str());
        DAG = std::make_unique<DAGManager>();

        // Initialize DB with genesis block
        std::vector<RecordPtr> genesisLvs = {std::make_shared<NodeRecord>(GENESIS_RECORD)};
        CAT->StoreRecords(genesisLvs);
        CAT->EnableOBC();
    }

    void TearDown() {
        DAG.reset();
        CAT.reset();
    }

    static void TearDownTestCase() {
        // Delete the temporary test data folder
        std::string cmd = "rm -r " + prefix;
        system(cmd.c_str());
    }
};

std::string TestConsensus::prefix = "";

TEST_F(TestConsensus, SyntaxChecking) {
    Block b = GENESIS;
    EXPECT_TRUE(b.Verify());

    // Create a random block with bad difficulty target
    Block block = Block(GetParams().version, fac.CreateRandomHash(), fac.CreateRandomHash(), fac.CreateRandomHash(),
                        time(nullptr), 1, 1);
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
    Block b     = fac.CreateBlock(1, 67);
    UTXO utxo   = UTXO(b.GetTransaction()->GetOutputs()[66], 66);
    uint256 key = utxo.GetKey();

    arith_uint256 bHash = UintToArith256(b.GetHash());
    arith_uint256 index = arith_uint256("0x4200000000000000000000000000000000000000000000000000000000");
    EXPECT_EQ(ArithToUint256(bHash ^ index), key);
}

TEST_F(TestConsensus, MilestoneDifficultyUpdate) {
    constexpr size_t HEIGHT = 100;
    std::array<std::shared_ptr<ChainState>, HEIGHT> arrayMs;
    arrayMs[0] = GENESIS_RECORD.snapshot;
    ASSERT_EQ(0, arrayMs[0]->height);

    for (size_t i = 1; i < HEIGHT; i++) {
        auto rec   = fac.CreateConsecutiveRecordPtr();
        arrayMs[i] = fac.CreateChainStatePtr(arrayMs[i - 1], rec);
        ASSERT_EQ(i, arrayMs[i]->height);

        if (((i + 1) % GetParams().timeInterval) == 0) {
            ASSERT_NE(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);
        } else if (i > 1 && ((i + 1) % GetParams().timeInterval) != 1) {
            ASSERT_EQ(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);
        }
        ASSERT_NE(0, arrayMs[i - 1]->hashRate);
        ASSERT_LE(arrayMs[i - 1]->chainwork, arrayMs[i]->chainwork);
    }
}

TEST_F(TestConsensus, AddNewBlocks) {
    // Construct a fully connected and syntatical valid level set without a ms
    std::vector<NodeRecord> blocks;
    while (blocks.size() <= 2) {
        blocks = fac.CreateChain(GENESIS_RECORD, 2).back();
        blocks.pop_back(); // remove milestone such that all blocks will stay in pending
    }

    spdlog::info("Number of blocks to be added: {}", blocks.size());

    // Shuffle order of blocks to make some of them not solid
    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(blocks), std::end(blocks), rng);

    ///////////////////////////
    // Test starts here
    //
    for (const auto& block : blocks) {
        auto p = block.cblock;
        DAG->AddNewBlock(p, nullptr);
    }

    usleep(50000);
    CAT->Stop();
    DAG->Stop();

    for (const auto& blk : blocks) {
        auto bhash = blk.cblock->GetHash();
        ASSERT_TRUE(CAT->DAGExists(bhash));
        auto blkCache = CAT->GetBlockCache(bhash);
        ASSERT_TRUE(blkCache);
    }

    EXPECT_EQ(DAG->GetBestChain().GetPendingBlockCount(), blocks.size());
}

TEST_F(TestConsensus, AddForks) {
    // Construct a fully connected graph with main chain and forks
    int chain_length = 20;
    int n_branches  = 10;

    std::vector<TestChain> branches;
    branches.reserve(n_branches);

    // branches[0] is the main chain
    branches.emplace_back(fac.CreateChain(GENESIS_RECORD, chain_length));

    for (int i = 0; i < n_branches - 1; ++i) {
        // randomly pick a branch and fork it at random height
        auto chain_id         = fac.GetRand() % branches.size();
        auto& picked_chain    = branches[chain_id];
        auto& new_split_point = picked_chain[fac.GetRand() % (picked_chain.size() - 2) + 1].back();
        branches.emplace_back(fac.CreateChain(new_split_point, chain_length));
    }

    ///////////////////////////
    // Test starts here
    //
    for (const auto& chain : branches) {
        for (const auto& lvs : chain) {
            for (const auto& blk : lvs) {
                auto p = blk.cblock;
                DAG->AddNewBlock(p, nullptr);
            }
        }
    }

    usleep(50000);
    CAT->Stop();
    DAG->Stop();

    ASSERT_EQ(DAG->GetChains().size(), n_branches);
}
