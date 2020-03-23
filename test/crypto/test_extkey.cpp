// Copyright (c) 2013-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "extended_key.h"
#include "stream.h"
#include "utilstrencodings.h"

#include <string>
#include <vector>

class TestKeyDerivation : public testing::Test {
public:
    struct TestDerivation {
        std::string pub;
        std::string prv;
        unsigned int nChild;

        TestDerivation(std::string _prv, std::string _pub, uint32_t _nChild) : pub(_pub), prv(_prv), nChild(_nChild) {}
    };

    struct TestVector {
        std::string strHexMaster;
        std::vector<TestDerivation> vDerive;

        explicit TestVector(std::string strHexMasterIn) : strHexMaster(strHexMasterIn) {}
        TestVector& operator()(std::string prv, std::string pub, unsigned int nChild) {
            vDerive.emplace_back(prv, pub, nChild);
            return *this;
        }
    };


    std::array<TestVector, 3> testData = {
        // clang-format off
        TestVector{"000102030405060708090a0b0c0d0e0f"}
            ("NCApQUytpwKpJVDJn5e3TdE4aPjWM3McPQ3zkbLwUkiLxVurSupvieBgC2R8QeJL76FvyhTPhGssHmpGp8AftHwcus7gRcJ33YrhrBRZBR75",
             "WeBCq8PkVFTKM99atVPm5wD9yQQdYfh8egorRD7G2Yv2L9XQ9rBmgyBMuDHHGAPjprYQwn7xbspXtVXFgVoxz8nUk8djdamAz5VnxuoZGbY2",
             0x80000000)
            ("NCE9caJrGTSdiCk29Rogt6zePVdCqdtLMNV6mPBBzxch5b7gkDQB3WrPNZwZ2H87aZkBRu4xBQoiyqVbwDfuWPR6eBRj2Gq2vVTggr1wY33R",
             "WeEY3Dihvma8krgJFqZQWQyjnWJL3GDrcfExRzwWYkpNTEjET9m21qr55kpGyXStkM5cTHw5MrgzQafB2iMRrwyXr3326DA3uBU7ZA2wqMxg",
             1)
            ("NCEqzgN1arinzCJeYpbCAMHGvbBtrbds7eRFxTgS5AHM5jUmadtgA9geE1vq5Vfgvx1AgNq44Ct8DkiFbHL5z1kQUxyDyXJjBXVduKD4hfNV",
             "WeFERKmsFArJ2rEvfELunfGNKbs24DyPNwB7d5SkcxV2TP6KHaFX8UgKwCoXQVwyXsfNLDHJijVKu1nr5whiGHhpftv2t2MxBQwTohAjk2eJ",
             0x80000002)
            ("NCGzyrCzkPfdtz1d3kwSB8NUUnBHzEC57e7f9afJR94h4ahuzhPck1BFa7vgKn1tWAJ9cBuwvozhgo4tJrWFHjBwvyL2yXxYKStXwUaFTLEB",
             "WeHPQVcrQho8wdwuAAh9oSMZsnrRBrXbNvsWpCRcxwGNSEKThdkTiLAwHJnk9bxG1JrzCvqWCynbciHrAvfs1mP6D5W7rL3SXbbgSfb4hgad",
             2)
            ("NCJoHiyr7h46BkJCxmanUfwkZK3gJ5VyQ5t6eSxtDHXzZmTKjjVZkkskVpcsHhQKDCQhM4Cvc6dtG2UC4scdCeqHmnkMoFMydcmPqucnefmR",
             "WeKBiNPhn1BbEQEV5BLW6yvqxKioVhqVfNdxK4jCm5jfwR4sSfrQj5sSD1WVWzZZPxJ4viH7xAPWaBF39RW5NuLt8oQnQbhFLYUCMXqE5mck",
             1000000000)
            ("NCLdXrtDXniynFpedsXxmwbzKBXB2v2QQ3PsH5Z1MeGE4byDJbw2uGwsSN8YanfWK2zdVGFdCzXKRg6uambApVS7a1keRgTBxGaYgcDpsooj",
             "WeM1xWJ5C6rUpukvkHHgQFb5iCCJEYMvfL9iwhKKuSTuSFam1YHssbwZ9Z1RZEebS5NcJCDLw5UiQkTxJqMXixKcakhGB2LiPgEr7pohTvca",
             0),

        TestVector
            {"fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c69666360"
                "5d5a5754514e4b484542"}
            ("NCApQUytpwKpJWp6ydwZe9Adj3jokfnLEVYFZupKUodBNvrhDKRgduWhvXTKsKGEUJYSqmMBbt9Ca6h5BbuA4nt6ixcJ83EaG8Akz2xmN27J",
             "WeBCq8PkVFTKMAkP63hHGT9j84QvxJ7rVnJ7EXae2bprkaUEvFnXcEWPdiKS4oXiTpSjZgrWNPBgF3QDtmpRwUixAzcTyDDi41pKw9VGRBoa",
             0)
            ("NCEF2qFBZNkXe3b45GEK4sAvTZnfqmNaEGs84XSocTRiKkNFsMniJGhajEFbCAmXYrHNJ3kZNTDx4sF94sUpBPjwWCfD4vJatdPAr7hGc9fh",
             "WeEdTUf3Dgt2ghXLBfz2hBA1raTo3Pi6VZcyj9D8AFdPhPyoaJ9ZGbhGSR8xrEY2AR5SGPucpB9KHPQapP8NRuXmzBq54ppxUUUcHT5qdEwz",
             0xFFFFFFFF)
            ("NCGFTv32RvYWPN3QoZjB4wnwQZo7kLS1DJ9e1H7CVf4zNSJyCp4GK4sKRtnFH68bysPWHdyjuP4UvzuM8SyMTYxZw65H3uXHJGx2FUj7Y1Yp",
             "WeGdtZSt6Eg1S1yguyUthFn2oaUEwxmXUauVftsX3TGfk5vWukR7HPs195cZJ7s8okhZvqhEaJ4WJJAWxW8HET4b8BiFyo9i8oLLSDKXWtQx",
             1)
            ("NCGnw4n8jrKdXsp6WdqkhF7ecVoJ9xkchEjrk2YzV5QevgzUBUxKLLNSqPGcXtDF6cKWYg3zcMgxM21TzWoy6UQWnBtgzao7nPVe3UH4ywjP",
             "WeHBMiBzQAT8aXkNd3bUKZ6k1WURMb68xXViQeKK2scLJLc1tRKAJfN8Ya9mdFnshCVTtX4bKtKoN8uKCFKSAF7QAspMXroAMLMR8L9Sqry7",
             0xFFFFFFFE)
            ("NCJpytwQDvqbUpob3AzbSVu9CyAcByLLRWh4BgXLh4HE1gKHTqUhCd2rG36T8bFDDKaNVNZeJGNj8vGP4Vvn3TLRwQjABBoBHXrPXhub6TgQ",
             "WeKDQYMFtEy6XUjs9akK4otEbyqjPbfrgoSurJHfErUuPKvqAmqYAx2XyDx8hbjRVvwuggDvaWwtzS2f7Tid4Pk7aLSxPSv8sXUwEKHjDTag",
             2)
            ("NCMnoMv1Ym1Suz3L76LYhLw4KJPoy5CdY2nbDyYi7rBGanUkSSQiq9keShPfkQDyhL8Sa7XQ53j2y5PQns3ykrGM2SwpgbAVkwCFfdVnA98L",
             "WeNBE1KsD58wxdycDW6GKev9iK4wAhY9oKYStbK2feNwxS6J9NmZoUkL9tDhmbkq3msdfdBtRprabDF3Y1MHjBjgnvSDrmA7oLZppJ7FVnjb",
             0),

        TestVector
            {"4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d14"
                "57df2e5a3c51c73235be"}
            ("NCApQUytpwKpJV6nfDutVn3LqC684bx2545MuCEeGa7yNWM2iU8eHEBL48QKbUE88uezaqyKQ3cNnSGNZzkpPsxGHUVSzWshPFfdLKsc7BHK",
             "WeBCq8PkVFTKM934mdfc862SECmFGEHYLLqDZozxpNKek9xaRQVVFZB1mKGdEbXP8kadkonvUVnB6fVpM2rBjwpfUU2Qnz3B5ngUGiiXy47a",
             0x80000000)
            ("NCCtDPRqQj8uyBCLvSpN1eyqmsoGJ8i49VQF53L7ktUTfhf8tMPFp6RePtn8UVx8pp1YVLdUXkzDbpSfatUWZ7cygU5vLptbJYpAoCQ4HjPR",
             "WeDGe2qh53GR1q8d2ra5dxxwAtUPVm3aQnA6jf6SJgg93MGgbHk6nRRL75cBdg9QN1bJe5wLLxeCixjHJDdKCmtqTTmRo7r7NW5QS65zE5Cy",
             0)
        // clang-format on
    };
};

