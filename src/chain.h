#ifndef __SRC_CHAIN_H__
#define __SRC_CHAIN_H__

#include <deque>
#include <optional>
#include <vector>

#include "concurrent_container.h"
#include "consensus.h"

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
    // Elements in chainworks:
    //      {chainwork, counter of consecutive chainworks that are equal}
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
    explicit Chain(bool WithGenesis);

    Chain(const Chain&) = delete;
    Chain()             = delete;

    /**
     * Create a forked chain from $chain which has the new fork in $fork;
     * In other words, the last common chain state is the record of the previous milestone of $fork.
     * Moreover, it does not contain the corresponding chain state of this $fork.
     * We have to further verify it to update chain state.
     */
    Chain(const Chain&, ConstBlockPtr fork);

    ChainStatePtr GetChainHead() const;

    /** Adds the block to pending and returns its milestone status */
    void AddPendingBlock(ConstBlockPtr);
    void AddPendingUTXOs(const std::vector<UTXOPtr>&);

    bool IsBlockPending(const uint256&) const;
    std::vector<ConstBlockPtr> GetPendingBlocks() const;
    std::size_t GetPendingBlockCount() const;
    std::vector<uint256> GetPendingHashes() const;
    ConstBlockPtr GetRandomTip() const;

    RecordPtr GetMsRecordCache(const uint256&);
    RecordPtr GetRecordCache(const uint256&);
    RecordPtr GetRecord(const uint256&) const;

    /** Gets a list of block to verify by the post-order DFS */
    std::vector<ConstBlockPtr> GetSortedSubgraph(const ConstBlockPtr& pblock);

    friend inline bool operator<(const Chain& a, const Chain& b) {
        auto a_chainwork = a.GetStates().empty() ? 0 : a.GetChainHead()->chainwork;
        auto b_chainwork = b.GetStates().empty() ? 0 : b.GetChainHead()->chainwork;
        return a_chainwork < b_chainwork;
    }

    inline bool IsMainChain() const {
        return ismainchain_;
    }

    const ConcurrentQueue<ChainStatePtr>& GetStates() const {
        return states_;
    }

    void AddNewState(const NodeRecord& ms) {
        states_.emplace_back(ms.snapshot);
    }

    /**
     * Off-line verification (building ledger) on a level set
     * performed when we add a milestone block to this chain.
     * Updates TXOC of the of the chain on whole level set
     */
    RecordPtr Verify(const ConstBlockPtr&);

    /**
     * TODO:
     * Take snapshots and increases the height of the chain by 1
     */
    void UpdateChainState(const std::vector<RecordPtr>&);

    /**
     * Removes oldest chain state as well as corresponding data
     */
    void PopOldest(const std::vector<uint256>&, const TXOC&, size_t);
    std::tuple<std::vector<std::vector<RecordPtr>>, std::unordered_map<uint256, UTXOPtr>, std::unordered_set<uint256>>
        GetDataToCAT(size_t);

    bool IsMilestone(const uint256&) const;

    friend class Chains;

private:
    // true if this chain is the main chain, false otherwise;
    bool ismainchain_;

    /**
     * Stores a (probabily recent) list of milestones
     * TODO: thinking of a lifetime mechanism for it
     */
    ConcurrentQueue<ChainStatePtr> states_;

    /**
     * Stores data not yet verified in this chain
     */
    ConcurrentHashMap<uint256, ConstBlockPtr> pendingBlocks_;
    std::unordered_map<uint256, UTXOPtr> pendingUTXOs_;

    /**
     * Stores verified blocks on this chain as cache
     */
    ConcurrentHashMap<uint256, RecordPtr> recordHistory_;

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

    /**
     * Checks whether the block contains a valide tx
     * and update its NR info
     * Returns TXOC of the single block
     */
    std::optional<TXOC> Validate(NodeRecord& record, RegChange&);

    // offline verification for transactions
    std::optional<TXOC> ValidateRedemption(NodeRecord& record, RegChange&);
    std::optional<TXOC> ValidateTx(NodeRecord& record);
    bool IsValidDistance(const NodeRecord&, const arith_uint256&);

    const Coin& GetPrevReward(const NodeRecord& rec) {
        return GetRecord(rec.cblock->GetPrevHash())->cumulativeReward;
    }

    // friend decleration for running a test
    friend class TestChainVerification;
};

typedef std::unique_ptr<Chain> ChainPtr;

#endif // __SRC_CHAIN_H__
