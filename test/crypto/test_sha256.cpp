#include <iostream>
#include <gtest/gtest.h>

#include <sha256.h>
#include <hash.h>

class SHA256Test : public testing::Test { };

TEST_F(SHA256Test, NATIVE_SHA_TEST) {
    EXPECT_TRUE(SHA256SelfTest());
}
