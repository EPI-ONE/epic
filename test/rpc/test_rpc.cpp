// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>
//#include <gmock/gmock.h>

#include "opcodes.h"
#include "pubkey.h"
#include "rpc_client.h"
#include "rpc_server.h"
#include "subscription.h"
#include "test_env.h"

#include <chrono>
#include <google/protobuf/util/json_util.h>
#include <memory>
#include <string>

using google::protobuf::StringPiece;
using google::protobuf::util::JsonStringToMessage;

class TestRPCServer : public testing::Test {
public:
    TestFactory fac;
    const std::string prefix   = "test_rpc/";
    const std::string addr     = "0.0.0.0:3789";
    const uint256& double0hash = Hash::GetDoubleZeroHash();
    std::unique_ptr<RPCClient> client;

    void SetUp() {
        EpicTestEnvironment::SetUpDAG(prefix);
        auto netAddress = NetAddress::GetByIP(addr);
        RPC             = std::make_unique<RPCServer>(
            *netAddress, std::vector{RPCServiceType::BLOCK_EXPLORER_SERVER, RPCServiceType::COMMAND_LINE_SERVER});
        RPC->Start();

        while (!RPC->IsRunning()) {
            std::this_thread::yield();
        }
        client = std::make_unique<RPCClient>(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));
    }

    void TearDown() {
        RPC->Shutdown();
        EpicTestEnvironment::TearDownDAG(prefix);
    }

    bool SameHash(const uint256& _hash, const std::string& strhash) {
        return std::to_string(_hash) == strhash;
    }

    bool SameOutpoint(const TxOutPoint& outpoint, const rpc::Outpoint& rpc_outpoint) {
        return SameHash(outpoint.bHash, rpc_outpoint.from_block()) && outpoint.txIndex == rpc_outpoint.tx_idx() &&
               outpoint.outIndex == rpc_outpoint.out_idx();
    }

    bool SameOutput(const TxOutput& output, const rpc::Output rpcOutput) {
        if (std::to_string(output.listingContent) != rpcOutput.listing()) {
            return false;
        }

        if (output.value.GetValue() != rpcOutput.money()) {
            return false;
        }
        return true;
    }

    bool SameTx(const Transaction& tx, const rpc::Transaction& rpcTx) {
        const auto& input   = tx.GetInputs();
        const auto& output  = tx.GetOutputs();
        const auto& rpc_in  = rpcTx.inputs();
        const auto& rpc_out = rpcTx.outputs();

        if (!(input.size() == rpc_in.size() && output.size() == rpc_out.size())) {
            return false;
        }

        for (size_t i = 0; i < input.size(); i++) {
            if (!SameOutpoint(input[i].outpoint, rpc_in[i].outpoint())) {
                return false;
            }

            if (std::to_string(input[i].listingContent) != rpc_in[i].listing()) {
                return false;
            }
        }

        for (size_t i = 0; i < output.size(); i++) {
            if (!SameOutput(output[i], rpc_out[i])) {
                return false;
            }
        }
        return true;
    }

    bool SameBlock(const Block& blk, const rpc::Block& rpcBlk) {
        if (!SameHash(blk.GetHash(), rpcBlk.hash()) || !SameHash(blk.GetMilestoneHash(), rpcBlk.mshash()) ||
            !SameHash(blk.GetPrevHash(), rpcBlk.prevhash()) || !SameHash(blk.GetTipHash(), rpcBlk.tiphash())) {
            return false;
        }

        if (blk.GetVersion() != rpcBlk.version() || blk.GetDifficultyTarget() != rpcBlk.difftarget() ||
            blk.GetNonce() != rpcBlk.nonce() || blk.GetTime() != rpcBlk.time()) {
            return false;
        }

        const auto& prf    = blk.GetProof();
        const auto& rpcPrf = rpcBlk.proof();
        const auto& txs    = blk.GetTransactions();
        const auto& rpcTxs = rpcBlk.transactions();

        if (prf.size() != rpcPrf.size() || txs.size() != rpcTxs.size()) {
            return false;
        }

        for (size_t i = 0; i < prf.size(); i++) {
            if (prf[i] != rpcPrf[i]) {
                return false;
            }
        }

        for (size_t i = 0; i < txs.size(); i++) {
            if (!SameTx(*txs[i], rpcTxs[i])) {
                return false;
            }
        }

        return true;
    }

    bool SameVertex(const Vertex& vertex, const rpc::Vertex rpcVer) {
        if (!SameBlock(*vertex.cblock, rpcVer.block())) {
            return false;
        }

        if (vertex.height != rpcVer.height() || vertex.isMilestone != rpcVer.ismilestone() ||
            vertex.isRedeemed != rpcVer.redemptionstatus() || vertex.cumulativeReward.GetValue() != rpcVer.rewards()) {
            return false;
        }

        const auto& txStatus = vertex.validity;
        const auto& rpcTxSta = rpcVer.txstatus();

        if (txStatus.size() != rpcTxSta.size()) {
            return false;
        }

        for (size_t i = 0; i < txStatus.size(); i++) {
            if (txStatus[i] != rpcTxSta[i]) {
                return false;
            }
        }

        return true;
    }

    bool SameMilstone(const Milestone& ms, const rpc::Milestone rpcMs) {
        if (ms.height != rpcMs.height() || static_cast<uint64_t>(ms.hashRate) != rpcMs.hashrate()) {
            return false;
        }

        if (ms.GetBlockDifficulty() != rpcMs.blkdiff() || ms.GetMsDifficulty() != rpcMs.msdiff()) {
            return false;
        }

        if (std::to_string(ms.chainwork) != rpcMs.chainwork()) {
            return false;
        }

        return true;
    }
};

