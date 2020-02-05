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

class Chain;

class Cumulator;
namespace std {
string to_string(const Cumulator& b);
} // namespace std

class Cumulator {
public:
    static size_t GetCap() {
        return cap_;
    }

    Cumulator() {
        cap_ = GetParams().punctualityThred + GetParams().sortitionThreshold;
    }

    void Add(const Vertex& block, const Chain&, bool ascending);
    double Percentage(size_t height) const;
    bool Full() const;
    bool Empty() const;
    void Clear();

    friend std::string std::to_string(const Cumulator&);
    friend struct std::hash<Cumulator>;

private:
    static size_t cap_;

    // <
    //    level set height,
    //    <
    //      size of the level set,
    //      size of the segment of the miner chain contained in the level set
    //    >
    // >
    std::deque<std::pair<size_t, std::pair<size_t, size_t>>> sizes_;

    // Caches the sums of the level set segments of length sortitionThreshold,
    // so that each sum need only be calculated at most once at each height.
    mutable std::unordered_map<size_t, std::pair<double, double>> sum_cache_;
};

/** Hasher for unordered_map */
// template <>
// struct std::hash<Cumulator> {
// size_t operator()(const Cumulator& x) const {
// return (size_t) x.lvs_sum_ ^ (size_t) x.miner_sum_;
//}
//};

class Chain {
public:
    Chain();
    Chain(const Chain&) = delete;
    Chain& operator=(const Chain&) = delete;

    /**
     * Create a forked chain from $chain which has the new fork in $fork;
     * In other words, the last common milestone is the vertex of the previous milestone of $fork.
     * Moreover, it does not contain the corresponding milestone of this $fork.
     * We have to further verify it to update the milestone.
     */
    Chain(const Chain&, const ConstBlockPtr& fork);

    MilestonePtr GetChainHead() const;

    /** Adds the block to pending and returns its milestone status */
    void AddPendingBlock(ConstBlockPtr);
    void AddPendingUTXOs(std::vector<UTXOPtr>);

    bool IsBlockPending(const uint256&) const;
    std::vector<ConstBlockPtr> GetPendingBlocks() const;
    std::size_t GetPendingBlockCount() const;
    std::vector<uint256> GetPendingHashes() const;
    ConstBlockPtr GetRandomTip() const;

    VertexPtr GetVertexCache(const uint256&) const;
    VertexPtr GetVertex(const uint256&) const;
    VertexPtr GetMsVertexCache(const uint256&) const;
    MilestonePtr GetMsVertex(size_t height) const;

    /** Gets a list of block to verify by the post-order DFS */
    std::vector<ConstBlockPtr> GetSortedSubgraph(const ConstBlockPtr& pblock);

    friend inline bool operator<(const Chain& a, const Chain& b) {
        auto a_chainwork = a.GetMilestones().empty() ? 0 : a.GetChainHead()->chainwork;
        auto b_chainwork = b.GetMilestones().empty() ? 0 : b.GetChainHead()->chainwork;
        return a_chainwork < b_chainwork;
    }

    bool IsMainChain() const {
        return ismainchain_;
    }

    const ConcurrentQueue<MilestonePtr>& GetMilestones() const {
        return milestones_;
    }

    size_t GetLeastHeightCached() const {
        if (milestones_.empty()) {
            return UINT64_MAX;
        }

        return milestones_.front()->height;
    }

    std::vector<uint256> GetPeerChainHead() const;

    void AddNewMilestone(const Vertex& ms) {
        milestones_.emplace_back(ms.snapshot);
    }

    const Cumulator* GetCumulator(const uint256&) const;

    /**
     * Off-line verification (building ledger) on a level set
     * performed when we add a milestone block to this chain.
     * Updates TXOC of the of the chain on whole level set
     */
    VertexPtr Verify(const ConstBlockPtr&);

    /**
     * Removes oldest milestone as well as corresponding data
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
    ConcurrentQueue<MilestonePtr> milestones_;

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
    ConcurrentHashMap<uint256, uint256> prevRedempHashMap_;

    /**
     * Caches all the hashes of the previous registration blocks that
     * is going to have their redemption status changed.
     */
    ConcurrentHashSet<uint256> prevRegsToModify_;

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
    void CheckTxPartition(Vertex&);

    Coin GetPrevReward(const Vertex& vtx) const {
        return GetVertex(vtx.cblock->GetPrevHash())->cumulativeReward;
    }

    uint256 GetPrevRedempHash(const uint256& h) const;

    // friend decleration for running a test
    friend class TestChainVerification;
};

typedef std::unique_ptr<Chain> ChainPtr;

inline double CalculateAllowedDist(const Cumulator& cum, size_t msHeight) {
    return cum.Percentage(msHeight) * (GetParams().sortitionCoefficient * GetParams().maxTarget.GetDouble());
}

#endif // EPIC_CHAIN_H
