// Copyright (c) 2012-2018 The Bitcoin Core developers
// Copyright (c) 2019 EPIC
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cstdio>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "base58.h"
#include "hash.h"
#include "key.h"
#include "tinyformat.h"
#include "uint256.h"
#include "utilstrencodings.h"

class TestECKey : public testing::Test {
protected:
    const std::string randstr         = "frog learns chess";
    const std::string testStrSeckey1  = "5JN8uzRnFa6VHMiiRhuV23BgXDHtcVmQN7ymZb4FEapLRQG1KJa";
    const std::string testStrSeckey2  = "5J53YAc8zUM7aCcDN1vppc7Ecdesqp1UFVR1wgwmLCSSEStWmhs";
    const std::string testStrSeckey1C = "KyeHvtUNjK5DqCT7r3Yefae9SMFYofxz8DoZgqKKpxFviWqydoUT";
    const std::string testStrSeckey2C = "KxLrCZQ6Gfhn1YERZm6a57xX6GnBQSV5GxR1APJifjVGNLSRbaAm";
};

TEST_F(TestECKey, key_initial_sanity_test) {
    ASSERT_TRUE(ECC_InitSanityCheck());
}

TEST_F(TestECKey, key_workflow_test) {
    // prepare private key and public key
    CKey seckey = CKey();
    ASSERT_FALSE(seckey.IsValid() || seckey.IsCompressed());
    seckey.MakeNewKey(true);
    ASSERT_TRUE(seckey.IsValid() && seckey.IsCompressed());
    CPubKey pubkey = seckey.GetPubKey();
    ASSERT_TRUE(seckey.VerifyPubKey(pubkey));

    // make a uncompressed private key by the same keydata;
    CKey seckeyUC = CKey();
    seckeyUC.Set(seckey.begin(), seckey.end(), false);
    ASSERT_TRUE(seckeyUC.IsValid() && !seckeyUC.IsCompressed());

    // load a private key
    CKey seckeyDup = CKey();
    ASSERT_TRUE(seckeyDup.Load(seckey.GetPrivKey(), pubkey));
    ASSERT_EQ(seckey, seckeyDup);

    // signing
    uint256 hashMsg = HashSHA2<1>(&randstr, randstr.size());
    std::vector<unsigned char> detsig;
    ASSERT_TRUE(seckey.Sign(hashMsg, detsig));

    // verify signature through pubkey key
    ASSERT_TRUE(pubkey.IsValid() && pubkey.IsFullyValid());
    ASSERT_TRUE(pubkey.IsCompressed());
    ASSERT_TRUE(pubkey.Verify(hashMsg, detsig));

    // encoding and decoding
    std::string strSeckey = EncodeSecret(seckey);
    CKey decodeSeckey     = *DecodeSecret(strSeckey);
    ASSERT_EQ(seckey, decodeSeckey);
    std::string strAddr = EncodeAddress(pubkey.GetID());
    auto decodeAddr     = DecodeAddress(strAddr);
    ASSERT_EQ(pubkey.GetID(), *decodeAddr);

    // cross verification
    CPubKey pubkeyUC = seckeyUC.GetPubKey();
    ASSERT_NE(seckey, seckeyUC);
    ASSERT_NE(pubkey, pubkeyUC);
    ASSERT_FALSE(seckeyUC.VerifyPubKey(pubkey));
    ASSERT_FALSE(seckey.VerifyPubKey(pubkeyUC));

    std::vector<unsigned char> detsigUC;
    ASSERT_TRUE(seckeyUC.Sign(hashMsg, detsigUC));
    ASSERT_TRUE(pubkeyUC.Verify(hashMsg, detsigUC));
    ASSERT_TRUE(pubkey.Verify(hashMsg, detsigUC));
    ASSERT_TRUE(pubkeyUC.Verify(hashMsg, detsig));
}

