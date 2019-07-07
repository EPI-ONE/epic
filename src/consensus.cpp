#include "consensus.h"

////////////////
// ChainState
//
ChainState::ChainState(std::shared_ptr<ChainState> previous,
                       const ConstBlockPtr& msBlock,
                       std::vector<uint256>&& lvsHash)
    : height(previous->height + 1), lastUpdateTime(previous->lastUpdateTime),
      milestoneTarget(previous->milestoneTarget), blockTarget(previous->blockTarget), lvsHashes_(std::move(lvsHash)) {
    chainwork = previous->chainwork + (GetParams().maxTarget / UintToArith256(msBlock->GetHash()));
    UpdateDifficulty(msBlock->GetTime());
}

ChainState::ChainState(VStream& payload) {
    payload >> *this;
}

void ChainState::UpdateDifficulty(uint32_t blockUpdateTime) {
    uint32_t timespan = blockUpdateTime - lastUpdateTime, targetTimespan = GetParams().targetTimespan;
    if (timespan < targetTimespan / 4) {
        timespan = targetTimespan / 4;
    }

    if (timespan > targetTimespan * 4) {
        timespan = targetTimespan * 4;
    }

    if (height == 1) {
        timespan = GetParams().timeInterval;
    }

    if (!IsDiffTransition()) {
        hashRate = ((height + 1) % GetParams().interval) * GetMsDifficulty() / timespan;
        return;
    }

    hashRate        = GetParams().interval * GetMsDifficulty() / timespan;
    milestoneTarget = milestoneTarget * timespan / targetTimespan;
    blockTarget     = milestoneTarget * arith_uint256(GetParams().targetTPS);
    blockTarget *= GetParams().timeInterval;

    if (blockTarget > GetParams().maxTarget) {
        // in case that it is not difficult in this round
        blockTarget     = GetParams().maxTarget;
        milestoneTarget = blockTarget * arith_uint256(GetParams().timeInterval / GetParams().targetTPS);
    }

    lastUpdateTime = blockUpdateTime;
}

void ChainState::UpdateTXOC(TXOC&& txoc) {
    txoc_.Merge(std::move(txoc));
}

ChainStatePtr CreateNextChainState(ChainStatePtr previous, NodeRecord& record, std::vector<uint256>&& hashes) {
    auto pcs = std::make_shared<ChainState>(previous, record.cblock, std::move(hashes));
    record.LinkChainState(pcs);
    return pcs;
}

////////////////
// NodeRecord
//
NodeRecord::NodeRecord() : minerChainHeight(0), validity(UNKNOWN), optimalStorageSize_(0) {}

NodeRecord::NodeRecord(const ConstBlockPtr& blk)
    : cblock(blk), minerChainHeight(0), validity(UNKNOWN), optimalStorageSize_(0) {}

NodeRecord::NodeRecord(const Block& blk) : minerChainHeight(0), validity(UNKNOWN), optimalStorageSize_(0) {
    cblock = std::make_shared<Block>(blk);
}

NodeRecord::NodeRecord(Block&& blk) : minerChainHeight(0), validity(UNKNOWN), optimalStorageSize_(0) {
    cblock = std::make_shared<Block>(std::move(blk));
}

NodeRecord::NodeRecord(VStream& s) {
    s >> *this;
}

void NodeRecord::LinkChainState(const ChainStatePtr& pcs) {
    snapshot    = pcs;
    isMilestone = true;
}

void NodeRecord::UpdateReward(const Coin& prevReward) {
    assert(validity != Validity::UNKNOWN);
    // cumulate reward without fee; default for blocks except first registration
    cumulativeReward = prevReward + GetParams().reward;

    if (cblock->HasTransaction() && validity != Validity::INVALID) {
        if (cblock->IsRegistration()) {
            // remaining reward = last cumulative reward - redemption amount
            cumulativeReward = prevReward - cblock->GetTransaction()->GetOutputs()[0].value;
        } else { // for normal valid transaction
            cumulativeReward = prevReward + fee;
        }
    }

    // update reward of miletone
    if (isMilestone) {
        cumulativeReward +=
            GetParams().reward * ((snapshot->GetRecordHashes().size() - 1) / GetParams().msRewardCoefficient);
    }
}

size_t NodeRecord::GetOptimalStorageSize() {
    if (optimalStorageSize_ > 0) {
        return optimalStorageSize_;
    }

    optimalStorageSize_ += GetSizeOfVarInt(height)                        // block height
                           + GetSizeOfVarInt(cumulativeReward.GetValue()) // reward
                           + GetSizeOfVarInt(minerChainHeight)            // miner chain height
                           + 1                                            // validity
                           + 1                                            // RedemptionStatus
                           + 1;                                           // MilestoneStatus

    // ChainState
    if (snapshot != nullptr) {
        optimalStorageSize_ += (GetSizeOfVarInt(snapshot->height)     // ms height
                                + GetSizeOfVarInt(snapshot->hashRate) // hash rate
                                + 4                                   // last update time
                                + 4                                   // chain work
                                + 4                                   // ms target
                                + 4                                   // block target
        );
    }

    return optimalStorageSize_;
}

std::string std::to_string(const ChainState& cs) {
    std::string s = "Chain State {\n";
    s += strprintf("   height: %s \n", cs.height);
    s += strprintf("   chainwork: %s \n", cs.chainwork.GetCompact());
    s += strprintf("   last update time: %s \n", cs.lastUpdateTime);
    s += strprintf("   ms target: %s \n", cs.milestoneTarget.GetCompact());
    s += strprintf("   block target: %s \n", cs.blockTarget.GetCompact());
    s += strprintf("   hash rate: %s \n", cs.hashRate);
    s += "   }\n";
    return s;
}

std::string std::to_string(const NodeRecord& rec, bool showtx) {
    std::string s = "NodeRecord {\n";
    s += strprintf("   is milestone: %s \n\n", rec.isMilestone);
    s += strprintf("   contained%s \n", std::to_string(*(rec.cblock), showtx));
    s += strprintf("   miner chain height: %s \n", rec.minerChainHeight);
    s += strprintf("   cumulative reward: %s \n", rec.cumulativeReward.GetValue());

    if (rec.snapshot) {
        s += "   with snapshot of ";
        s += to_string(*(rec.snapshot));
    }

    static const std::array<std::string, 3> enumName{"UNKNOWN", "VALID", "INVALID"};
    s += strprintf("   status: %s \n }", enumName[rec.validity]);
    return s;
}
