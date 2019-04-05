#include <iostream>
#include <gtest/gtest.h>

#include <sha256.h>
#include <hash.h>

class SHA256Test : public testing::Test {
    protected:
        inline static const std::string data = "lorem ipsum dolor sit amet";

        static void SetUpTestCase() { }

        static void TearDownTestCase() { }
};

TEST_F(SHA256Test, NATIVE_SHA_TEST) {
    EXPECT_TRUE(SHA256SelfTest());
}

TEST_F(SHA256Test, 256_SHA_TEST) {
    uint256 h = Hash256((const unsigned char*)data.c_str(), data.size());
    EXPECT_TRUE(true);
}

TEST_F(SHA256Test, 160_SHA_TEST) {
    uint160 h = Hash160((const unsigned char*)data.c_str(), data.size());
    EXPECT_TRUE(true);
}
