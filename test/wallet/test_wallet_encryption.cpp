// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "config.h"
#include "crypter.h"
#include "mnemonics.h"
#include "test_env.h"
#include "wallet.h"

class TestWalletEncryption : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();
};

TEST_F(TestWalletEncryption, word_dictionary) {
    WordReader reader{};
    for (size_t i = 0; i < 100; i++) {
        auto rd    = fac.GetRand() % 2048;
        auto word  = reader.GetWord(rd);
        auto index = reader.GetIndex(*word);
        ASSERT_EQ(rd, *index);
    }

    ASSERT_FALSE(reader.GetIndex("dfew"));
    ASSERT_FALSE(reader.GetWord(2099));
}

TEST_F(TestWalletEncryption, mnemonics_and_crypter) {
    // create a random mnemonics
    CONFIG = std::make_unique<Config>();

    Mnemonics mne;
    mne.Generate();
    mne.PrintToFile(CONFIG->GetWalletPath());

    auto strs = mne.GetMnemonics();
    Mnemonics dupmne;
    ASSERT_TRUE(dupmne.Load(strs));

    strs.back() = "wrongword";
    Mnemonics wrongmne;
    ASSERT_FALSE(wrongmne.Load(strs));

    auto [masterMaterial, num] = mne.GetMasterKeyAndSeed();
    CKey master{};
    master.Set(masterMaterial.begin(), masterMaterial.end(), true);
    ASSERT_TRUE(master.IsValid());
    ASSERT_TRUE(master.IsCompressed());

    SecureByte masterdata{master.begin(), master.end()};
    Crypter crypter{};
    ASSERT_FALSE(crypter.IsReady());
    SecureString keydata{"random frog"};
    std::vector<unsigned char> salt = {14, 24, 242, 1, 192, 78, 45, 145};
    ASSERT_TRUE(crypter.SetKeyFromPassphrase(keydata, salt, 100));
    ASSERT_TRUE(crypter.IsReady());

    std::vector<unsigned char> ciphertext;
    ASSERT_TRUE(crypter.EncryptMaster(masterdata, ciphertext));

    auto [key, pubkey] = fac.CreateKeyPair();
    std::vector<unsigned char> crypted;
    ASSERT_TRUE(crypter.EncryptKey(masterdata, pubkey, key, crypted));

    CKey newkey{};
    ASSERT_TRUE(crypter.DecryptKey(masterdata, pubkey, crypted, newkey));
    ASSERT_EQ(key, newkey);

    // new crypter with correct passphrase to decrypt master
    Crypter newCrypter{};
    ASSERT_FALSE(newCrypter.IsReady());
    ASSERT_TRUE(newCrypter.SetKeyFromPassphrase(keydata, salt, 100));
    ASSERT_TRUE(newCrypter.IsReady());
    SecureByte masterDecrypted;
    ASSERT_TRUE(newCrypter.DecryptMaster(ciphertext, masterDecrypted));
    ASSERT_EQ(masterdata, masterDecrypted);

    // new crypter with wrong passphrase
    Crypter newCrypter_1{};
    SecureString wrongdata{"bad frog"};
    ASSERT_FALSE(newCrypter_1.IsReady());
    ASSERT_TRUE(newCrypter_1.SetKeyFromPassphrase(wrongdata, salt, 100));
    ASSERT_TRUE(newCrypter_1.IsReady());
    SecureByte wrongMasterDecrypted;
    ASSERT_FALSE(newCrypter_1.DecryptMaster(ciphertext, wrongMasterDecrypted));
    ASSERT_NE(masterdata, wrongMasterDecrypted);
}

TEST_F(TestWalletEncryption, wallet_encryption) {
    const std::string dir = "test_wallet_encryption/";
    Wallet wallet{dir, 0, 0};
    wallet.GenerateMaster();

    SecureString passphrase{"godsio"};
    ASSERT_TRUE(wallet.SetPassphrase(passphrase));

    SecureString wrongPhrase{"godsvoid"};
    SecureString newPhrase{"godsash"};
    ASSERT_NE(passphrase, wrongPhrase);
    ASSERT_FALSE(wallet.ChangePassphrase(wrongPhrase, newPhrase));
    ASSERT_TRUE(wallet.ChangePassphrase(passphrase, newPhrase));

    std::string cmd = "exec rm -r " + dir;
    system(cmd.c_str());
}
