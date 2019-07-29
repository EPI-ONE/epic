#include <algorithm>
#include <gtest/gtest.h>
#include <random>

#include "caterpillar.h"
#include "key.h"
#include "test_env.h"

class TestConsensus : public testing::Test {
public:
    TestFactory fac;
    const std::string prefix = "test_consensus/";

    void SetUp() override {
        EpicTestEnvironment::SetUpDAG(prefix);
    }

    void TearDown() override {
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
    TimeGenerator timeGenerator{GENESIS.GetTime(), 25, 400, fac.GetRand()};

    constexpr size_t HEIGHT = 100;
    std::array<std::shared_ptr<ChainState>, HEIGHT> arrayMs;
    arrayMs[0] = GENESIS_RECORD.snapshot;
    ASSERT_EQ(0, arrayMs[0]->height);

    for (size_t i = 1; i < HEIGHT; i++) {
        auto rec   = fac.CreateConsecutiveRecordPtr(timeGenerator.NextTime());
        arrayMs[i] = fac.CreateChainStatePtr(arrayMs[i - 1], rec);
        ASSERT_EQ(i, arrayMs[i]->height);

        if (arrayMs[i]->IsDiffTransition()) {
            ASSERT_NE(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);
        } else if (i > 1 && ((i + 1) % GetParams().timeInterval) != 1) {
            ASSERT_EQ(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);
        }
        if (i > GetParams().interval + 3) {
            ASSERT_NE(0, arrayMs[i - 1]->hashRate);
        }
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
        blocks                    = std::move(chain.back());
        blocks.pop_back(); // remove milestone such that all blocks will stay in pending
    }

    spdlog::info("Number of blocks to be added: {}", blocks.size());

    // Shuffle order of blocks to make some of them not solid
    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(blocks), std::end(blocks), rng);

    ///////////////////////////
    // Test starts here
    //
    CAT->EnableOBC();

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
    constexpr int chain_length = 5;
    constexpr int n_branches   = 5;

    std::vector<TestChain> branches;
    std::vector<std::vector<NodeRecord>> branches_rec;
    branches.reserve(n_branches);
    branches.reserve(n_branches);

    auto [chain, vMsRec] = fac.CreateChain(GENESIS_RECORD, chain_length);
    // fac.PrintChain(chain);
    branches.emplace_back(std::move(chain));
    branches_rec.emplace_back(std::move(vMsRec));

    for (int i = 1; i < n_branches; ++i) {
        // randomly pick a branch and fork it at random height
        auto& picked_chain    = branches_rec[fac.GetRand() % branches_rec.size()];
        auto& new_split_point = picked_chain[fac.GetRand() % (chain_length - 3)];

        auto [chain, vMsRec] = fac.CreateChain(new_split_point, chain_length);
        // fac.PrintChain(chain);

        branches.emplace_back(std::move(chain));
        branches_rec.emplace_back(std::move(vMsRec));
    }

    ///////////////////////////
    // Test starts here
    //
    // spdlog::set_level(spdlog::level::debug);
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

TEST_F(TestConsensus, flush_single_chain_to_cat) {
    constexpr size_t FLUSHED = 10;
    const size_t HEIGHT      = GetParams().cacheStatesSize + FLUSHED;
    TestChain chain;
    std::tie(chain, std::ignore) = fac.CreateChain(GENESIS_RECORD, HEIGHT);

    for (size_t i = 0; i < chain.size(); i++) {
        if (i > GetParams().cacheStatesSize) {
            usleep(50000);
        }
        for (auto& blkptr : chain[i]) {
            DAG->AddNewBlock(blkptr, nullptr);
        }
    }

    usleep(50000);

    CAT->Wait();
    DAG->Wait();

    ASSERT_EQ(CAT->GetHeadHeight(), FLUSHED);
    auto chain_it = chain.cbegin();
    auto blk_it   = chain_it->begin();
    for (uint64_t height = 1; height < FLUSHED; height++) {
        auto lvs = CAT->GetLevelSetAt(height);
        ASSERT_GT(lvs.size(), 0);

        std::swap(lvs.front(), lvs.back());
        for (auto& blkptr : lvs) {
            ASSERT_EQ(**blk_it, *blkptr);
            ASSERT_TRUE(blkptr.unique());
            ASSERT_FALSE(bool(CAT->GetBlockCache(blkptr->GetHash())));
            blk_it++;
            if (blk_it == chain_it->end()) {
                chain_it++;
                blk_it = chain_it->begin();
            }
        }
    }
}

TEST_F(TestConsensus, delete_fork_and_flush_multiple_chains) {
    const size_t HEIGHT    = GetParams().cacheStatesSize - 1;
    constexpr size_t hfork = 15;
    auto [chain1, vMsRec]  = fac.CreateChain(GENESIS_RECORD, HEIGHT);

    std::array<TestChain, 4> chains;
    chains[0]                        = std::move(chain1);
    std::tie(chains[1], std::ignore) = fac.CreateChain(vMsRec[1], 1);
    std::tie(chains[2], std::ignore) = fac.CreateChain(vMsRec[hfork], HEIGHT - hfork + 5);
    std::tie(chains[3], std::ignore) = fac.CreateChain(vMsRec.back(), 5);

    // add blocks in a carefully assigned sequence
    for (int i : {0, 1, 2, 3}) {
        for (size_t j = 0; j < chains[i].size(); j++) {
            for (auto& blkptr : chains[i][j]) {
                DAG->AddNewBlock(blkptr, nullptr);
            }
        }
        if (i == 2) {
            usleep(100000);
        }
    }
    usleep(50000);

    CAT->Wait();
    DAG->Wait();

    // here we set less or equal as $chain[2] might be deleted with a small probability
    ASSERT_LE(DAG->GetChains().size(), 2);
    ASSERT_EQ(CAT->GetHeadHeight(), GetParams().cacheStatesToDelete);

    auto chain_it = chains[0].cbegin();
    auto blk_it   = chain_it->begin();
    for (uint64_t height = 1; height < GetParams().cacheStatesToDelete; height++) {
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
