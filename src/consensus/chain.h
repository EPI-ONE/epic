// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_CHAIN_H
#define EPIC_CHAIN_H

#include "concurrent_container.h"
#include "vertex.h"

#include <algorithm>
#include <deque>
#include <optional>
#include <vector>

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
    bool Empty() const;

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
    Chain();
    Chain(const Chain&) = delete;
    Chain& operator=(const Chain&) = delete;

    /**
     * Create a forked chain from $chain which has the new fork in $fork;
     * In other words, the last common milestone is the vertex of the previous milestone of $fork.
     * Moreover, it does not contain the corresponding chain state of this $fork.
     * We have to further verify it to update chain state.
     */
    Chain(const Chain&, const ConstBlockPtr& fork);

    MilestonePtr GetChainHead() const;

    /** Adds the block to pending and returns its milestone status */
    void AddPendingBlock(ConstBlockPtr);
    void AddPendingUTXOs(std::vector<UTXOPtr>&&);

    bool IsBlockPending(const uint256&) const;
    std::vector<ConstBlockPtr> GetPendingBlocks() const;
    std::size_t GetPendingBlockCount() const;
    std::vector<uint256> GetPendingHashes() const;
    ConstBlockPtr GetRandomTip() const;

    VertexPtr GetVertexCache(const uint256&) const;
    VertexPtr GetVertex(const uint256&) const;
    VertexPtr GetMsVertexCache(const uint256&) const;

    /** Gets a list of block to verify by the post-order DFS */
    std::vector<ConstBlockPtr> GetSortedSubgraph(const ConstBlockPtr& pblock);

    friend inline bool operator<(const Chain& a, const Chain& b) {
        auto a_chainwork = a.GetStates().empty() ? 0 : a.GetChainHead()->chainwork;
        auto b_chainwork = b.GetStates().empty() ? 0 : b.GetChainHead()->chainwork;
        return a_chainwork < b_chainwork;
    }

    bool IsMainChain() const {
        return ismainchain_;
    }

    const ConcurrentQueue<MilestonePtr>& GetStates() const {
        return states_;
    }

    size_t GetLeastHeightCached() const {
        if (states_.empty()) {
            return UINT64_MAX;
        }

        return states_.front()->height;
    }

    void AddNewState(const Vertex& ms) {
        states_.emplace_back(ms.snapshot);
    }

    /**
     * Off-line verification (building ledger) on a level set
     * performed when we add a milestone block to this chain.
     * Updates TXOC of the of the chain on whole level set
     */
    VertexPtr Verify(const ConstBlockPtr&);

    /**
     * Removes oldest chain state as well as corresponding data
     */
    void PopOldest(const std::vector<uint256>&, const TXOC&);
    std::tuple<std::vector<VertexWPtr>, std::unordered_map<uint256, UTXOPtr>, std::unordered_set<uint256>>
        GetDataToSTORE(MilestonePtr);

    bool IsMilestone(const uint256&) const;
    bool IsTxFitsLedger(const ConstTxPtr& tx) const;

    friend class Chains;

private:
    // true if this chain is the main chain, false otherwise;
    bool ismainchain_;

    /**
     * Stores a (probabily recent) list of milestones
     */
    ConcurrentQueue<MilestonePtr> states_;

    /**
     * Stores data not yet verified in this chain
     */
    ConcurrentHashMap<uint256, ConstBlockPtr> pendingBlocks_;

    /**
     * Stores verified blocks on this chain as cache
     */
    ConcurrentHashMap<uint256, VertexPtr> recentHistory_;

    /*
     * Stores blocks being verified in a level set
     */
    std::unordered_map<uint256, VertexPtr> verifying_;

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
     * Caches the hashes of the previous registration block for each peer chain.
     * Key: hash of the head of peer chain
     * Value: hash of the previous reg block of the corresponding key
     */
    std::unordered_map<uint256, uint256> prevRedempHashMap_;

    /**
     * Checks whether the block contains a valide tx
     * and update its NR info
     * Returns valid TXOC and invalid TXOC of the single block
     */
    std::pair<TXOC, TXOC> Validate(Vertex& vertex, RegChange&);

    // offline verification for transactions
    std::optional<TXOC> ValidateRedemption(Vertex&, RegChange&);
    bool ValidateTx(const Transaction&, uint32_t index, TXOC&, Coin& fee);
    TXOC ValidateTxns(Vertex&);
    void CheckTxPartition(Vertex&, const arith_uint256&);

    Coin GetPrevReward(const Vertex& vtx) const {
        return GetVertex(vtx.cblock->GetPrevHash())->cumulativeReward;
    }

    uint256 GetPrevRedempHash(const uint256& h) const;

    // friend decleration for running a test
    friend class TestChainVerification;
};

typedef std::unique_ptr<Chain> ChainPtr;

inline arith_uint256 CalculateAllowedDist(const Cumulator& cum, const arith_uint256& msHashRate) {
    return std::max((arith_uint256)(cum.Sum() / (cum.TimeSpan() + 1)), arith_uint256(1)) /
           GetParams().sortitionCoefficient * (GetParams().maxTarget / (msHashRate + 1));
}

#endif // EPIC_CHAIN_H
