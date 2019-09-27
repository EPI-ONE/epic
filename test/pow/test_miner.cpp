// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "miner.h"
#include "test_env.h"
#include "utilstrencodings.h"

class TestMiner : public testing::Test {
    void SetUp() override {}
    void TearDown() override {}

public:
    TestFactory fac = EpicTestEnvironment::GetFactory();

    static void SetUpEnv() {
        EpicTestEnvironment::SetUpDAG("test_miner/", true);

        CKey key;
        key.MakeNewKey(false);
        auto tx = std::make_shared<Transaction>(key.GetPubKey().GetID());
        MEMPOOL->PushRedemptionTx(tx);
    }

    static void TearDownEnv() {
        EpicTestEnvironment::TearDownDAG("test_miner/");
    }
};

TEST_F(TestMiner, Solve) {
    Block block = fac.CreateBlock(1, 1);

    CPUMiner m(4);
    m.Start();
    m.Solve(block);
    m.Stop();

    EXPECT_TRUE(block.Verify());
}

#ifdef __CUDA_ENABLED__
TEST_F(TestMiner, SolveCuckaroo) {
    SetLogLevel(SPDLOG_LEVEL_DEBUG);
    SelectParams(ParamsType::TESTNET);

    Block b = fac.CreateBlock(2, 2, false, 5);

    Miner m(5);
    m.Start();
    m.Solve(b);
    m.Stop();

    ASSERT_TRUE(b.CheckPOW());

    ResetLogLevel();
    SelectParams(ParamsType::UNITTEST);
}
#endif

TEST_F(TestMiner, Run) {
    SetUpEnv();

    CPUMiner m(2);
    m.Run();
    usleep(500000);
    m.Stop();

    DAG->Stop();

    ASSERT_TRUE(m.GetSelfChainHead());
    ASSERT_TRUE(DAG->GetBestChain().GetStates().size() > 1);
    ASSERT_TRUE(DAG->GetChains().size() == 1);

    TearDownEnv();
}

TEST_F(TestMiner, Restart) {
    SetUpEnv();

    CPUMiner m(2);
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

    TearDownEnv();
}
