#include <gtest/gtest.h>

#include "base58.h"
#include "hash.h"
#include "key.h"
#include "pubkey.h"
#include "serialize.h"
#include "stream.h"
#include "tasm/functors.h"
#include "tasm/opcodes.h"
#include "tasm/tasm.h"
#include "test_env.h"
#include "tinyformat.h"
#include "transaction.h"
#include "uint256.h"
#include "utilstrencodings.h"

class TestTasm : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();
};

TEST_F(TestTasm, simple_listing) {
    VStream v;
    Tasm t(functors);
    std::vector<uint8_t> p = {SUCCESS};
    Tasm::Listing l(p, v);
    ASSERT_TRUE(t.ExecListing(l));
}

TEST_F(TestTasm, verify) {
    Tasm t(functors);
    VStream v;

    CKey seckey = CKey();
    seckey.MakeNewKey(true);
    CPubKey pubkey      = seckey.GetPubKey();
    std::string randstr = "frog learns chess";
    uint256 msg         = Hash<1>(randstr.cbegin(), randstr.cend());
    std::vector<unsigned char> sig;
    seckey.Sign(msg, sig);

    v << pubkey << sig << msg << EncodeAddress(pubkey.GetID());

    std::vector<uint8_t> p = {VERIFY};
    Tasm::Listing l(p, v);
    ASSERT_TRUE(t.ExecListing(l));
}

TEST_F(TestTasm, transaction_in_out_verify) {
    TestFactory fac{};
    Tasm tasm(functors);
    VStream indata{}, outdata{};

    auto keypair = fac.CreateKeyPair();
    CKeyID addr  = keypair.second.GetID();
    auto [hashMsg, sig] = fac.CreateSig(keypair.first);

    // construct transaction output listing
    auto encodedAddr = EncodeAddress(addr);
    outdata << encodedAddr;
    Tasm::Listing outputListing{Tasm::Listing{std::vector<uint8_t>{VERIFY}, outdata}};

    // construct transaction input
    indata << keypair.second << sig << hashMsg;
    TxInput txin{Tasm::Listing{indata}};

    ASSERT_TRUE(VerifyInOut(txin, outputListing));
}

TEST_F(TestTasm, verify_bad_pubkeyhash) {
    Tasm t(functors);
    VStream v;

    CKey seckey          = CKey();
    CKey maliciousSeckey = CKey();
    seckey.MakeNewKey(true);
    maliciousSeckey.MakeNewKey(true);
    CPubKey pubkey              = seckey.GetPubKey();
    CPubKey maliciousPubkey     = maliciousSeckey.GetPubKey();
    std::string randstr         = "frog learns chess";
    uint256 msg                 = Hash<1>(randstr.cbegin(), randstr.cend());
    std::vector<unsigned char> sig;
    seckey.Sign(msg, sig);

    v << pubkey;
    v << sig;
    v << msg;
    v << EncodeAddress(maliciousPubkey.GetID());

    std::vector<uint8_t> p = {VERIFY};
    Tasm::Listing l(p, v);

    ASSERT_FALSE(t.ExecListing(l));
}

TEST_F(TestTasm, verify_bad_signature) {
    Tasm t(functors);
    VStream v;

    CKey seckey          = CKey();
    CKey maliciousSeckey = CKey();
    seckey.MakeNewKey(true);
    maliciousSeckey.MakeNewKey(true);
    CPubKey pubkey      = seckey.GetPubKey();
    std::string randstr = "frog learns chess";
    uint256 msg         = Hash<1>(randstr.cbegin(), randstr.cend());
    std::vector<unsigned char> maliciousSig;
    maliciousSeckey.Sign(msg, maliciousSig);

    v << pubkey;
    v << maliciousSig;
    v << msg;
    v << EncodeAddress(pubkey.GetID());

    std::vector<uint8_t> p = {VERIFY};
    Tasm::Listing l(p, v);

    ASSERT_FALSE(t.ExecListing(l));
}

