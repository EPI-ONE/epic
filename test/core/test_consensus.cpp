#include <algorithm>
#include <gtest/gtest.h>
#include <random>

#include "caterpillar.h"
#include "key.h"
#include "test_env.h"

class TestConsensus : public testing::Test {
public:
    TestFactory fac;
    std::string prefix = "test_consensus/";

    void SetUp() {
        EpicTestEnvironment::SetUpDAG(prefix);

        // Initialize DB with genesis block
        //std::vector<RecordPtr> genesisLvs = {std::make_shared<NodeRecord>(GENESIS_RECORD)};
        //CAT->StoreRecords(genesisLvs);
        //CAT->EnableOBC();
    }

    void TearDown() {
        EpicTestEnvironment::TearDownDAG(prefix);
    }
};


TEST_F(TestConsensus, SyntaxChecking) {
    Block b = GENESIS;
    EXPECT_TRUE(b.Verify());

    // Create a random block with bad difficulty target
    Block block = Block(GetParams().version, fac.CreateRandomHash(), fac.CreateRandomHash(), fac.CreateRandomHash(),
                        time(nullptr), 1, 1);
    EXPECT_FALSE(block.Verify());
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
    ///////////////////////////
    // Prepare for test data
    //
    // Construct a fully connected and syntatical valid random graph

    std::vector<ConstBlockPtr> blocks;
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
    for (auto& blockptr : blocks) {
        DAG->AddNewBlock(blockptr, nullptr);
    }

    usleep(50000);
    CAT->Stop();
    DAG->Stop();

    for (const auto& blk : blocks) {
        auto bhash = blk->GetHash();
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
            for (const auto& blkptr : lvs) {
                DAG->AddNewBlock(blkptr, nullptr);
            }
        }
    }

    usleep(50000);
    CAT->Stop();
    DAG->Stop();

    ASSERT_EQ(DAG->GetChains().size(), n_branches);
}

TEST_F(TestConsensus, flush_to_cat) {
    constexpr size_t HEIGHT = 13;
    auto chain    = fac.CreateChain(GENESIS_RECORD, HEIGHT);

    std::vector<RecordPtr> genesisLvs = {std::make_shared<NodeRecord>(GENESIS_RECORD)};
    CAT->StoreRecords(genesisLvs);
    CAT->EnableOBC();

    for (auto& levelSet : chain) {
        for (auto& blkptr : levelSet) {
            DAG->AddNewBlock(blkptr, nullptr);
        }
    }

    usleep(50000);
    CAT->Wait();
    DAG->Wait();

    const size_t FLUSHED = HEIGHT - GetParams().cacheStatesSize;
    auto chain_it = chain.cbegin();
    auto blk_it = chain_it->begin();
    for (uint64_t height = 1; height < FLUSHED; height++) {
        auto lvs = CAT->GetLevelSetAt(height);
        ASSERT_GT(lvs.size(), 0);

        std::swap(lvs.front(), lvs.back());
        for (auto& blkptr : lvs) {
            ASSERT_EQ(**blk_it, *blkptr);
            blk_it++;
            if (blk_it == chain_it->end()) {
                chain_it++;
                blk_it = chain_it->begin();
            }
        }
    }
}
