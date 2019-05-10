#include <algorithm>
#include <gtest/gtest.h>
#include <random>

#include "caterpillar.h"
#include "chain.h"
#include "test_factory.h"

static std::string prefix = "test_validation/";

class TestValidation : public testing::Test {
public:
    TestFactory fac;

    static void SetUpTestCase() {
        std::ostringstream os;
        os << time(nullptr);
        std::string filename = prefix + os.str();
        CAT                  = std::make_unique<Caterpillar>(filename);
    }

    static void TearDownTestCase() {
        std::string cmd = "exec rm -r " + prefix;
        system(cmd.c_str());
        CAT.reset();
    }
};

TEST_F(TestValidation, ValidDistanceNormalChain) {
    auto genesisPtr = std::make_shared<BlockNet>(GENESIS);

    BlockNet registration = fac.CreateBlockNet(1, 1);
    registration.SetMilestoneHash(GENESIS.GetHash());
    registration.SetPrevHash(genesisPtr->GetHash());
    registration.SetTipHash(genesisPtr->GetHash());
    registration.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());
    registration.SetTime(1);

    auto registrationPtr      = std::make_shared<BlockNet>(registration);
    NodeRecord registrationNR = NodeRecord(registrationPtr);
    RecordPtr registrationR   = std::make_shared<NodeRecord>(registrationNR);


    BlockNet goodBlock = fac.CreateBlockNet(170, 170);
    goodBlock.SetMilestoneHash(GENESIS.GetHash());
    goodBlock.SetPrevHash(registrationPtr->GetHash());
    goodBlock.SetTipHash(registrationPtr->GetHash());
    goodBlock.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());

    auto goodBlockPtr      = std::make_shared<BlockNet>(goodBlock);
    NodeRecord goodBlockNR = NodeRecord(goodBlockPtr);
    RecordPtr goodBlockR   = std::make_shared<NodeRecord>(goodBlockNR);

    CAT->StoreRecord(std::make_shared<NodeRecord>(GENESIS_RECORD));
    CAT->StoreRecord(registrationR);
    CAT->StoreRecord(goodBlockR);

    arith_uint256 ms_hashrate = 1;
    EXPECT_TRUE(Chain::IsValidDistance(goodBlockR, ms_hashrate));
}

TEST_F(TestValidation, ValidDistanceMaliciousChain) {
    auto genesisPtr = std::make_shared<BlockNet>(GENESIS);

    BlockNet registration = fac.CreateBlockNet(1, 1);
    registration.SetMilestoneHash(GENESIS.GetHash());
    registration.SetPrevHash(genesisPtr->GetHash());
    registration.SetTipHash(genesisPtr->GetHash());
    registration.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());
    registration.SetTime(666);

    auto registrationPtr      = std::make_shared<BlockNet>(registration);
    NodeRecord registrationNR = NodeRecord(registrationPtr);
    RecordPtr registrationR   = std::make_shared<NodeRecord>(registrationNR);

    BlockNet goodBlock = fac.CreateBlockNet(170, 170);
    goodBlock.SetMilestoneHash(GENESIS.GetHash());
    goodBlock.SetPrevHash(registrationPtr->GetHash());
    goodBlock.SetTipHash(registrationPtr->GetHash());
    goodBlock.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());

    auto goodBlockPtr      = std::make_shared<BlockNet>(goodBlock);
    NodeRecord goodBlockNR = NodeRecord(goodBlockPtr);
    RecordPtr goodBlockR   = std::make_shared<NodeRecord>(goodBlockNR);

    BlockNet badBlock = fac.CreateBlockNet(2, 2);
    badBlock.SetMilestoneHash(GENESIS.GetHash());
    badBlock.SetPrevHash(goodBlockPtr->GetHash());
    badBlock.SetTipHash(goodBlockPtr->GetHash());
    badBlock.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());

    auto badBlockPtr      = std::make_shared<BlockNet>(badBlock);
    NodeRecord badBlockNR = NodeRecord(goodBlockPtr);
    RecordPtr badBlockR   = std::make_shared<NodeRecord>(badBlockNR);

    CAT->StoreRecord(std::make_shared<NodeRecord>(GENESIS_RECORD));
    CAT->StoreRecord(registrationR);
    CAT->StoreRecord(goodBlockR);
    CAT->StoreRecord(badBlockR);

    arith_uint256 ms_hashrate = 9999;
    EXPECT_FALSE(Chain::IsValidDistance(badBlockR, ms_hashrate));
}
