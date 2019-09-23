// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "block_store.h"
#include "chain.h"
#include "test_env.h"

#include <algorithm>

class TestChainVerification : public testing::Test {
public:
    TestFactory fac          = EpicTestEnvironment::GetFactory();
    const std::string prefix = "test_validation/";

    void SetUp() override {
        EpicTestEnvironment::SetUpDAG(prefix);
    }

    void TearDown() override {
        EpicTestEnvironment::TearDownDAG(prefix);
    }

    void AddToHistory(Chain* c, VertexPtr pvtx) {
        c->recentHistory_.emplace(pvtx->cblock->GetHash(), pvtx);
    }

    void AddToLedger(Chain* c, ChainLedger&& ledger) {
        c->ledger_ = ledger;
    }

    std::unique_ptr<Chain> make_chain(const ConcurrentQueue<MilestonePtr>& states,
                                      const std::vector<VertexPtr>& vtcs,
                                      bool ismain = false) {
        auto chain          = std::make_unique<Chain>();
        chain->ismainchain_ = ismain;
        chain->states_      = states;
        for (const auto& pVtx : vtcs) {
            chain->recentHistory_.emplace(pVtx->cblock->GetHash(), pVtx);
        }
        return chain;
    }

    std::optional<TXOC> ValidateRedemption(Chain* c, Vertex& vertex, RegChange& regChange) {
        return c->ValidateRedemption(vertex, regChange);
    }

    TXOC ValidateTx(Chain* c, Vertex& vertex) {
        return c->ValidateTxns(vertex);
    }

    bool IsValidDistance(Chain* c, Vertex& vtx, uint64_t msHashRate) {
        c->CheckTxPartition(vtx, msHashRate);
        for (size_t i = 0; i < vtx.validity.size(); ++i) {
            if (vtx.validity[i] == Vertex::Validity::INVALID) {
                return false;
            }
        }

        return true;
    }

    VertexPtr GetVertex(Chain* c, const uint256& h) {
        return c->GetVertex(h);
    }
};

TEST_F(TestChainVerification, chain_with_genesis) {
    ASSERT_EQ(DAG->GetMilestoneHead()->height, 0);
    ASSERT_EQ(DAG->GetMilestoneHead()->snapshot->GetLevelSet().size(), 1);
    ASSERT_EQ(*DAG->GetMilestoneHead()->snapshot->GetLevelSet()[0].lock()->cblock, *GENESIS);
    ASSERT_EQ(*DAG->GetBestChain().GetVertex(GENESIS->GetHash()), *GENESIS_VERTEX);
}

TEST_F(TestChainVerification, UTXO) {
    Block b     = fac.CreateBlock(1, 67);
    UTXO utxo   = UTXO(b.GetTransactions()[0]->GetOutputs()[66], 0, 66);
    uint256 key = utxo.GetKey();

    arith_uint256 bHash = UintToArith256(b.GetHash());
    arith_uint256 index = arith_uint256("0x42000000000000000000000000000000000000000000000000");
    EXPECT_EQ(ArithToUint256(bHash ^ 0 ^ index), key);
}

