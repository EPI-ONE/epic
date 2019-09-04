// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>
#include <memory>

#include "block.h"
#include "obc.h"
#include "params.h"
#include "test_factory.h"

class OBCTest : public testing::Test {
public:
    std::vector<Block> blocks;
    TestFactory fac;

    void SetUp() {
        std::size_t n = 10;
        blocks.reserve(n);

        for (std::size_t i = 0; i < n; i++) {
            blocks.push_back(fac.CreateBlock());

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
    obc.AddBlock(std::make_shared<const Block>(blocks[0]), 0);

    /* since the added block is no
     * orphan we expect an empty OBC */
    EXPECT_EQ(obc.Size(), 0);
}

TEST_F(OBCTest, simple_one_block_test) {
    OrphanBlocksContainer obc;

    obc.AddBlock(std::make_shared<const Block>(blocks[0]), M_MISSING);

    /* now we should have one
     * block in the OBC */
    EXPECT_EQ(obc.Size(), 1);

    auto result = obc.SubmitHash(blocks[2].GetHash());

    /* check if a value was even returned */
    EXPECT_FALSE(result.empty());

    /* check if there was exactly one value returned*/
    EXPECT_EQ(result.size(), 1);

    /* check if that returned value was the one we expected to see */
    EXPECT_TRUE(result[0]->GetHash() == blocks[0].GetHash());
}

TEST_F(OBCTest, complex_secondary_deps_test) {
    OrphanBlocksContainer obc;

    /* hash missing */
    const uint256 dep_hash = blocks[8].GetHash();

    /* hash of the block that must remain in OBC (9) */
    const uint256 rem_hash = blocks[9].GetHash();

    /* fill OBC */
    obc.AddBlock(std::make_shared<const Block>(blocks[7]), T_MISSING);
    obc.AddBlock(std::make_shared<const Block>(blocks[1]), P_MISSING);
    obc.AddBlock(std::make_shared<const Block>(blocks[0]), P_MISSING);
    obc.AddBlock(std::make_shared<const Block>(blocks[9]), T_MISSING | P_MISSING);

    EXPECT_EQ(obc.Size(), 4);

    /* submit missing hash */
    std::vector<ConstBlockPtr> result = obc.SubmitHash(dep_hash);

    /* check if a value was even returned */
    EXPECT_FALSE(result.empty());

    /* check if there were exactly three
     * values returned as the lose end 9
     * is not tied since it has two deps */
    EXPECT_EQ(result.size(), 1);

    /* check if the OBC has one element left */
    EXPECT_EQ(obc.Size(), 3);

    /* check if that remaining block is 9*/
    EXPECT_TRUE(obc.Contains(rem_hash));
}

TEST_F(OBCTest, test_prune) {
    OrphanBlocksContainer obc;

    auto current_time = time(nullptr);

    blocks[7].SetTime(current_time);
    blocks[1].SetTime(current_time);
    blocks[0].SetTime(current_time + 7200);
    blocks[9].SetTime(current_time + 7200);

    // we only consider the prev chain: 4 <- 7 <- 1 <- 0
    obc.AddBlock(std::make_shared<const Block>(blocks[0]), P_MISSING);
    obc.AddBlock(std::make_shared<const Block>(blocks[1]), P_MISSING);
    obc.AddBlock(std::make_shared<const Block>(blocks[7]), P_MISSING);

    // we only consider the tip chain: 3 <- 9. block[9] and block[3] is irrelevant to above 3 blocks
    obc.AddBlock(std::make_shared<const Block>(blocks[9]), T_MISSING);

    ASSERT_EQ(obc.Size(), 4);
    ASSERT_EQ(obc.GetDepNodeSize(), 6);

    // now block[1] and block[7] have the older block time and will be pruned while block[9] will not be pruned
    obc.Prune(0);

    // but the later block[0] which depends on block[1] and the former dep_node without a real block instance(block[4]
    // will be pruned too while block[9] and block[3] are still safe
    ASSERT_EQ(obc.GetDepNodeSize(), 2);

    ASSERT_EQ(obc.Size(), 1);
}
