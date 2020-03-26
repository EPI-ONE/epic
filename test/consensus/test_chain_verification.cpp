// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "block_store.h"
#include "chain.h"
#include "opcodes.h"
#include "test_env.h"

#include <algorithm>

class TestChainVerification : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();
    Miner m{1};
    const std::string prefix = "test_validation/";

    void SetUp() override {
        EpicTestEnvironment::SetUpDAG(prefix);
        m.Start();
    }

    void TearDown() override {
        m.Stop();
        EpicTestEnvironment::TearDownDAG(prefix);
    }

    void AddToHistory(Chain* c, VertexPtr pvtx) {
        c->recentHistory_.emplace(pvtx->cblock->GetHash(), pvtx);
    }

    void AddToLedger(Chain* c, ChainLedger&& ledger) {
        c->ledger_ = ledger;
    }

    void UpdateLedger(Chain* c, const TXOC& txoc) {
        c->ledger_.Update(txoc);
    }

    auto& GetPrevRegHashes(Chain* c) {
        return c->prevRegsToModify_;
    }

    std::unique_ptr<Chain> make_chain(const ConcurrentQueue<MilestonePtr>& msChain,
                                      const std::vector<VertexPtr>& vtcs,
                                      bool ismain = false) {
        auto chain          = std::make_unique<Chain>();
        chain->ismainchain_ = ismain;
        chain->milestones_  = msChain;
        for (const auto& pVtx : vtcs) {
            chain->recentHistory_.emplace(pVtx->cblock->GetHash(), pVtx);
        }
        return chain;
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
    ASSERT_EQ(*DAG->GetBestChain()->GetVertex(GENESIS->GetHash()), *GENESIS_VERTEX);
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
    // Prepare keys and signature
    auto keypair        = fac.CreateKeyPair();
    const auto addr     = keypair.second.GetID();
    auto [hashMsg, sig] = fac.CreateSig(keypair.first);

    // Chain configuration
    constexpr size_t HEIGHT = 30;
    std::array<VertexPtr, HEIGHT> vtcs;
    std::array<int, HEIGHT> lvsSizes;
    std::array<uint256, HEIGHT> hashes;
    std::array<bool, HEIGHT> isRedemption{};
    std::array<bool, HEIGHT> isMilestone{};
    Coin baseRewardIssued{};
    Coin baseRewardCalculated{};
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

    // Construct first registration
    const auto& ghash = GENESIS->GetHash();
    Block b1{1, ghash, ghash, ghash, uint256(), fac.NextTime(), GetParams().maxTarget.GetCompact(), 0};
    b1.AddTransaction(Transaction{addr});
    b1.SetMerkle();
    b1.CalculateOptimalEncodingSize();
    m.Solve(b1);
    ASSERT_TRUE(b1.IsFirstRegistration());
    ASSERT_TRUE(b1.IsRegistration());
    const auto b1hash = b1.GetHash();

    // Construct a chain with only redemption blocks and blocks without transaction
    Chain c{};
    c.AddPendingBlock(std::make_shared<const Block>(std::move(b1)));
    auto prevHash    = b1hash;
    auto prevRedHash = b1hash;
    auto prevMs      = GENESIS_VERTEX->snapshot;
    for (size_t i = 0; i < HEIGHT; i++) {
        Block blk{GetParams().version,
                  ghash,
                  prevHash,
                  ghash,
                  uint256(),
                  fac.NextTime(),
                  GetParams().maxTarget.GetCompact(),
                  0};
        if (isRedemption[i]) {
            Transaction redeem{};
            redeem.AddInput(TxInput(TxOutPoint{prevRedHash, UNCONNECTED, UNCONNECTED}, keypair.second, hashMsg, sig))
                .AddOutput(0, addr);
            ASSERT_TRUE(redeem.IsRegistration());
            redeem.FinalizeHash();
            blk.AddTransaction(redeem);
            blk.SetMerkle();
        }

        blk.CalculateOptimalEncodingSize();
        m.Solve(blk);
        if (isMilestone[i]) {
            if (UintToArith256(blk.GetHash()) > prevMs->milestoneTarget) {
                blk.SetNonce(blk.GetNonce() + 1);
                m.Solve(blk);
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
            c.AddNewMilestone(*ms);

            prevMs = c.GetChainHead();
            ASSERT_EQ(c.GetPendingBlockCount(), 0);
            ASSERT_EQ(prevMs->GetMilestoneHash(), prevHash);

            auto lvs = ms->snapshot->GetLevelSet();
            STORE->StoreLevelSet(lvs);
            STORE->SaveHeadHeight(ms->height);
            std::vector<uint256> lvs_hashes;
            std::transform(lvs.begin(), lvs.end(), std::back_inserter(lvs_hashes),
                           [](const VertexWPtr& v) -> uint256 { return v.lock()->cblock->GetHash(); });
            c.PopOldest(lvs_hashes, {});

            lvsSizes[i] = lvs.size();
        }
    }

    // Check testing results
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
            auto block_reward = GetParams().GetReward(vtcs[i]->height);
            if (i > 0 && !isMilestone[i]) {
                ASSERT_EQ(vtcs[i]->cumulativeReward, vtcs[i - 1]->cumulativeReward + block_reward);
            } else if (i == 0) {
                ASSERT_EQ(vtcs[i]->cumulativeReward, block_reward);
            } else {
                ASSERT_EQ(vtcs[i]->cumulativeReward.GetValue(),
                          (vtcs[i - 1]->cumulativeReward + block_reward * lvsSizes[i]).GetValue());
            }
        }

        if (isMilestone[i]) {
            ASSERT_TRUE(vtcs[i]->isMilestone);
        } else {
            ASSERT_FALSE(vtcs[i]->isMilestone);
        }
    }

    // Construct and test for invalid redemptions
    auto ConstructFalseRedempt = [&](const uint256& prevHash, const Transaction& tx) {
        Block b{GetParams().version,
                ghash,
                prevHash,
                ghash,
                uint256(),
                fac.NextTime(),
                GetParams().maxTarget.GetCompact(),
                0};
        b.AddTransaction(tx);
        b.SetMerkle();
        b.CalculateOptimalEncodingSize();
        m.Solve(b);
        if (UintToArith256(b.GetHash()) > prevMs->milestoneTarget) {
            b.SetNonce(b.GetNonce() + 1);
            m.Solve(b);
        }

        return std::make_shared<const Block>(std::move(b));
    };

    auto ValidateRedemption = [&](const ConstBlockPtr& b) -> uint8_t {
        c.AddPendingBlock(b);
        return c.Verify(b)->validity[0];
    };

    // Invalid outpoint
    auto last_reg_tx      = *vtcs[lastRdm]->cblock->GetTransactions()[0];
    auto invalid_outpoint = ConstructFalseRedempt(vtcs[lastRdm]->cblock->GetHash(), last_reg_tx);
    ASSERT_EQ(ValidateRedemption(invalid_outpoint), Vertex::Validity::INVALID);

    // Double-redemption
    auto double_redempt = ConstructFalseRedempt(last_reg_tx.GetInputs()[0].outpoint.bHash, last_reg_tx);
    ASSERT_EQ(ValidateRedemption(double_redempt), Vertex::Validity::INVALID);

    // Wrong redemption value
    Transaction wrong_value_tx{};
    wrong_value_tx.AddInput(TxInput(TxOutPoint{prevRedHash, UNCONNECTED, UNCONNECTED}, keypair.second, hashMsg, sig))
        .AddOutput(10000, addr);
    ASSERT_TRUE(wrong_value_tx.IsRegistration());
    wrong_value_tx.FinalizeHash();
    auto wrong_value = ConstructFalseRedempt(prevHash, wrong_value_tx);
    ASSERT_EQ(ValidateRedemption(wrong_value), Vertex::Validity::INVALID);

    // Signature failure
    Transaction invalid_sig_tx{};
    invalid_sig_tx
        .AddInput(TxInput(TxOutPoint{prevRedHash, UNCONNECTED, UNCONNECTED}, CKey().MakeNewKey(true).GetPubKey(),
                          hashMsg, sig))
        .AddOutput(0, addr);
    ASSERT_TRUE(wrong_value_tx.IsRegistration());
    invalid_sig_tx.FinalizeHash();
    auto invalid_sig = ConstructFalseRedempt(wrong_value->GetHash(), invalid_sig_tx);
    ASSERT_EQ(ValidateRedemption(invalid_sig), Vertex::Validity::INVALID);
}