TEST_F(TestChainVerification, verify_with_redemption_and_reward) {
    // prepare keys and signature
    auto keypair        = fac.CreateKeyPair();
    const auto addr     = keypair.second.GetID();
    auto [hashMsg, sig] = fac.CreateSig(keypair.first);

    // chain configuration
    constexpr size_t HEIGHT = 30;
    std::array<VertexPtr, HEIGHT> vtcs;
    std::array<uint256, HEIGHT> hashes;
    std::array<bool, HEIGHT> isRedemption{};
    std::array<bool, HEIGHT> isMilestone{};
    isRedemption.fill(false);
    isMilestone.fill(false);

    NumberGenerator numGen{fac.GetRand(), 1, 10};
    uint32_t redeemRand{numGen.GetRand()}, redeemCnt{0};
    uint32_t msRand{numGen.GetRand()}, msCnt{0};
    for (size_t i = 0; i < HEIGHT; i++) {
        if (redeemRand == redeemCnt) {
            isRedemption[i] = true;
            redeemCnt       = 0;
            redeemRand      = numGen.GetRand();
        } else {
            redeemCnt++;
        }
        if (msRand == msCnt) {
            isMilestone[i] = true;
            msCnt          = 0;
            msRand         = numGen.GetRand();
        } else {
            msCnt++;
        }
    }

    // construct first registration
    const auto& ghash = GENESIS->GetHash();
    Block b1{1, ghash, ghash, ghash, uint256(), fac.NextTime(), GetParams().maxTarget.GetCompact(), 0};
    b1.AddTransaction(Transaction{addr});
    b1.Solve();
    ASSERT_TRUE(b1.IsFirstRegistration());
    const auto b1hash = b1.GetHash();

    // construct a chain with only redemption blocks and blocks without transaction
    Chain c{};
    c.AddPendingBlock(std::make_shared<const Block>(std::move(b1)));
    auto prevHash    = b1hash;
    auto prevRedHash = b1hash;
    auto prevMs      = GENESIS_VERTEX->snapshot;
    for (size_t i = 0; i < HEIGHT; i++) {
        Block blk{1, ghash, prevHash, ghash, uint256(), fac.NextTime(), GetParams().maxTarget.GetCompact(), 0};
        if (isRedemption[i]) {
            Transaction redeem{};
            redeem.AddInput(TxInput(TxOutPoint{prevRedHash, UNCONNECTED, UNCONNECTED}, keypair.second, hashMsg, sig))
                .AddOutput(0, addr);
            ASSERT_TRUE(redeem.IsRegistration());
            redeem.FinalizeHash();
            blk.AddTransaction(redeem);
        }

        blk.Solve();
        if (isMilestone[i]) {
            if (UintToArith256(blk.GetHash()) > prevMs->milestoneTarget) {
                blk.SetNonce(blk.GetNonce() + 1);
                blk.Solve();
            }
        }
        hashes[i] = blk.GetHash();

        prevHash = blk.GetHash();
        if (isRedemption[i]) {
            prevRedHash = blk.GetHash();
        }
        auto blkptr = std::make_shared<const Block>(std::move(blk));
        c.AddPendingBlock(blkptr);
        if (isMilestone[i]) {
            auto ms = c.Verify(blkptr);
            c.AddNewState(*ms);

            prevMs = c.GetChainHead();
            ASSERT_EQ(c.GetPendingBlockCount(), 0);
            ASSERT_EQ(prevMs->GetMilestoneHash(), prevHash);
        }
    }

    // check testing results
    auto firstRegVtx = GetVertex(&c, b1hash);
    ASSERT_EQ(firstRegVtx->minerChainHeight, 1);
    ASSERT_TRUE(firstRegVtx->cumulativeReward == 0);
    ASSERT_EQ(firstRegVtx->isRedeemed, Vertex::IS_REDEEMED);
    uint32_t lastMs = HEIGHT - 1;
    while (!isMilestone[lastMs]) {
        lastMs--;
    }
    uint32_t lastRdm = lastMs;
    while (!isRedemption[lastRdm]) {
        lastRdm--;
    }
    for (size_t i = 0; i < lastMs; i++) {
        vtcs[i] = GetVertex(&c, hashes[i]);
        ASSERT_EQ(vtcs[i]->minerChainHeight, i + 2);
        if (isRedemption[i]) {
            if (i < lastRdm) {
                ASSERT_EQ(vtcs[i]->isRedeemed, Vertex::IS_REDEEMED);
            } else {
                ASSERT_EQ(vtcs[i]->isRedeemed, Vertex::NOT_YET_REDEEMED);
            }
        } else {
            if (i > 0 && !isMilestone[i]) {
                ASSERT_TRUE(vtcs[i]->cumulativeReward == vtcs[i - 1]->cumulativeReward + GetParams().reward);
            } else if (i == 0) {
                ASSERT_TRUE(vtcs[i]->cumulativeReward == GetParams().reward);
            } else {
                ASSERT_TRUE(vtcs[i]->cumulativeReward ==
                            vtcs[i - 1]->cumulativeReward +
                                GetParams().reward * vtcs[i]->snapshot->GetLevelSet().size());
            }
        }
        if (isMilestone[i]) {
            ASSERT_TRUE(vtcs[i]->isMilestone);
        } else {
            ASSERT_FALSE(vtcs[i]->isMilestone);
        }
    }
}

