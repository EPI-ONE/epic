#include <gtest/gtest.h>

#include "aes.h"
#include "test_env.h"
#include "utilstrencodings.h"

class TestAES256 : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();

    // source of test vectors from
    // https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program/block-ciphers#AES
    std::vector<std::string> keys{"632bac4fe4db44cfcf18cfa90b43f86f378611b8d968595eb89e7ae98624564a",
                                  "c7b8fb8a3bb2985143909d189bfa0c0f731212b3c7ead6095bd7b137e2bdfdb9",
                                  "8eb04615677eaa057afe2408bf526f77743dcb6c6756514065f58550189859b2"};
    std::vector<std::string> ivs{"ff8127621be616803e3f002377730185", "4494030b1e828f57e349cbde6499abf3",
                                 "072fd9dfa0bc87493e223467fa25a40b"};
    std::vector<std::string> texts{"90ed17475f0a62bc381ba1f3ffbfff33", "a49357c5df69dc9e8c8852b190b9f460",
                                   "4908bd9f5ccc3254396eb91024a86378"};
    std::vector<std::string> results{"c4c51bb178814440f25994c287255626", "b5696b2f8db50687e31a064db108cc9d",
                                     "c310e4cd5349fdbc78b1d8c99b2c9e55"};

    std::string RunAES256CBCEnc(const std::string& hexkey,
                             const std::string& hexiv,
                             const std::string& hextext,
                             bool padding) {
        std::vector<unsigned char> key = ParseHex(hexkey);
        std::vector<unsigned char> iv = ParseHex(hexiv);
        std::vector<unsigned char> text = ParseHex(hextext);
        std::vector<unsigned char> output(text.size() + AES_BLOCKSIZE);

        // Encrypt the plaintext and verify that it equals the cipher
        AES256CBCEncrypt encryptor{key.data(), iv.data(), padding};
        int size = encryptor.Encrypt(text.data(), text.size(), output.data());
        output.resize(size);

        return HexStr(output);
    }

    std::string RunAES256CBCDec(const std::string& hexkey,
                             const std::string& hexiv,
                             const std::string& hexoutput,
                             bool padding) {
        std::vector<unsigned char> key = ParseHex(hexkey);
        std::vector<unsigned char> iv = ParseHex(hexiv);
        std::vector<unsigned char> output = ParseHex(hexoutput);
        std::vector<unsigned char> text(output.size());

        // Encrypt the plaintext and verify that it equals the cipher
        AES256CBCDecrypt decryptor{key.data(), iv.data(), padding};
        int size = decryptor.Decrypt(output.data(), output.size(), text.data());
        text.resize(size);

        return HexStr(text);
    }
};

TEST_F(TestAES256, aes256CBC_encryption_decryption) {
    for (int i = 0; i < keys.size(); i++) {
        auto encResult = RunAES256CBCEnc(keys[i], ivs[i], texts[i], false);
        ASSERT_EQ(encResult, results[i]);

        auto decResult = RunAES256CBCDec(keys[i], ivs[i], results[i], false);
        ASSERT_EQ(decResult, texts[i]);
    }
}

TEST_F(TestAES256, aes256CBC_random_str_enc_dec) {
    for (int i = 0; i < 10; i++) {
        auto randstr = fac.GetRandomString(30);
        auto hexstr  = HexStr(randstr.begin(), randstr.begin() + AES_BLOCKSIZE);
        auto tempstr = fac.GetRandomString(40);
        auto hexkey  = HexStr(tempstr.begin(), tempstr.begin() + AES256_KEYSIZE);
        tempstr      = fac.GetRandomString(30);
        auto hexiv   = HexStr(tempstr.begin(), tempstr.begin() + AES_BLOCKSIZE);

        auto encResult = RunAES256CBCEnc(hexkey, hexiv, hexstr, false);
        auto decResult = RunAES256CBCDec(hexkey, hexiv, encResult, false);
        ASSERT_EQ(hexstr, decResult);

        auto encResultP = RunAES256CBCEnc(hexkey, hexiv, hexstr, true);
        auto decResultP = RunAES256CBCDec(hexkey, hexiv, encResultP, true);
        ASSERT_EQ(hexstr, decResultP);

        auto encResultPP = RunAES256CBCEnc(hexkey, hexiv, hexstr, true);
        auto decResultNP = RunAES256CBCDec(hexkey, hexiv, encResultPP, false);
        ASSERT_NE(hexstr, decResultNP);

        auto encResultNP = RunAES256CBCEnc(hexkey, hexiv, hexstr, false);
        auto decResultPP = RunAES256CBCDec(hexkey, hexiv, encResultNP, true);
        ASSERT_NE(hexstr, decResultPP);
    }
}
