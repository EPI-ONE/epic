// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "block_store.h"
#include "dag_manager.h"
#include "vertex.h"

#include <algorithm>
#include <numeric>

Milestone::Milestone(const std::shared_ptr<Milestone>& previous,
                     const ConstBlockPtr& msBlock,
                     std::vector<VertexWPtr>&& lvs)
    : height(previous->height + 1), milestoneTarget(previous->milestoneTarget), blockTarget(previous->blockTarget),
      hashRate(previous->hashRate), lastUpdateTime(previous->lastUpdateTime), nTxnsCounter_(previous->nTxnsCounter_),
      nBlkCounter_(previous->nBlkCounter_), lvs_(std::move(lvs)) {
    if (previous->chainwork == 0) {
        previous->chainwork = UintToArith256(STORE->GetBestChainWork());
    }
    chainwork = previous->chainwork + (GetParams().maxTarget / previous->milestoneTarget);
    UpdateDifficulty(msBlock->GetTime());
}

Milestone::Milestone(VStream& payload) {
    payload >> *this;
}

void Milestone::UpdateDifficulty(uint32_t blockUpdateTime) {
    auto sumTxns = [](const VertexWPtr& vtxPtr) -> uint32_t {
        const std::vector<uint8_t>& validity = (*vtxPtr.lock()).validity;
        return std::accumulate(validity.begin(), validity.end(), 0,
                               [](const size_t& sum, const uint8_t& v) { return sum + (v & Vertex::Validity::VALID); });
    };

    if (!lastUpdateTime) {
        // Traverse back to the last difficulty update point to recover
        // necessary info for updating difficulty.
        // Although the traversal is expensive, it happens only once when
        // constructing the first new milestone after restarting the daemon.
        nTxnsCounter_ = 0;
        nBlkCounter_  = 0;

        // Start from the previous ms
        auto cursor = DAG->GetState(GetMilestone()->cblock->GetMilestoneHash());

        while (!cursor->snapshot->IsDiffTransition()) {
            for (const auto& vtxPtr : DAG->GetLevelSet(cursor->cblock->GetHash(), false)) {
                nTxnsCounter_ += sumTxns(vtxPtr);
                nBlkCounter_ += cursor->snapshot->lvs_.size();
            }
            cursor = DAG->GetState(cursor->snapshot->GetMilestone()->cblock->GetMilestoneHash());
        }

        lastUpdateTime = cursor->snapshot->GetMilestone()->cblock->GetTime();
    }

    uint32_t timespan         = blockUpdateTime - lastUpdateTime;
    const auto targetTimespan = GetParams().targetTimespan;
    if (timespan < targetTimespan / 4) {
        timespan = targetTimespan / 4;
    }

    if (timespan > targetTimespan * 4) {
        timespan = targetTimespan * 4;
    }

    if (height == 1) {
        timespan = GetParams().timeInterval;
    }

    // Count the total number of valid transactions and blocks
    // in the period with exponential smoothing
    for (const auto& vtxPtr : lvs_) {
        nTxnsCounter_ += sumTxns(vtxPtr);
        nBlkCounter_ += lvs_.size();
    }

    // Exponential moving average
    static const double alpha = 0.8; // smoothing parameter
    hashRate =
        hashRate * alpha + ((height - 1) % GetParams().interval + 1) * GetMsDifficulty() / timespan * (1 - alpha);

    if (!IsDiffTransition()) {
        return;
    }

    uint64_t oldMsDiff  = GetMsDifficulty();
    uint64_t oldBlkDiff = GetBlockDifficulty();

    milestoneTarget = milestoneTarget / targetTimespan * timespan;
    milestoneTarget.Round(sizeof(uint32_t));

    if (milestoneTarget > GetParams().maxTarget) {
        milestoneTarget = GetParams().maxTarget;
        blockTarget     = milestoneTarget;
    } else {
        static const uint32_t nTxnsCap = GetParams().targetTPS * GetParams().targetTimespan;

        nTxnsCounter_ = std::min(nTxnsCounter_, nTxnsCap);

        // If the average number of txns per block is larger than 95% of the block capacity,
        // we increases the estimation of the actual number of txn arrivals by a factor of 1.1
        // to take into consideration the lost txns due to the limited block capacity.
        if (nTxnsCounter_ / nBlkCounter_ > GetParams().blockCapacity * 0.95) {
            nTxnsCounter_ *= 1.1;
        }

        // We will calculate blockTarget by
        //
        //    milestoneTarget / GetParams().blockCapacity * nTxnsCounter_,
        //
        // but the multiplier nTxnsCounter_ may cause blockTarget overflow,
        // and thus we limit its value to the largest possible value
        // that will not cause blockTarget overflow.

        if (nTxnsCounter_ <= GetParams().blockCapacity) {
            blockTarget = milestoneTarget;
        } else {
            blockTarget = milestoneTarget / GetParams().blockCapacity;

            uint32_t limit = 1 << blockTarget.LeadingZeros();
            nTxnsCounter_  = std::min(nTxnsCounter_, limit);
            nTxnsCounter_  = std::max(nTxnsCounter_, (uint32_t) 1);

            blockTarget *= nTxnsCounter_;
            blockTarget.Round(sizeof(uint32_t));
        }

        if (blockTarget > GetParams().maxTarget) {
            blockTarget = GetParams().maxTarget;
        }
    }

    spdlog::info("Adjusted difficulty. Milestone: {} => {}, normal block: {} => {}.", oldMsDiff, GetMsDifficulty(),
                 oldBlkDiff, GetBlockDifficulty());

    lastUpdateTime = blockUpdateTime;
    nTxnsCounter_  = 0;
    nBlkCounter_   = 0;
}

void Milestone::UpdateTXOC(TXOC&& txoc) {
    txoc_.Merge(std::move(txoc));
}

const uint256& Milestone::GetMilestoneHash() const {
    return (*lvs_.back().lock()).cblock->GetHash();
}

MilestonePtr CreateNextMilestone(MilestonePtr previous, Vertex& vertex, std::vector<VertexWPtr>&& lvs) {
    auto pms = std::make_shared<Milestone>(previous, vertex.cblock, std::move(lvs));
    vertex.LinkMilestone(pms);
    return pms;
}

std::string std::to_string(const Milestone& ms) {
    std::string s = "Chain State {\n";
    s += strprintf("   height:                %s \n", ms.height);
    s += strprintf("   chainwork:             %s \n", ms.chainwork.GetCompact());
    s += strprintf("   last update time:      %s \n", ms.lastUpdateTime);
    s += strprintf("   ms target:             %s \n", ms.milestoneTarget.GetCompact());
    s += strprintf("   block target:          %s \n", ms.blockTarget.GetCompact());
    s += strprintf("   hash rate:             %s \n", ms.hashRate);
    s += strprintf("   avg. # txns per block: %s \n", ms.GetAverageTxnsPerBlock());
    s += "   }\n";
    return s;
}
