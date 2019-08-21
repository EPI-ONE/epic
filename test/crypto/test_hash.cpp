#include <gtest/gtest.h>
#include <iostream>

#include "hash.h"
#include "sha256.h"
#include "stream.h"

class TestHash : public testing::Test {
protected:
    VStream data;
    std::string longString;
    void SetUp() {
        longString =
            "the fog was where i wanted to be. halfway down the path you can't see this house. you'd never know it was "
            "here. or any of the other places down the avenue. i couldn't see but a few feet ahead. i didn't meet a "
            "soul. everything looked and sounded unreal. nothing was what it is. that's what i wanted to be alone with "
            "myself in another world where truth is untrue and life can hide from itself. out beyond the harbor, where "
            "the road runs along the beach, i even lost the feeling of being on land. the fog and the sea seemed part "
            "of each other. it was like walking on the bottom of the sea. as if i had drowned long ago. as if i was "
            "the ghost belonging to the fog, and the fog was the ghost of the sea. it felt damned peaceful to be "
            "nothing more than a ghost within a ghost.";

        data.write((char*) longString.c_str(), longString.size());
    }
};

TEST_F(TestHash, SHA256) {
    EXPECT_TRUE(SHA256SelfTest());

    // zero hash
    VStream s;
    uint256 result   = HashSHA2<1>(s);
    uint256 expected = uintS<256>("55b852781b9995a44c939b64e441ae2724b96f99c8f4fb9a141cfc9842c4b0e3");
    EXPECT_EQ(expected, result);

    // single hash
    uint256 hash1     = HashSHA2<1>(data);
    uint256 expected1 = uintS<256>("d76982e0bbffbd17ad548d2217c8c9eb0eabe1bd82db5e6afcbe64efc6da6db9");
    EXPECT_EQ(expected1, hash1);

    // double hash
    uint256 hash2     = HashSHA2<2>(data);
    uint256 expected2 = uintS<256>("083ba4e5288fd3140213a5dca517b9b0a8d1bf2084c59bc88e5eb6fbe15a89e5");
    EXPECT_EQ(expected2, hash2);
}

TEST_F(TestHash, BLAKE2) {
    EXPECT_TRUE(BLAKE2BSelfTest());

    uint256 hash256 = HashBLAKE2<256>(data);
    EXPECT_EQ(uintS<256>("a49e1eaefd799361779817b20b5f595ec709dd5cfa7bed6d18a6f60d77b13e8e"), hash256);

    uint512 hash512 = HashBLAKE2<512>(data);
    EXPECT_EQ(uintS<512>("ea45ff8ddfaf8708ae375cf4ba7ae678efc24627c4046732295b8e55923436c0"
                         "805d79de268f4145660cc5bf85a116b68ac218f219c877f3550b65d0c13bd234"),
              hash512);
}
