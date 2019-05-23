#include <gtest/gtest.h>

#include "mempool.h"

class MemPoolTest : public testing::Test {
public:
    std::vector<ConstTxPtr> transactions;

    void SetUp() {
        std::size_t n = 4;
        Transaction* tx;

        for (std::size_t i = 0; i < n; ++i) {
            tx = new Transaction();
            tx->RandomizeHash();

            transactions.push_back(std::shared_ptr<const Transaction>(tx));
        }
    }
};

/* this test basically tests
 * the get and set methods of
 * mempool */
TEST_F(MemPoolTest, simple_test) {
    MemPool pool;

    pool.Insert(transactions[0]);
    pool.Insert(transactions[1]);
    pool.Insert(transactions[2]);

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

TEST_F(MemPoolTest, get_transaction_test) {
    MemPool pool;

    /* hash used to simulate block */
    uint256 hash;
    hash.randomize();

    /* this transaction is used to
     * simulate three cases:
     * 1. d(bhash, thash) < threshold
     * 2. d(bhash, thash) = threshold
     * 3. d(bhash, thash) > threshold */
    pool.Insert(transactions[0]);

    /* = */
    arith_uint256 threshold = UintToArith256(transactions[0]->GetHash()) ^ UintToArith256(hash);
    EXPECT_TRUE(pool.GetTransaction(hash, threshold).has_value());

    /* > */
    threshold += 1;
    EXPECT_TRUE(pool.GetTransaction(hash, threshold).has_value());

    /* < */
    threshold -= 2;
    EXPECT_FALSE(pool.GetTransaction(hash, threshold).has_value());
}
