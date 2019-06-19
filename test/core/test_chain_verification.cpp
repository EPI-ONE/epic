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

    void AddToLedger(Chain* c, ChainLedger&& ledger) {
        c->ledger_ = ledger;
    }

    Chain make_chain(const std::deque<ChainStatePtr>& states, const std::vector<RecordPtr>& recs, bool ismain = false) {
        Chain chain{};
        chain.ismainchain_ = ismain;
        chain.states_      = states;
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

    bool IsValidDistance(Chain* c, const NodeRecord& rec, const arith_uint256& msHashRate) {
        return c->IsValidDistance(rec, msHashRate);
    }

    RecordPtr GetRecord(Chain* c, const uint256& h) {
        return c->GetRecord(h);
    }
};

TEST_F(TestChainVerification, chain_with_genesis) {
    Chain c{};
    ASSERT_EQ(c.GetChainHead()->height, 0);
    ASSERT_EQ(c.GetChainHead()->GetRecordHashes().size(), 1);
    ASSERT_EQ(c.GetChainHead()->GetRecordHashes()[0], GENESIS.GetHash());
    ASSERT_EQ(*GetRecord(&c, GENESIS.GetHash()), GENESIS_RECORD);
}

TEST_F(TestChainVerification, VerifyRedemption) {
    // prepare keys and signature
    auto keypair        = fac.CreateKeyPair();
    auto addr           = keypair.second.GetID();
    auto [hashMsg, sig] = fac.CreateSig(keypair.first);

    // construct first registration
    const auto& ghash = GENESIS.GetHash();
    Block b1{1, ghash, ghash, ghash, GetParams().maxTarget.GetCompact(), fac.NextTime(), 0};
    b1.AddTransaction(Transaction{addr});
    b1.FinalizeHash();
    auto b1hash             = b1.GetHash();
    RecordPtr firstRecord   = std::make_shared<NodeRecord>(Block(std::move(b1)));
    firstRecord->isRedeemed = NodeRecord::NOT_YET_REDEEMED;

    // construct redemption block
    Transaction redeem{};
    redeem.AddSignedInput(TxOutPoint{b1hash, 0}, keypair.second, hashMsg, sig).AddOutput(0, addr);
    Block b2{1, ghash, b1hash, ghash, fac.NextTime(), GetParams().maxTarget.GetCompact(), 0};
    b2.AddTransaction(redeem);
    NodeRecord redemption{Block{std::move(b2)}};
    redemption.prevRedemHash = b1hash;

    // start testing
    Chain c{};
    AddToHistory(&c, firstRecord);
    auto txoc = ValidateRedemption(&c, redemption);
    ASSERT_TRUE(bool(txoc));
    ASSERT_EQ(firstRecord->isRedeemed, NodeRecord::IS_REDEEMED);
    ASSERT_EQ(redemption.isRedeemed, NodeRecord::NOT_YET_REDEEMED);
}

TEST_F(TestChainVerification, VerifyTx) {
    Coin valueIn{4}, valueOut1{2}, valueOut2{1};
    // prepare keys and signature
    auto keypair        = fac.CreateKeyPair();
    auto addr           = keypair.second.GetID();
    auto [hashMsg, sig] = fac.CreateSig(keypair.first);

    // construct transaction output to add into the ledger
    const auto& ghash = GENESIS.GetHash();
    auto encodedAddr  = EncodeAddress(addr);
    VStream outdata(encodedAddr);
    Tasm::Listing outputListing{Tasm::Listing{std::vector<uint8_t>{VERIFY}, outdata}};
    TxOutput output{valueIn, outputListing};
    Block b1{Block{1, ghash, ghash, ghash, GENESIS.GetTime(), GENESIS_RECORD.snapshot->blockTarget.GetCompact(), 0}};
    Transaction tx1{};
    b1.AddTransaction(tx1.AddOutput(std::move(output)));
    b1.FinalizeHash();
    ASSERT_NE(b1.GetChainWork(), 0);

    auto rec1          = std::make_shared<NodeRecord>(std::move(b1));
    const auto& b1hash = rec1->cblock->GetHash();

    Chain c{};
    DAG        = std::make_unique<DAGManager>();
    auto putxo = std::make_shared<UTXO>(rec1->cblock->GetTransaction()->GetOutputs()[0], 0);
    ChainLedger ledger{
        std::unordered_map<uint256, UTXOPtr>{}, {{putxo->GetKey(), putxo}}, std::unordered_map<uint256, UTXOPtr>{}};
    AddToLedger(&c, std::move(ledger));
    AddToHistory(&c, rec1);

    // construct block
    Transaction tx{};
    tx.AddSignedInput(TxOutPoint{b1hash, 0}, keypair.second, hashMsg, sig)
        .AddOutput(valueOut1, addr)
        .AddOutput(valueOut2, addr);
    Block b2{1, ghash, b1hash, ghash, fac.NextTime(), GENESIS_RECORD.snapshot->blockTarget.GetCompact(), 0};
    b2.AddTransaction(tx);
    NodeRecord record{Block{std::move(b2)}};

    auto txoc{ValidateTx(&c, record)};
    ASSERT_TRUE(bool(txoc));

    auto& spent   = txoc->GetTxOutsSpent();
    auto spentKey = XOR(b1hash, 0);
    ASSERT_EQ(spent.size(), 1);
    ASSERT_EQ(spent.count(spentKey), 1);

    auto& created = txoc->GetTxOutsCreated();
    ASSERT_EQ(created.size(), 2);
    ASSERT_EQ(record.fee, valueIn - valueOut1 - valueOut2);
}