TEST_F(TestTasm, continuous_verify) {
    Tasm t(functors);
    VStream v;

    std::string randstr[3] = {"first random string", "second random string", "third random string"};
    for (int i = 0; i < 3; ++i) {
        CKey seckey = CKey();
        seckey.MakeNewKey(true);
        CPubKey pubkey     = seckey.GetPubKey();
        uint256 msg        = Hash<1>(randstr[i].cbegin(), randstr[i].cend());
        std::vector<unsigned char> sig;
        seckey.Sign(msg, sig);

        v << pubkey;
        v << sig;
        v << msg;
        v << EncodeAddress(pubkey.GetID());
    }

    std::vector<uint8_t> p = {VERIFY, FAIL, VERIFY, FAIL, VERIFY};
    Tasm::Listing l(p, v);
    ASSERT_TRUE(t.ExecListing(l));
}

TEST_F(TestTasm, continuous_verify_bad_pubkeyhash) {
    Tasm t(functors);
    VStream v;

    std::string randstr[3] = {"first random string", "second random string", "third random string"};
    for (int i = 0; i < 3; ++i) {
        if (i != 2) {
            CKey seckey = CKey();
            seckey.MakeNewKey(true);
            CPubKey pubkey     = seckey.GetPubKey();
            uint256 msg        = Hash<1>(randstr[i].cbegin(), randstr[i].cend());
            std::vector<unsigned char> sig;
            seckey.Sign(msg, sig);

            pubkey.Serialize(v);
            Serialize(v, sig);
            msg.Serialize(v);
            v << EncodeAddress(pubkey.GetID());
        } else {
            CKey seckey          = CKey();
            CKey maliciousSeckey = CKey();
            seckey.MakeNewKey(true);
            maliciousSeckey.MakeNewKey(true);
            CPubKey pubkey              = seckey.GetPubKey();
            CPubKey maliciousPubkey     = maliciousSeckey.GetPubKey();
            uint256 msg                 = Hash<1>(randstr[i].cbegin(), randstr[i].cend());
            std::vector<unsigned char> sig;
            seckey.Sign(msg, sig);

            v << pubkey;
            v << sig;
            v << msg;
            v << EncodeAddress(maliciousPubkey.GetID());
        }
    }

    std::vector<uint8_t> p = {VERIFY, FAIL, VERIFY, FAIL, VERIFY};
    Tasm::Listing l(p, v);
    ASSERT_FALSE(t.ExecListing(l));
}

TEST_F(TestTasm, continuous_verify_bad_signature) {
    Tasm t(functors);
    VStream v;

    std::string randstr[3] = {"first random string", "second random string", "third random string"};
    for (int i = 0; i < 3; ++i) {
        if (i != 2) {
            CKey seckey = CKey();
            seckey.MakeNewKey(true);
            CPubKey pubkey     = seckey.GetPubKey();
            uint256 msg        = Hash<1>(randstr[i].cbegin(), randstr[i].cend());
            std::vector<unsigned char> sig;
            seckey.Sign(msg, sig);

            pubkey.Serialize(v);
            Serialize(v, sig);
            msg.Serialize(v);
            v << EncodeAddress(pubkey.GetID());
        } else {
            CKey seckey          = CKey();
            CKey maliciousSeckey = CKey();
            seckey.MakeNewKey(true);
            maliciousSeckey.MakeNewKey(true);
            CPubKey pubkey      = seckey.GetPubKey();
            std::string randstr = "frog learns chess";
            uint256 msg         = Hash<1>(randstr.cbegin(), randstr.cend());
            std::vector<unsigned char> maliciousSig;
            maliciousSeckey.Sign(msg, maliciousSig);

            v << pubkey;
            v << maliciousSig;
            v << msg;
            v << EncodeAddress(pubkey.GetID());
        }
    }

    std::vector<uint8_t> p = {VERIFY, FAIL, VERIFY, FAIL, VERIFY};
    Tasm::Listing l(p, v);
    ASSERT_FALSE(t.ExecListing(l));
}
