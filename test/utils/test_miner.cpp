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

#ifdef __CUDA_ENABLED__
TEST_F(TestMiner, SolveCuckaroo) {
    SetLogLevel(SPDLOG_LEVEL_TRACE);

    Block b = fac.CreateBlock();

    Miner m(5, 16);
    m.Start();
    m.SolveCuckaroo(b);
    m.Stop();

    ResetLogLevel();
}
#endif

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

    std::vector<uint16_t> versions     = {100, 10, 1};
    std::vector<uint32_t> difficulties = {0x1f00ffffL, 0x1e00ffffL, 0x1d00ffffL};

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

    std::vector<Block> genesisBlocks;
    for (int i = 0; i < versions.size(); ++i) {
        Block b{versions[i]};
        b.AddTransaction(tx);
        b.SetDifficultyTarget(difficulties[i]);
        b.SetTime(1559859000L);
        b.SetNonce(0);
        b.FinalizeHash();
        b.CalculateOptimalEncodingSize();
        genesisBlocks.emplace_back(std::move(b));
    }

    /////////////////////////////////////////////////////////////////////
    // Uncomment the following lines to mine
    /////////////////////////////////////////////////////////////////////
    // int numThreads = 44;
    // Miner m(numThreads);
    // m.Start();
    // for (auto& genesisBlock : genesisBlocks) {
    // m.Solve(genesisBlock);
    // std::cout << std::to_string(genesisBlock) << std::endl;
    // VStream gvs(genesisBlock);
    // std::cout << "HEX string for version [" << genesisBlock.GetVersion() << "]: \n"
    //<< HexStr(gvs.cbegin(), gvs.cend()) << std::endl;
    // EXPECT_TRUE(genesisBlock.CheckPOW());
    //}
    // m.Stop();
    /////////////////////////////////////////////////////////////////////

    // Last mining result
    genesisBlocks[0].SetNonce(15649);     // UnitTest
    genesisBlocks[1].SetNonce(37692687);  // TestNet
    genesisBlocks[2].SetNonce(984142618); // MainNet

    for (auto& genesisBlock : genesisBlocks) {
        genesisBlock.FinalizeHash();
    }

    EXPECT_TRUE(GENESIS.Verify());
}
