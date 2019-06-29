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
        CAT = std::make_unique<Caterpillar>(prefix + os.str());
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
    NodeRecord redemption{std::move(b2)};
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
    DAG = std::make_unique<DAGManager>();
    Chain c{};

    Coin valueIn{4}, valueOut1{2}, valueOut2{1};
    // prepare keys and signature
    auto key     = DecodeSecret("KySymVGpRJzSKonDu21bSL5QVhXUhH1iU5VFKfXFuAB4w1R9ZiTx");
    auto addr    = key.GetPubKey().GetID();
    auto hashMsg = uint256S("4de04506f44155e2a59d2e8af4e6e15e9f50f5f0b1dc7a0742021799981180c2");
    std::vector<unsigned char> sig;
    key.Sign(hashMsg, sig);

    // construct transaction output to add into the ledger
    const auto& ghash = GENESIS.GetHash();
    auto encodedAddr  = EncodeAddress(addr);
    VStream outdata(encodedAddr);
    Tasm::Listing outputListing{Tasm::Listing{std::vector<uint8_t>{VERIFY}, outdata}};
    TxOutput output{valueIn, outputListing};

    uint32_t t = 1561117638;
    Block b1{GetParams().version, ghash, ghash, ghash, t, GENESIS_RECORD.snapshot->blockTarget.GetCompact(), 0};
    Transaction tx1{};
    b1.AddTransaction(tx1.AddOutput(std::move(output)));
    b1.Solve();
    ASSERT_NE(b1.GetChainWork(), 0);
    auto rec1              = std::make_shared<NodeRecord>(std::move(b1));
    rec1->minerChainHeight = 1;
    const auto& b1hash     = rec1->cblock->GetHash();

    auto putxo = std::make_shared<UTXO>(rec1->cblock->GetTransaction()->GetOutputs()[0], 0);
    ChainLedger ledger{
        std::unordered_map<uint256, UTXOPtr>{}, {{putxo->GetKey(), putxo}}, std::unordered_map<uint256, UTXOPtr>{}};
    AddToLedger(&c, std::move(ledger));
    AddToHistory(&c, rec1);

    // construct an empty block
    Block b2{GetParams().version, ghash, b1hash, ghash, t + 1, GENESIS_RECORD.snapshot->blockTarget.GetCompact(), 0};
    b2.Solve();
    NodeRecord rec2{std::move(b2)};
    rec2.minerChainHeight = 2;
    const auto& b2hash    = rec2.cblock->GetHash();
    AddToHistory(&c, std::make_shared<NodeRecord>(rec2));

    // construct another block
    Transaction tx{};
    tx.AddSignedInput(TxOutPoint{b1hash, 0}, key.GetPubKey(), hashMsg, sig)
        .AddOutput(valueOut1, addr)
        .AddOutput(valueOut2, addr);
    Block b3{GetParams().version, ghash, b2hash, ghash, t + 2, GENESIS_RECORD.snapshot->blockTarget.GetCompact(), 0};
    b3.AddTransaction(tx);
    b3.Solve();
    NodeRecord rec3{std::move(b3)};
    rec3.minerChainHeight = 3;

    auto txoc{ValidateTx(&c, rec3)};
    ASSERT_TRUE(bool(txoc));

    auto& spent   = txoc->GetTxOutsSpent();
    auto spentKey = XOR(b1hash, 0);
    ASSERT_EQ(spent.size(), 1);
    ASSERT_EQ(spent.count(spentKey), 1);

    auto& created = txoc->GetTxOutsCreated();
    ASSERT_EQ(created.size(), 2);
    ASSERT_EQ(rec3.fee, valueIn - valueOut1 - valueOut2);
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

TEST_F(TestChainVerification, ValidDistance) {
    // Test for block with valid distance has been done in the above test case VerifyTx.
    // Here we only test for malicious blocks.

    Chain c{};

    // Block with transaction but minerChainHeight not reached sortitionThreshold
    auto ghash = GENESIS.GetHash();
    Block b1{
        GetParams().version, ghash, ghash, ghash, fac.NextTime(), GENESIS_RECORD.snapshot->blockTarget.GetCompact(), 0};
    NodeRecord rec1{b1};
    rec1.minerChainHeight = 1;
    AddToHistory(&c, std::make_shared<NodeRecord>(rec1));

    Block b2{GetParams().version,
             ghash,
             b1.GetHash(),
             ghash,
             fac.NextTime(),
             GENESIS_RECORD.snapshot->blockTarget.GetCompact(),
             0};
    Transaction tx = fac.CreateTx(1, 1);
    b2.AddTransaction(tx);
    NodeRecord rec2{b2};
    rec2.minerChainHeight = 2;
    AddToHistory(&c, std::make_shared<NodeRecord>(rec2));
    EXPECT_FALSE(IsValidDistance(&c, rec2, GENESIS_RECORD.snapshot->hashRate));

    // Block with invalid distance
    Block b3{GetParams().version,
             ghash,
             b2.GetHash(),
             ghash,
             fac.NextTime(),
             GENESIS_RECORD.snapshot->blockTarget.GetCompact(),
             0};
    Transaction tx1 = fac.CreateTx(1, 1);
    b3.AddTransaction(tx1);
    NodeRecord rec3{b3};
    rec3.minerChainHeight = 3;
    EXPECT_FALSE(IsValidDistance(&c, rec3, 1000000000));
}
