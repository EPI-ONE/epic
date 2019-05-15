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

    pool.Erase(transactions[1]);

    /* after erasing the mempool should not
     * contain the transaction anymore */
    EXPECT_FALSE(pool.Contains(transactions[1]));

    /* check if now there are zero
     * transactions in the pool */
    EXPECT_EQ(pool.Size(), 2);
}
