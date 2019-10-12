// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "key.h"
#include "rpc_client.h"
#include "rpc_server.h"
#include "rpc_tools.h"
#include "test_env.h"

#include <chrono>
#include <string>

class TestRPCServer : public testing::Test {
public:
    TestFactory fac;
    std::string prefix = "test_rpc/";
    static std::string addr;
    const uint256 double0hash = Hash::GetDoubleZeroHash();

    static void SetUpTestCase() {
        addr = "0.0.0.0:3789";
    }

    void SetUp() {
        EpicTestEnvironment::SetUpDAG(prefix);
        auto netAddress = NetAddress::GetByIP(addr);
        RPC             = std::make_unique<RPCServer>(
            *netAddress, std::vector{RPCServiceType::BLOCK_EXPLORER_SERVER, RPCServiceType::COMMAND_LINE_SERVER});
        RPC->Start();

        while (!RPC->IsRunning()) {
            std::this_thread::yield();
        }
    }

    void TearDown() {
        EpicTestEnvironment::TearDownDAG(prefix);
        RPC->Shutdown();
    }
};

std::string TestRPCServer::addr = "";

TEST_F(TestRPCServer, GetBlock) {
    RPCClient client(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));

    auto req_hash = std::to_string(GENESIS->GetHash());
    auto res      = client.GetBlock(req_hash);
    ASSERT_TRUE(res.has_value());

    auto blk = ToBlock(*res);
    ASSERT_EQ(blk, *GENESIS);

    auto res_fake = client.GetBlock(std::to_string(double0hash));
    ASSERT_TRUE(res_fake.has_value());

    auto blk_fake    = ToBlock(*res_fake);
    auto blk_default = ToBlock(rpc::Block().default_instance());
    ASSERT_EQ(blk_fake, blk_default);
}

TEST_F(TestRPCServer, GetLevelSetAndItsSize) {
    int size = 1;
    std::vector<VertexPtr> lvs;
    lvs.reserve(size);
    auto ms = fac.CreateVertexPtr(1, 1, true);
    fac.CreateMilestonePtr(GENESIS_VERTEX->snapshot, ms);
    ms->isMilestone      = true;
    ms->snapshot->height = 1;
    ms->height           = 1;
    lvs.push_back(ms);
    for (int i = 1; i < size; ++i) {
        auto b         = fac.CreateVertexPtr(fac.GetRand() % 10, fac.GetRand() % 10, true);
        b->isMilestone = false;
        b->height      = ms->height;
        lvs.push_back(b);
    }
    ASSERT_TRUE(STORE->StoreLevelSet(lvs));

    RPCClient client(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));

    for (int i = 0; i < size; ++i) {
        auto req_hash = std::to_string(lvs[i]->cblock->GetHash());
        auto res_size = client.GetLevelSetSize(req_hash);
        ASSERT_TRUE(res_size.has_value());
        EXPECT_EQ(res_size.value(), size);
    }
    auto req_hash = std::to_string(lvs[0]->cblock->GetHash());
    auto res_set  = client.GetLevelSet(req_hash);
    ASSERT_TRUE(res_set.has_value());

    for (size_t i = 0; i < lvs.size(); ++i) {
        auto res_blk = ToBlock(res_set.value()[i]);
        EXPECT_EQ(res_blk, *lvs[i]->cblock);
    }
}

TEST_F(TestRPCServer, GetLatestMilestone) {
    int size       = 5;
    auto chain     = fac.CreateChain(GENESIS_VERTEX, size);
    auto latest_ms = chain.back().back();
    for (auto lvs : chain) {
        for (auto elem : lvs) {
            DAG->AddNewBlock(elem->cblock, nullptr);
        }
    }

    usleep(50000);
    DAG->Stop();

    RPCClient client(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));

    auto res = client.GetLatestMilestone();
    ASSERT_TRUE(res.has_value());

    auto res_blk = ToBlock(*res);
    EXPECT_EQ(res_blk, *latest_ms->cblock);
}

