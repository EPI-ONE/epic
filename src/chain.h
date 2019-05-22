#ifndef __SRC_CHAIN_H__
#define __SRC_CHAIN_H__

#include <deque>
#include <unordered_map>
#include <vector>

#include "block.h"
#include "consensus.h"
#include "params.h"
#include "uint256.h"
#include "utxo.h"

class Chain;
typedef std::unique_ptr<Chain> ChainPtr;

class Chain {
public:
    Chain() : Chain(false) {}
    Chain(bool mainchain);
    Chain(const Chain&);

    /**
     * Create a forked chain from $chain which has the new fork in $pfork;
     * In other words, the last common chain state is the previous one of the last record of $forkedRecord.
     * Moreover, it does not contain the corresponding chain state of this $pfork.
     * We have to further verify it to update chain state.
     */
    Chain(const Chain& chain, ConstBlockPtr pfork);

    /** Now for test only */
    Chain(const std::deque<ChainStatePtr>& states, bool ismain = false) : ismainchain_(ismain), states_(states) {}

    /** Adds block to this chain */
    void AddBlock(ConstBlockPtr pblock);

    ChainStatePtr GetChainHead() const;

    /**
     * Returns true only when the block is successfully added to the chain.
     * Returns false when this block is a new fork.
     */
    MilestoneStatus AddPendingBlock(ConstBlockPtr pblock);

    void RemovePendingBlock(const uint256& hash);

    bool IsBlockPending(const uint256& hash) const;

    std::size_t GetPendingBlockCount() const;

    /** Gets a list of block to verify by a post-order DFS */
    std::vector<RecordPtr> GetSortedSubgraph(ConstBlockPtr pblock);

    friend inline bool operator<(const Chain& a, const Chain& b) {
        return (a.GetChainHead()->chainwork < b.GetChainHead()->chainwork);
    }

    inline bool IsMainChain() const {
        return ismainchain_;
    }

    const std::deque<ChainStatePtr>& GetStates() const {
        return states_;
    }

    /**
     * TODO:
     * Off-line verification (building ledger) on a level set
     * performed when we add a milestone block to this chain.
     * Updates TXOC of the of the chain on whole level set
     */
    void Verify(std::vector<RecordPtr>&);

    /**
     * TODO:
     * Take snapshots and increases the height of the chain by 1
     */
    void UpdateChainState(const std::vector<RecordPtr>&);

    static bool IsValidDistance(const RecordPtr, const arith_uint256);

private:
    // 1 if this chain is main chain, 0 otherwise;
    bool ismainchain_;

    /**
     * Stores a (probabily recent) list of milestones
     * TODO: thinking of a lifetime mechanism for it
     */
    std::deque<ChainStatePtr> states_;

    // stores blocks not yet verified in this chain
    std::unordered_map<uint256, ConstBlockPtr> pendingBlocks_;

    // stores blocks verified in this chain
    std::unordered_map<uint256, RecordPtr> recordHistory_;

    bool CheckMsPOW(const ConstBlockPtr&, const ChainStatePtr&);

    /**
     * Returns IS_TRUE_MILESTONE if the block reaches ms diff target in the head CS
     * Returns IS_FAKE_MILESTONE if the block reaches ms diff target in a CS other than the head
     * Returns IS_NOT_MILESTONE otherwise, meaning that this block is not a milestone
     */
    MilestoneStatus IsMilestone(const ConstBlockPtr& pblock);

    /**
     * TODO:
     * Checks whether the block contains a valide tx
     * and update its NR info
     * Returns TXOC of the single block
     */
    TXOC Validate(NodeRecord& record);
};

#endif // __SRC_CHAIN_H__