TEST_F(TestKeyDerivation, derivation_workflow_test) {
    for (const auto& test : testData) {
        std::vector<unsigned char> seed = ParseHex(test.strHexMaster);
        CExtKey key;
        CExtPubKey pubkey;
        key.SetSeed(seed.data(), seed.size());
        pubkey = key.Neuter();
        for (const TestDerivation &derive : test.vDerive) {
            // Test private key
            auto encKey = EncodeExtKey(key);
            EXPECT_EQ(encKey, derive.prv);
            auto oDecKey = DecodeExtKey(derive.prv);
            ASSERT_TRUE(oDecKey.has_value());
            EXPECT_EQ(oDecKey.value(), key); 

            // Test public key
            auto encPub = EncodeExtPubKey(pubkey);
            ASSERT_EQ(encPub, derive.pub);
            auto oDecPub = DecodeExtPubKey(derive.pub);
            ASSERT_TRUE(oDecPub.has_value());
            ASSERT_EQ(oDecPub.value(), pubkey);

            // Derive new keys
            CExtKey keyNew;
            EXPECT_TRUE(key.Derive(keyNew, derive.nChild));
            CExtPubKey pubkeyNew = keyNew.Neuter();
            if (!(derive.nChild & 0x80000000)) {
                // Compare with public derivation
                CExtPubKey pubkeyNew2;
                EXPECT_TRUE(pubkey.Derive(pubkeyNew2, derive.nChild));
                EXPECT_EQ(pubkeyNew ,pubkeyNew2);
            }
            key = keyNew;
            pubkey = pubkeyNew;
        }
    }
}