TEST_F(TestRPCServer, GetNewMilestoneSince) {
    int size = 5;
    std::vector<VertexPtr> mss;
    std::vector<VertexPtr> mss_to_check;
    auto first_ms = fac.CreateVertexPtr(1, 1, true);
    fac.CreateMilestonePtr(GENESIS_VERTEX->snapshot, first_ms);
    first_ms->isMilestone      = true;
    first_ms->snapshot->height = 1;
    first_ms->height           = 1;
    mss.push_back(first_ms);
    mss_to_check.push_back(first_ms);
    ASSERT_TRUE(STORE->StoreLevelSet(mss));
    mss.clear();
    auto prev = first_ms->snapshot;
    for (int i = 2; i < size; ++i) {
        auto ms = fac.CreateVertexPtr(i, i, true);
        fac.CreateMilestonePtr(prev, ms);
        ms->isMilestone            = true;
        first_ms->snapshot->height = i;
        ms->height                 = i;
        mss.push_back(ms);
        mss_to_check.push_back(ms);
        ASSERT_TRUE(STORE->StoreLevelSet(mss));
        mss.clear();
        prev = ms->snapshot;
    }
    STORE->SaveHeadHeight(size - 1);

    usleep(50000);
    DAG->Stop();

    RPCClient client(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));

    size_t request_milestone_number = 3;
    auto req_hash                   = std::to_string(mss_to_check[0]->cblock->GetHash());
    auto res                        = client.GetNewMilestoneSince(req_hash, request_milestone_number);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res.value().size(), request_milestone_number);
    for (int i = 0; i < request_milestone_number; ++i) {
        auto res_blk = ToBlock(res.value()[i]);
        EXPECT_EQ(res_blk, *mss_to_check[i + 1]->cblock);
    }
}

TEST_F(TestRPCServer, wallet_passphrase) {
    RPCClient client{grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())};

    enum class PhraseCode { NOTSTART, ENCRYPTED, NOPHRASE, FAILTOSET, FAILTOLOGIN, FAILTOCHANGE, LOGIN, SET, UPDATE };

    auto testCode = std::map<PhraseCode, std::string>{
        {PhraseCode::NOTSTART, "Wallet has not been started"},
        {PhraseCode::ENCRYPTED, "Wallet has already be encrypted with a passphrase"},
        {PhraseCode::NOPHRASE, "Wallet has no phrase set. Please set one first"},
        {PhraseCode::FAILTOSET, "Failed to set passphrase"},
        {PhraseCode::FAILTOLOGIN, "Failed to login with the passphrase. Please check passphrase"},
        {PhraseCode::FAILTOCHANGE, "Failed to change passphrase. Please check passphrase"},
        {PhraseCode::LOGIN, "You are already logged in"},
        {PhraseCode::SET, "Your passphrase has been successfully set!"},
        {PhraseCode::UPDATE, "Your passphrase is successfully updated!"},
    };

    std::string phrase         = "mypass";
    std::string phrase_phantom = "phantom";
    ASSERT_EQ(client.SetPassphrase(phrase).value(), testCode[PhraseCode::NOTSTART]);
    ASSERT_EQ(client.ChangePassphrase(phrase, phrase_phantom).value(), testCode[PhraseCode::NOTSTART]);
    ASSERT_EQ(client.Login(phrase).value(), testCode[PhraseCode::NOTSTART]);

    WALLET = std::make_unique<Wallet>(prefix + "/data/", 0, 0);
    ASSERT_EQ(client.Login(phrase).value(), testCode[PhraseCode::NOPHRASE]);
    ASSERT_EQ(client.ChangePassphrase(phrase, phrase_phantom).value(), testCode[PhraseCode::NOPHRASE]);
    ASSERT_EQ(client.SetPassphrase(phrase).value(), testCode[PhraseCode::FAILTOSET]);

    ASSERT_TRUE(WALLET->GenerateMaster());
    ASSERT_EQ(client.SetPassphrase(phrase).value(), testCode[PhraseCode::SET]);

    std::string phrase_wrong = "wrong";
    ASSERT_EQ(client.SetPassphrase(phrase_wrong).value(), testCode[PhraseCode::ENCRYPTED]);
    ASSERT_EQ(client.ChangePassphrase(phrase_wrong, phrase_phantom).value(), testCode[PhraseCode::FAILTOCHANGE]);
    ASSERT_EQ(client.Login(phrase_wrong).value(), testCode[PhraseCode::FAILTOLOGIN]);

    ASSERT_EQ(client.Login(phrase).value(), testCode[PhraseCode::LOGIN]);
    ASSERT_EQ(client.ChangePassphrase(phrase, phrase_phantom).value(), testCode[PhraseCode::UPDATE]);
    ASSERT_EQ(client.Login(phrase).value(), testCode[PhraseCode::FAILTOLOGIN]);
    ASSERT_EQ(client.Login(phrase_phantom).value(), testCode[PhraseCode::LOGIN]);

    // restart wallet
    WALLET.reset();
    WALLET = std::make_unique<Wallet>(prefix + "/data/", 0, 0);
    ASSERT_TRUE(WALLET->ExistMasterInfo());
    ASSERT_EQ(client.Login(phrase_wrong).value(), testCode[PhraseCode::FAILTOLOGIN]);
    ASSERT_EQ(client.Login(phrase).value(), testCode[PhraseCode::FAILTOLOGIN]);
    ASSERT_EQ(client.Login(phrase_phantom).value(), testCode[PhraseCode::LOGIN]);

    WALLET.reset();
}

