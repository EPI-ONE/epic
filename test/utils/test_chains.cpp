// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "chains.h"
#include "test_factory.h"

class TestChains : public testing::Test {
public:
    TestFactory fac;
};

TEST_F(TestChains, BasicFunctions) {
    int testSize = 10000;
    uint64_t mcw = rand() + 100; // max chainwork

    auto chainworkOf = [](ChainPtr& cp) -> arith_uint256& { return cp->GetChainHead()->chainwork; };

    // Construct chains, each containing one milestone
    // with a random chainwork less than mcw
    std::vector<ChainPtr> random_chains;
    for (int i = 0; i < testSize; ++i) {
        auto chain            = std::make_unique<Chain>();
        VertexPtr randomBlock = fac.CreateVertexPtr(1, 1, true, 1);
        fac.CreateMilestonePtr(GENESIS_VERTEX->snapshot, randomBlock);
        chain->AddNewMilestone(*randomBlock);
        chainworkOf(chain) = rand() % mcw;
        random_chains.push_back(std::move(chain));
    }

    // Replace a random element in random_chains with mcw
    auto best_chain       = std::make_unique<Chain>();
    VertexPtr randomBlock = fac.CreateVertexPtr(1, 1, true, 1);
    fac.CreateMilestonePtr(GENESIS_VERTEX->snapshot, randomBlock);
    best_chain->AddNewMilestone(*randomBlock);
    chainworkOf(best_chain)          = mcw;
    random_chains[rand() % testSize] = std::unique_ptr<Chain>(std::move(best_chain));

    // Push elements into Chains
    Chains q;
    q.reserve(testSize);
    for (int i = 0; i < testSize; ++i) {
        q.push(std::move(random_chains[i]));
    }

    EXPECT_EQ(q.size(), testSize);
    EXPECT_EQ(chainworkOf(q.best()), mcw);

    // Replace the first chain by a better chain
    uint64_t new_mcw         = mcw + 1;
    auto new_best            = std::make_unique<Chain>();
    VertexPtr randomNewBlock = fac.CreateVertexPtr(1, 1, true, 1);
    fac.CreateMilestonePtr(GENESIS_VERTEX->snapshot, randomNewBlock);
    new_best->AddNewMilestone(*randomNewBlock);
    chainworkOf(new_best) = new_mcw;
    *q.begin()            = std::move(new_best);
    q.update_best(q.begin());

    EXPECT_EQ(chainworkOf(q.best()), new_mcw);

    // Erasing the best chain is not allowed
    q.erase(q.begin());
    EXPECT_EQ(q.size(), testSize);

    // Erasing a worse chain is allowed
    q.erase(q.begin() + 1);
    EXPECT_EQ(q.size(), testSize - 1);

    // Pop the best chain
    q.pop();
    EXPECT_EQ(q.size(), testSize - 2);
}
