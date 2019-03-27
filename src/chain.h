#ifndef __SRC_CHAIN_H__
#define __SRC_CHAIN_H__

#include "block.h"
#include "milestone.h"
#include "uint256.h"
#include <list>
#include <unordered_map>
#include <vector>

class Chain {
public:
    // add block to this chain
    void AddBlock(const std::shared_ptr<Block> pblock);
    // find the most recent common milestone compared with the other chain
    std::shared_ptr<Milestone> FindSplit(std::shared_ptr<const Chain> pchain) const;

    // get methods
    std::shared_ptr<Milestone> GetChainHead() const {
        vmilestones_.front();
    }

private:
    // 1 if this chain is main chain, 0 otherwise;
    bool ismainchain_;
    // store blocks not yet verified in this chain
    std::unordered_map<uint256, const Block*> mpending_;
    // store a (probabily recent) list of milestones
    std::list<std::shared_ptr<Milestone>> vmilestones_;

    bool IsMilestone(const std::shared_ptr<Block> pblock);
    // when we add a milestone block to this chain, we start verification
    std::shared_ptr<Milestone> MilestoneVerify(const std::shared_ptr<Block> pblock);
    // get a list of block to verify by a post-order DFS
    std::vector<std::shared_ptr<Block>> FindValidSubgraph(const std::shared_ptr<Block> pblock);
    // do validity check on the block
    void Validate(const std::shared_ptr<Block> pblock);
};

#endif // __SRC_CHAIN_H__