TEST_F(TestRPCServer, transaction_and_miner) {
    RPCClient client{grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())};
    MEMPOOL = std::make_unique<MemPool>();
    MINER   = std::make_unique<Miner>(4);

    enum class AnswerCode {
        MINER_NOT_RUNNING,
        MINER_STOP_FAIL,
        MINER_STOP,
        WALLET_NOT_START,
        NOT_LOG_IN,
        NO_OUTPUT,
        WRONG_ADDR,
        CREATE_TX_FAIL,
        CREATE_TX
    };

    auto testCode = std::map<AnswerCode, std::string>{
        {AnswerCode::MINER_NOT_RUNNING, "Miner is not running yet"},
        {AnswerCode::MINER_STOP_FAIL, "Failed to stop miner"},
        {AnswerCode::MINER_STOP, "Miner is successfully stopped"},
        {AnswerCode::WALLET_NOT_START, "Wallet has not been started"},
        {AnswerCode::NOT_LOG_IN, "Please log in or set up a new passphrase"},
        {AnswerCode::NO_OUTPUT, "Please specify at least one output"},
        {AnswerCode::WRONG_ADDR, "Wrong address: "},
        {AnswerCode::CREATE_TX_FAIL, "Failed to create tx. Please check if you have enough balance."},
        {AnswerCode::CREATE_TX, "Now wallet is creating tx"},
    };

    ASSERT_EQ(client.StopMiner().value(), testCode[AnswerCode::MINER_NOT_RUNNING]);
    ASSERT_EQ(client.StartMiner().value(), true);
    ASSERT_EQ(client.StartMiner().value(), false);

    ASSERT_EQ(client.CreateRandomTx(1).value(), testCode[AnswerCode::WALLET_NOT_START]);
    ASSERT_EQ(client.CreateTx({}, 0).value(), testCode[AnswerCode::WALLET_NOT_START]);
    ASSERT_EQ(client.GenerateNewKey().value(), testCode[AnswerCode::WALLET_NOT_START]);
    ASSERT_EQ(client.GetBalance().value(), testCode[AnswerCode::WALLET_NOT_START]);

    WALLET = std::make_unique<Wallet>(prefix + "/data/", 0, 0);
    ASSERT_EQ(client.CreateRandomTx(1).value(), testCode[AnswerCode::NOT_LOG_IN]);
    ASSERT_EQ(client.CreateTx({}, 0).value(), testCode[AnswerCode::NOT_LOG_IN]);
    ASSERT_EQ(client.GenerateNewKey().value(), testCode[AnswerCode::NOT_LOG_IN]);
    ASSERT_EQ(client.GetBalance().value(), testCode[AnswerCode::NOT_LOG_IN]);

    DAG->RegisterOnLvsConfirmedCallback(
        [&](auto vec, auto map1, auto map2) { WALLET->OnLvsConfirmed(vec, map1, map2); });
    WALLET->GenerateMaster();
    WALLET->SetPassphrase("");
    WALLET->RPCLogin();
    WALLET->Start();

    ASSERT_EQ(client.CreateRandomTx(2).value(), testCode[AnswerCode::CREATE_TX]);
    ASSERT_EQ(client.CreateTx({}, 1).value(), testCode[AnswerCode::NO_OUTPUT]);
    while (WALLET->GetBalance() < 10) {
        std::this_thread::yield();
    }

    // malicious address
    std::string wrong_addr = std::to_string(fac.CreateKeyPair().second.GetID()) + "deadbeef";
    ASSERT_EQ(client.CreateTx({{1, wrong_addr}}, 0).value(), testCode[AnswerCode::WRONG_ADDR] + wrong_addr);

    auto opAddr = client.GenerateNewKey();
    ASSERT_TRUE(opAddr.has_value());

    // not enough balance
    ASSERT_EQ(client.CreateTx({{1100, *opAddr}}, 1010), testCode[AnswerCode::CREATE_TX_FAIL]);
    auto opBalance = client.GetBalance();
    ASSERT_TRUE(opBalance.has_value());
    ASSERT_TRUE(client.CreateTx({{std::stoi(*opBalance) - 1, *opAddr}}, 1));

    ASSERT_EQ(client.StopMiner().value(), testCode[AnswerCode::MINER_STOP]);
    ASSERT_TRUE(client.Stop());
}
