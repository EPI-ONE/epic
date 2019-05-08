#ifndef __SRC_CHAIN_H__
#define __SRC_CHAIN_H__

#include <unordered_map>
#include <vector>

#include "block.h"
#include "params.h"
#include "uint256.h"
#include "consensus.h"

class Chain {
public:
    Chain() : Chain(false) {}
    Chain(bool mainchain);
    Chain(const Chain&);
    // Create a forked chain from $chain which has the new fork $pfork;
    // In other words, the last common chain state is the previous milestone of $pfork. 
    // Hence check before calling this constructor
    Chain(const Chain& chain, std::shared_ptr<ChainState> pfork);

    // now for test only
    Chain(const std::vector<std::shared_ptr<ChainState>>& vms, bool ismain=false) : ismainchain_(ismain), vmilestones_(vms) {}

    // add block to this chain
    void AddBlock(const std::shared_ptr<const Block> pblock);

    // find the most recent common milestone compared with the other chain
    std::shared_ptr<ChainState> FindSplit(const Chain& pchain) const;

    std::shared_ptr<ChainState> GetChainHead() const;

    void AddPendingBlock(ConstBlockPtr pblock);
    /* for test only now */
    void AddPendingBlock(const Block& block);

    void RemovePendingBlock(const uint256& hash);

    bool IsBlockPending(const uint256& hash) const;

    std::size_t GetPendingBlockCount() const;

    // get a list of block to verify by a post-order DFS
    std::vector<std::shared_ptr<const Block>> GetSortedSubgraph(const Block& pblock);

    std::vector<std::shared_ptr<const Block>> GetSortedSubgraph(const std::shared_ptr<const Block> pblock);

    friend inline bool operator<(const Chain& a, const Chain& b) {
        return (a.GetChainHead()->chainwork < b.GetChainHead()->chainwork);
    }

    inline bool IsMainChain() const {
        return ismainchain_;
    }

private:
    // 1 if this chain is main chain, 0 otherwise;
    bool ismainchain_;

    // store a (probabily recent) list of milestones
    // TODO: thinking of a lifetime mechanism for it
    std::vector<std::shared_ptr<ChainState>> vmilestones_;
    // store blocks not yet verified in this chain
    std::unordered_map<uint256, ConstBlockPtr> pendingBlocks_;

    bool IsMilestone(const std::shared_ptr<Block> pblock);

    // when we add a milestone block to this chain, we start verification
    std::shared_ptr<ChainState> MilestoneVerify(const RecordPtr pblock);

    // do validity check on the block
    void Validate(const std::shared_ptr<Block> pblock);
};

#endif // __SRC_CHAIN_H__
