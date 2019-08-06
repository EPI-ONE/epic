#include <gtest/gtest.h>

#include "test_env.h"
#include "test_factory.h"

class TestInit : public testing::Test {};

TEST_F(TestInit, test_init_dag) {
    const std::string dir          = "test_init/";
    const uint32_t testChainHeight = GetParams().cacheStatesSize;

    file::SetDataDirPrefix(dir);
    CAT                               = std::make_unique<Caterpillar>(dir);
    DAG                               = std::make_unique<DAGManager>();
    std::vector<RecordPtr> genesisLvs = {std::make_shared<NodeRecord>(GENESIS_RECORD)};
    CAT->StoreLevelSet(genesisLvs);

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
    CAT->Stop();
    DAG.reset();
    CAT.reset();

    // restart
    CAT = std::make_unique<Caterpillar>(dir);
    DAG = std::make_unique<DAGManager>();

    ASSERT_TRUE(DAG->Init());
    ASSERT_TRUE(DAG->GetBestChain().GetStates().empty());
    ASSERT_EQ(DAG->GetBestMilestoneHeight(), 0);

    EpicTestEnvironment::TearDownDAG(dir);
}
