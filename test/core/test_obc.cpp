#include <gtest/gtest.h>

#include "obc.h"

class OBCTest : public testing::Test {
public:
    std::vector<Block> blocks;

    void SetUp() {
        std::size_t n = 10;
        blocks.reserve(n);

        for (std::size_t i = 0; i < n; i++) {
            blocks.push_back(Block());

            // faster than solving the blocks
            blocks[i].RandomizeHash();

            // time is used as the node id
            blocks[i].SetTime(i);
        }

        /*
         * below the test dag is initialized;
         * the structure is the following:
         * X0123456789
         * M2558X8X5X5
         * P1745XXX4X1
         * T3666XXX8X3
         */
        blocks[0].SetMilestoneHash(blocks[2].GetHash());
        blocks[1].SetMilestoneHash(blocks[5].GetHash());
        blocks[2].SetMilestoneHash(blocks[5].GetHash());
        blocks[3].SetMilestoneHash(blocks[8].GetHash());
        blocks[5].SetMilestoneHash(blocks[8].GetHash());
        blocks[7].SetMilestoneHash(blocks[5].GetHash());
        blocks[9].SetMilestoneHash(blocks[5].GetHash());

        blocks[0].SetPrevHash(blocks[1].GetHash());
        blocks[1].SetPrevHash(blocks[7].GetHash());
        blocks[2].SetPrevHash(blocks[4].GetHash());
        blocks[3].SetPrevHash(blocks[5].GetHash());
        blocks[7].SetPrevHash(blocks[4].GetHash());
        blocks[9].SetPrevHash(blocks[1].GetHash());

        blocks[0].SetTipHash(blocks[3].GetHash());
        blocks[1].SetTipHash(blocks[6].GetHash());
        blocks[2].SetTipHash(blocks[6].GetHash());
        blocks[3].SetTipHash(blocks[6].GetHash());
        blocks[7].SetTipHash(blocks[8].GetHash());
        blocks[9].SetTipHash(blocks[3].GetHash());
    }
};

TEST_F(OBCTest, wrong_argument_test) {
    OrphanBlocksContainer obc;

    /* add a block to an OBC that is actually not an orphan */
    obc.AddBlock(std::make_shared<const BlockNet>(blocks[0]), 0);

    /* since the added block is no
     * orphan we expect an empty OBC */
    EXPECT_EQ(obc.Size(), 0);
}

TEST_F(OBCTest, simple_one_block_test) {
    OrphanBlocksContainer obc;

    obc.AddBlock(std::make_shared<const BlockNet>(blocks[0]), M_MISSING);

    /* now we should have one
     * block in the OBC */
    EXPECT_EQ(obc.Size(), 1);

    const uint256 dep_hash                           = blocks[2].GetHash();
    std::optional<std::vector<ConstBlockPtr>> result = obc.SubmitHash(dep_hash);

    /* check if a value was even returned */
    EXPECT_TRUE(result.has_value());

    /* check if there was exactly one value returned*/
    EXPECT_EQ(result.value().size(), 1);

    /* check if that returned value was the one we expected to see */
    EXPECT_TRUE(result.value()[0]->GetHash() == blocks[0].GetHash());
}

TEST_F(OBCTest, complex_secondary_deps_test) {
    OrphanBlocksContainer obc;

    /* hash missing */
    const uint256 dep_hash = blocks[8].GetHash();

    /* hash of the block that must remain in OBC (9) */
    const uint256 rem_hash = blocks[9].GetHash();

    /* fill OBC */
    obc.AddBlock(std::make_shared<const BlockNet>(blocks[7]), T_MISSING);
    obc.AddBlock(std::make_shared<const BlockNet>(blocks[1]), P_MISSING);
    obc.AddBlock(std::make_shared<const BlockNet>(blocks[0]), P_MISSING);
    obc.AddBlock(std::make_shared<const BlockNet>(blocks[9]), T_MISSING | P_MISSING);

    /* we have two lose end 7 & 9
     * therefore the obc.Size should
     * be equal to two */
    EXPECT_EQ(obc.Size(), 2);

    /* submit missing hash */
    std::optional<std::vector<ConstBlockPtr>> result = obc.SubmitHash(dep_hash);

    /* check if a value was even returned */
    EXPECT_TRUE(result.has_value());

    /* check if there were exactly three
     * values returned as the lose end 9
     * is not tied since it has two deps */
    EXPECT_EQ(result.value().size(), 3);

    /* check if the OBC has one element left */
    EXPECT_EQ(obc.DependencySize(), 1);

    /* check if that remaining block is 9*/
    EXPECT_TRUE(obc.IsOrphan(rem_hash));
}
