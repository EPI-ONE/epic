// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "test_env.h"
#include "transaction.h"

class TestTasm : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();
};

TEST_F(TestTasm, simple_listing) {
    VStream v;
    Tasm t;
    std::vector<uint8_t> p = {SUCCESS};
    Tasm::Listing l(p, v);
    ASSERT_TRUE(t.ExecListing(std::move(l)));
}

TEST_F(TestTasm, verify) {
    Tasm t;
    VStream v;

    CKey seckey         = CKey().MakeNewKey(true);
    CPubKey pubkey      = seckey.GetPubKey();
    std::string randstr = "frog learns chess";
    uint256 msg         = HashSHA2<1>(randstr.data(), randstr.size());
    std::vector<unsigned char> sig;
    seckey.Sign(msg, sig);

    v << pubkey << sig << msg << EncodeAddress(pubkey.GetID());

    std::vector<uint8_t> p = {VERIFY};
    Tasm::Listing l(p, std::move(v));
    ASSERT_TRUE(t.ExecListing(std::move(l)));
}

TEST_F(TestTasm, transaction_in_out_verify) {
    TestFactory fac{};
    VStream indata{}, outdata{};

    auto keypair        = fac.CreateKeyPair();
    CKeyID addr         = keypair.second.GetID();
    auto [hashMsg, sig] = fac.CreateSig(keypair.first);

    // construct transaction output listing
    auto encodedAddr = EncodeAddress(addr);
    outdata << encodedAddr;
    Tasm::Listing outputListing{Tasm::Listing{std::vector<uint8_t>{VERIFY}, std::move(outdata)}};

    // construct transaction input
    indata << keypair.second << sig << hashMsg;
    TxInput txin{Tasm::Listing{indata}};

    ASSERT_TRUE(VerifyInOut(txin, outputListing));
}

TEST_F(TestTasm, verify_bad_pubkeyhash) {
    Tasm t;
    VStream v;

    CKey seckey             = CKey().MakeNewKey(true);
    CKey maliciousSeckey    = CKey().MakeNewKey(true);
    CPubKey pubkey          = seckey.GetPubKey();
    CPubKey maliciousPubkey = maliciousSeckey.GetPubKey();
    std::string randstr     = "frog learns chess";
    uint256 msg             = HashSHA2<1>(randstr.data(), randstr.size());
    std::vector<unsigned char> sig;
    seckey.Sign(msg, sig);

    v << pubkey;
    v << sig;
    v << msg;
    v << EncodeAddress(maliciousPubkey.GetID());

    std::vector<uint8_t> p = {VERIFY};
    Tasm::Listing l(p, std::move(v));

    ASSERT_FALSE(t.ExecListing(std::move(l)));
}

TEST_F(TestTasm, verify_bad_signature) {
    Tasm t;
    VStream v;

    CKey seckey          = CKey().MakeNewKey(true);
    CKey maliciousSeckey = CKey().MakeNewKey(true);
    CPubKey pubkey       = seckey.GetPubKey();
    std::string randstr  = "frog learns chess";
    uint256 msg          = HashSHA2<1>(randstr.data(), randstr.size());
    std::vector<unsigned char> maliciousSig;
    maliciousSeckey.Sign(msg, maliciousSig);

    v << pubkey;
    v << maliciousSig;
    v << msg;
    v << EncodeAddress(pubkey.GetID());

    std::vector<uint8_t> p = {VERIFY};
    Tasm::Listing l(p, std::move(v));

    ASSERT_FALSE(t.ExecListing(std::move(l)));
}

TEST_F(TestTasm, continuous_verify) {
    Tasm t;
    VStream v;

    std::string randstr[3] = {"first random string", "second random string", "third random string"};
    for (int i = 0; i < 3; ++i) {
        CKey seckey    = CKey().MakeNewKey(true);
        CPubKey pubkey = seckey.GetPubKey();
        uint256 msg    = HashSHA2<1>(randstr[i].data(), randstr[i].size());
        std::vector<unsigned char> sig;
        seckey.Sign(msg, sig);

        v << pubkey;
        v << sig;
        v << msg;
        v << EncodeAddress(pubkey.GetID());
    }

    std::vector<uint8_t> p = {VERIFY, FAIL, VERIFY, FAIL, VERIFY};
    Tasm::Listing l(p, std::move(v));
    ASSERT_TRUE(t.ExecListing(std::move(l)));
}

TEST_F(TestTasm, continuous_verify_bad_pubkeyhash) {
    Tasm t;
    VStream v;

    std::string randstr[3] = {"first random string", "second random string", "third random string"};
    for (int i = 0; i < 3; ++i) {
        if (i != 2) {
            CKey seckey    = CKey().MakeNewKey(true);
            CPubKey pubkey = seckey.GetPubKey();
            uint256 msg    = HashSHA2<1>(randstr[i].data(), randstr[i].size());
            std::vector<unsigned char> sig;
            seckey.Sign(msg, sig);

            pubkey.Serialize(v);
            Serialize(v, sig);
            msg.Serialize(v);
            v << EncodeAddress(pubkey.GetID());
        } else {
            CKey seckey             = CKey().MakeNewKey(true);
            CKey maliciousSeckey    = CKey().MakeNewKey(true);
            CPubKey pubkey          = seckey.GetPubKey();
            CPubKey maliciousPubkey = maliciousSeckey.GetPubKey();
            uint256 msg             = HashSHA2<1>(randstr[i].data(), randstr[i].size());
            std::vector<unsigned char> sig;
            seckey.Sign(msg, sig);

            v << pubkey;
            v << sig;
            v << msg;
            v << EncodeAddress(maliciousPubkey.GetID());
        }
    }

    std::vector<uint8_t> p = {VERIFY, FAIL, VERIFY, FAIL, VERIFY};
    Tasm::Listing l(p, std::move(v));
    ASSERT_FALSE(t.ExecListing(std::move(l)));
}

TEST_F(TestTasm, continuous_verify_bad_signature) {
    Tasm t;
    VStream v;

    std::string randstr[3] = {"first random string", "second random string", "third random string"};
    for (int i = 0; i < 3; ++i) {
        if (i != 2) {
            CKey seckey    = CKey().MakeNewKey(true);
            CPubKey pubkey = seckey.GetPubKey();
            uint256 msg    = HashSHA2<1>(randstr[i].data(), randstr[i].size());
            std::vector<unsigned char> sig;
            seckey.Sign(msg, sig);

            pubkey.Serialize(v);
            Serialize(v, sig);
            msg.Serialize(v);
            v << EncodeAddress(pubkey.GetID());
        } else {
            CKey seckey          = CKey().MakeNewKey(true);
            CKey maliciousSeckey = CKey().MakeNewKey(true);
            CPubKey pubkey       = seckey.GetPubKey();
            std::string randstr  = "frog learns chess";
            uint256 msg          = HashSHA2<1>(randstr.data(), randstr.size());
            std::vector<unsigned char> maliciousSig;
            maliciousSeckey.Sign(msg, maliciousSig);

            v << pubkey;
            v << maliciousSig;
            v << msg;
            v << EncodeAddress(pubkey.GetID());
        }
    }

    std::vector<uint8_t> p = {VERIFY, FAIL, VERIFY, FAIL, VERIFY};
    Tasm::Listing l(p, std::move(v));
    ASSERT_FALSE(t.ExecListing(std::move(l)));
}
