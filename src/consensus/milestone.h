// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_MILESTONE_H
#define EPIC_MILESTONE_H

#include "utxo.h"

class Vertex;

class Milestone {
public:
    uint64_t height;
    arith_uint256 chainwork;
    arith_uint256 milestoneTarget;
    arith_uint256 blockTarget;
    float hashRate;
    uint32_t lastUpdateTime = 0;
    bool stored             = false;

    // constructor of a milestone of genesis.
    Milestone() = default;
    // constructor of a milestone with all data fields
    Milestone(const std::shared_ptr<Milestone>&,
              const ConstBlockPtr&,
              std::vector<std::weak_ptr<Vertex>>&&,
              RegChange&&,
              TXOC&&);
    Milestone(VStream&);
    Milestone(const Milestone&) = default;

    /* simple constructor (now for test only) */
    Milestone(uint64_t height,
              arith_uint256 chainwork,
              arith_uint256 milestoneTarget,
              arith_uint256 blockTarget,
              float hashRate,
              uint32_t lastUpdateTime,
              std::vector<std::weak_ptr<Vertex>>&& lvs,
              uint32_t nTxnsCounter = 0,
              uint32_t nBlkCounter  = 0)
        : height(height), chainwork(chainwork), milestoneTarget(milestoneTarget), blockTarget(blockTarget),
          hashRate(hashRate), lastUpdateTime(lastUpdateTime), nTxnsCounter_(nTxnsCounter), nBlkCounter_(nBlkCounter),
          lvs_(std::move(lvs)) {}

    bool IsDiffTransition() const {
        return (height % GetParams().interval) == 0;
    }

    uint64_t GetBlockDifficulty() const {
        return (arith_uint256(GetParams().maxTarget) / (blockTarget + 1)).GetLow64();
    }

    uint64_t GetMsDifficulty() const {
        return (arith_uint256(GetParams().maxTarget) / (milestoneTarget + 1)).GetLow64();
    }

    // returns the cumulative number of transactions starting from the previous difficulty transition milestone
    uint32_t GetTxnsCounter() const {
        return nTxnsCounter_;
    }

    uint32_t GetAverageTxnsPerBlock() const {
        if (nBlkCounter_ != 0) {
            return nTxnsCounter_ / nBlkCounter_;
        }
        return 0;
    }

    const std::vector<std::weak_ptr<Vertex>>& GetLevelSet() const {
        return lvs_;
    }

    void PushBlkToLvs(const std::shared_ptr<Vertex>& vtx) {
        lvs_.emplace_back(vtx);
    }

    std::shared_ptr<Vertex> GetMilestone() const {
        return lvs_.back().lock();
    }

    const uint256& GetMilestoneHash() const;

    const TXOC& GetTXOC() const {
        return txoc_;
    }

    const RegChange& GetRegChange() const {
        return regChange_;
    }

    // returns the number of total transactions in this milestone
    size_t GetNumOfValidTxns() const;

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(hashRate);
        if (ser_action.ForRead()) {
            milestoneTarget.SetCompact(ser_readdata32(s));
            blockTarget.SetCompact(ser_readdata32(s));
        } else {
            ::Serialize(s, milestoneTarget.GetCompact());
            ::Serialize(s, blockTarget.GetCompact());
        }
    }

    /**
     * Does NOT compare lvs_, regChange, and txoc_
     */
    bool operator==(const Milestone& rhs) const {
        // clang-format off
        return hashRate                     == rhs.hashRate &&
               milestoneTarget.GetCompact() == rhs.milestoneTarget.GetCompact() &&
               blockTarget.GetCompact()     == rhs.blockTarget.GetCompact() &&
               (chainwork == 0 || rhs.chainwork == 0) ? true : chainwork == rhs.chainwork &&
               (!lastUpdateTime || !rhs.lastUpdateTime) ? true : lastUpdateTime == rhs.lastUpdateTime;
        // clang-format on
    }

    bool operator!=(const Milestone& another) const {
        return !(*this == another);
    }

private:
    // counters from last updated time
    uint32_t nTxnsCounter_;
    uint32_t nBlkCounter_;

    // a vector consists of hashes of blocks in level set of this milestone where the last element is vertex of
    // milestone
    std::vector<std::weak_ptr<Vertex>> lvs_;

    // TXOC: changes on transaction outputs from previous milestone
    TXOC txoc_;

    // Incremental change of the last registration block on each peer chain,
    // whose elements are pairs consisting of:
    //   <peer chain head, hash of the last registration block on this peer chain>
    RegChange regChange_;

    void UpdateDifficulty(uint32_t blockUpdateTime);
};

using MilestonePtr = std::shared_ptr<Milestone>;

MilestonePtr CreateNextMilestone(MilestonePtr previous,
                                 Vertex& vertex,
                                 std::vector<std::weak_ptr<Vertex>>&& lvs,
                                 RegChange&& = RegChange{},
                                 TXOC&&      = TXOC{});

namespace std {
string to_string(const Milestone&);
} // namespace std

#endif // EPIC_MILESTONE_H
