#ifndef __SRC_CHAIN_H__
#define __SRC_CHAIN_H__

#include <deque>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "block.h"
#include "consensus.h"
#include "params.h"
#include "uint256.h"
#include "utxo.h"

class Cumulator;
namespace std {
string to_string(const Cumulator& b);
} // namespace std

class Cumulator {
public:
    void Add(const ConstBlockPtr& block, bool ascending);
    arith_uint256 Sum() const;
    uint32_t TimeSpan() const;
    bool Full() const;

    friend std::string std::to_string(const Cumulator&);

private:
    // Elements in chainworks: {chainwork, counter of consecutive chainworks that are equal}
    // For example, the queue of chainworks
    //      { 1, 1, 3, 2, 2, 2, 2, 2, 2, 2 }
    // are stored as:
    //      { {1, 2}, {3, 1}, {2, 7} }
    std::deque<std::pair<uint32_t, uint16_t>> chainworks;
    std::deque<uint32_t> timestamps;
    arith_uint256 sum = 0;
};

/** Hasher for unordered_map */
template <>
struct std::hash<Cumulator> {
    size_t operator()(const Cumulator& x) const {
        return x.Sum().GetCompact() ^ x.TimeSpan();
    }
};

class Chain {
public:
    /** Init a chain with genesis */
    Chain();
    Chain(const Chain&);

    /**
     * Create a forked chain from $chain which has the new fork in $pfork;
     * In other words, the last common chain state is the previous one of the last record of $forkedRecord.
     * Moreover, it does not contain the corresponding chain state of this $pfork.
     * We have to further verify it to update chain state.
     */
    Chain(const Chain&, ConstBlockPtr fork);

    /** Now for test only */
    Chain(const std::deque<ChainStatePtr>& states, bool ismain = false) : ismainchain_(ismain), states_(states) {}

    ChainStatePtr GetChainHead() const;

    /** Adds the block to pending and returns its milestone status */
    void AddPendingBlock(ConstBlockPtr);

    void AddPendingUTXOs(const std::vector<UTXOPtr>&);

    RecordPtr GetMsRecordCache(const uint256&);

    RecordPtr GetRecordCache(const uint256&);

    void RemovePendingBlock(const uint256&);

    bool IsBlockPending(const uint256&) const;

    std::size_t GetPendingBlockCount() const;

    /** Gets a list of block to verify by the post-order DFS */
    std::vector<ConstBlockPtr> GetSortedSubgraph(const ConstBlockPtr& pblock);

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
     * Off-line verification (building ledger) on a level set
     * performed when we add a milestone block to this chain.
     * Updates TXOC of the of the chain on whole level set
     */
    void Verify(const ConstBlockPtr&);

    /**
     * TODO:
     * Take snapshots and increases the height of the chain by 1
     */
    void UpdateChainState(const std::vector<RecordPtr>&);

    static bool IsValidDistance(const RecordPtr&, const arith_uint256&);

private:
    // 1 if this chain is main chain, 0 otherwise;
    bool ismainchain_;

    /**
     * Stores a (probabily recent) list of milestones
     * TODO: thinking of a lifetime mechanism for it
     */
    std::deque<ChainStatePtr> states_;

    /**
     * Stores data not yet verified in this chain
     */
    std::unordered_map<uint256, ConstBlockPtr> pendingBlocks_;
    std::unordered_map<uint256, UTXOPtr> pendingUTXOs_;

    /**
     * Stores verified blocks on this chain as cache
     */
    std::unordered_map<uint256, RecordPtr> recordHistory_;

    /*
     * Stores blocks being verified in a level set
     */
    std::unordered_map<uint256, RecordPtr> verifying_;

    /**
     * Manages UTXO
     */
    ChainLedger ledger_;

    /**
     * Caches the sum of chainwork and timestamps in the sortition window
     * to speed up calculation of transaction distance
     */
    std::unordered_map<uint256, Cumulator> cumulatorMap_;

    bool CheckMsPOW(const ConstBlockPtr&, const ChainStatePtr&);

    /**
     * Checks whether the block contains a valide tx
     * and update its NR info
     * Returns TXOC of the single block
     */
    std::optional<TXOC> Validate(NodeRecord& record);

    // offline verification for transactions
    std::optional<TXOC> ValidateRedemption(NodeRecord& record);
    std::optional<TXOC> ValidateTx(NodeRecord& record);

    const Coin& GetPrevReward(const NodeRecord& rec) {
        // TODO: may add more check
        return recordHistory_[rec.cblock->GetPrevHash()]->cumulativeReward;
    }

    bool IsValidDistance(const NodeRecord&, const arith_uint256&);

    RecordPtr GetRecord(const uint256&) const;

    // friend decleration for running a test
    friend class TestChainVerification;
};

typedef std::unique_ptr<Chain> ChainPtr;

#endif // __SRC_CHAIN_H__
