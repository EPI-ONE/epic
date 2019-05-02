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

/*
TEST_F(DFSTest, complex_test) {
    Chain chain;

    std::size_t n = 10;
    std::vector<Block> blocks;
    blocks.reserve(n);

    for (std::size_t i = 0; i < n; i++) {
        blocks.push_back(Block());
        blocks.back().SetTime(i);
        blocks.back().SetDifficultyTarget(EASIEST_COMP_DIFF_TARGET);
        blocks.back().Solve();
    }

    EXPECT_TRUE(true);
}
*/