TEST_F(TestRPCServer, basic_dag_info_query) {
    // add blocks into dag
    std::vector<VertexPtr> blocks;
    const auto HEIGHT = static_cast<size_t>(500);
    auto chain        = fac.CreateChain(GENESIS_VERTEX, HEIGHT, false);
    auto latest_ms    = chain.back().back();
    for (auto& lvs : chain) {
        for (auto b : lvs) {
            blocks.emplace_back(std::move(b));
        }
    }

    STORE->EnableOBC();

    // SetLogLevel(SPDLOG_LEVEL_DEBUG);
    for (auto& vtx : blocks) {
        DAG->AddNewBlock(vtx->cblock, nullptr);
    }

    usleep(50000);
    STORE->Wait();
    STORE->Stop();
    DAG->Stop();

    const auto STORED_HEIGHT = HEIGHT - GetParams().punctualityThred;

    for (size_t i = 0; i < STORED_HEIGHT; i++) {
        const auto& hash    = chain[i].back()->cblock->GetHash();
        const auto req_hash = std::to_string(hash);

        auto re_size = client->GetLevelSetSize(req_hash); // get level set size
        rpc::UintMessage rpc_get_lvs_size;
        JsonStringToMessage(StringPiece(*re_size), &rpc_get_lvs_size);
        ASSERT_EQ(rpc_get_lvs_size.value(), chain[i].size());

        auto re_set = client->GetLevelSet(req_hash); // get level set
        rpc::BlockList rpc_get_lvs;
        JsonStringToMessage(StringPiece(*re_set), &rpc_get_lvs);

        for (size_t j = 0; j < chain[i].size(); j++) {
            ASSERT_TRUE(SameBlock(*(chain[i][j]->cblock), rpc_get_lvs.blocks()[j]));
        }

        auto re_ms = client->GetMilestone(req_hash); // get milestone
        rpc::Milestone rpc_get_ms;
        JsonStringToMessage(StringPiece(*re_ms), &rpc_get_ms);
        ASSERT_TRUE(SameMilstone(*(DAG->GetMsVertex(hash)->snapshot), rpc_get_ms));
    }

    auto re_latest = client->GetLatestMilestone(); // get lastest milestone
    rpc::Milestone rpc_latest;
    JsonStringToMessage(StringPiece(*re_latest), &rpc_latest);
    ASSERT_TRUE(SameMilstone(*latest_ms->snapshot, rpc_latest));

    // get milestone from block xxx to next yyy milestone
    constexpr size_t SIZE   = 100;
    constexpr size_t OFFSET = 200;

    auto re_new_ms = client->GetMilestonesFromHead(OFFSET, SIZE);

    rpc::MilestoneList rpc_new_ms;
    JsonStringToMessage(StringPiece(*re_new_ms), &rpc_new_ms);

    auto iter = rpc_new_ms.milestones().cbegin();
    for (size_t i = 1; i <= SIZE; i++) {
        auto levelset = chain[chain.size() - OFFSET - i];
        auto hash     = levelset[levelset.size() - 1]->cblock->GetHash();
        ASSERT_TRUE(SameMilstone(*(DAG->GetMsVertex(hash)->snapshot), *iter++));
    }

    for (const auto& blk : blocks) {
        const auto& pick_hash    = blk->cblock->GetHash();
        const auto pick_hash_str = std::to_string(pick_hash);

        auto re_block  = client->GetBlock(pick_hash_str);  // get blocks
        auto re_vertex = client->GetVertex(pick_hash_str); // get vertices

        rpc::Block rpc_get_blk;
        JsonStringToMessage(StringPiece(*re_block), &rpc_get_blk);
        ASSERT_TRUE(SameBlock(*(blk->cblock), rpc_get_blk));

        rpc::Vertex rpc_get_ver;
        JsonStringToMessage(StringPiece(*re_vertex), &rpc_get_ver);
        ASSERT_TRUE(SameVertex(*(DAG->GetMainChainVertex(pick_hash)), rpc_get_ver));
    }

    auto re_forks = client->GetForks(); // get forks
    rpc::MsChainList rpc_forks;
    JsonStringToMessage(StringPiece(*re_forks), &rpc_forks);
    ASSERT_EQ(rpc_forks.chains().size(), 1);

    auto re_pc = client->GetPeerChains(); // get peer chains
    rpc::ChainList rpc_pc;
    JsonStringToMessage(StringPiece(*re_pc), &rpc_pc);
    ASSERT_EQ(rpc_pc.chains().size(), 1);

    uint32_t nBlkCached = 0;
    for (size_t i = STORED_HEIGHT; i < HEIGHT; i++) {
        nBlkCached += chain[i].size();
    }
    auto re_recent_stat = client->GetRecentStat(); // get recent statistic data
    rpc::GetRecentStatResponse rpc_recent_stat;
    JsonStringToMessage(StringPiece(*re_recent_stat), &rpc_recent_stat);
    ASSERT_EQ(rpc_recent_stat.nblks(), nBlkCached);

    auto re_stat = client->Statistic(); // get total statistic data
    rpc::StatisticResponse rpc_stat;
    JsonStringToMessage(StringPiece(*re_stat), &rpc_stat);
    ASSERT_EQ(rpc_stat.height(), HEIGHT);

    uint32_t nBlkStored = 0;
    for (size_t i = 0; i < STORED_HEIGHT; i++) {
        nBlkStored += chain[i].size();
    }
    ASSERT_EQ(rpc_stat.nblks(), nBlkStored);
}

