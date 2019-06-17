#include "consensus.h"

////////////////
// ChainState
//
ChainState::ChainState(std::shared_ptr<ChainState> previous, const ConstBlockPtr& msBlock, std::vector<uint256>&& lvsHash)
    : height(previous->height + 1), lastUpdateTime(previous->lastUpdateTime),
      milestoneTarget(previous->milestoneTarget), blockTarget(previous->blockTarget), lvsHashes_(lvsHash) {
    chainwork    = previous->chainwork + (GetParams().maxTarget / UintToArith256(msBlock->GetHash()));
    UpdateDifficulty(msBlock->GetTime());
}

ChainState::ChainState(VStream& payload) {
    payload >> *this;
}

void ChainState::UpdateDifficulty(const uint64_t blockUpdateTime) {
    uint64_t timespan = blockUpdateTime - lastUpdateTime, targetTimespan = GetParams().targetTimespan;
    if (timespan < targetTimespan / 4) {
        timespan = targetTimespan / 4;
    }

    if (timespan > targetTimespan * 4) {
        timespan = targetTimespan * 4;
    }

    if (height == 1) {
        lastUpdateTime = blockUpdateTime;
        timespan       = GetParams().timeInterval;
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

ChainStatePtr make_shared_ChainState(ChainStatePtr previous, NodeRecord& record, std::vector<uint256>&& hashes) {
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

NodeRecord::NodeRecord(Block&& blk) : minerChainHeight(0), validity(UNKNOWN), optimalStorageSize_(0) {
    cblock = std::make_shared<BlockNet>(std::move(blk));
}

NodeRecord::NodeRecord(VStream& s) {
    Deserialize(s);
}

void NodeRecord::InvalidateMilestone() {
    isMilestone = false;
    snapshot.reset();
}

void NodeRecord::LinkChainState(const ChainStatePtr& pcs) {
    snapshot    = pcs;
    isMilestone = true;
}

void NodeRecord::UpdateReward(const Coin& prevReward) {
    assert(validity != Validity::UNKNOWN);
    // cumulate reward without fee
    cumulativeReward = prevReward + GetParams().reward;

    if (!(cblock->HasTransaction() && validity == Validity::VALID)) {
        return;
    }

    if (cblock->IsRegistration()) {
        // remaining reward = last cumulative reward - redemption amount
        cumulativeReward = prevReward - cblock->GetTransaction()->GetOutputs()[0].value;
    } else { // for normal valid transaction
        cumulativeReward = prevReward + fee;
    }
}

void NodeRecord::Serialize(VStream& s) const {
    s << *cblock;
    s << VARINT(cumulativeReward.GetValue());
    s << VARINT(minerChainHeight);

    if (cblock->HasTransaction()) {
        s << (uint8_t) validity;
    }

    s << (uint8_t) isRedeemed;
    if (isRedeemed != RedemptionStatus::IS_NOT_REDEMPTION) {
        s << prevRedemHash;
    }

    if (isMilestone) {
        s << (uint8_t) IS_TRUE_MILESTONE;
    } else if (snapshot != nullptr) {
        s << (uint8_t) IS_FAKE_MILESTONE;
    } else {
        s << (uint8_t) IS_NOT_MILESTONE;
    }

    if (snapshot != nullptr) {
        s << *snapshot;
    }
}

void NodeRecord::Deserialize(VStream& s) {
    cblock     = std::make_shared<BlockNet>(s);
    uint64_t r = 0;
    s >> VARINT(r);
    cumulativeReward = Coin(r);
    s >> VARINT(minerChainHeight);

    if (cblock->HasTransaction()) {
        validity = static_cast<Validity>(ser_readdata8(s));
    }

    isRedeemed = static_cast<RedemptionStatus>(ser_readdata8(s));
    if (isRedeemed != RedemptionStatus::IS_NOT_REDEMPTION) {
        s >> prevRedemHash;
    }

    auto msFlag = static_cast<MilestoneStatus>(ser_readdata8(s));
    isMilestone = (msFlag == IS_TRUE_MILESTONE);

    if (msFlag > 0) {
        snapshot = std::make_shared<ChainState>(s);
    }
}

size_t NodeRecord::GetOptimalStorageSize() {
    if (optimalStorageSize_ > 0) {
        return optimalStorageSize_;
    }
    optimalStorageSize_ = cblock->GetOptimalEncodingSize();
    optimalStorageSize_ += GetSizeOfVarInt(cumulativeReward.GetValue());
    optimalStorageSize_ += GetSizeOfVarInt(minerChainHeight);

    if (cblock->HasTransaction()) {
        optimalStorageSize_ += 1;
    }

    optimalStorageSize_ += 1; // RedemptionStatus
    if (isRedeemed != RedemptionStatus::IS_NOT_REDEMPTION) {
        optimalStorageSize_ += 32; // prevRedemHash size
    }
    optimalStorageSize_ += 1; // MilestoneStatus

    if (snapshot != nullptr) {
        optimalStorageSize_ += GetSizeOfVarInt(snapshot->height);
        optimalStorageSize_ += 4;
        optimalStorageSize_ += 8;
        optimalStorageSize_ += 4;
        optimalStorageSize_ += 4;
        optimalStorageSize_ += GetSizeOfVarInt(snapshot->hashRate);
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
