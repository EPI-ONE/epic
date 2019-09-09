// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
        EpicTestEnvironment::SetUpDAG("test_miner/", true);

        CKey key;
        key.MakeNewKey(false);
        auto tx = std::make_shared<Transaction>(key.GetPubKey().GetID());
        MEMPOOL->PushRedemptionTx(tx);
    }

    static void TearDownEnv() {
        EpicTestEnvironment::TearDownDAG("test_miner/");
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
        cursor = STORE->FindBlock(cursor->GetPrevHash());
    }

    ASSERT_EQ(*cursor, *selfChainHead);

    TearDownEnv();
}

TEST_F(TestMiner, MineGenesis) {
    /**
     * MainNet: {version: 1, PROOFSIZE: 4}
     * TestNet: {version:10, PROOFSIZE: 14}
     * UnitTest: {version:100, PROOFSIZE: 42}
     */

    // std::vector<uint16_t> versions = {100, 10, 1};

#undef PROOFSIZE
#define PROOFSIZE 4
    std::vector<uint16_t> versions = {100};

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
        b.SetDifficultyTarget(0x2000ffffL);
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
    // m.SolveCuckaroo(genesisBlock);
    // std::cout << std::to_string(genesisBlock) << std::endl;
    // VStream gvs(genesisBlock);
    // std::cout << "HEX string for version [" << genesisBlock.GetVersion() << "]: \n"
    //<< HexStr(gvs.cbegin(), gvs.cend()) << std::endl;
    // EXPECT_TRUE(genesisBlock.CheckPOW());
    //}
    // m.Stop();
    /////////////////////////////////////////////////////////////////////

    // Last mining result
    genesisBlocks[0].SetNonce(1063);
    genesisBlocks[0].SetProof({155323463, 249094318, 300653832, 329365019}); // UnitTest
    genesisBlocks[1].SetNonce(608);
    genesisBlocks[1].SetProof({138505277, 226668951, 481608353, 487218457}); // TestNet
    genesisBlocks[2].SetNonce(1430);
    genesisBlocks[2].SetProof({161077755, 430449326, 494942486, 513180537}); // MainNet

    for (auto& genesisBlock : genesisBlocks) {
        genesisBlock.FinalizeHash();
    }

    EXPECT_TRUE(GENESIS.Verify());
}