TEST_F(TestRPCServer, wallet_passphrase) {
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
    ASSERT_EQ(client->SetPassphrase(phrase).value(), testCode[PhraseCode::NOTSTART]);
    ASSERT_EQ(client->ChangePassphrase(phrase, phrase_phantom).value(), testCode[PhraseCode::NOTSTART]);
    ASSERT_EQ(client->Login(phrase).value(), testCode[PhraseCode::NOTSTART]);

    WALLET = std::make_unique<Wallet>(prefix + "/data/", 0, 0);
    ASSERT_EQ(client->Login(phrase).value(), testCode[PhraseCode::NOPHRASE]);
    ASSERT_EQ(client->ChangePassphrase(phrase, phrase_phantom).value(), testCode[PhraseCode::NOPHRASE]);
    ASSERT_EQ(client->SetPassphrase(phrase).value(), testCode[PhraseCode::FAILTOSET]);

    ASSERT_TRUE(WALLET->GenerateMaster());
    ASSERT_EQ(client->SetPassphrase(phrase).value(), testCode[PhraseCode::SET]);

    std::string phrase_wrong = "wrong";
    ASSERT_EQ(client->SetPassphrase(phrase_wrong).value(), testCode[PhraseCode::ENCRYPTED]);
    ASSERT_EQ(client->ChangePassphrase(phrase_wrong, phrase_phantom).value(), testCode[PhraseCode::FAILTOCHANGE]);
    ASSERT_EQ(client->Login(phrase_wrong).value(), testCode[PhraseCode::FAILTOLOGIN]);

    ASSERT_EQ(client->Login(phrase).value(), testCode[PhraseCode::LOGIN]);
    ASSERT_EQ(client->ChangePassphrase(phrase, phrase_phantom).value(), testCode[PhraseCode::UPDATE]);
    ASSERT_EQ(client->Login(phrase).value(), testCode[PhraseCode::FAILTOLOGIN]);
    ASSERT_EQ(client->Login(phrase_phantom).value(), testCode[PhraseCode::LOGIN]);

    // restart wallet
    WALLET.reset();
    WALLET = std::make_unique<Wallet>(prefix + "/data/", 0, 0);
    ASSERT_TRUE(WALLET->ExistMasterInfo());
    ASSERT_EQ(client->Login(phrase_wrong).value(), testCode[PhraseCode::FAILTOLOGIN]);
    ASSERT_EQ(client->Login(phrase).value(), testCode[PhraseCode::FAILTOLOGIN]);
    ASSERT_EQ(client->Login(phrase_phantom).value(), testCode[PhraseCode::LOGIN]);

    WALLET.reset();
}

