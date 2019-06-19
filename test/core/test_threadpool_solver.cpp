#include <gtest/gtest.h>

#include "pow.h"
#include "utilstrencodings.h"

class TestThreadPoolSolver : public testing::Test {};

TEST_F(TestThreadPoolSolver, simple_test) {
    /*
     * Create a basic block to solve
     */
    Transaction tx;
    Block block(GetParams().version);

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

TEST_F(TestThreadPoolSolver, mine_genesis) {
    // The line `m.Solve(genesisBlock, solverPool)`
    // should only be uncommented when using this code
    // to mine a new genesis block.

    Block genesisBlock{100};
    Transaction tx;

    // Construct a script containing the difficulty bits and the following message:
    std::string hexStr("04ffff001d0104454974206973206e6f772074656e2070617374207"
                       "4656e20696e20746865206576656e696e6720616e64207765206172"
                       "65207374696c6c20776f726b696e6721");

    // Convert the string to bytes
    auto vs = VStream(ParseHex(hexStr));

    // Add input and output
    tx.AddInput(TxInput(Tasm::Listing(vs)));

    std::optional<CKeyID> pubKeyID = DecodeAddress("14u6LvvWpReA4H2GwMMtm663P2KJGEkt77");
    tx.AddOutput(TxOutput(66, Tasm::Listing(VStream(pubKeyID.value())))).FinalizeHash();

    genesisBlock.AddTransaction(tx);
    genesisBlock.SetDifficultyTarget(0x1f00ffffL);
    genesisBlock.SetTime(1560941102L);
    genesisBlock.SetNonce(0);
    genesisBlock.FinalizeHash();
    genesisBlock.CalculateOptimalEncodingSize();

    /////////////////////////////////////////////////////////////////////
    // Uncomment the following lines to mine
    /////////////////////////////////////////////////////////////////////
    // int numThreads = 44;
    // ThreadPool solverPool(numThreads);
    // solverPool.Start();
    // Miner m;
    // m.Solve(genesisBlock, solverPool);
    // solverPool.Stop();
    // std::cout << std::to_string(genesisBlock) << std::endl;
    // VStream gvs(genesisBlock);
    // std::cout << "HEX string: \n" << HexStr(gvs.cbegin(), gvs.cend());
    /////////////////////////////////////////////////////////////////////

    // Last mining result
    genesisBlock.SetNonce(105198);
    genesisBlock.FinalizeHash();
    EXPECT_TRUE(genesisBlock.Verify());
}
