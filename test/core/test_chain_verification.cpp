#include <algorithm>
#include <gtest/gtest.h>
#include <random>

#include "caterpillar.h"
#include "chain.h"
#include "test_env.h"

static std::string prefix = "test_validation/";

class TestChainVerification : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();

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

    void AddToHistory(Chain* c, RecordPtr prec) {
        c->recordHistory_.emplace(prec->cblock->GetHash(), prec);
    }

    Chain make_chain(const std::deque<ChainStatePtr>& states, const std::vector<RecordPtr>& recs, bool ismain = false) {
        Chain chain{};
        chain.ismainchain_ = ismain;
        chain.states_ = states;
        for (const auto& pRec : recs) {
            chain.recordHistory_.emplace(pRec->cblock->GetHash(), pRec);
        }
        return chain;
    }

    std::optional<TXOC> ValidateRedemption(Chain* c, NodeRecord& record) {
        return c->ValidateRedemption(record);
    }

    std::optional<TXOC> ValidateTx(Chain* c, NodeRecord& record) {
        return c->ValidateTx(record);
    }

    bool IsValidDistance(const NodeRecord& rec, const arith_uint256& msHashRate) {
        return Chain::IsValidDistance(rec, msHashRate);
    }

    Chain* pchain;
};

TEST_F(TestChainVerification, try_syntax) {
    Chain c{};
}

TEST_F(TestChainVerification, VerifyRedemption) {
    // prepare keys and signature
    auto keypair = fac.CreateKeyPair();
    auto addr    = keypair.second.GetID();
    auto msg     = fac.GetRandomString(10);
    auto hashMsg = Hash<1>(msg.cbegin(), msg.cend());
    std::vector<unsigned char> sig;
    keypair.first.Sign(hashMsg, sig);

    // construct first registration
    const auto& ghash = GENESIS.GetHash();
    Block b1{1, ghash, ghash, ghash, fac.NextTime(), params.maxTarget.GetCompact(), 0};
    b1.AddTransaction(Transaction{addr});
    b1.Solve();
    RecordPtr firstRecord = std::make_shared<NodeRecord>(BlockNet(std::move(b1)));
    firstRecord->isRedeemed = NodeRecord::NOT_YET_REDEEMED;

    // construct redemption block
    Block b2{1, ghash, b1.GetHash(), ghash, fac.NextTime(), params.maxTarget.GetCompact(), 0};
    TxOutPoint outpoint{b1.GetHash(), 0};
    Transaction redeem{};
    redeem.AddSignedInput(outpoint, keypair.second, hashMsg, sig).AddOutput(0, addr);
    b2.AddTransaction(redeem);
    NodeRecord redemption{BlockNet{std::move(b2)}};
    redemption.prevRedemHash = b1.GetHash();

    // start testing
    Chain c{};
    AddToHistory(&c, firstRecord);
    ASSERT_TRUE(bool((ValidateRedemption(&c, redemption))));
}

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
        dqcs.push_back(fac.CreateChainStatePtr(dqcs[i - 1], recs[i - 1]));
        if (i == 5) {
            // create a forked chain state at height 5
            auto blk = fac.CreateBlock();
            split    = dqcs[i];
            blk.SetMilestoneHash(split->GetMilestoneHash());
            blk.Solve();
            forkblk = std::make_shared<const BlockNet>(blk);
        }
    }
    Chain chain = make_chain(dqcs, recs, true);
    Chain fork{chain, forkblk};

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
    EXPECT_TRUE(IsValidDistance(*goodBlockR, ms_hashrate));
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
    EXPECT_FALSE(IsValidDistance(*badBlockR, ms_hashrate));
}