TEST_F(TestChainVerification, verify_tx_and_utxo) {
    Chain c{};

    Coin valueIn{4}, valueOut1{2}, valueOut2{1};
    // prepare keys and signature
    auto key     = *DecodeSecret("KySymVGpRJzSKonDu21bSL5QVhXUhH1iU5VFKfXFuAB4w1R9ZiTx");
    auto addr    = key.GetPubKey().GetID();
    auto hashMsg = uintS<256>("4de04506f44155e2a59d2e8af4e6e15e9f50f5f0b1dc7a0742021799981180c2");
    std::vector<unsigned char> sig;
    key.Sign(hashMsg, sig);

    // construct transaction output to add into the ledger
    const auto& ghash = GENESIS->GetHash();
    auto encodedAddr  = EncodeAddress(addr);
    VStream outdata(encodedAddr);
    Tasm::Listing outputListing{Tasm::Listing{std::vector<uint8_t>{VERIFY}, outdata}};
    TxOutput output{valueIn, outputListing};

    uint32_t t = time(nullptr);
    Block b1{
        GetParams().version, ghash, ghash, ghash, uint256(), t, GENESIS_VERTEX->snapshot->blockTarget.GetCompact(), 0};
    Transaction tx1{};
    tx1.AddOutput(std::move(output));
    tx1.FinalizeHash();
    b1.AddTransaction(tx1);
    b1.Solve();
    ASSERT_NE(b1.GetChainWork(), 0);
    auto vtx1              = std::make_shared<Vertex>(std::move(b1));
    vtx1->minerChainHeight = 1;
    const auto& b1hash     = vtx1->cblock->GetHash();

    auto putxo = std::make_shared<UTXO>(vtx1->cblock->GetTransactions()[0]->GetOutputs()[0], 0, 0);
    ChainLedger ledger{
        std::unordered_map<uint256, UTXOPtr>{}, {{putxo->GetKey(), putxo}}, std::unordered_map<uint256, UTXOPtr>{}};
    AddToLedger(&c, std::move(ledger));
    AddToHistory(&c, vtx1);

    // construct an empty block
    Block b2{
        GetParams().version, ghash, b1hash, ghash, uint256(), t, GENESIS_VERTEX->snapshot->blockTarget.GetCompact(), 0};
    b2.Solve();
    Vertex vtx2{std::move(b2)};
    vtx2.minerChainHeight = 2;
    const auto& b2hash    = vtx2.cblock->GetHash();
    AddToHistory(&c, std::make_shared<Vertex>(vtx2));

    // construct another block
    Transaction tx{};
    tx.AddInput(TxInput(TxOutPoint{b1hash, 0, 0}, key.GetPubKey(), hashMsg, sig))
        .AddOutput(valueOut1, addr)
        .AddOutput(valueOut2, addr)
        .FinalizeHash();
    Block b3{GetParams().version,
             ghash,
             b2hash,
             ghash,
             uint256(),
             t + 1,
             GENESIS_VERTEX->snapshot->blockTarget.GetCompact(),
             0};
    b3.AddTransaction(tx);
    b3.Solve();
    Vertex vtx3{std::move(b3)};
    vtx3.minerChainHeight = 3;

    auto txoc{ValidateTx(&c, vtx3)};
    ASSERT_FALSE(txoc.Empty());

    auto& spent   = txoc.GetSpent();
    auto spentKey = ComputeUTXOKey(b1hash, 0, 0);
    ASSERT_EQ(spent.size(), 1);
    ASSERT_EQ(spent.count(spentKey), 1);

    auto& created = txoc.GetCreated();
    ASSERT_EQ(created.size(), 2);
    ASSERT_EQ(vtx3.fee, valueIn - valueOut1 - valueOut2);
}

