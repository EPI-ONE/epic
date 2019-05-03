#include <gtest/gtest.h>

#include "chain.h"

class DFSTest : public testing::Test {};

TEST_F(DFSTest, empty_pending_blocks_map) {
    Chain chain;
    Block block_0;
    block_0.SetDifficultyTarget(EASIEST_COMP_DIFF_TARGET);
    block_0.Solve();

    chain.addPendingBlock(block_0);
    auto graph = chain.getSortedSubgraph(std::make_shared<const Block>(block_0));
    ASSERT_EQ(chain.getPendingBlockCount(), 0);
    ASSERT_EQ(graph.size(), 1);
}

TEST_F(DFSTest, complex_test) {
    Chain chain;

    std::size_t n = 10;
    std::vector<Block> blocks;
    blocks.reserve(n);

    for (std::size_t i = 0; i < n; i++) {
        blocks.push_back(Block());

        // faster than solving the blocks
        blocks[i].randomizeHash();

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
    blocks[0].setMilestoneHash(blocks[2].GetHash());
    blocks[1].setMilestoneHash(blocks[5].GetHash());
    blocks[2].setMilestoneHash(blocks[5].GetHash());
    blocks[3].setMilestoneHash(blocks[8].GetHash());
    blocks[5].setMilestoneHash(blocks[8].GetHash());
    blocks[7].setMilestoneHash(blocks[5].GetHash());
    blocks[9].setMilestoneHash(blocks[5].GetHash());

    blocks[0].setPrevHash(blocks[1].GetHash());
    blocks[1].setPrevHash(blocks[7].GetHash());
    blocks[2].setPrevHash(blocks[4].GetHash());
    blocks[3].setPrevHash(blocks[5].GetHash());
    blocks[7].setPrevHash(blocks[4].GetHash());
    blocks[9].setPrevHash(blocks[1].GetHash());

    blocks[0].setTIPHash(blocks[3].GetHash());
    blocks[1].setTIPHash(blocks[6].GetHash());
    blocks[2].setTIPHash(blocks[6].GetHash());
    blocks[3].setTIPHash(blocks[6].GetHash());
    blocks[7].setTIPHash(blocks[8].GetHash());
    blocks[9].setTIPHash(blocks[3].GetHash());

    /* populate the pending Block
     * map for the first time */
    for (std::size_t i = 0; i < n; i++) {
        chain.addPendingBlock(blocks[i]);
    }

    /*
     * first test class with 0 as pivot
     */
    auto graph = chain.getSortedSubgraph(blocks[0]);

    /* simple test to check if the right
     * amount of nodes are left after execution */
    ASSERT_EQ(chain.getPendingBlockCount(), 1);
    ASSERT_EQ(graph.size(), 9);

    /* check if the result matches the expectation */
    ASSERT_EQ(graph[0].get()->GetTime(), 8);
    ASSERT_EQ(graph[1].get()->GetTime(), 5);
    ASSERT_EQ(graph[2].get()->GetTime(), 4);
    ASSERT_EQ(graph[3].get()->GetTime(), 6);
    ASSERT_EQ(graph[4].get()->GetTime(), 2);
    ASSERT_EQ(graph[5].get()->GetTime(), 7);
    ASSERT_EQ(graph[6].get()->GetTime(), 1);
    ASSERT_EQ(graph[7].get()->GetTime(), 3);
    ASSERT_EQ(graph[8].get()->GetTime(), 0);

    /* populate the pending Block
     * map for the second time for
     * second test case and the assert
     * that no duplicates were inserted*/
    for (std::size_t i = 0; i < n; i++) {
        chain.addPendingBlock(blocks[i]);
    }

    // size check
    ASSERT_EQ(chain.getPendingBlockCount(), 10);

    /*
     * second test class with 9 as pivot
     */
    graph = chain.getSortedSubgraph(blocks[9]);

    /* simple test to check if the right
     * amount of nodes are left after execution */
    ASSERT_EQ(chain.getPendingBlockCount(), 2);
    ASSERT_EQ(graph.size(), 8);

    /* check if the result matches the expectation */
    ASSERT_EQ(graph[0].get()->GetTime(), 8);
    ASSERT_EQ(graph[1].get()->GetTime(), 5);
    ASSERT_EQ(graph[2].get()->GetTime(), 4);
    ASSERT_EQ(graph[3].get()->GetTime(), 7);
    ASSERT_EQ(graph[4].get()->GetTime(), 6);
    ASSERT_EQ(graph[5].get()->GetTime(), 1);
    ASSERT_EQ(graph[6].get()->GetTime(), 3);
    ASSERT_EQ(graph[7].get()->GetTime(), 9);
}
