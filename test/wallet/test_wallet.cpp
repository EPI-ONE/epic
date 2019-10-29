// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "mempool.h"
#include "miner.h"
#include "test_env.h"
#include "wallet.h"

#include <chrono>

class TestWallet : public testing::Test {
public:
    const std::string dir  = "test_wallet/";
    const std::string path = "test_wallet_data/";
    const uint32_t period  = 600;
    TestFactory fac;

    void SetUp() override {}

    void TearDown() override {
        std::string cmd = "exec rm -rf " + dir + " " + path;
        system(cmd.c_str());
    }
};

TEST_F(TestWallet, basic_workflow_in_wallet) {
    Coin init_money{100};
    auto wallet = new Wallet{dir, 1};
    wallet->GenerateMaster();
    wallet->SetPassphrase("");
    wallet->DisableRedemptions();
    wallet->Start();
    wallet->CreateNewKey(false);
    MEMPOOL   = std::make_unique<MemPool>();
    auto addr = wallet->GetRandomAddress();
    Transaction tx;
    tx.AddOutput(init_money, addr);
    tx.FinalizeHash();

    Block block;
    block.AddTransaction(tx);
    block.SetMerkle();
    block.CalculateHash();
    block.SetParents();

    auto utxo           = std::make_shared<UTXO>(block.GetTransactions()[0]->GetOutputs()[0], 0, 0);
    auto vertex         = std::make_shared<Vertex>(block);
    vertex->validity[0] = Vertex::VALID;
    wallet->OnLvsConfirmed({vertex}, {{utxo->GetKey(), utxo}}, {});

    while (wallet->GetBalance() != init_money) {
        std::this_thread::yield();
    }
    ASSERT_EQ(wallet->GetBalance(), init_money);
    ASSERT_EQ(wallet->GetUnspent().size(), 1);

    wallet->CreateNewKey(false);
    Coin spent_money{10};
    std::vector<std::pair<Coin, CKeyID>> outputs{{spent_money, CKeyID()}};
    auto new_tx = wallet->CreateTx(outputs);

    ASSERT_EQ(new_tx->GetOutputs().size(), 2);
    auto totalOutput = new_tx->GetOutputs()[0].value + new_tx->GetOutputs()[1].value;
    ASSERT_EQ(totalOutput, init_money - MIN_FEE);
    ASSERT_TRUE(new_tx);
    ASSERT_EQ(new_tx->GetOutputs().size(), outputs.size() + 1);
    ASSERT_EQ(wallet->GetBalance(), 0);
    ASSERT_EQ(wallet->GetUnspent().size(), 0);
    ASSERT_EQ(wallet->GetPending().size(), 1);
    ASSERT_EQ(wallet->GetSpent().size(), 0);
    ASSERT_EQ(wallet->GetPendingTx().size(), 1);
    ASSERT_TRUE(wallet->GetPendingTx().contains(new_tx->GetHash()));

    MEMPOOL = std::make_unique<MemPool>();
    ASSERT_TRUE(wallet->SendTxToMemPool(new_tx));
    ASSERT_EQ(MEMPOOL->Size(), 1);

    Block new_block;
    new_block.AddTransaction(std::move(new_tx));
    new_block.SetMerkle();
    new_block.CalculateHash();
    new_block.SetParents();
    auto outpoint = new_block.GetTransactions()[0]->GetInputs()[0].outpoint;
    auto stxokey  = ComputeUTXOKey(outpoint.bHash, outpoint.txIndex, outpoint.outIndex);
    ASSERT_EQ(stxokey, utxo->GetKey());

    std::unordered_map<uint256, UTXOPtr> utxos;

    int index = 0;
    for (auto& output : new_block.GetTransactions()[0]->GetOutputs()) {
        auto putxo = std::make_shared<UTXO>(output, index, index);
        utxos.emplace(putxo->GetKey(), putxo);
        index++;
    }

    auto new_vertex         = std::make_shared<Vertex>(new_block);
    new_vertex->validity[0] = Vertex::VALID;

    wallet->OnLvsConfirmed({new_vertex}, std::move(utxos), {stxokey});
    while (wallet->GetBalance() == init_money - spent_money - MIN_FEE) {
        std::this_thread::yield();
    }
    wallet->Stop();
    ASSERT_EQ(wallet->GetUnspent().size(), 1);
    ASSERT_EQ(wallet->GetPending().size(), 0);
    ASSERT_EQ(wallet->GetSpent().size(), 1);
    ASSERT_EQ(wallet->GetPendingTx().size(), 0);
    ASSERT_EQ(wallet->GetBalance(), init_money - spent_money - MIN_FEE);
    MEMPOOL.reset();

    delete wallet;

    Wallet newWallet{dir, period};
    ASSERT_EQ(newWallet.GetUnspent().size(), 1);
    ASSERT_EQ(newWallet.GetPending().size(), 0);
    ASSERT_EQ(newWallet.GetSpent().size(), 1);
    ASSERT_EQ(newWallet.GetPendingTx().size(), 0);

    std::string cmd = "exec rm -r " + dir;
    system(cmd.c_str());
}

