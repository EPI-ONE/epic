#include <gtest/gtest.h>

#include "mempool.h"
#include "test_env.h"

class TestMemPool : public testing::Test {
public:
    std::vector<ConstTxPtr> transactions;
    TestFactory fac;
    const std::string dir = "test_mempool/";

    void SetUp() {
        std::size_t N = 4;

        for (std::size_t i = 0; i < N; ++i) {
            transactions.push_back(
                std::make_shared<Transaction>(fac.CreateTx(fac.GetRand() % 11 + 1, fac.GetRand() % 11 + 1)));
        }
    }
};

TEST_F(TestMemPool, simple_get_and_set) {
    MemPool pool;

    ASSERT_TRUE(pool.Insert(transactions[0]));
    ASSERT_TRUE(pool.Insert(transactions[1]));
    ASSERT_TRUE(pool.Insert(transactions[2]));

    /* check if now there are three
     * transactions in the pool */
    EXPECT_EQ(pool.Size(), 3);

    /* check if IsEmpty returns the right value */
    EXPECT_FALSE(pool.IsEmpty());

    /* check if all elements are found
     * when passing the ptr */
    EXPECT_TRUE(pool.Contains(transactions[0]));
    EXPECT_TRUE(pool.Contains(transactions[1]));
    EXPECT_TRUE(pool.Contains(transactions[2]));
    EXPECT_FALSE(pool.Contains(transactions[3]));

    /* check that a not existent element can
     * not be deleted */
    EXPECT_FALSE(pool.Erase(transactions[3]));

    /* check if the delete was successful */
    EXPECT_TRUE(pool.Erase(transactions[1]));

    /* after erasing the mempool should not
     * contain the transaction anymore */
    EXPECT_FALSE(pool.Contains(transactions[1]));

    /* check if now there are zero
     * transactions in the pool */
    EXPECT_EQ(pool.Size(), 2);
}

TEST_F(TestMemPool, get_transaction_test) {
    MemPool pool;

    /* hash used to simulate block */
    uint256 blkHash = fac.CreateRandomHash();

    /* this transaction is used to
     * simulate three cases:
     * 1. d(bhash, thash) < threshold
     * 2. d(bhash, thash) = threshold
     * 3. d(bhash, thash) > threshold */
    pool.Insert(transactions[0]);

    /* = */
    arith_uint256 threshold = (UintToArith256(transactions[0]->GetHash()) ^ UintToArith256(blkHash)) << 32;
    ASSERT_FALSE(pool.GetTransaction(blkHash, threshold));

    /* > */
    threshold--;
    ASSERT_FALSE(pool.GetTransaction(blkHash, threshold));

    /* < */
    threshold += 2;
    ASSERT_TRUE(pool.GetTransaction(blkHash, threshold));

    ASSERT_TRUE(pool.ExtractTransaction(blkHash, threshold));
    ASSERT_TRUE(pool.IsEmpty());
}

