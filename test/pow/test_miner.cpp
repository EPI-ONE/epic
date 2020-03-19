// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "miner.h"
#include "solver_manager.h"
#include "test_env.h"
#include "utilstrencodings.h"

class TestMiner : public testing::Test {
    void SetUp() override {
        EpicTestEnvironment::SetUpDAG("test_miner/", true);

        CKey key;
        key.MakeNewKey(true);
        auto tx = std::make_shared<Transaction>(key.GetPubKey().GetID());
        MEMPOOL->PushRedemptionTx(tx);
    }
    void TearDown() override {
        EpicTestEnvironment::TearDownDAG("test_miner/");
    }

public:
    TestFactory fac = EpicTestEnvironment::GetFactory();
};

TEST_F(TestMiner, Solve) {
    Block block = fac.CreateBlock(1, 1);

    Miner m(4);
    m.Start();
    m.Solve(block);
    m.Stop();

    EXPECT_TRUE(block.Verify());
}

#ifdef __CUDA_ENABLED__
TEST_F(TestMiner, SolveCuckaroo) {
    SetLogLevel(SPDLOG_LEVEL_DEBUG);
    SelectParams(ParamsType::SPADE);

    Block b = fac.CreateBlock(2, 2, false, 5);
    SolverManager solverManager(1);
    solverManager.Start();
    auto task          = std::make_shared<SolverTask>();
    task->step         = 1;
    task->target       = b.GetTargetAsInteger();
    task->cycle_length = GetParams().cycleLen;
    task->blockHeader  = VStream(b.GetHeader());
    task->init_time    = b.GetTime();
    task->init_nonce   = 0;
    task->id           = 0;
    auto res           = solverManager.Solve(task);
    ASSERT_TRUE(res.first);
    b.SetProof(std::move(res.first->proof));
    b.SetNonce(res.first->final_nonce);
    b.SetTime(res.first->final_time);
    b.FinalizeHash();

    ASSERT_TRUE(b.CheckPOW());
    solverManager.Stop();
    ResetLogLevel();
    SelectParams(ParamsType::UNITTEST);
}
#endif

TEST_F(TestMiner, Run) {
    Miner m(2);
    m.Run();
    usleep(500000);
    m.Stop();

    DAG->Stop();

    ASSERT_TRUE(m.GetSelfChainHead());
    ASSERT_TRUE(DAG->GetBestChain()->GetMilestones().size() > 1);
    ASSERT_TRUE(DAG->GetChains().size() == 1);
}

TEST_F(TestMiner, Restart) {
    Miner m(2);
    m.Run();
    usleep(100000);
    m.Stop();

    DAG->Wait();

    auto selfChainHead = m.GetSelfChainHead();

    m.Run();
    usleep(100000);
    m.Stop();

    DAG->Stop();

    auto cursor = m.GetSelfChainHead();

    ASSERT_NE(*cursor, *selfChainHead);

    while (*cursor != *GENESIS && *cursor != *selfChainHead) {
        cursor = STORE->FindBlock(cursor->GetPrevHash());
    }

    ASSERT_EQ(*cursor, *selfChainHead);

    selfChainHead = m.GetSelfChainHead();
}