TEST_F(TestChainVerification, verify_tx_and_utxo) {
    /**
     * Generates consecutive blocks along the prevHash link
     */
    auto GenerateVertex = [&](Transaction* tx = nullptr) -> VertexPtr {
        static int height        = 1;
        static uint256 prev_hash = GENESIS->GetHash();
        static uint32_t t        = time(nullptr);

        static const auto& ghash = GENESIS->GetHash();
        Block b{GetParams().version,
                ghash,
                prev_hash,
                ghash,
                uint256(),
                t,
                GENESIS_VERTEX->snapshot->blockTarget.GetCompact(),
                0};

        if (tx) {
            b.AddTransaction(*tx);
        }

        b.SetMerkle();
        b.CalculateOptimalEncodingSize();
        m.Solve(b);

        VertexPtr vtx         = std::make_shared<Vertex>(std::move(b));
        vtx->minerChainHeight = height;

        height++;
        prev_hash = vtx->cblock->GetHash();
        t++;

        return vtx;
    };

    Chain c{};

    Coin valueIn{4}, valueOut1{2}, valueOut2{1};
    // prepare keys and signature
    auto key     = *DecodeSecret("KySymVGpRJzSKonDu21bSL5QVhXUhH1iU5VFKfXFuAB4w1R9ZiTx");
    auto addr    = key.GetPubKey().GetID();
    auto hashMsg = uintS<256>("4de04506f44155e2a59d2e8af4e6e15e9f50f5f0b1dc7a0742021799981180c2");
    std::vector<unsigned char> sig;
    key.Sign(hashMsg, sig);

    // Construct transaction output to add into the ledger
    auto encodedAddr = EncodeAddress(addr);
    VStream outdata(encodedAddr);
    tasm::Listing outputListing{tasm::Listing{std::vector<uint8_t>{tasm::VERIFY}, outdata}};
    TxOutput output{valueIn, outputListing};

    Transaction tx1{};
    tx1.AddOutput(std::move(output));
    tx1.FinalizeHash();
    auto vtx1 = GenerateVertex(&tx1);
    ASSERT_NE(vtx1->cblock->GetChainWork(), 0);
    const auto& b1hash = vtx1->cblock->GetHash();

    auto putxo = std::make_shared<UTXO>(vtx1->cblock->GetTransactions()[0]->GetOutputs()[0], 0, 0);
    ChainLedger ledger{
        std::unordered_map<uint256, UTXOPtr>{}, {{putxo->GetKey(), putxo}}, std::unordered_map<uint256, UTXOPtr>{}};
    AddToLedger(&c, std::move(ledger));
    AddToHistory(&c, vtx1);

    // Construct an empty block
    auto vtx2 = GenerateVertex();
    AddToHistory(&c, vtx2);

    // Construct another block with a valid tx
    Transaction tx{};
    tx.AddInput(TxInput(TxOutPoint{b1hash, 0, 0}, key.GetPubKey(), hashMsg, sig))
        .AddOutput(valueOut1, addr)
        .AddOutput(valueOut2, addr)
        .FinalizeHash();
    auto vtx3 = GenerateVertex(&tx);

    c.AddPendingUTXOs({std::make_shared<UTXO>(vtx3->cblock->GetTransactions()[0]->GetOutputs()[0], 0, 0),
                       std::make_shared<UTXO>(vtx3->cblock->GetTransactions()[0]->GetOutputs()[1], 0, 1)});

    auto txoc{ValidateTx(&c, *vtx3)};
    ASSERT_FALSE(txoc.Empty());

    auto& spent   = txoc.GetSpent();
    auto spentKey = ComputeUTXOKey(b1hash, 0, 0);
    ASSERT_EQ(spent.size(), 1);
    ASSERT_EQ(spent.count(spentKey), 1);

    auto& created = txoc.GetCreated();
    ASSERT_EQ(created.size(), 2);
    ASSERT_EQ(vtx3->fee, valueIn - valueOut1 - valueOut2);

    const auto& b3hash = vtx3->cblock->GetHash();
    AddToHistory(&c, vtx3);
    UpdateLedger(&c, txoc);

    // Construct a block with a double-spent tx
    auto vtx4 = GenerateVertex(&tx);
    auto txoc4{ValidateTx(&c, *vtx4)};
    ASSERT_TRUE(txoc4.Empty());
    AddToHistory(&c, vtx4);

    // Construct a block with invalid output value
    Transaction invalid_out{};
    invalid_out.AddInput(TxInput(TxOutPoint{b3hash, 0, 0}, key.GetPubKey(), hashMsg, sig))
        .AddOutput(valueOut1 + 1, addr)
        .FinalizeHash();
    auto vtx5 = GenerateVertex(&invalid_out);

    auto txoc5{ValidateTx(&c, *vtx5)};
    ASSERT_TRUE(txoc5.Empty());
    AddToHistory(&c, vtx5);

    // Construct a block with invalid input value
    Transaction invalid_in{};
    invalid_in.AddInput(TxInput(TxOutPoint{b3hash, 0, 0}, key.GetPubKey(), hashMsg, sig))
        .AddInput(TxInput(TxOutPoint{b3hash, 0, 1}, key.GetPubKey(), hashMsg, sig))
        .AddOutput(valueOut1, addr)
        .AddOutput(valueOut2, addr)
        .AddOutput(1, addr)
        .FinalizeHash();
    auto vtx6 = GenerateVertex(&invalid_in);

    auto txoc6{ValidateTx(&c, *vtx6)};
    ASSERT_TRUE(txoc6.Empty());
    AddToHistory(&c, vtx6);

    // Construct a block with invalid signature
    Transaction invalid_sig{};
    invalid_sig.AddInput(TxInput(TxOutPoint{b3hash, 0, 0}, CKey().MakeNewKey(true).GetPubKey(), hashMsg, sig))
        .AddOutput(valueOut1, addr)
        .FinalizeHash();
    auto vtx7 = GenerateVertex(&invalid_sig);

    auto txoc7{ValidateTx(&c, *vtx7)};
    ASSERT_TRUE(txoc7.Empty());
    AddToHistory(&c, vtx7);
}

