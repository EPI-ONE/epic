#include <gtest/gtest.h>

#include "block.h"

class ThreadPoolSolverTest : public testing::Test {};

TEST_F(ThreadPoolSolverTest, simple_test) {
    /*
     * Create a basic block to solve
     */
    Transaction tx;
    Block block;

    std::string hexStr("0123456789ABCDEF");
    auto vs = VStream(ParseHex(hexStr));
    tx.AddInput(TxInput(Tasm::Listing(vs)));

    std::optional<CKeyID> pubKeyID = DecodeAddress("14u6LvvWpReA4H2GwMMtm663P2KJGEkt77");
    tx.AddOutput(TxOutput(66, Tasm::Listing(VStream(pubKeyID.value())))).FinalizeHash();

    block.AddTransaction(tx);
    block.SetMinerChainHeight(0);
    block.ResetReward();
    block.SetDifficultyTarget(EASIEST_COMP_DIFF_TARGET);


    /*
     * test the solver
     */
    std::size_t numThreads = 4;
    ThreadPool solverPool(numThreads);

    solverPool.Start();
    block.Solve(numThreads, solverPool);
    solverPool.Stop();

    EXPECT_TRUE(block.Verify());
}
