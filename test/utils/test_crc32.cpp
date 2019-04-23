#include "crc32.h"
#include <gtest/gtest.h>


class TestCrc32 : public testing::Test {
    void SetUp() {}

    void TearDown() {}
};

TEST_F(TestCrc32, simple_string) {
    char data[]     = "123456789";
    uint32_t result = getCrc32((unsigned char*) data, strlen(data));
    EXPECT_EQ(result, 0xCBF43926);
}

TEST_F(TestCrc32, long_string) {
    char data[] = "j3PTmgdHwBVdqCS0MS61TeEFbvMnW0MYEhGnIrD56Oi4W2p8KI6B6uZbyzAu"
                  "NH0xBTvobg5E2ssjRCFEyi4iqCLH8Dhv3h1dBk5";
    uint32_t result = getCrc32((unsigned char*) data, strlen(data));
    EXPECT_EQ(result, 0x6117A242);
}

TEST_F(TestCrc32, simple_hex) {
    unsigned char data[] = {0x12, 0x34, 0x56, 0x78};
    uint32_t result      = getCrc32(data, sizeof(data));
    EXPECT_EQ(result, 0x4A090E98);
}

TEST_F(TestCrc32, long_hex) {
    unsigned char data[] = {0xe4, 0xa0, 0x53, 0xf6, 0xa8, 0x13, 0x6a, 0x71,
        0xfc, 0xb2, 0x77, 0x8f, 0xcc, 0xd6, 0x89, 0x97, 0xf5, 0xa2, 0xff, 0x1a,
        0x02, 0xac, 0xfb, 0x68, 0x62, 0xd4, 0x08, 0x04, 0x98, 0x72, 0xae, 0xee};
    uint32_t result      = getCrc32(data, sizeof(data));
    EXPECT_EQ(result, 0xAAF06E25);
}

TEST_F(TestCrc32, long_hex_2) {
    unsigned char data[] = {0xa0, 0x53, 0xf6, 0xa8, 0x13, 0x6a, 0x71, 0xfc,
        0xb2, 0x77, 0x8f, 0xcc, 0xd6, 0x89, 0x97, 0xf5, 0xa2, 0xff, 0x1a, 0x02,
        0xac, 0xfb, 0x68, 0x62, 0xd4, 0x08, 0x04, 0x98, 0x72, 0xae, 0xee};
    uint32_t result      = getCrc32(data, sizeof(data));
    EXPECT_EQ(result, 0x0E659C2C);
}