TEST_F(TestKeyDerivation, parse_hdkey) {
    std::vector<uint32_t> keypath;

    ASSERT_TRUE(ParseHDKeypath("1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1", keypath));
    ASSERT_TRUE(!ParseHDKeypath("///////////////////////////", keypath));

    ASSERT_TRUE(ParseHDKeypath("1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1'/1", keypath));
    ASSERT_TRUE(!ParseHDKeypath("//////////////////////////'/", keypath));

    ASSERT_TRUE(ParseHDKeypath("1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/", keypath));
    ASSERT_TRUE(!ParseHDKeypath("1///////////////////////////", keypath));

    ASSERT_TRUE(ParseHDKeypath("1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1/1'/", keypath));
    ASSERT_TRUE(!ParseHDKeypath("1/'//////////////////////////", keypath));

    ASSERT_TRUE(ParseHDKeypath("", keypath));
    ASSERT_TRUE(!ParseHDKeypath(" ", keypath));

    ASSERT_TRUE(ParseHDKeypath("0", keypath));
    ASSERT_TRUE(!ParseHDKeypath("O", keypath));

    ASSERT_TRUE(ParseHDKeypath("0000'/0000'/0000'", keypath));
    ASSERT_TRUE(!ParseHDKeypath("0000,/0000,/0000,", keypath));

    ASSERT_TRUE(ParseHDKeypath("01234", keypath));
    ASSERT_TRUE(!ParseHDKeypath("0x1234", keypath));

    ASSERT_TRUE(ParseHDKeypath("1", keypath));
    ASSERT_TRUE(!ParseHDKeypath(" 1", keypath));

    ASSERT_TRUE(ParseHDKeypath("42", keypath));
    ASSERT_TRUE(!ParseHDKeypath("m42", keypath));

    ASSERT_TRUE(ParseHDKeypath("4294967295", keypath)); // 4294967295 == 0xFFFFFFFF (uint32_t max)
    ASSERT_TRUE(!ParseHDKeypath("4294967296", keypath)); // 4294967296 == 0xFFFFFFFF (uint32_t max) + 1

    ASSERT_TRUE(ParseHDKeypath("m", keypath));
    ASSERT_TRUE(!ParseHDKeypath("n", keypath));

    ASSERT_TRUE(ParseHDKeypath("m/", keypath));
    ASSERT_TRUE(!ParseHDKeypath("n/", keypath));

    ASSERT_TRUE(ParseHDKeypath("m/0", keypath));
    ASSERT_TRUE(!ParseHDKeypath("n/0", keypath));

    ASSERT_TRUE(ParseHDKeypath("m/0'", keypath));
    ASSERT_TRUE(!ParseHDKeypath("m/0''", keypath));

    ASSERT_TRUE(ParseHDKeypath("m/0'/0'", keypath));
    ASSERT_TRUE(!ParseHDKeypath("m/'0/0'", keypath));

    ASSERT_TRUE(ParseHDKeypath("m/0/0", keypath));
    ASSERT_TRUE(!ParseHDKeypath("n/0/0", keypath));

    ASSERT_TRUE(ParseHDKeypath("m/0/0/00", keypath));
    ASSERT_TRUE(!ParseHDKeypath("m/0/0/f00", keypath));

    ASSERT_TRUE(ParseHDKeypath("m/0/0/000000000000000000000000000000000000000000000000000000000000000000000000000000000000", keypath));
    ASSERT_TRUE(!ParseHDKeypath("m/1/1/111111111111111111111111111111111111111111111111111111111111111111111111111111111111", keypath));

    ASSERT_TRUE(ParseHDKeypath("m/0/00/0", keypath));
    ASSERT_TRUE(!ParseHDKeypath("m/0'/00/'0", keypath));

    ASSERT_TRUE(ParseHDKeypath("m/1/", keypath));
    ASSERT_TRUE(!ParseHDKeypath("m/1//", keypath));

    ASSERT_TRUE(ParseHDKeypath("m/0/4294967295", keypath)); // 4294967295 == 0xFFFFFFFF (uint32_t max)
    ASSERT_TRUE(!ParseHDKeypath("m/0/4294967296", keypath)); // 4294967296 == 0xFFFFFFFF (uint32_t max) + 1

    ASSERT_TRUE(ParseHDKeypath("m/4294967295", keypath)); // 4294967295 == 0xFFFFFFFF (uint32_t max)
    ASSERT_TRUE(!ParseHDKeypath("m/4294967296", keypath)); // 4294967296 == 0xFFFFFFFF (uint32_t max) + 1

}
