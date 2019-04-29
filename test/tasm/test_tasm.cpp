#include "base58.h"
#include "hash.h"
#include "key.h"
#include "pubkey.h"
#include "serialize.h"
#include "stream.h"
#include "tasm/functors.hpp"
#include "tasm/opcodes.h"
#include "tasm/tasm.hpp"
#include "tinyformat.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include <gtest/gtest.h>


class TestTasm : public testing::Test {
protected:
    void SetUp() {
        ECC_Start();
    }

    void TearDown() {
        ECC_Stop();
    }
};

TEST_F(TestTasm, simple_listing) {
    VStream v;
    tasm t(functors);
    std::vector<uint8_t> p = {SUCCESS};
    tasm::listing l(p, v);
    ASSERT_TRUE(t.exec_listing(l));
}

TEST_F(TestTasm, verify) {
    tasm t(functors);
    VStream v;

    CKey seckey = CKey();
    seckey.MakeNewKey(true);
    CPubKey pubkey      = seckey.GetPubKey();
    uint160 pubkeyHash  = Hash160(pubkey.begin(), pubkey.end());
    std::string randstr = "frog learns chess";
    uint256 msg         = Hash<1>(randstr.cbegin(), randstr.cend());
    std::vector<unsigned char> sig;
    seckey.Sign(msg, sig);

    pubkeyHash.Serialize(v);
    pubkey.Serialize(v);
    Serialize(v, sig);
    msg.Serialize(v);

    std::vector<uint8_t> p = {VERIFY};
    tasm::listing l(p, v);
    ASSERT_TRUE(t.exec_listing(l));
}

TEST_F(TestTasm, verify_bad_pubkeyhash) {
    tasm t(functors);
    VStream v;

    CKey seckey          = CKey();
    CKey maliciousSeckey = CKey();
    seckey.MakeNewKey(true);
    maliciousSeckey.MakeNewKey(true);
    CPubKey pubkey              = seckey.GetPubKey();
    uint160 pubkeyHash          = Hash160(pubkey.begin(), pubkey.end());
    CPubKey maliciousPubkey     = maliciousSeckey.GetPubKey();
    uint160 maliciousPubkeyHash = Hash160(maliciousPubkey.begin(), maliciousPubkey.end());
    std::string randstr         = "frog learns chess";
    uint256 msg                 = Hash<1>(randstr.cbegin(), randstr.cend());
    std::vector<unsigned char> sig;
    seckey.Sign(msg, sig);

    v << maliciousPubkeyHash;
    v << pubkey;
    v << sig;
    v << msg;

    std::vector<uint8_t> p = {VERIFY};
    tasm::listing l(p, v);

    ASSERT_FALSE(t.exec_listing(l));
}

TEST_F(TestTasm, verify_bad_signature) {
    tasm t(functors);
    VStream v;

    CKey seckey          = CKey();
    CKey maliciousSeckey = CKey();
    seckey.MakeNewKey(true);
    maliciousSeckey.MakeNewKey(true);
    CPubKey pubkey      = seckey.GetPubKey();
    uint160 pubkeyHash  = Hash160(pubkey.begin(), pubkey.end());
    std::string randstr = "frog learns chess";
    uint256 msg         = Hash<1>(randstr.cbegin(), randstr.cend());
    std::vector<unsigned char> maliciousSig;
    maliciousSeckey.Sign(msg, maliciousSig);

    v << pubkeyHash;
    v << pubkey;
    v << maliciousSig;
    v << msg;

    std::vector<uint8_t> p = {VERIFY};
    tasm::listing l(p, v);

    ASSERT_FALSE(t.exec_listing(l));
}

