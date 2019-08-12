#include <gtest/gtest.h>

#include "miner.h"
#include "test_env.h"
#include "utilstrencodings.h"

class TestMiner : public testing::Test {
    void SetUp() override {}
    void TearDown() override {}

public:
    TestFactory fac = EpicTestEnvironment::GetFactory();

    static void SetUpEnv() {
        EpicTestEnvironment::SetUpDAG("test_miner/");

        CKey key;
        key.MakeNewKey(false);
        auto tx = std::make_shared<Transaction>(key.GetPubKey().GetID());

        MEMPOOL = std::make_unique<MemPool>();
        MEMPOOL->PushRedemptionTx(tx);
    }

    static void TearDownEnv() {
        EpicTestEnvironment::TearDownDAG("test_miner/");
        MEMPOOL.reset();
    }
};

TEST_F(TestMiner, Solve) {
    Block block = fac.CreateBlock(1, 1);

    Miner m(4);
    m.Start();
    m.Solve(block);
    m.Stop();

    EXPECT_TRUE(block.Verify());
}

TEST_F(TestMiner, SolveCuckaroo) {
    SetLogLevel(SPDLOG_LEVEL_TRACE);

    Block b = fac.CreateBlock();

    Miner m(5, 16);
    m.Start();
    m.SolveCuckaroo(b);
    m.Stop();

    ResetLogLevel();
}

TEST_F(TestMiner, Run) {
    SetUpEnv();

    Miner m(2);
    m.Run();
    usleep(500000);
    m.Stop();

    DAG->Stop();

    ASSERT_TRUE(m.GetSelfChainHead());
    ASSERT_TRUE(DAG->GetBestChain().GetStates().size() > 1);
    ASSERT_TRUE(DAG->GetChains().size() == 1);

    TearDownEnv();
}

TEST_F(TestMiner, Restart) {
    SetUpEnv();

    Miner m(2);
    m.Run();
    usleep(100000);
    m.Stop();

    DAG->Wait();

    auto selfChainHead = m.GetSelfChainHead();

    m.Run();
    usleep(100000);
    m.Stop();

    DAG->Stop();

    auto cursor = m.GetSelfChainHead();

    ASSERT_NE(*cursor, *selfChainHead);

    while (*cursor != GENESIS && *cursor != *selfChainHead) {
        cursor = CAT->FindBlock(cursor->GetPrevHash());
    }

    ASSERT_EQ(*cursor, *selfChainHead);

    TearDownEnv();
}

TEST_F(TestMiner, MineGenesis) {
    /**
     * MainNet: {version: 1, difficulty target: 0x1d00ffffL}
     * TestNet: {version:10, difficulty target: 0x1e00ffffL}
     * UnitTest: {version:100, difficulty target: 0x1f00ffffL}
     */

    Block genesisBlock{1};
    Transaction tx;

    // Construct a script containing the difficulty bits and the following
    // message:
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
    genesisBlock.SetDifficultyTarget(0x1d00ffffL);
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
    // std::cout << "HEX string: \n" << HexStr(gvs.cbegin(), gvs.cend()) <<
    // std::endl; EXPECT_TRUE(genesisBlock.Verify());
    /////////////////////////////////////////////////////////////////////

    // Last mining result
    // genesisBlock.SetNonce(88981); // UnitTest
    // genesisBlock.SetNonce(6187482); // TestNet
    // genesisBlock.SetNonce(255364577); // MainNet

    genesisBlock.FinalizeHash();

    // EXPECT_TRUE(GENESIS.Verify());
}
