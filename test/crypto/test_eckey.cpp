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
    const std::string randstr = "frog learns chess";

    void SetUp() { ECC_Start(); }

    void TearDown() { ECC_Stop(); }
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

    // load a private key
    CKey seckeyDup = CKey();
    ASSERT_TRUE(seckeyDup.Load(seckey.GetPrivKey(), pubkey));
    ASSERT_EQ(seckey, seckeyDup);

    // signing
    uint256 hashMsg = Hash<1>(randstr.cbegin(), randstr.cend());
    std::vector<unsigned char> detsig;
    ASSERT_TRUE(seckey.Sign(hashMsg, detsig));

    // verify signature through pubkey key
    ASSERT_TRUE(pubkey.IsValid() && pubkey.IsFullyValid());
    ASSERT_TRUE(pubkey.IsCompressed());
    ASSERT_TRUE(pubkey.Verify(hashMsg, detsig));

    // encoding and decoding
    std::string strSeckey = EncodeSecret(seckey);
    CKey decodeSeckey     = DecodeSecret(strSeckey);
    ASSERT_EQ(seckey, decodeSeckey);
}
