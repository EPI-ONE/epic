#include <gtest/gtest.h>

#include "pow.h"

class TestThreadPoolSolver: public testing::Test {};

TEST_F(TestThreadPoolSolver, simple_test) {
    /*
     * Create a basic block to solve
     */
    Transaction tx;
    Block block(GENESIS_BLOCK_VERSION);

    std::string hexStr("0123456789ABCDEF");
    auto vs = VStream(ParseHex(hexStr));
    tx.AddInput(TxInput(Tasm::Listing(vs)));

    std::optional<CKeyID> pubKeyID = DecodeAddress("14u6LvvWpReA4H2GwMMtm663P2KJGEkt77");
    tx.AddOutput(TxOutput(66, Tasm::Listing(VStream(pubKeyID.value())))).FinalizeHash();

    block.AddTransaction(tx);
    block.SetDifficultyTarget(EASIEST_COMP_DIFF_TARGET);

    /*
     * test the solver
     */
    ThreadPool solverPool(4);

    solverPool.Start();
    Miner m;
    m.Solve(block, solverPool);
    solverPool.Stop();

    EXPECT_TRUE(block.Verify());
}
