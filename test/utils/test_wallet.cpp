#include "test_factory.h"
#include "wallet.h"
#include <gtest/gtest.h>

class TestWallet : public testing::Test {
public:
    TestFactory factory;
};

TEST_F(TestWallet, test_update_utxo) {
    Wallet wallet;
    auto key_pair = factory.CreateKeyPair(true);
    wallet.ImportKey(key_pair.first, key_pair.second);
    auto addr = key_pair.second.GetID();
    Transaction tx;
    tx.AddOutput(100, addr);
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

    ASSERT_EQ(wallet.GetBalance().GetValue(), 100);
}