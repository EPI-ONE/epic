// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "block_store.h"
#include "key.h"
#include "test_env.h"
#include "vertex.h"

#include <algorithm>
#include <array>
#include <random>
#include <string>

class TestConsensus : public testing::Test {
public:
    TestFactory fac;
    Miner m{1};
    const std::string prefix = "test_consensus/";

    void SetUp() override {
        EpicTestEnvironment::SetUpDAG(prefix);
        m.Start();
    }

    void TearDown() override {
        EpicTestEnvironment::TearDownDAG(prefix);
    }

    bool SealAndCheck(Block& b) {
        b.SetMerkle();
        b.CalculateOptimalEncodingSize();
        m.Solve(b);
        return b.Verify();
    }
};

TEST_F(TestConsensus, SyntaxChecking) {
    Block b = fac.CreateBlock(1, 1, true);
    EXPECT_TRUE(b.Verify());

    // Wrong version
    Block wrong_version =
        Block(GetParams().version + 1, fac.CreateRandomHash(), fac.CreateRandomHash(), fac.CreateRandomHash(),
              uint256(), time(nullptr), GENESIS_VERTEX->snapshot->blockTarget.GetCompact(), 0);
    wrong_version.FinalizeHash();
    EXPECT_FALSE(wrong_version.Verify());

    // Bad proof size
    Block bad_pf_size = fac.CreateBlock();
    bad_pf_size.SetProof(std::vector<uint32_t>(GetParams().cycleLen + 1));
    m.Solve(bad_pf_size);
    EXPECT_FALSE(bad_pf_size.Verify());

    // Bad difficulty target
    Block bad_target = Block(GetParams().version, fac.CreateRandomHash(), fac.CreateRandomHash(),
                             fac.CreateRandomHash(), uint256(), time(nullptr), 0, 1);
    bad_target.FinalizeHash();
    EXPECT_FALSE(bad_target.Verify());

    // Invalid pow
    Block invalid_pow = fac.CreateBlock();
    invalid_pow.SetDifficultyTarget(((arith_uint256)(GetParams().maxTarget / 10000000)).GetCompact());
    invalid_pow.FinalizeHash();
    EXPECT_FALSE(invalid_pow.Verify());

    // Duplicated txns in a merkle tree branch
    Block dup_txns_merkle = fac.CreateBlock();
    auto dup_tx           = fac.CreateTx(1, 1);
    dup_tx.FinalizeHash();
    dup_txns_merkle.AddTransaction(dup_tx);
    dup_txns_merkle.AddTransaction(dup_tx);
    EXPECT_FALSE(SealAndCheck(dup_txns_merkle));

    // Bad merkle root
    Block bad_merkle = fac.CreateBlock(1, 1);
    bad_merkle.SetMerkle(fac.CreateRandomHash());
    m.Solve(bad_merkle);
    EXPECT_FALSE(bad_merkle.Verify());

    // Too advanced in time
    Block too_advanced = fac.CreateBlock();
    too_advanced.SetTime(time(nullptr) + ALLOWED_TIME_DRIFT + 1);
    EXPECT_FALSE(SealAndCheck(too_advanced));

    // Too many txns
    Block exceeds_cap = fac.CreateBlock(1, 1, false, GetParams().blockCapacity + 1);
    EXPECT_FALSE(SealAndCheck(exceeds_cap));

    // Too big
    Block too_big = fac.CreateBlock();
    Transaction big_tx;
    big_tx.AddInput(TxInput(fac.CreateRandomHash(), 0, 0, Tasm::Listing(std::vector<unsigned char>(MAX_BLOCK_SIZE))));
    big_tx.AddOutput(TxOutput(0, Tasm::Listing(std::vector<unsigned char>(1))));
    big_tx.FinalizeHash();
    too_big.AddTransaction(std::move(big_tx));
    EXPECT_FALSE(SealAndCheck(too_big));

    // Empty inputs in txn (empty output is nothing different)
    Block empty_in = fac.CreateBlock();
    empty_in.AddTransaction(fac.CreateTx(0, 1));
    EXPECT_FALSE(SealAndCheck(empty_in));

    // double spending
    Block double_spent = fac.CreateBlock();
    Transaction tx1    = fac.CreateTx(1, 1);
    tx1.AddInput(TxInput(TxOutPoint(tx1.GetInputs()[0].outpoint), Tasm::Listing(std::vector<unsigned char>(1))));
    tx1.FinalizeHash();
    double_spent.AddTransaction(tx1);
    EXPECT_FALSE(SealAndCheck(double_spent));

    // zero output
    Block zero_out            = fac.CreateBlock();
    Transaction tx2           = fac.CreateTx(1, 1);
    tx2.GetOutputs()[0].value = 0;
    tx2.FinalizeHash();
    zero_out.AddTransaction(tx2);
    EXPECT_FALSE(SealAndCheck(zero_out));

    // Duplicated txns
    Block dup_txns = fac.CreateBlock(1, 1);
    for (int i = 0; i < 5; ++i) {
        dup_txns.AddTransaction(fac.CreateTx(2, 3));
    }
    dup_txns.AddTransaction(*dup_txns.GetTransactions()[2]);
    EXPECT_FALSE(SealAndCheck(dup_txns));

    // Empty first reg
    Block empty_reg = fac.CreateBlock();
    empty_reg.SetPrevHash(GENESIS->GetHash());
    m.Solve(empty_reg);
    EXPECT_FALSE(empty_reg.Verify());

    // Bad first reg
    Block bad_reg = fac.CreateBlock(1, 1);
    bad_reg.SetPrevHash(GENESIS->GetHash());
    m.Solve(bad_reg);
    EXPECT_FALSE(bad_reg.Verify());
}

