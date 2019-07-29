#include "mempool.h"
#include "miner.h"
#include "test_env.h"
#include "test_factory.h"
#include "wallet.h"

#include <chrono>
#include <gtest/gtest.h>

class TestWallet : public testing::Test {
public:
    const std::string dir = "test_wallet";
    const uint32_t period = 600;
    TestFactory fac;

    void TearDown() override {
        std::string cmd = "exec rm -r " + dir;
        system(cmd.c_str());
    }
};

TEST_F(TestWallet, basic_workflow_in_wallet) {
    {
    Coin init_money{100};
    Wallet wallet{dir, 1};
    wallet.Start();
    wallet.CreateNewKey(false);
    auto addr = wallet.GetRandomAddress();
    Transaction tx;
    tx.AddOutput(init_money, addr);
    tx.FinalizeHash();

    Block block;
    block.AddTransaction(tx);
    block.CalculateHash();
    block.SetParents();

    auto utxo        = std::make_shared<UTXO>(block.GetTransaction()->GetOutputs()[0], 0);
    auto record      = std::make_shared<NodeRecord>(block);
    record->validity = NodeRecord::VALID;
    wallet.ProcessRecord(record);
    wallet.ProcessUTXO(utxo);

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
    auto outputPoint = new_block.GetTransaction()->GetInputs()[0].outpoint;
    UTXOKey stxo     = XOR(outputPoint.bHash, outputPoint.index);
    ASSERT_EQ(stxo, utxo->GetKey());

    std::unordered_set<UTXOPtr> utxos;

    int index = 0;
    for (auto& output : new_block.GetTransaction()->GetOutputs()) {
        utxos.emplace(std::make_shared<UTXO>(output, index));
        index++;
    }

    auto new_record = std::make_shared<NodeRecord>(new_block);

    wallet.OnLvsConfirmed({new_record}, utxos, {stxo});
    ASSERT_EQ(wallet.GetUnspent().size(), 1);
    ASSERT_EQ(wallet.GetPending().size(), 0);
    ASSERT_EQ(wallet.GetSpent().size(), 1);
    ASSERT_EQ(wallet.GetPendingTx().size(), 0);
    ASSERT_EQ(wallet.GetBalance(), init_money - spent_money - MIN_FEE);
    MEMPOOL.reset();

    // wait for wallet to backup
    std::this_thread::sleep_for(std::chrono::seconds(1));
    wallet.Stop();
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

    store.KeysToFile("keys");

    std::ifstream input{"keys"};
    std::string line;
    ASSERT_TRUE(std::getline(input, line));
    ASSERT_EQ(line, EncodeSecret(priv));

    store.ClearOldData();
    ASSERT_EQ(store.GetAllTx().size(), 0);
}

TEST_F(TestWallet, work_flow) {
    // spdlog::set_level(spdlog::level::debug);
    std::string path = "test_wallet/";
    EpicTestEnvironment::SetUpDAG(path);
    MEMPOOL = std::make_unique<MemPool>();
    MINER   = std::make_unique<Miner>();
    WALLET  = std::make_shared<Wallet>(dir, period);
    DAG->RegisterOnLvsConfirmedListener(WALLET);

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
    std::vector<std::pair<Coin, CKeyID>> outputs{{Coin{10}, CKeyID()}};
    auto tx = WALLET->CreateTx(outputs);
    ASSERT_EQ(WALLET->GetBalance().GetValue(), 0);
    ASSERT_TRUE(WALLET->SendTxToMemPool(tx));
    ASSERT_EQ(WALLET->GetPendingTx().size(), 1);
    ASSERT_EQ(WALLET->GetPending().size(), 1);

    // receive the change of last transaction
    while (WALLET->GetBalance().GetValue() == 0) {
        std::this_thread::yield();
    }

    ASSERT_EQ(WALLET->GetUnspent().size(), 1);
    ASSERT_EQ(WALLET->GetPendingTx().size(), 0);
    ASSERT_EQ(WALLET->GetPending().size(), 0);
    ASSERT_EQ(WALLET->GetSpent().size(), 1);

    MINER->Stop();
    MEMPOOL.reset();
    MINER.reset();
    WALLET.reset();
    EpicTestEnvironment::TearDownDAG(path);
}
