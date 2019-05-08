#include <gtest/gtest.h>

#include "chain.h"
#include "test_factory.h"

class DFSTest : public testing::Test {
public:
    TestFactory fac;
};

TEST_F(DFSTest, empty_pending_blocks_map) {
    Chain chain;
    Block block_0;
    block_0.SetDifficultyTarget(EASIEST_COMP_DIFF_TARGET);
    block_0.Solve();

    chain.AddPendingBlock(block_0);
    auto graph = chain.GetSortedSubgraph(std::make_shared<const Block>(block_0));
    ASSERT_EQ(chain.GetPendingBlockCount(), 0);
    ASSERT_EQ(graph.size(), 1);
}

TEST_F(DFSTest, complex_test) {
    Chain chain;

    std::size_t n = 10; std::vector<Block> blocks;
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

    /* populate the pending Block
     * map for the first time */
    for (std::size_t i = 0; i < n; i++) {
        chain.AddPendingBlock(blocks[i]);
    }

    /*
     * first test class with 0 as pivot
     */
    auto graph = chain.GetSortedSubgraph(blocks[0]);

    /* simple test to check if the right
     * amount of nodes are left after execution */
    ASSERT_EQ(chain.GetPendingBlockCount(), 1);
    ASSERT_EQ(graph.size(), 9);

    /* check if the result matches the expectation */
    ASSERT_EQ(graph[0]->GetTime(), 8);
    ASSERT_EQ(graph[1]->GetTime(), 5);
    ASSERT_EQ(graph[2]->GetTime(), 4);
    ASSERT_EQ(graph[3]->GetTime(), 6);
    ASSERT_EQ(graph[4]->GetTime(), 2);
    ASSERT_EQ(graph[5]->GetTime(), 7);
    ASSERT_EQ(graph[6]->GetTime(), 1);
    ASSERT_EQ(graph[7]->GetTime(), 3);
    ASSERT_EQ(graph[8]->GetTime(), 0);

    /* populate the pending Block
     * map for the second time for
     * second test case and the assert
     * that no duplicates were inserted*/
    for (std::size_t i = 0; i < n; i++) {
        chain.AddPendingBlock(blocks[i]);
    }

    // size check
    ASSERT_EQ(chain.GetPendingBlockCount(), 10);

    /*
     * second test class with 9 as pivot
     */
    graph = chain.GetSortedSubgraph(blocks[9]);

    /* simple test to check if the right
     * amount of nodes are left after execution */
    ASSERT_EQ(chain.GetPendingBlockCount(), 2);
    ASSERT_EQ(graph.size(), 8);

    /* check if the result matches the expectation */
    ASSERT_EQ(graph[0]->GetTime(), 8);
    ASSERT_EQ(graph[1]->GetTime(), 5);
    ASSERT_EQ(graph[2]->GetTime(), 4);
    ASSERT_EQ(graph[3]->GetTime(), 7);
    ASSERT_EQ(graph[4]->GetTime(), 6);
    ASSERT_EQ(graph[5]->GetTime(), 1);
    ASSERT_EQ(graph[6]->GetTime(), 3);
    ASSERT_EQ(graph[7]->GetTime(), 9);
}
