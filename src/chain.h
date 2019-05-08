#ifndef __SRC_CHAIN_H__
#define __SRC_CHAIN_H__

#include <deque>
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

    // Create a forked chain from $chain which has the new fork in $pfork;
    // In other words, the last common chain state is the previous one of the last record of $forkedRecord.
    // Moreover, it does not contain the corresponding chain state of this $pfork.
    // We have to further verify it to update chain state.
    Chain(const Chain& chain, ConstBlockPtr pfork);

    // now for test only
    Chain(const std::deque<ChainStatePtr>& dqcs, bool ismain=false) : ismainchain_(ismain), dqChainStates_(dqcs) {}

    ~Chain();
    // add block to this chain
    void AddBlock(ConstBlockPtr pblock);

    ChainStatePtr GetChainHead() const;

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

    const std::deque<ChainStatePtr>& GetStates() const {
        return dqChainStates_; 
    }

private:
    // 1 if this chain is main chain, 0 otherwise;
    bool ismainchain_;

    // store a (probabily recent) list of milestones
    // TODO: thinking of a lifetime mechanism for it
    std::deque<ChainStatePtr> dqChainStates_;
    // store blocks not yet verified in this chain
    std::unordered_map<uint256, ConstBlockPtr> pendingBlocks_;
    // store blocks verified in this chain
    std::unordered_map<uint256, RecordPtr> recordHistory_;

    bool IsMilestone(const std::shared_ptr<BlockNet> pblock);

    // when we add a milestone block to this chain, we start verification
    // TODO: should return tx ouput changes
    void Verify(std::vector<RecordPtr>&);

    // do validity check on the block
    // TODO: should return tx ouput changes
    void Validate(NodeRecord& record);
};

typedef std::unique_ptr<Chain> ChainPtr;

#endif // __SRC_CHAIN_H__