TEST_F(TestRPCServer, transaction_and_miner) {
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
        {AnswerCode::WRONG_ADDR, "Invalid address: "},
        {AnswerCode::CREATE_TX_FAIL, "Failed to create tx. Please check if you have enough balance."},
        {AnswerCode::CREATE_TX, "Now wallet is creating tx"},
    };

    ASSERT_EQ(client->StopMiner().value(), testCode[AnswerCode::MINER_NOT_RUNNING]);
    ASSERT_EQ(client->StartMiner().value(), true);
    ASSERT_EQ(client->StartMiner().value(), false);

    ASSERT_EQ(client->CreateRandomTx(1).value(), testCode[AnswerCode::WALLET_NOT_START]);
    ASSERT_EQ(client->CreateTx({}, 0).value(), testCode[AnswerCode::WALLET_NOT_START]);
    ASSERT_EQ(client->GenerateNewKey().value(), testCode[AnswerCode::WALLET_NOT_START]);
    ASSERT_EQ(client->GetBalance().value(), testCode[AnswerCode::WALLET_NOT_START]);

    WALLET = std::make_unique<Wallet>(prefix + "/data/", 0, 0);
    ASSERT_EQ(client->CreateRandomTx(1).value(), testCode[AnswerCode::NOT_LOG_IN]);
    ASSERT_EQ(client->CreateTx({}, 0).value(), testCode[AnswerCode::NOT_LOG_IN]);
    ASSERT_EQ(client->GenerateNewKey().value(), testCode[AnswerCode::NOT_LOG_IN]);
    ASSERT_EQ(client->GetBalance().value(), testCode[AnswerCode::NOT_LOG_IN]);

    DAG->RegisterOnLvsConfirmedCallback(
        [&](auto vec, auto map1, auto map2) { WALLET->OnLvsConfirmed(vec, map1, map2); });
    WALLET->GenerateMaster();
    WALLET->SetPassphrase("");
    WALLET->RPCLogin();
    WALLET->Start();

    ASSERT_EQ(client->CreateRandomTx(2).value(), testCode[AnswerCode::CREATE_TX]);
    ASSERT_EQ(client->CreateTx({}, 1).value(), testCode[AnswerCode::NO_OUTPUT]);
    while (WALLET->GetBalance() < 10) {
        std::this_thread::yield();
    }

    // malicious address
    std::string wrong_addr = std::to_string(fac.CreateKeyPair().second.GetID()) + "deadbeef";
    ASSERT_EQ(client->CreateTx({{1, wrong_addr}}, 0).value(), testCode[AnswerCode::WRONG_ADDR] + wrong_addr);

    auto opAddr = client->GenerateNewKey();
    ASSERT_TRUE(opAddr.has_value());

    // not enough balance
    ASSERT_EQ(client->CreateTx({{1100, *opAddr}}, 1010).value(), testCode[AnswerCode::CREATE_TX_FAIL]);
    auto opBalance = client->GetBalance();
    ASSERT_TRUE(opBalance.has_value());
    ASSERT_TRUE(client->CreateTx({{std::stoi(*opBalance) - 1, *opAddr}}, 1));

    while (WALLET->GetUnspent().size() != 1) {
        std::this_thread::yield();
    }
    ASSERT_EQ(WALLET->GetUnspent().size(), 1);

    ASSERT_EQ(client->StopMiner().value(), testCode[AnswerCode::MINER_STOP]);

    // checkout GetWalletAddrs and GetAllTxout
    const auto allAddrs = WALLET->GetAllAddresses();
    std::string allAddrsResult;
    for (const auto& addr : allAddrs) {
        allAddrsResult += EncodeAddress(addr) + "\n";
    }
    auto opAllAddrs = client->GetWalletAddrs();
    ASSERT_TRUE(opAllAddrs.has_value());
    ASSERT_EQ(allAddrsResult, opAllAddrs.value());

    const auto opAllTxout = client->GetAllTxout();
    ASSERT_TRUE(opAllTxout.has_value());

    std::istringstream inputstr{opAllAddrs.value()};
    for (std::string line; std::getline(inputstr, line);) {
        ASSERT_NE(opAllAddrs->find(line), std::string::npos);
    }

    ASSERT_TRUE(client->Stop());
}