TEST_F(TestConsensus, MerkleRoot) {
    // Check that the merkle root (and thus the hash) is different
    // with different transaction orders
    Block block1 = fac.CreateBlock();
    auto block2  = block1;

    for (int i = 0; i < 10; ++i) {
        block1.AddTransaction(fac.CreateTx(2, 3));
    }
    block1.SetMerkle();
    block1.CalculateOptimalEncodingSize();
    m.Solve(block1);

    auto txns = block1.GetTransactions();
    txns[0].swap(txns[5]);
    block2.AddTransactions(std::move(txns));
    block2.SetMerkle();

    EXPECT_NE(block1.GetTransactions(), block2.GetTransactions());

    m.Solve(block2);

    EXPECT_NE(block1, block2);
}

TEST_F(TestConsensus, MilestoneDifficultyUpdate) {
    TimeGenerator timeGenerator{GENESIS->GetTime(), 25, 400, fac.GetRand()};

    constexpr size_t HEIGHT = 100;
    std::array<std::shared_ptr<Milestone>, HEIGHT> arrayMs;
    arrayMs[0] = GENESIS_VERTEX->snapshot;
    ASSERT_EQ(0, arrayMs[0]->height);

    for (size_t i = 1; i < HEIGHT; i++) {
        auto vtx = fac.CreateConsecutiveVertexPtr(timeGenerator.NextTime(), m);

        // Generate some "valid" txns
        auto s = (i - 1) % GetParams().blockCapacity + 1;
        vtx->validity.resize(s);
        memset(vtx->validity.data(), Vertex::VALID, s);

        arrayMs[i] = fac.CreateMilestonePtr(arrayMs[i - 1], vtx);
        ASSERT_EQ(i, arrayMs[i]->height);

        if (arrayMs[i]->IsDiffTransition()) {
            ASSERT_LT(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);

        } else if (i > 1 && ((i + 1) % GetParams().timeInterval) != 1) {
            ASSERT_EQ(arrayMs[i - 1]->lastUpdateTime, arrayMs[i]->lastUpdateTime);

            if (!arrayMs[i - 1]->IsDiffTransition()) {
                ASSERT_EQ(arrayMs[i]->GetTxnsCounter(), arrayMs[i - 1]->GetTxnsCounter() + s);
            }
        }
        ASSERT_LE(arrayMs[i - 1]->chainwork, arrayMs[i]->chainwork);
    }

    auto chain = fac.CreateChain(GENESIS_VERTEX, HEIGHT);
    std::vector<VertexPtr> mses;
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
            auto recovered_ms = CreateNextMilestone(mses[i - 1]->snapshot, *mses[i], std::move(lvs));
            auto expected_cs  = DAG->GetMsVertex(mses[i]->cblock->GetHash())->snapshot;
            ASSERT_EQ(*expected_cs, *recovered_ms);

            if (mses[i]->height > STORE->GetHeadHeight()) {
                ASSERT_EQ(expected_cs->chainwork, recovered_ms->chainwork);
            }
        }
    }
}

TEST_F(TestConsensus, AddNewBlocks) {
    ///////////////////////////
    // Prepare for test data
    //
    // Construct a fully connected and syntactical valid random graph

    std::vector<VertexPtr> blocks;
    auto chain = fac.CreateChain(GENESIS_VERTEX, 1000);
    for (auto& lvs : chain) {
        for (auto& b : lvs) {
            blocks.emplace_back(std::move(b));
        }
    }

    spdlog::info("Number of blocks to be added: {}", blocks.size());

    // Shuffle order of blocks to make some of them not solid
    std::random_device rd;
    std::mt19937_64 g(rd());
    std::shuffle(std::begin(blocks), std::end(blocks), g);

    ///////////////////////////
    // Test starts here
    //
    STORE->EnableOBC();

    for (auto& vtx : blocks) {
        DAG->AddNewBlock(vtx->cblock, nullptr);
    }

    usleep(50000);
    STORE->Wait();
    STORE->Stop();
    DAG->Stop();

    for (const auto& blk : blocks) {
        auto bhash = blk->cblock->GetHash();
        ASSERT_TRUE(STORE->DAGExists(bhash));
    }

    EXPECT_TRUE(STORE->GetOBC().Empty());
}