TEST_F(TestChainVerification, ChainForking) {
    Chain chain1{};
    ASSERT_EQ(chain1.GetChainHead()->height, GENESIS_RECORD.snapshot->height);

    // construct the main chain and fork
    std::deque<ChainStatePtr> dqcs{std::make_shared<ChainState>()};
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
            forkblk = std::make_shared<const Block>(blk);
        }
    }
    Chain chain = make_chain(dqcs, recs, true);
    Chain fork{chain, forkblk};

    ASSERT_EQ(fork.GetChainHead()->height, 5);
    ASSERT_EQ(*split, *fork.GetChainHead());
}

TEST_F(TestChainVerification, ValidDistanceNormalChain) {
    Block registration = fac.CreateBlock(1, 1);
    registration.SetMilestoneHash(GENESIS.GetHash());
    registration.SetPrevHash(GENESIS.GetHash());
    registration.SetTipHash(GENESIS.GetHash());
    registration.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());
    registration.SetTime(GENESIS.GetTime());
    ASSERT_NE(registration.GetChainWork(), 0);

    auto registrationPtr      = std::make_shared<Block>(registration);
    NodeRecord registrationNR = NodeRecord(registrationPtr);
    RecordPtr registrationR   = std::make_shared<NodeRecord>(registrationNR);

    Block goodBlock = fac.CreateBlock(170, 170);
    goodBlock.SetMilestoneHash(GENESIS.GetHash());
    goodBlock.SetPrevHash(registrationPtr->GetHash());
    goodBlock.SetTipHash(registrationPtr->GetHash());
    goodBlock.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());

    auto goodBlockPtr      = std::make_shared<Block>(goodBlock);
    NodeRecord goodBlockNR = NodeRecord(goodBlockPtr);
    RecordPtr goodBlockR   = std::make_shared<NodeRecord>(goodBlockNR);

    CAT->StoreRecord(std::make_shared<NodeRecord>(GENESIS_RECORD));
    CAT->StoreRecord(registrationR);
    CAT->StoreRecord(goodBlockR);

    arith_uint256 ms_hashrate = 1;
    Chain c{};
    EXPECT_TRUE(IsValidDistance(&c, *goodBlockR, ms_hashrate));
}

TEST_F(TestChainVerification, ValidDistanceMaliciousChain) {
    auto genesisPtr = std::make_shared<Block>(GENESIS);

    Block registration = fac.CreateBlock(1, 1);
    registration.SetMilestoneHash(GENESIS.GetHash());
    registration.SetPrevHash(genesisPtr->GetHash());
    registration.SetTipHash(genesisPtr->GetHash());
    registration.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());
    registration.SetTime(666);

    auto registrationPtr      = std::make_shared<Block>(registration);
    NodeRecord registrationNR = NodeRecord(registrationPtr);
    RecordPtr registrationR   = std::make_shared<NodeRecord>(registrationNR);

    Block goodBlock = fac.CreateBlock(170, 170);
    goodBlock.SetMilestoneHash(GENESIS.GetHash());
    goodBlock.SetPrevHash(registrationPtr->GetHash());
    goodBlock.SetTipHash(registrationPtr->GetHash());
    goodBlock.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());

    auto goodBlockPtr      = std::make_shared<Block>(goodBlock);
    NodeRecord goodBlockNR = NodeRecord(goodBlockPtr);
    RecordPtr goodBlockR   = std::make_shared<NodeRecord>(goodBlockNR);

    Block badBlock = fac.CreateBlock(2, 2);
    badBlock.SetMilestoneHash(GENESIS.GetHash());
    badBlock.SetPrevHash(goodBlockPtr->GetHash());
    badBlock.SetTipHash(goodBlockPtr->GetHash());
    badBlock.SetDifficultyTarget(GENESIS_RECORD.snapshot->blockTarget.GetCompact());

    auto badBlockPtr      = std::make_shared<Block>(badBlock);
    NodeRecord badBlockNR = NodeRecord(goodBlockPtr);
    RecordPtr badBlockR   = std::make_shared<NodeRecord>(badBlockNR);

    CAT->StoreRecord(std::make_shared<NodeRecord>(GENESIS_RECORD));
    CAT->StoreRecord(registrationR);
    CAT->StoreRecord(goodBlockR);
    CAT->StoreRecord(badBlockR);

    arith_uint256 ms_hashrate = 9999;
    Chain c{};
    EXPECT_FALSE(IsValidDistance(&c, *badBlockR, ms_hashrate));
}
