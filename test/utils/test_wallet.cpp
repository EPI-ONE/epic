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
        std::string cmd = "exec rm -rf " + dir;
        system(cmd.c_str());
    }
};

TEST_F(TestWallet, basic_workflow_in_wallet) {
    {
        Coin init_money{100};
        Wallet wallet{dir, 1};
        wallet.Start();
        wallet.CreateNewKey(false);
        MEMPOOL   = std::make_unique<MemPool>();
        auto addr = wallet.GetRandomAddress();
        Transaction tx;
        tx.AddOutput(init_money, addr);
        tx.FinalizeHash();

        Block block;
        block.AddTransaction(tx);
        block.CalculateHash();
        block.SetParents();

        auto utxo           = std::make_shared<UTXO>(block.GetTransactions()[0]->GetOutputs()[0], 0, 0);
        auto vertex         = std::make_shared<Vertex>(block);
        vertex->validity[0] = Vertex::VALID;
        wallet.OnLvsConfirmed({vertex}, {{utxo->GetKey(), utxo}}, {});

        while (wallet.GetBalance() != init_money) {
            std::this_thread::yield();
        }
        ASSERT_EQ(wallet.GetBalance(), init_money);
        ASSERT_EQ(wallet.GetUnspent().size(), 1);

        wallet.CreateNewKey(false);
        Coin spent_money{10};
        std::vector<std::pair<Coin, CKeyID>> outputs{{spent_money, CKeyID()}};
        auto new_tx = wallet.CreateTx(outputs);

        ASSERT_EQ(new_tx->GetOutputs().size(), 2);
        auto totalOutput = new_tx->GetOutputs()[0].value + new_tx->GetOutputs()[1].value;
        ASSERT_EQ(totalOutput, init_money - MIN_FEE);
        ASSERT_TRUE(new_tx);
        ASSERT_EQ(new_tx->GetOutputs().size(), outputs.size() + 1);
        ASSERT_EQ(wallet.GetBalance(), 0);
        ASSERT_EQ(wallet.GetUnspent().size(), 0);
        ASSERT_EQ(wallet.GetPending().size(), 1);
        ASSERT_EQ(wallet.GetSpent().size(), 0);
        ASSERT_EQ(wallet.GetPendingTx().size(), 1);
        ASSERT_TRUE(wallet.GetPendingTx().contains(new_tx->GetHash()));

        MEMPOOL = std::make_unique<MemPool>();
        ASSERT_TRUE(wallet.SendTxToMemPool(new_tx));
        ASSERT_EQ(MEMPOOL->Size(), 1);

        Block new_block;
        new_block.AddTransaction(std::move(new_tx));
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

        wallet.OnLvsConfirmed({new_vertex}, std::move(utxos), {stxokey});
        while (wallet.GetBalance() == init_money - spent_money - MIN_FEE) {
            std::this_thread::yield();
        }
        wallet.Stop();
        ASSERT_EQ(wallet.GetUnspent().size(), 1);
        ASSERT_EQ(wallet.GetPending().size(), 0);
        ASSERT_EQ(wallet.GetSpent().size(), 1);
        ASSERT_EQ(wallet.GetPendingTx().size(), 0);
        ASSERT_EQ(wallet.GetBalance(), init_money - spent_money - MIN_FEE);
        MEMPOOL.reset();
    }

    Wallet newWallet{dir, period};
    ASSERT_EQ(newWallet.GetUnspent().size(), 1);
    ASSERT_EQ(newWallet.GetPending().size(), 0);
    ASSERT_EQ(newWallet.GetSpent().size(), 1);
    ASSERT_EQ(newWallet.GetPendingTx().size(), 0);
}

TEST_F(TestWallet, test_wallet_store) {
    WalletStore store{dir};
    // very simple tx tests
    auto tx = fac.CreateTx(fac.GetRand() % 10, fac.GetRand() % 10);
    store.StoreTx(tx);

    auto txs = store.GetAllTx();
    ASSERT_EQ(tx, *(txs.at(tx.GetHash())));

    // very simple key tests
    auto [priv, pub] = fac.CreateKeyPair();
    auto addr        = pub.GetID();
    store.StoreKeys(addr, priv);

    auto keys = store.GetAllKey();
    ASSERT_EQ(keys.count(addr), 1);
    ASSERT_TRUE(store.IsExistKey(addr));

    ASSERT_EQ(store.KeysToFile("keys"), 0);

    std::ifstream input{"keys"};
    std::string line;
    ASSERT_TRUE(std::getline(input, line));
    ASSERT_EQ(line, EncodeSecret(priv));

    store.ClearOldData();
    ASSERT_EQ(store.GetAllTx().size(), 0);

    std::string cmd = "exec rm keys";
    system(cmd.c_str());
}

TEST_F(TestWallet, workflow) {
    EpicTestEnvironment::SetUpDAG(path, true, true);
    WALLET->Start();

    WALLET->CreateNewKey(false);
    auto old          = WALLET->GetRandomAddress();
    auto registration = WALLET->CreateFirstRegistration(old);
    ASSERT_TRUE(registration);

    MEMPOOL->PushRedemptionTx(registration);

    MINER->Start();
    MINER->Run();

    while (WALLET->GetCurrentMinerReward() < 200) {
        std::this_thread::yield();
    }
    auto redemption = WALLET->CreateRedemption(old, old, "dssss");
    MEMPOOL->PushRedemptionTx(redemption);

    while (WALLET->GetBalance() < 200) {
        std::this_thread::yield();
    }

    ASSERT_EQ(WALLET->GetUnspent().size(), 1);

    auto tx = WALLET->CreateTx({{10, CKeyID()}});
    ASSERT_EQ(WALLET->GetBalance().GetValue(), 0);
    ASSERT_TRUE(WALLET->SendTxToMemPool(tx));
    ASSERT_EQ(WALLET->GetPendingTx().size(), 1);
    ASSERT_EQ(WALLET->GetPending().size(), 1);

    // receive the change of last transaction

    while (WALLET->GetSpent().empty()) {
        usleep(50000);
    }

    EXPECT_EQ(WALLET->GetUnspent().size(), 1);
    EXPECT_EQ(WALLET->GetPendingTx().size(), 0);
    EXPECT_EQ(WALLET->GetPending().size(), 0);
    EXPECT_EQ(WALLET->GetSpent().size(), 1);

    MINER->Stop();
    WALLET->Stop();
    DAG->Stop();
    STORE->Stop();
    EpicTestEnvironment::TearDownDAG(path);
}

TEST_F(TestWallet, normal_workflow) {
    EpicTestEnvironment::SetUpDAG(path, true, true);
    WALLET->Start();

    WALLET->CreateNewKey(false);

    MINER->Start();
    MINER->Run();

    WALLET->CreateRandomTx(4);

    // receive the change of last transaction
    while (WALLET->GetSpent().size() != 1) {
        usleep(500000);
    }

    ASSERT_EQ(WALLET->GetUnspent().size(), 3);
    ASSERT_EQ(WALLET->GetPendingTx().size(), 0);
    ASSERT_EQ(WALLET->GetPending().size(), 0);
    ASSERT_EQ(WALLET->GetSpent().size(), 1);

    MINER->Stop();
    WALLET->Stop();
    DAG->Stop();
    STORE->Stop();
    EpicTestEnvironment::TearDownDAG(path);
}
