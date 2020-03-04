// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "opcodes.h"
#include "test_env.h"
#include "transaction.h"

using namespace tasm;

class TestTasm : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();
};

TEST_F(TestTasm, simple_listing) {
    VStream v;
    Tasm t;
    std::vector<uint8_t> p = {SUCCESS};
    tasm::Listing l(p, v);
    ASSERT_TRUE(t.Exec(std::move(l)));
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

    std::vector<uint8_t> p = {tasm::VERIFY};
    tasm::Listing l(p, std::move(v));
    ASSERT_TRUE(t.Exec(std::move(l)));
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
    tasm::Listing outputListing{tasm::Listing{std::vector<uint8_t>{tasm::VERIFY}, std::move(outdata)}};

    // construct transaction input
    indata << keypair.second << sig << hashMsg;
    TxInput txin{tasm::Listing{indata}};

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

    std::vector<uint8_t> p = {tasm::VERIFY};
    tasm::Listing l(p, std::move(v));

    ASSERT_FALSE(t.Exec(std::move(l)));
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

    std::vector<uint8_t> p = {tasm::VERIFY};
    tasm::Listing l(p, std::move(v));

    ASSERT_FALSE(t.Exec(std::move(l)));
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

    std::vector<uint8_t> p = {tasm::VERIFY, tasm::VERIFY, tasm::VERIFY};
    tasm::Listing l(p, std::move(v));
    ASSERT_TRUE(t.Exec(std::move(l)));
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

    std::vector<uint8_t> p = {tasm::VERIFY, tasm::VERIFY, tasm::VERIFY};
    tasm::Listing l(p, std::move(v));
    ASSERT_FALSE(t.Exec(std::move(l)));
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

    std::vector<uint8_t> p = {tasm::VERIFY, tasm::VERIFY, tasm::VERIFY};
    tasm::Listing l(p, std::move(v));
    ASSERT_FALSE(t.Exec(std::move(l)));
}

// select 2 from 3
TEST_F(TestTasm, multisig_verify) {
    std::vector<std::pair<CPubKey, std::pair<std::vector<unsigned char>, uint256>>> vin{};
    std::vector<std::string> vEncAddr{};

    std::string randstr[3] = {"first random string", "second random string", "third random string"};
    for (int i = 0; i < 3; ++i) {
        CKey seckey    = CKey().MakeNewKey(true);
        CPubKey pubkey = seckey.GetPubKey();
        uint256 msg    = HashSHA2<1>(randstr[i].data(), randstr[i].size());
        std::vector<unsigned char> sig;
        seckey.Sign(msg, sig);

        vEncAddr.emplace_back(EncodeAddress(pubkey.GetID()));
        vin.emplace_back(std::move(pubkey), std::make_pair(std::move(sig), std::move(msg)));
    }

    VStream outdata{}, indata1{}, indata2{}, indata3{}, indata4{}, indata5{};
    // construct transaction output listing
    outdata << static_cast<uint8_t>(2) << vEncAddr;
    tasm::Listing outputListing{tasm::Listing{std::vector<uint8_t>{tasm::MULTISIG}, std::move(outdata)}};

    // construct transaction inputs
    indata1 << std::vector<decltype(vin)::value_type>{vin[0]};
    ASSERT_FALSE(VerifyInOut(TxInput{tasm::Listing{indata1}}, outputListing));

    indata2 << std::vector<decltype(vin)::value_type>{vin[0], vin[1]};
    ASSERT_TRUE(VerifyInOut(TxInput{tasm::Listing{indata2}}, outputListing));

    indata3 << std::vector<decltype(vin)::value_type>{vin[0], vin[2]};
    ASSERT_TRUE(VerifyInOut(TxInput{tasm::Listing{indata3}}, outputListing));

    indata4 << std::vector<decltype(vin)::value_type>{vin[1], vin[2]};
    ASSERT_TRUE(VerifyInOut(TxInput{tasm::Listing{indata4}}, outputListing));

    indata5 << vin;
    ASSERT_FALSE(VerifyInOut(TxInput{tasm::Listing{indata5}}, outputListing));
}