TEST_F(TestWallet, test_wallet_store) {
    CKeyID addr;
    auto store = new WalletStore{dir};
    // very simple tx tests
    auto tx = fac.CreateTx(fac.GetRand() % 10, fac.GetRand() % 10);
    store->StoreTx(tx);

    auto txs = store->GetAllTx();
    ASSERT_EQ(tx, *(txs.at(tx.GetHash())));

    // very simple key tests
    auto [priv, pub] = fac.CreateKeyPair();
    addr             = pub.GetID();
    auto testCipher  = ParseHex("f5f7228bfe8d771c7f860338cf6fa2d609aa1fdf8167046cc3f4ebdc3169d6ad");
    store->StoreKeys(addr, testCipher, pub);

    auto keys = store->GetAllKey();
    ASSERT_EQ(keys.count(addr), 1);
    ASSERT_TRUE(store->IsExistKey(addr));

    uint256 fakeHash = fac.CreateRandomHash();
    store->StoreUnspent(fakeHash, addr, 0, 0, 5);
    auto unspent = store->GetAllUnspent();
    ASSERT_FALSE(unspent.empty());
    ASSERT_EQ(unspent.size(), 1);
    ASSERT_EQ(unspent.count(fakeHash), 1);
    ASSERT_TRUE(std::get<3>(unspent.at(fakeHash)) == 5);

    ASSERT_EQ(store->KeysToFile("keys"), 0);
    ASSERT_TRUE(store->StoreFirstRegInfo());
    ASSERT_TRUE(store->GetFirstRegInfo());

    store->ClearOldData();
    ASSERT_EQ(store->GetAllTx().size(), 0);

    delete store;

    WalletStore newStore{dir};
    ASSERT_TRUE(newStore.IsExistKey(addr));
    ASSERT_TRUE(newStore.GetFirstRegInfo());

    std::string cmd = "exec rm keys";
    system(cmd.c_str());

    cmd = "exec rm -r " + dir;
    system(cmd.c_str());
}

