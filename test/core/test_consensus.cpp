#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <string>

#include "caterpillar.h"
#include "consensus.h"
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

    // Bad difficulty target
    Block block = Block(GetParams().version, fac.CreateRandomHash(), fac.CreateRandomHash(), fac.CreateRandomHash(),
                        uint256(), time(nullptr), 1, 1);
    block.FinalizeHash();
    EXPECT_FALSE(block.Verify());

    // Duplicated txns in a merkle tree branch
    Block block1 = fac.CreateBlock();
    auto tx      = fac.CreateTx(1, 1);
    tx.FinalizeHash();
    block1.AddTransaction(tx);
    block1.AddTransaction(tx);
    block1.Solve();
    EXPECT_FALSE(block1.Verify());

    // Duplicated txns
    Block block2 = fac.CreateBlock(1, 1);
    for (int i = 0; i < 5; ++i) {
        block2.AddTransaction(fac.CreateTx(2, 3));
    }

    block2.AddTransaction(*block2.GetTransactions()[2]);
    block2.Solve();
    EXPECT_FALSE(block2.Verify());
}

TEST_F(TestConsensus, MerkleRoot) {
    // Check that the merkle root (and thus the hash) is different
    // with different transaction orders
    Block block1 = fac.CreateBlock();
    auto block2  = block1;

    for (int i = 0; i < 10; ++i) {
        block1.AddTransaction(fac.CreateTx(2, 3));
    }
    block1.Solve();

    auto txns = block1.GetTransactions();
    txns[0].swap(txns[5]);
    block2.AddTransactions(txns);

    EXPECT_NE(block1.GetTransactions(), block2.GetTransactions());

    block2.Solve();

    EXPECT_NE(block1, block2);
}

TEST_F(TestConsensus, MilestoneDifficultyUpdate) {
    TimeGenerator timeGenerator{GENESIS.GetTime(), 25, 400, fac.GetRand()};

    constexpr size_t HEIGHT = 100;
    std::array<std::shared_ptr<ChainState>, HEIGHT> arrayMs;
    arrayMs[0] = GENESIS_RECORD.snapshot;
    ASSERT_EQ(0, arrayMs[0]->height);

    for (size_t i = 1; i < HEIGHT; i++) {
        auto rec = fac.CreateConsecutiveRecordPtr(timeGenerator.NextTime());

        // Generate some valid txns
        auto s = (i - 1) % GetParams().blockCapacity + 1;
        rec->validity.resize(s);
        memset(rec->validity.data(), NodeRecord::VALID, s);

        arrayMs[i] = fac.CreateChainStatePtr(arrayMs[i - 1], rec);
        ASSERT_EQ(i, arrayMs[i]->height);

        if (arrayMs[i]->IsDiffTransition()) {
            ASSERT_LT(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);

        } else if (i > 1 && ((i + 1) % GetParams().timeInterval) != 1) {
            ASSERT_EQ(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);

            if (!arrayMs[i - 1]->IsDiffTransition()) {
                ASSERT_LE(arrayMs[i - 1]->GetTxnsCounter(), arrayMs[i]->GetTxnsCounter());
            }
        }
        ASSERT_LE(arrayMs[i - 1]->chainwork, arrayMs[i]->chainwork);
    }

    auto chain = fac.CreateChain(GENESIS_RECORD, 100);
    std::vector<RecordPtr> mses;
    for (const auto& lvs : chain) {
        // zero out some of the lastUpdateTime to check
        // if they can be successfully recovered
        auto ms                      = lvs.back();
        ms->snapshot->lastUpdateTime = 0;
        mses.emplace_back(std::move(ms));

        for (const auto& b : lvs) {
            DAG->AddNewBlock(b->cblock, nullptr);
        }
    }

    usleep(50000);
    DAG->Wait();

    for (size_t i = 0; i < mses.size(); ++i) {
        if (mses[i]->height > 5) {
            auto lvs          = mses[i]->snapshot->GetLevelSet();
            auto recovered_ms = CreateNextChainState(mses[i - 1]->snapshot, *mses[i], std::move(lvs));
            auto expected_cs  = DAG->GetState(mses[i]->cblock->GetHash())->snapshot;
            if (*expected_cs != *recovered_ms) {
                std::cout << "expected " << std::to_string(*expected_cs) << std::endl;
                std::cout << "recovered " << std::to_string(*recovered_ms) << std::endl;
            }
            ASSERT_EQ(*expected_cs, *recovered_ms);
        }
    }
}

TEST_F(TestConsensus, AddNewBlocks) {
    ///////////////////////////
    // Prepare for test data
    //
    // Construct a fully connected and syntatical valid random graph

    std::vector<RecordPtr> blocks;
    while (blocks.size() <= 2) {
        auto chain = fac.CreateChain(GENESIS_RECORD, 1);
        blocks     = std::move(chain.back());
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

    for (auto& rec : blocks) {
        DAG->AddNewBlock(rec->cblock, nullptr);
    }

    usleep(50000);
    CAT->Stop();
    DAG->Stop();

    for (const auto& blk : blocks) {
        auto bhash = blk->cblock->GetHash();
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

    std::vector<TestRawChain> branches;
    std::vector<std::vector<RecordPtr>> branches_rec;
    branches.reserve(n_branches);
    branches.reserve(n_branches);

    auto [chain, vMsRec] = fac.CreateRawChain(GENESIS_RECORD, chain_length);
    // fac.PrintChain(chain);
    branches.emplace_back(std::move(chain));
    branches_rec.emplace_back(std::move(vMsRec));

    for (int i = 1; i < n_branches; ++i) {
        // randomly pick a branch and fork it at random height
        auto& picked_chain    = branches_rec[fac.GetRand() % branches_rec.size()];
        auto& new_split_point = picked_chain[fac.GetRand() % (chain_length - 3)];

        auto [chain, vMsRec] = fac.CreateRawChain(new_split_point, chain_length);
        // fac.PrintChain(chain);

        branches.emplace_back(std::move(chain));
        branches_rec.emplace_back(std::move(vMsRec));
    }

    ///////////////////////////
    // Test starts here
    //
    // SetLogLevel(SPDLOG_LEVEL_DEBUG);
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
    TestRawChain chain;
    std::tie(chain, std::ignore) = fac.CreateRawChain(GENESIS_RECORD, HEIGHT - 1);

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
        auto lvs = CAT->GetLevelSetBlksAt(height);
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
    auto [chain1, vMsRec]  = fac.CreateRawChain(GENESIS_RECORD, HEIGHT);

    std::array<TestRawChain, 3> chains;
    chains[0]                        = std::move(chain1);
    std::tie(chains[1], std::ignore) = fac.CreateRawChain(vMsRec[hfork], HEIGHT - hfork + 5);
    std::tie(chains[2], std::ignore) = fac.CreateRawChain(vMsRec.back(), 5);

    // add blocks in a carefully assigned sequence
    for (int i : {0, 1, 2}) {
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

    // here we set less or equal as $chain[1] might be deleted with a small probability
    ASSERT_LE(DAG->GetChains().size(), 2);
    ASSERT_EQ(CAT->GetHeadHeight(), GetParams().cacheStatesToDelete);

    auto chain_it = chains[0].cbegin();
    auto blk_it   = chain_it->begin();
    for (uint64_t height = 1; height < GetParams().cacheStatesToDelete; height++) {
        auto lvs = CAT->GetLevelSetBlksAt(height);
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
