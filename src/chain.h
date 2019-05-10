#ifndef __SRC_CHAIN_H__
#define __SRC_CHAIN_H__

#include <unordered_map>
#include <vector>

#include "block.h"
#include "params.h"
#include "uint256.h"

class Chain {
public:
    Chain() : Chain(false) {}
    Chain(bool mainchain);
    Chain(const Chain&);
    // add block to this chain
    void AddBlock(const std::shared_ptr<const Block> pblock);

    // find the most recent common milestone compared with the other chain
    std::shared_ptr<Milestone> FindSplit(std::shared_ptr<const Chain> pchain) const;

    std::shared_ptr<Milestone> GetChainHead() const;

    void AddPendingBlock(Block& block);

    void RemovePendingBlock(const uint256& hash);

    bool IsBlockPending(const uint256& hash) const;

    std::size_t GetPendingBlockCount() const;

    // get a list of block to verify by a post-order DFS
    std::vector<std::shared_ptr<const Block>> GetSortedSubgraph(const Block& pblock);

    std::vector<std::shared_ptr<const Block>> GetSortedSubgraph(const std::shared_ptr<const Block> pblock);

private:
    // 1 if this chain is main chain, 0 otherwise;
    bool ismainchain_;

    // store a (probabily recent) list of milestones
    std::vector<std::shared_ptr<Milestone>> vmilestones_;
    // store blocks not yet verified in this chain
    std::unordered_map<uint256, std::shared_ptr<const Block>> pendingBlocks_;

    bool IsMilestone(const std::shared_ptr<Block> pblock);

    // when we add a milestone block to this chain, we start verification
    std::shared_ptr<Milestone> MilestoneVerify(const std::shared_ptr<Block> pblock);

    // do validity check on the block
    void Validate(const std::shared_ptr<Block> pblock);
};

#endif // __SRC_CHAIN_H__
