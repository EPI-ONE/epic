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
        std::vector<RecordPtr> genesisLvs = {std::make_shared<NodeRecord>(GENESIS_RECORD)};
        CAT->StoreRecords(genesisLvs);
        CAT->EnableOBC();
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
        auto [chain, placeholder] = fac.CreateChain(GENESIS_RECORD, 2);
        blocks = std::move(chain.back());
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
    constexpr int chain_length = 11;
    constexpr int n_branches  = 10;

    std::vector<TestChain> branches;
    std::vector<std::vector<NodeRecord>> branches_rec;
    branches.reserve(n_branches);
    branches.reserve(n_branches);

    auto [chain, vMsRec] = fac.CreateChain(GENESIS_RECORD, chain_length);
    //fac.PrintChain(chain);
    branches.emplace_back(std::move(chain));
    branches_rec.emplace_back(std::move(vMsRec));

    for (int i = 1; i < n_branches; ++i) {
        // randomly pick a branch and fork it at random height
        auto chain_id         = fac.GetRand() % branches_rec.size();
        auto& picked_chain    = branches_rec[chain_id];
        int luck_draw = fac.GetRand() % (picked_chain.size() - 2) + 1;
        auto& new_split_point = picked_chain[luck_draw > 5 ? 5 : luck_draw];

        auto [chain, vMsRec] = fac.CreateChain(new_split_point, chain_length);
        //fac.PrintChain(chain);

        branches.emplace_back(std::move(chain));
        branches_rec.emplace_back(std::move(vMsRec));
    }

    ///////////////////////////
    // Test starts here
    //
    //spdlog::set_level(spdlog::level::trace);
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
    constexpr size_t FLUSHED = 10;
    const size_t HEIGHT      = GetParams().cacheStatesSize + FLUSHED;
    TestChain chain;
    std::tie(chain, std::ignore) = fac.CreateChain(GENESIS_RECORD, HEIGHT);

    for (auto& levelSet : chain) {
        for (auto& blkptr : levelSet) {
            DAG->AddNewBlock(blkptr, nullptr);
        }
    }

    usleep(50000);
    CAT->Wait();
    DAG->Wait();

    auto chain_it = chain.cbegin();
    auto blk_it   = chain_it->begin();
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