TEST_F(TestChainVerification, ChainForking) {
    // Construct the main chain and fork
    ConcurrentQueue<MilestonePtr> dqms{{GENESIS_VERTEX->snapshot}};
    std::vector<VertexPtr> vtcs{};
    ConstBlockPtr forkblk;
    MilestonePtr split;
    for (int i = 1; i < 10; i++) { // reach height 9
        vtcs.emplace_back(fac.CreateConsecutiveVertexPtr(fac.NextTime(), m));
        dqms.push_back(fac.CreateMilestonePtr(dqms.back(), vtcs[i - 1]));
        if (i == 5) {
            // create a forked milestone chain at height 5
            auto blk = fac.CreateBlock();
            split    = dqms.back();
            blk.SetMilestoneHash(split->GetMilestoneHash());
            m.Solve(blk);
            forkblk = std::make_shared<const Block>(blk);
        }
    }
    auto chain = make_chain(dqms, vtcs, true);
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
    reg_inv.SetMerkle();
    reg_inv.CalculateOptimalEncodingSize();
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
    reg.SetMerkle();
    reg.CalculateOptimalEncodingSize();
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
    b1.SetMerkle();
    b1.CalculateOptimalEncodingSize();
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
    b2.SetMerkle();
    b2.CalculateOptimalEncodingSize();
    Vertex vtx2{b2};
    vtx2.minerChainHeight = 3;
    EXPECT_FALSE(IsValidDistance(&c, vtx2, 1000000000));
}