TEST_F(TestMemPool, receive_and_release) {
    // prepare dag
    EpicTestEnvironment::SetUpDAG(dir);
    const auto& ghash      = GENESIS.GetHash();
    auto [privkey, pubkey] = fac.CreateKeyPair();
    auto [hashMsg, sig]    = fac.CreateSig(privkey);
    auto [hashMsg2, sig2]  = fac.CreateSig(privkey);
    auto addr              = pubkey.GetID();

    Block blkTemplate{GetParams().version, ghash, ghash, ghash, fac.NextTime(), GetParams().maxTarget.GetCompact(), 0};
    auto firstReg = std::make_shared<const Transaction>(addr);
    Block b1      = blkTemplate;
    b1.AddTransaction(firstReg);
    b1.Solve();
    while (UintToArith256(b1.GetHash()) > GENESIS_RECORD.snapshot->milestoneTarget) {
        b1.SetNonce(b1.GetNonce() + 1);
        b1.Solve();
    }
    const auto& b1hash = b1.GetHash();

    DAG->AddNewBlock(std::make_shared<const Block>(std::move(b1)), nullptr);
    usleep(10000);
    DAG->Wait();

    auto [chain, vRecs] = fac.CreateChain(*DAG->GetMilestoneHead(), 5);
    std::for_each(chain.begin(), chain.end(), [](auto lvs) {
        std::for_each(lvs.begin(), lvs.end(), [](auto b) { DAG->AddNewBlock((b), nullptr); });
    });

    usleep(10000);
    DAG->Wait();

    Transaction redeem{};
    redeem.AddInput(TxInput{TxOutPoint{b1hash, UNCONNECTED}, pubkey, hashMsg, sig}).AddOutput(10, addr);
    auto redemption = std::make_shared<const Transaction>(std::move(redeem));

    Block b2 = blkTemplate;
    b2.SetMilestoneHash(DAG->GetMilestoneHead()->cblock->GetHash());
    b2.SetPrevHash(chain.back().back()->GetHash());
    b2.SetTime(chain.back().back()->GetTime() + 10);
    b2.AddTransaction(redemption);
    b2.Solve();
    while (UintToArith256(b2.GetHash()) > DAG->GetBestChain().GetChainHead()->milestoneTarget) {
        b2.SetNonce(b2.GetNonce() + 1);
        b2.Solve();
    }
    auto b2hash = b2.GetHash();

    DAG->AddNewBlock(std::make_shared<const Block>(std::move(b2)), nullptr);

    usleep(10000);
    DAG->Wait();

    ASSERT_EQ(DAG->GetMilestoneHead()->cblock->GetHash(), b2hash);

    // prepare test data
    auto keypair = fac.CreateKeyPair();
    auto newAddr = keypair.second.GetID();

    Transaction tx_reg{};
    Transaction tx_normal_1{};
    Transaction tx_normal_2{};
    Transaction tx_normal_3{};
    Transaction tx_conflict{};

    tx_reg.AddInput(TxInput{TxOutPoint{b2hash, UNCONNECTED}, pubkey, hashMsg, sig}).AddOutput(1, addr);
    tx_normal_1.AddInput(TxInput{TxOutPoint{b2hash, 0}, pubkey, hashMsg, sig}).AddOutput(5, newAddr);
    tx_normal_2.AddInput(TxInput{TxOutPoint{b2hash, 0}, pubkey, hashMsg, sig}).AddOutput(10, newAddr);
    tx_normal_3.AddInput(TxInput{TxOutPoint{b2hash, 0}, pubkey, hashMsg2, sig2}).AddOutput(10, newAddr);
    tx_conflict.AddInput(TxInput{TxOutPoint{b1hash, 0}, pubkey, hashMsg, sig}).AddOutput(3, newAddr);

    auto ptx_reg      = std::make_shared<const Transaction>(std::move(tx_reg));
    auto ptx_normal_1 = std::make_shared<const Transaction>(std::move(tx_normal_1));
    auto ptx_normal_2 = std::make_shared<const Transaction>(std::move(tx_normal_2));
    auto ptx_normal_3 = std::make_shared<const Transaction>(std::move(tx_normal_3));
    auto ptx_conflict = std::make_shared<const Transaction>(std::move(tx_conflict));

    MemPool pool;
    ASSERT_FALSE(pool.ReceiveTx(ptx_reg));
    ASSERT_FALSE(pool.ReceiveTx(ptx_conflict));
    ASSERT_TRUE(pool.ReceiveTx(ptx_normal_1));
    ASSERT_TRUE(pool.ReceiveTx(ptx_normal_2));
    ASSERT_TRUE(pool.ReceiveTx(ptx_normal_3));
    ASSERT_EQ(pool.Size(), 3);

    pool.ReleaseTxFromConfirmed(*ptx_normal_3, false);
    ASSERT_EQ(pool.Size(), 2);

    pool.ReleaseTxFromConfirmed(*ptx_normal_1, true);
    ASSERT_TRUE(pool.IsEmpty());

    EpicTestEnvironment::TearDownDAG(dir);
}
