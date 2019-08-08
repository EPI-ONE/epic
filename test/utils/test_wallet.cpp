#include "mempool.h"
#include "miner.h"
#include "test_env.h"
#include "wallet.h"

#include <chrono>
#include <gtest/gtest.h>

class TestWallet : public testing::Test {
public:
    const std::string dir  = "test_wallet/";
    const std::string path = "test_wallet_data/";
    const uint32_t period  = 600;
    TestFactory fac;

    void SetUp() override {}

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
        MEMPOOL   = std::make_unique<MemPool>();
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
        wallet.OnLvsConfirmed({record}, {{utxo->GetKey(), utxo}}, {});

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
        auto outputPoint = new_block.GetTransaction()->GetInputs()[0].outpoint;
        auto stxokey     = XOR(outputPoint.bHash, outputPoint.index);
        ASSERT_EQ(stxokey, utxo->GetKey());

        std::unordered_map<uint256, UTXOPtr> utxos;

        int index = 0;
        for (auto& output : new_block.GetTransaction()->GetOutputs()) {
            auto putxo = std::make_shared<UTXO>(output, index);
            utxos.emplace(putxo->GetKey(), putxo);
            index++;
        }

        auto new_record      = std::make_shared<NodeRecord>(new_block);
        new_record->validity = NodeRecord::VALID;

        wallet.OnLvsConfirmed({new_record}, std::move(utxos), {stxokey});
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

TEST_F(TestWallet, work_flow) {
    EpicTestEnvironment::SetUpDAG(path);
    MEMPOOL = std::make_unique<MemPool>();
    MINER   = std::make_unique<Miner>();
    WALLET  = std::make_shared<Wallet>(dir, 0);
    DAG->RegisterOnLvsConfirmedListener(WALLET);
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

    while (WALLET->GetSpent().empty())
        ;
    ASSERT_EQ(WALLET->GetUnspent().size(), 1);
    ASSERT_EQ(WALLET->GetPendingTx().size(), 0);
    ASSERT_EQ(WALLET->GetPending().size(), 0);
    ASSERT_EQ(WALLET->GetSpent().size(), 1);

    MINER->Stop();
    MEMPOOL.reset();
    MINER.reset();
    WALLET->Stop();
    WALLET.reset();
    DAG->Stop();
    CAT->Stop();
    EpicTestEnvironment::TearDownDAG(path);
}

TEST_F(TestWallet, normal_workflow) {
    EpicTestEnvironment::SetUpDAG(path);
    MEMPOOL = std::make_unique<MemPool>();
    MINER   = std::make_unique<Miner>();
    WALLET  = std::make_shared<Wallet>(dir, 0);
    DAG->RegisterOnLvsConfirmedListener(WALLET);
    WALLET->Start();

    WALLET->CreateNewKey(false);

    MINER->Start();
    MINER->Run();


    WALLET->CreateRandomTx(4);
    // receive the change of last transaction
    while (true) {
        auto spent = WALLET->GetSpent();
        if (spent.size() == 1) {
            break;
        }
        std::this_thread::yield();
    }

    ASSERT_EQ(WALLET->GetUnspent().size(), 3);
    ASSERT_EQ(WALLET->GetPendingTx().size(), 0);
    ASSERT_EQ(WALLET->GetPending().size(), 0);
    ASSERT_EQ(WALLET->GetSpent().size(), 1);

    MINER->Stop();
    MEMPOOL.reset();
    MINER.reset();
    WALLET->Stop();
    WALLET.reset();
    DAG->Stop();
    CAT->Stop();
    EpicTestEnvironment::TearDownDAG(path);
}