TEST_F(TestECKey, key_regular_test) {
    CKey key1  = *DecodeSecret(testStrSeckey1);
    CKey key2  = *DecodeSecret(testStrSeckey2);
    CKey key1C = *DecodeSecret(testStrSeckey1C);
    CKey key2C = *DecodeSecret(testStrSeckey2C);
    ASSERT_TRUE(key1.IsValid() && !key1.IsCompressed());
    ASSERT_TRUE(key2.IsValid() && !key2.IsCompressed());
    ASSERT_TRUE(key1C.IsValid() && key1C.IsCompressed());
    ASSERT_TRUE(key2C.IsValid() && key2C.IsCompressed());

    CPubKey pubkey1  = key1.GetPubKey();
    CPubKey pubkey2  = key2.GetPubKey();
    CPubKey pubkey1C = key1C.GetPubKey();
    CPubKey pubkey2C = key2C.GetPubKey();

    ASSERT_TRUE(key1.VerifyPubKey(pubkey1));
    ASSERT_TRUE(!key1.VerifyPubKey(pubkey1C));
    ASSERT_TRUE(!key1.VerifyPubKey(pubkey2));
    ASSERT_TRUE(!key1.VerifyPubKey(pubkey2C));

    ASSERT_TRUE(!key1C.VerifyPubKey(pubkey1));
    ASSERT_TRUE(key1C.VerifyPubKey(pubkey1C));
    ASSERT_TRUE(!key1C.VerifyPubKey(pubkey2));
    ASSERT_TRUE(!key1C.VerifyPubKey(pubkey2C));

    ASSERT_TRUE(!key2.VerifyPubKey(pubkey1));
    ASSERT_TRUE(!key2.VerifyPubKey(pubkey1C));
    ASSERT_TRUE(key2.VerifyPubKey(pubkey2));
    ASSERT_TRUE(!key2.VerifyPubKey(pubkey2C));

    ASSERT_TRUE(!key2C.VerifyPubKey(pubkey1));
    ASSERT_TRUE(!key2C.VerifyPubKey(pubkey1C));
    ASSERT_TRUE(!key2C.VerifyPubKey(pubkey2));
    ASSERT_TRUE(key2C.VerifyPubKey(pubkey2C));

    for (int n = 0; n < 10; n++) {
        std::string strMsg = strprintf("EPIC secret number %i: 42", n);
        uint256 hashMsg    = HashSHA2<1>(&strMsg, strMsg.size());

        // normal signature
        std::vector<unsigned char> sign1, sign2, sign1C, sign2C;

        ASSERT_TRUE(key1.Sign(hashMsg, sign1));
        ASSERT_TRUE(key2.Sign(hashMsg, sign2));
        ASSERT_TRUE(key1C.Sign(hashMsg, sign1C));
        ASSERT_TRUE(key2C.Sign(hashMsg, sign2C));

        ASSERT_TRUE(pubkey1.Verify(hashMsg, sign1));
        ASSERT_TRUE(!pubkey1.Verify(hashMsg, sign2));
        ASSERT_TRUE(pubkey1.Verify(hashMsg, sign1C));
        ASSERT_TRUE(!pubkey1.Verify(hashMsg, sign2C));

        ASSERT_TRUE(!pubkey2.Verify(hashMsg, sign1));
        ASSERT_TRUE(pubkey2.Verify(hashMsg, sign2));
        ASSERT_TRUE(!pubkey2.Verify(hashMsg, sign1C));
        ASSERT_TRUE(pubkey2.Verify(hashMsg, sign2C));

        ASSERT_TRUE(pubkey1C.Verify(hashMsg, sign1));
        ASSERT_TRUE(!pubkey1C.Verify(hashMsg, sign2));
        ASSERT_TRUE(pubkey1C.Verify(hashMsg, sign1C));
        ASSERT_TRUE(!pubkey1C.Verify(hashMsg, sign2C));

        ASSERT_TRUE(!pubkey2C.Verify(hashMsg, sign1));
        ASSERT_TRUE(pubkey2C.Verify(hashMsg, sign2));
        ASSERT_TRUE(!pubkey2C.Verify(hashMsg, sign1C));
        ASSERT_TRUE(pubkey2C.Verify(hashMsg, sign2C));

        // compact signatures (with key recovery)
        std::vector<unsigned char> csign1, csign2, csign1C, csign2C;

        ASSERT_TRUE(key1.SignCompact(hashMsg, csign1));
        ASSERT_TRUE(key2.SignCompact(hashMsg, csign2));
        ASSERT_TRUE(key1C.SignCompact(hashMsg, csign1C));
        ASSERT_TRUE(key2C.SignCompact(hashMsg, csign2C));

        CPubKey rkey1, rkey2, rkey1C, rkey2C;

        ASSERT_TRUE(rkey1.RecoverCompact(hashMsg, csign1));
        ASSERT_TRUE(rkey2.RecoverCompact(hashMsg, csign2));
        ASSERT_TRUE(rkey1C.RecoverCompact(hashMsg, csign1C));
        ASSERT_TRUE(rkey2C.RecoverCompact(hashMsg, csign2C));

        ASSERT_EQ(rkey1, pubkey1);
        ASSERT_EQ(rkey2, pubkey2);
        ASSERT_EQ(rkey1C, pubkey1C);
        ASSERT_EQ(rkey2C, pubkey2C);
    }

    // test deterministic signing
    std::vector<unsigned char> detsig, detsigc;
    std::string strMsg = "Very deterministic message";
    uint256 hashMsg    = HashSHA2<1>(&strMsg, strMsg.size());
    ASSERT_TRUE(key1.Sign(hashMsg, detsig));
    ASSERT_TRUE(key1C.Sign(hashMsg, detsigc));
    ASSERT_EQ(detsig, detsigc);

    ASSERT_TRUE(key2.Sign(hashMsg, detsig));
    ASSERT_TRUE(key2C.Sign(hashMsg, detsigc));
    ASSERT_EQ(detsig, detsigc);

    ASSERT_TRUE(key1.SignCompact(hashMsg, detsig));
    ASSERT_TRUE(key1C.SignCompact(hashMsg, detsigc));
    ASSERT_NE(detsig, detsigc);

    ASSERT_TRUE(key2.SignCompact(hashMsg, detsig));
    ASSERT_TRUE(key2C.SignCompact(hashMsg, detsigc));
    ASSERT_NE(detsig, detsigc);
}
