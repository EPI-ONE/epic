#include "consensus.h"
#include "dag_manager.h"

#include <algorithm>
#include <numeric>

////////////////
// ChainState
//
ChainState::ChainState(const std::shared_ptr<ChainState>& previous,
                       const ConstBlockPtr& msBlock,
                       std::vector<RecordWPtr>&& lvs)
    : height(previous->height + 1), milestoneTarget(previous->milestoneTarget), blockTarget(previous->blockTarget),
      hashRate(previous->hashRate), lastUpdateTime(previous->lastUpdateTime), nTxnsCounter_(previous->nTxnsCounter_),
      nBlkCounter_(previous->nBlkCounter_), lvs_(std::move(lvs)) {
    chainwork = previous->chainwork + (GetParams().maxTarget / previous->milestoneTarget);
    UpdateDifficulty(msBlock->GetTime());
}

ChainState::ChainState(VStream& payload) {
    payload >> *this;
}

void ChainState::UpdateDifficulty(uint32_t blockUpdateTime) {
    auto sumTxns = [&](const RecordWPtr& recPtr) -> uint32_t {
        const std::vector<uint8_t>& validity = (*recPtr.lock()).validity;
        return std::accumulate(validity.begin(), validity.end(), 0, [](const size_t& sum, const uint8_t& v) {
            return sum + (v & NodeRecord::Validity::VALID);
        });
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
            for (const auto& recPtr : DAG->GetLevelSet(cursor->cblock->GetHash(), false)) {
                nTxnsCounter_ += sumTxns(recPtr);
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
    for (const auto& recPtr : lvs_) {
        nTxnsCounter_ += sumTxns(recPtr);
        nBlkCounter_ += lvs_.size();
    }

    // Exponential moving average
    static const double alpha = 0.8; // smoothing parameter
    hashRate =
        hashRate * alpha + ((height - 1) % GetParams().interval + 1) * GetMsDifficulty() / timespan * (1 - alpha);

    if (!IsDiffTransition()) {
        return;
    }

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

    lastUpdateTime = blockUpdateTime;
    nTxnsCounter_  = 0;
    nBlkCounter_   = 0;
}

void ChainState::UpdateTXOC(TXOC&& txoc) {
    txoc_.Merge(std::move(txoc));
}

const uint256& ChainState::GetMilestoneHash() const {
    return (*lvs_.back().lock()).cblock->GetHash();
}

ChainStatePtr CreateNextChainState(ChainStatePtr previous, NodeRecord& record, std::vector<RecordWPtr>&& lvs) {
    auto pcs = std::make_shared<ChainState>(previous, record.cblock, std::move(lvs));
    record.LinkChainState(pcs);
    return pcs;
}

////////////////
// NodeRecord
//
NodeRecord::NodeRecord() : minerChainHeight(0), optimalStorageSize_(0) {}

NodeRecord::NodeRecord(const ConstBlockPtr& blk) : cblock(blk), minerChainHeight(0), optimalStorageSize_(0) {
    if (cblock) {
        auto n = cblock->GetTransactionSize();
        if (n > 0) {
            validity.resize(n);
            memset(&validity[0], UNKNOWN, n);
        }
    }
}

NodeRecord::NodeRecord(const Block& blk) : minerChainHeight(0), optimalStorageSize_(0) {
    cblock = std::make_shared<Block>(blk);
    auto n = cblock->GetTransactionSize();
    if (n > 0) {
        validity.resize(n);
        memset(&validity[0], UNKNOWN, n);
    }
}

NodeRecord::NodeRecord(Block&& blk) : minerChainHeight(0), optimalStorageSize_(0) {
    cblock = std::make_shared<Block>(std::move(blk));
    auto n = cblock->GetTransactionSize();
    if (n > 0) {
        validity.resize(n);
        memset(&validity[0], UNKNOWN, n);
    }
}

NodeRecord::NodeRecord(VStream& s) {
    s >> *this;
}

void NodeRecord::LinkChainState(const ChainStatePtr& pcs) {
    snapshot    = pcs;
    isMilestone = true;
}

void NodeRecord::UpdateReward(const Coin& prevReward) {
    // cumulative reward without fee; default for blocks except first registration
    cumulativeReward = prevReward + GetParams().reward;

    if (cblock->HasTransaction()) {
        if (cblock->IsRegistration()) {
            // remaining reward = last cumulative reward - redemption amount
            cumulativeReward -= cblock->GetTransactions()[0]->GetOutputs()[0].value;
        }

        cumulativeReward += fee;
    }

    // update reward of miletone
    if (isMilestone) {
        cumulativeReward +=
            GetParams().reward * ((snapshot->GetLevelSet().size() - 1) / GetParams().msRewardCoefficient);
    }
}

size_t NodeRecord::GetOptimalStorageSize() {
    if (optimalStorageSize_ > 0) {
        return optimalStorageSize_;
    }

    optimalStorageSize_ += GetSizeOfVarInt(height)                                         // block height
                           + GetSizeOfVarInt(cumulativeReward.GetValue())                  // reward
                           + GetSizeOfVarInt(minerChainHeight)                             // miner chain height
                           + ::GetSizeOfCompactSize(validity.size()) + validity.size() * 1 // validity
                           + 1                                                             // RedemptionStatus
                           + 1;                                                            // MilestoneStatus

    // ChainState
    if (snapshot != nullptr) {
        optimalStorageSize_ += (GetSizeOfVarInt(snapshot->height)     // ms height
                                + GetSizeOfVarInt(snapshot->hashRate) // hash rate
                                + 4                                   // chain work
                                + 4                                   // ms target
                                + 4                                   // block target
        );
    }

    return optimalStorageSize_;
}

std::string std::to_string(const ChainState& cs) {
    std::string s = "Chain State {\n";
    s += strprintf("   height:                %s \n", cs.height);
    s += strprintf("   chainwork:             %s \n", cs.chainwork.GetCompact());
    s += strprintf("   last update time:      %s \n", cs.lastUpdateTime);
    s += strprintf("   ms target:             %s \n", cs.milestoneTarget.GetCompact());
    s += strprintf("   block target:          %s \n", cs.blockTarget.GetCompact());
    s += strprintf("   hash rate:             %s \n", cs.hashRate);
    s += strprintf("   avg. # txns per block: %s \n", cs.GetAverageTxnsPerBlock());
    s += "   }\n";
    return s;
}

std::string std::to_string(const NodeRecord& rec, bool showtx) {
    std::string s = "NodeRecord {\n";
    s += strprintf("   at height :   %i \n", rec.height);
    s += strprintf("   is milestone: %s \n\n", rec.isMilestone);

    if (rec.snapshot) {
        s += "   with snapshot of ";
        s += to_string(*(rec.snapshot));
    }

    if (rec.cblock) {
        s += strprintf("   contains%s \n", std::to_string(*(rec.cblock), showtx, rec.validity));
    }

    s += strprintf("   miner chain height: %s \n", rec.minerChainHeight);
    s += strprintf("   cumulative reward:  %s \n", rec.cumulativeReward.GetValue());

    const std::array<std::string, 3> enumRedeemption{"IS_NOT_REDEMPTION", "NOT_YET_REDEEMED", "IS_REDEEMED"};
    s += strprintf("   redemption status:  %s \n", enumRedeemption[rec.isRedeemed]);
    return s;
}
