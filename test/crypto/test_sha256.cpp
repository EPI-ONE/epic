#include <gtest/gtest.h>
#include <iostream>

#include "hash.h"
#include "sha256.h"
#include "stream.h"

class SHA256Test : public testing::Test {
protected:
    VStream data;
    void SetUp() {
        std::string longString =
            "The fog was where I wanted to be. Halfway down the path you can’t see this house. You’d never know it was "
            "here. Or any of the other places down the avenue. I couldn’t see but a few feet ahead. I didn’t meet a "
            "soul. Everything looked and sounded unreal. Nothing was what it is. That’s what I wanted—to be alone with "
            "myself in another world where truth is untrue and life can hide from itself. Out beyond the harbor, where "
            "the road runs along the beach, I even lost the feeling of being on land. The fog and the sea seemed part "
            "of each other. It was like walking on the bottom of the sea. As if I had drowned long ago. As if I was "
            "the ghost belonging to the fog, and the fog was the ghost of the sea. It felt damned peaceful to be "
            "nothing more than a ghost within a ghost.";

        data.write((char*) longString.c_str(), longString.size());
    }
};

TEST_F(SHA256Test, NATIVE_SHA_TEST) {
    EXPECT_TRUE(SHA256SelfTest());
}

TEST_F(SHA256Test, SINGLE_HASH_TEST) {
    uint256 result   = Hash<1>(data);
    uint256 expected = uint256S("8240ab53aa340ac4112daaed9fed59ef0790bdd02925335361f79b9ffd9c788a");
    EXPECT_EQ(expected, result);
}

TEST_F(SHA256Test, DOUBLE_HASH_TEST) {
    uint256 result   = Hash<2>(data);
    uint256 expected = uint256S("8d7b5da15ca6f77535c4612a887d25e66e7578c233e0049663b9e7df75a843a5");
    EXPECT_EQ(expected, result);
}

TEST_F(SHA256Test, ZERO_HASH) {
    VStream s;
    uint256 result   = Hash<1>(s);
    uint256 expected = uint256S("55b852781b9995a44c939b64e441ae2724b96f99c8f4fb9a141cfc9842c4b0e3");
    EXPECT_EQ(expected, result);
}
