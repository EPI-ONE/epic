#include <algorithm>
#include <gtest/gtest.h>
#include <random>

#include "caterpillar.h"
#include "chain.h"
#include "test_factory.h"

static std::string prefix = "test_validation/";

class TestImplChain : public Chain {
public:
    static bool IsValidDistance(const NodeRecord& rec, const arith_uint256& msHashRate) {
        return Chain::IsValidDistance(rec, msHashRate);
    }
};

class TestChainVerification : public testing::Test {
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

private:
    Chain chain;
};

TEST_F(TestChainVerification, ChainForking) {
    Chain chain1{};
    Chain chain2{false};
    ASSERT_EQ(chain1.GetChainHead()->height, chain2.GetChainHead()->height);

    // construct the main chain and fork
    std::deque<ChainStatePtr> dqcs{make_shared_ChainState()};
    std::vector<RecordPtr> recs{};
    ConstBlockPtr forkblk;
    ChainStatePtr split;
    for (int i = 1; i < 10; i++) { // reach height 9
        recs.emplace_back(fac.CreateConsecutiveRecordPtr());
        dqcs.push_back(fac.CreateChainStatePtr(dqcs[i - 1], recs[i-1]));
        if (i == 5) {
            // create a forked chain state at height 5
            auto blk = fac.CreateBlock();
            split    = dqcs[i];
            blk.SetMilestoneHash(split->GetMilestoneHash());
            blk.Solve();
            forkblk = std::make_shared<const BlockNet>(blk);
        }
    }
    Chain chain{dqcs, recs, true}, fork{chain, forkblk};

    ASSERT_EQ(fork.GetChainHead()->height, 5);
    ASSERT_EQ(*split, *fork.GetChainHead());
}

TEST_F(TestChainVerification, ValidDistanceNormalChain) {
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
    EXPECT_TRUE(TestImplChain::IsValidDistance(*goodBlockR, ms_hashrate));
}

TEST_F(TestChainVerification, ValidDistanceMaliciousChain) {
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
    EXPECT_FALSE(TestImplChain::IsValidDistance(*badBlockR, ms_hashrate));
}
