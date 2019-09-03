// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "test_env.h"

class TestInit : public testing::Test {};

TEST_F(TestInit, test_init_dag) {
    const std::string dir          = "test_init/";
    const uint32_t testChainHeight = GetParams().cacheStatesSize;

    file::SetDataDirPrefix(dir);
    STORE                             = std::make_unique<BlockStore>(dir);
    DAG                               = std::make_unique<DAGManager>();
    std::vector<RecordPtr> genesisLvs = {std::make_shared<NodeRecord>(GENESIS_RECORD)};
    STORE->StoreLevelSet(genesisLvs);

    // validate blocks and flush to DB
    auto factory = EpicTestEnvironment::GetFactory();
    auto chain   = factory.CreateChain(GENESIS_RECORD, testChainHeight, false);
    for (auto& lvs : chain) {
        for (auto& block : lvs) {
            DAG->AddNewBlock(block->cblock, nullptr);
        }
    }
    usleep(500000);
    DAG->Wait();
    ASSERT_EQ(DAG->GetBestChain().GetChainHead()->height, chain.size());

    // shutdown
    DAG->Stop();
    STORE->Stop();
    DAG.reset();
    STORE.reset();

    // restart
    STORE = std::make_unique<BlockStore>(dir);
    DAG   = std::make_unique<DAGManager>();

    ASSERT_TRUE(DAG->Init());
    ASSERT_TRUE(DAG->GetBestChain().GetStates().empty());
    ASSERT_EQ(DAG->GetBestMilestoneHeight(), 0);

    EpicTestEnvironment::TearDownDAG(dir);
}