TEST_F(TestChainVerification, ChainForking) {
    // construct the main chain and fork
    ConcurrentQueue<MilestonePtr> dqcs{{GENESIS_VERTEX->snapshot}};
    std::vector<VertexPtr> vtcs{};
    ConstBlockPtr forkblk;
    MilestonePtr split;
    for (int i = 1; i < 10; i++) { // reach height 9
        vtcs.emplace_back(fac.CreateConsecutiveVertexPtr(fac.NextTime()));
        dqcs.push_back(fac.CreateMilestonePtr(dqcs.back(), vtcs[i - 1]));
        if (i == 5) {
            // create a forked chain state at height 5
            auto blk = fac.CreateBlock();
            split    = dqcs.back();
            blk.SetMilestoneHash(split->GetMilestoneHash());
            blk.Solve();
            forkblk = std::make_shared<const Block>(blk);
        }
    }
    auto chain = make_chain(dqcs, vtcs, true);
    Chain fork{*chain, forkblk};

    ASSERT_EQ(fork.GetChainHead()->height, 5);
    // because we don't do any verification so there is no increment in chain height
    ASSERT_EQ(*split, *fork.GetChainHead());
}

TEST_F(TestChainVerification, CheckPartition) {
    Chain c{};
    auto ghash = GENESIS->GetHash();

    // Invalid registration block containing more than one txns
    Block reg_inv{GetParams().version,
                  ghash,
                  ghash,
                  ghash,
                  uint256(),
                  fac.NextTime(),
                  GENESIS_VERTEX->snapshot->blockTarget.GetCompact(),
                  0};
    reg_inv.AddTransaction(Transaction(CKeyID()));
    reg_inv.AddTransaction(fac.CreateTx(1, 1));
    Vertex reg_inv_vtx{reg_inv};
    reg_inv_vtx.minerChainHeight = 1;
    EXPECT_FALSE(IsValidDistance(&c, reg_inv_vtx, GENESIS_VERTEX->snapshot->hashRate));

    // Valid registration block
    Block reg{GetParams().version,
              ghash,
              ghash,
              ghash,
              uint256(),
              fac.NextTime(),
              GENESIS_VERTEX->snapshot->blockTarget.GetCompact(),
              0};
    reg.AddTransaction(Transaction(CKeyID()));
    Vertex reg_vtx{reg};
    reg_vtx.minerChainHeight = 1;
    AddToHistory(&c, std::make_shared<Vertex>(reg_vtx));
    EXPECT_TRUE(IsValidDistance(&c, reg_vtx, GENESIS_VERTEX->snapshot->hashRate));

    // Malicious blocks
    // Block with transaction but minerChainHeight not reached sortitionThreshold
    Block b1{GetParams().version,
             ghash,
             reg.GetHash(),
             ghash,
             uint256(),
             fac.NextTime(),
             GENESIS_VERTEX->snapshot->blockTarget.GetCompact(),
             0};
    b1.AddTransaction(fac.CreateTx(1, 1));
    Vertex vtx1{b1};
    vtx1.minerChainHeight = 2;
    AddToHistory(&c, std::make_shared<Vertex>(vtx1));
    EXPECT_FALSE(IsValidDistance(&c, vtx1, GENESIS_VERTEX->snapshot->hashRate));

    // Block with invalid distance
    Block b2{GetParams().version,
             ghash,
             b1.GetHash(),
             ghash,
             uint256(),
             fac.NextTime(),
             GENESIS_VERTEX->snapshot->blockTarget.GetCompact(),
             0};
    Transaction tx1 = fac.CreateTx(1, 1);
    b2.AddTransaction(tx1);
    Vertex vtx2{b2};
    vtx2.minerChainHeight = 3;
    EXPECT_FALSE(IsValidDistance(&c, vtx2, 1000000000));
}