TEST_F(TestTasm, continuous_verify) {
    tasm t(functors);
    VStream v;

    std::string randstr[3] = {"first random string", "second random string", "third random string"};
    for (int i = 0; i < 3; ++i) {
        CKey seckey = CKey();
        seckey.MakeNewKey(true);
        CPubKey pubkey     = seckey.GetPubKey();
        uint160 pubkeyHash = Hash160(pubkey.begin(), pubkey.end());
        uint256 msg        = Hash<1>(randstr[i].cbegin(), randstr[i].cend());
        std::vector<unsigned char> sig;
        seckey.Sign(msg, sig);

        v << pubkeyHash;
        v << pubkey;
        v << sig;
        v << msg;
    }

    std::vector<uint8_t> p = {VERIFY, FAIL, VERIFY, FAIL, VERIFY};
    tasm::listing l(p, v);
    ASSERT_TRUE(t.exec_listing(l));
}

TEST_F(TestTasm, continuous_verify_bad_pubkeyhash) {
    tasm t(functors);
    VStream v;

    std::string randstr[3] = {"first random string", "second random string", "third random string"};
    for (int i = 0; i < 3; ++i) {
        if (i != 2) {
            CKey seckey = CKey();
            seckey.MakeNewKey(true);
            CPubKey pubkey     = seckey.GetPubKey();
            uint160 pubkeyHash = Hash160(pubkey.begin(), pubkey.end());
            uint256 msg        = Hash<1>(randstr[i].cbegin(), randstr[i].cend());
            std::vector<unsigned char> sig;
            seckey.Sign(msg, sig);

            pubkeyHash.Serialize(v);
            pubkey.Serialize(v);
            Serialize(v, sig);
            msg.Serialize(v);
        } else {
            CKey seckey          = CKey();
            CKey maliciousSeckey = CKey();
            seckey.MakeNewKey(true);
            maliciousSeckey.MakeNewKey(true);
            CPubKey pubkey              = seckey.GetPubKey();
            uint160 pubkeyHash          = Hash160(pubkey.begin(), pubkey.end());
            CPubKey maliciousPubkey     = maliciousSeckey.GetPubKey();
            uint160 maliciousPubkeyHash = Hash160(maliciousPubkey.begin(), maliciousPubkey.end());
            uint256 msg                 = Hash<1>(randstr[i].cbegin(), randstr[i].cend());
            std::vector<unsigned char> sig;
            seckey.Sign(msg, sig);

            v << maliciousPubkeyHash;
            v << pubkey;
            v << sig;
            v << msg;
        }
    }

    std::vector<uint8_t> p = {VERIFY, FAIL, VERIFY, FAIL, VERIFY};
    tasm::listing l(p, v);
    ASSERT_FALSE(t.exec_listing(l));
}

TEST_F(TestTasm, continuous_verify_bad_signature) {
    tasm t(functors);
    VStream v;

    std::string randstr[3] = {"first random string", "second random string", "third random string"};
    for (int i = 0; i < 3; ++i) {
        if (i != 2) {
            CKey seckey = CKey();
            seckey.MakeNewKey(true);
            CPubKey pubkey     = seckey.GetPubKey();
            uint160 pubkeyHash = Hash160(pubkey.begin(), pubkey.end());
            uint256 msg        = Hash<1>(randstr[i].cbegin(), randstr[i].cend());
            std::vector<unsigned char> sig;
            seckey.Sign(msg, sig);

            pubkeyHash.Serialize(v);
            pubkey.Serialize(v);
            Serialize(v, sig);
            msg.Serialize(v);
        } else {
            CKey seckey          = CKey();
            CKey maliciousSeckey = CKey();
            seckey.MakeNewKey(true);
            maliciousSeckey.MakeNewKey(true);
            CPubKey pubkey      = seckey.GetPubKey();
            uint160 pubkeyHash  = Hash160(pubkey.begin(), pubkey.end());
            std::string randstr = "frog learns chess";
            uint256 msg         = Hash<1>(randstr.cbegin(), randstr.cend());
            std::vector<unsigned char> maliciousSig;
            maliciousSeckey.Sign(msg, maliciousSig);

            v << pubkeyHash;
            v << pubkey;
            v << maliciousSig;
            v << msg;
        }
    }

    std::vector<uint8_t> p = {VERIFY, FAIL, VERIFY, FAIL, VERIFY};
    tasm::listing l(p, v);
    ASSERT_FALSE(t.exec_listing(l));
}
