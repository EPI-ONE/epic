#include <gtest/gtest.h>

#include "miner.h"
#include "test_env.h"
#include "utilstrencodings.h"

class TestMiner : public testing::Test {
    void SetUp() override {
        EpicTestEnvironment::SetUpDAG("test_miner/");
    }
    void TearDown() override {
        EpicTestEnvironment::TearDownDAG("test_miner/");
    }
};

TEST_F(TestMiner, Solve) {
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
    block.SetDifficultyTarget(GetParams().maxTarget.GetCompact());

    /*
     * test the solver
     */
    Miner m(4);
    m.Start();
    m.Solve(block);
    m.Stop();

    EXPECT_TRUE(block.Verify());
}

TEST_F(TestMiner, Run) {
    PEERMAN = std::make_unique<PeerManager>();
    MEMPOOL     = std::make_unique<MemPool>();

    Miner m(2);
    m.Run();
    usleep(500000);
    m.Stop();

    usleep(50000);
    DAG->Stop();

    ASSERT_TRUE(m.GetSelfChainHead());
    ASSERT_TRUE(m.GetFirstKey().IsValid());

    if (DAG->GetBestChain().GetPendingBlocks().empty()) {
        ASSERT_TRUE(DAG->GetBestChain().GetStates().size() > 1);
    }

    PEERMAN.reset();
    MEMPOOL.reset();
}

TEST_F(TestMiner, MineGenesis) {
    /**
     * MainNet: {version: 1, difficulty target: 0x1d00ffffL}
     * TestNet: {version:10, difficulty target: 0x1e00ffffL}
     * UnitTest: {version:100, difficulty target: 0x1f00ffffL}
     */

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
    genesisBlock.SetTime(1559859000L);
    genesisBlock.SetNonce(0);
    genesisBlock.FinalizeHash();
    genesisBlock.CalculateOptimalEncodingSize();

    /////////////////////////////////////////////////////////////////////
    // Uncomment the following lines to mine
    /////////////////////////////////////////////////////////////////////
    // int numThreads = 44;
    // Miner m(numThreads);
    // m.Start();
    // m.Solve(genesisBlock);
    // m.Stop();
    // std::cout << std::to_string(genesisBlock) << std::endl;
    // VStream gvs(genesisBlock);
    // std::cout << "HEX string: \n" << HexStr(gvs.cbegin(), gvs.cend()) << std::endl;
    // EXPECT_TRUE(genesisBlock.Verify());
    /////////////////////////////////////////////////////////////////////

    // Last mining result
    genesisBlock.SetNonce(251319); // UnitTest
    // genesisBlock.SetNonce(29897782); // TestNet
    // genesisBlock.SetNonce(1701609359); // MainNet

    genesisBlock.FinalizeHash();

    EXPECT_TRUE(GENESIS.Verify());
}