TEST_F(TestConsensus, AddForks) {
    // Construct a fully connected graph with main chain and forks
    constexpr int chain_length = 5;
    constexpr int n_branches   = 5;

    std::vector<TestRawChain> branches;
    std::vector<std::vector<VertexPtr>> branches_vtx;
    branches.reserve(n_branches);
    branches.reserve(n_branches);

    auto [chain, vMsVtx] = fac.CreateRawChain(GENESIS_VERTEX, chain_length);
    // fac.PrintChain(chain);
    branches.emplace_back(std::move(chain));
    branches_vtx.emplace_back(std::move(vMsVtx));

    for (int i = 1; i < n_branches; ++i) {
        // randomly pick a branch and fork it at random height
        auto& picked_chain    = branches_vtx[fac.GetRand() % branches_vtx.size()];
        auto& new_split_point = picked_chain[fac.GetRand() % (chain_length - 3)];

        auto [chain, vMsVtx] = fac.CreateRawChain(new_split_point, chain_length);
        // fac.PrintChain(chain);

        branches.emplace_back(std::move(chain));
        branches_vtx.emplace_back(std::move(vMsVtx));
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
    STORE->Stop();
    DAG->Stop();

    ASSERT_EQ(DAG->GetChains().size(), n_branches);
}

TEST_F(TestConsensus, flush_single_chain_to_cat) {
    constexpr size_t FLUSHED = 10;
    const size_t HEIGHT      = GetParams().punctualityThred + FLUSHED;
    TestRawChain chain;
    std::tie(chain, std::ignore) = fac.CreateRawChain(GENESIS_VERTEX, HEIGHT);

    for (size_t i = 0; i < chain.size(); i++) {
        if (i > GetParams().punctualityThred) {
            usleep(50000);
        }
        for (auto& blkptr : chain[i]) {
            DAG->AddNewBlock(blkptr, nullptr);
        }
    }

    usleep(50000);

    STORE->Wait();
    DAG->Wait();

    ASSERT_EQ(STORE->GetHeadHeight(), FLUSHED);
    auto chain_it = chain.cbegin();
    auto blk_it   = chain_it->begin();
    for (uint64_t height = 1; height < FLUSHED; height++) {
        auto lvs = STORE->GetLevelSetBlksAt(height);
        ASSERT_GT(lvs.size(), 0);

        std::swap(lvs.front(), lvs.back());
        for (auto& blkptr : lvs) {
            ASSERT_EQ(**blk_it, *blkptr);
            ASSERT_TRUE(blkptr.unique());
            ASSERT_FALSE(bool(STORE->GetBlockCache(blkptr->GetHash())));
            blk_it++;
            if (blk_it == chain_it->end()) {
                chain_it++;
                blk_it = chain_it->begin();
            }
        }
    }
}

TEST_F(TestConsensus, delete_fork_and_flush_multiple_chains) {
    const size_t HEIGHT    = GetParams().punctualityThred + 3;
    constexpr size_t hfork = 15;
    auto [chain1, vMsVtx]  = fac.CreateRawChain(GENESIS_VERTEX, HEIGHT);

    std::array<TestRawChain, 3> chains;
    chains[0]                        = std::move(chain1);
    std::tie(chains[1], std::ignore) = fac.CreateRawChain(vMsVtx[hfork], HEIGHT - hfork + 8);
    std::tie(chains[2], std::ignore) = fac.CreateRawChain(vMsVtx.back(), 3);

    // add blocks in a carefully assigned sequence
    for (int i : {0, 1, 2}) {
        for (size_t j = 0; j < chains[i].size(); j++) {
            for (auto& blkptr : chains[i][j]) {
                DAG->AddNewBlock(blkptr, nullptr);
            }
        }
        usleep(50000);
    }

    STORE->Wait();
    DAG->Wait();

    // here we set less or equal as $chain[1] might be deleted with a small probability
    ASSERT_LE(DAG->GetChains().size(), 2);
    ASSERT_EQ(DAG->GetBestChain().GetMilestones().size(), GetParams().punctualityThred);

    auto chain_it = chains[0].cbegin();
    auto blk_it   = chain_it->begin();
    for (uint64_t height = 1; height < chains[0].size() - GetParams().punctualityThred; height++) {
        auto lvs = STORE->GetLevelSetBlksAt(height);
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