TEST_F(TestWallet, workflow) {
    EpicTestEnvironment::SetUpDAG(path, true, true);
    WALLET->GenerateMaster();
    WALLET->SetPassphrase("");
    WALLET->Start();

    WALLET->CreateNewKey(true);
    auto registration = WALLET->CreateFirstRegistration(WALLET->GetRandomAddress());
    ASSERT_FALSE(registration.empty());

    MINER->Run();

    while (WALLET->GetBalance() < 30) {
        std::this_thread::yield();
    }
    WALLET->DisableRedemptions();
    MINER->Stop();

    ASSERT_EQ(WALLET->GetUnspent().size(), 1);

    auto tx = WALLET->CreateTx({{WALLET->GetBalance() - MIN_FEE, WALLET->GetRandomAddress()}});
    ASSERT_EQ(WALLET->GetBalance().GetValue(), 0);
    ASSERT_TRUE(WALLET->SendTxToMemPool(tx));
    ASSERT_EQ(WALLET->GetPendingTx().size(), 1);
    ASSERT_EQ(WALLET->GetPending().size(), 1);
    ASSERT_TRUE(WALLET->GetUnspent().empty());

    while (MEMPOOL->GetRedemptionTx(false))
        ;
    MINER->Run();

    // Wait until receive the change of the last transaction
    while (!MEMPOOL->Empty() || WALLET->GetUnspent().empty()) {
        usleep(50000);
    }
    MINER->Stop();

    EXPECT_EQ(WALLET->GetUnspent().size(), 1);
    EXPECT_EQ(WALLET->GetPendingTx().size(), 0);
    EXPECT_EQ(WALLET->GetPending().size(), 0);
    EXPECT_EQ(WALLET->GetSpent().size(), 1);

    EpicTestEnvironment::TearDownDAG(path);
}

TEST_F(TestWallet, normal_workflow) {
    EpicTestEnvironment::SetUpDAG(path, true, true);
    WALLET->GenerateMaster();
    WALLET->SetPassphrase("");
    WALLET->Start();

    WALLET->CreateNewKey(false);
    WALLET->DisableRedemptions();

    MINER->Run();

    WALLET->CreateRandomTx(4);

    // receive the change of last transaction
    while (WALLET->GetSpent().size() != 1) {
        usleep(500000);
    }
    MINER->Stop();

    ASSERT_EQ(WALLET->GetUnspent().size(), 3);
    ASSERT_EQ(WALLET->GetPendingTx().size(), 0);
    ASSERT_EQ(WALLET->GetPending().size(), 0);
    ASSERT_EQ(WALLET->GetSpent().size(), 1);

    // check wallet restart
    auto balance = WALLET->GetBalance();
    ASSERT_GT(balance.GetValue(), 0);
    WALLET.reset(nullptr);

    usleep(100000);

    WALLET.reset(new Wallet("test_wallet_data//data/", 0));
    DAG->RegisterOnLvsConfirmedCallback(
        [&](auto vec, auto map1, auto map2) { WALLET->OnLvsConfirmed(vec, map1, map2); });
    WALLET->CheckPassphrase("");
    WALLET->Start();

    ASSERT_TRUE(WALLET->ExistMaster());
    ASSERT_EQ(balance, WALLET->GetBalance());

    MINER->Run();

    WALLET->CreateRandomTx(1);
    usleep(500000);
    while (WALLET->GetPending().size() != 0 || WALLET->GetPendingTx().size() != 0) {
        usleep(500000);
    }
    MINER->Stop();

    ASSERT_GE(WALLET->GetUnspent().size(), 3);
    ASSERT_EQ(WALLET->GetPendingTx().size(), 0);
    ASSERT_EQ(WALLET->GetPending().size(), 0);
    ASSERT_LE(WALLET->GetSpent().size(), 3);

    SecureString newPhrase = "realone";
    ASSERT_TRUE(WALLET->ChangePassphrase("", newPhrase));
    ASSERT_TRUE(WALLET->CheckPassphrase(newPhrase));
    WALLET->CreateRandomTx(1);

    MINER->Run();

    usleep(500000);
    while (WALLET->GetPending().size() != 0 || WALLET->GetPendingTx().size() != 0) {
        usleep(500000);
    }
    MINER->Stop();

    ASSERT_GE(WALLET->GetUnspent().size(), 3);
    ASSERT_EQ(WALLET->GetPendingTx().size(), 0);
    ASSERT_EQ(WALLET->GetPending().size(), 0);
    ASSERT_LE(WALLET->GetSpent().size(), 4);
    EpicTestEnvironment::TearDownDAG(path);
}