std::string parseContent(tasm::Listing& listing) {
    auto lstr = std::to_string(listing);
    auto n    = lstr.find("( ");
    auto m    = lstr.find(" )");
    return lstr.substr(n + 2, m - (n + 2));
}

std::vector<uint8_t> parseOp(tasm::Listing& listing) {
    auto lstr = std::to_string(listing);
    auto n    = lstr.find("[ ");
    auto m    = lstr.find(" ]");
    auto ops  = lstr.substr(n + 2, m - (n + 2));

    std::vector<uint8_t> v;
    std::stringstream strm;
    strm << ops;
    std::copy(std::istream_iterator<int>(strm), std::istream_iterator<int>(), std::back_inserter(v));
    return v;
}

TEST_F(TestRPCServer, stateless_test) {
    TestFactory fac{};
    VStream indata{}, outdata{};

    auto keypair        = fac.CreateKeyPair();
    CKeyID addr         = keypair.second.GetID();
    auto [hashMsg, sig] = fac.CreateSig(keypair.first);

    auto encodedAddr = EncodeAddress(addr);
    outdata << encodedAddr;
    tasm::Listing outputListing{tasm::Listing{std::vector<uint8_t>{tasm::VERIFY}, std::move(outdata)}};

    // construct transaction input
    indata << keypair.second << sig << hashMsg;
    tasm::Listing inputListing{tasm::Listing{indata}};

    ASSERT_TRUE(client->ValidateAddr(encodedAddr).value());
    ASSERT_TRUE(
        client->VerifyMessage(parseContent(inputListing), parseContent(outputListing), parseOp(outputListing)).value());
}

TEST_F(TestRPCServer, Subscription) {
    // basically subscibe and unsubscibe
    PUBLISHER = std::make_unique<Publisher>();
    client->Subscribe(addr, SubType::TX | SubType::BLOCK);
    ASSERT_EQ(1, PUBLISHER->GetSubscriberCount());
    client->DeleteSubscriber(addr);
    ASSERT_EQ(0, PUBLISHER->GetSubscriberCount());

    // subcribe but the server is not active
    client->Subscribe(addr, SubType::TX | SubType::BLOCK);
    auto tx = fac.CreateTx(1, 1);
    PUBLISHER->PushMsg(&tx, SubType::TX);
    ASSERT_EQ(0, PUBLISHER->GetSubscriberCount());

    PUBLISHER.reset();
}
