#include "consensus.h"

////////////////
// ChainState
//
ChainState::ChainState() : height(0), chainwork(arith_uint256(0)), lastUpdateTime(GENESIS.GetTime()) {
    milestoneTarget = params.initialMsTarget * 2 / arith_uint256(params.targetTimespan);
    blockTarget     = milestoneTarget * arith_uint256(params.targetTPS) * arith_uint256(params.timeInterval);
    hashRate        = GetMsDifficulty() / params.timeInterval;
}

ChainState::ChainState(std::shared_ptr<ChainState> previous, const ConstBlockPtr& msBlock, std::vector<uint256>&& lvsHash)
    : height(previous->height + 1), lastUpdateTime(previous->lastUpdateTime),
      milestoneTarget(previous->milestoneTarget), blockTarget(previous->blockTarget), lvsHash_(lvsHash) {
    chainwork    = previous->chainwork + (params.maxTarget / UintToArith256(msBlock->GetHash()));
    UpdateDifficulty(msBlock->GetTime());
}

ChainState::ChainState(VStream& payload) {
    payload >> *this;
}

void ChainState::UpdateDifficulty(const uint64_t blockUpdateTime) {
    uint64_t timespan = blockUpdateTime - lastUpdateTime, targetTimespan = params.targetTimespan;
    if (timespan < targetTimespan / 4) {
        timespan = targetTimespan / 4;
    }

    if (timespan > targetTimespan * 4) {
        timespan = targetTimespan * 4;
    }

    if (height == 1) {
        lastUpdateTime = blockUpdateTime;
        timespan       = params.timeInterval;
    }

    if (!IsDiffTransition()) {
        hashRate = ((height + 1) % params.interval) * GetMsDifficulty() / timespan;
        return;
    }

    hashRate        = params.interval * GetMsDifficulty() / timespan;
    milestoneTarget = milestoneTarget * timespan / targetTimespan;
    blockTarget     = milestoneTarget * arith_uint256(params.targetTPS);
    blockTarget *= params.timeInterval;

    if (blockTarget > params.maxTarget) {
        // in case that it is not difficult in this round
        blockTarget     = params.maxTarget;
        milestoneTarget = blockTarget * arith_uint256(params.timeInterval / params.targetTPS);
    }

    lastUpdateTime = blockUpdateTime;
}

ChainStatePtr make_shared_ChainState() {
    return std::make_shared<ChainState>();
}

ChainStatePtr make_shared_ChainState(ChainStatePtr previous, NodeRecord& record, std::vector<uint256>&& hashes) {
    auto pcs = std::make_shared<ChainState>(previous, record.cblock, std::move(hashes));
    record.LinkChainState(pcs);
    return pcs;
}

////////////////
// NodeRecord
//
NodeRecord::NodeRecord() : minerChainHeight(0), validity(UNKNOWN), optimalStorageSize(0) {}

NodeRecord::NodeRecord(const ConstBlockPtr& blk)
    : cblock(blk), minerChainHeight(0), validity(UNKNOWN), optimalStorageSize(0) {}

NodeRecord::NodeRecord(const BlockNet& blk) : minerChainHeight(0), validity(UNKNOWN), optimalStorageSize(0) {
    cblock = std::make_shared<BlockNet>(blk);
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

void NodeRecord::Serialize(VStream& s) const {
    s << *cblock;
    s << VARINT(cumulativeReward.GetValue());
    s << VARINT(minerChainHeight);

    if (cblock->HasTransaction()) {
        s << (uint8_t) validity;
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
    cblock = std::make_shared<BlockNet>(s);
    uint64_t r = 0;
    s >> VARINT(r);
    cumulativeReward = Coin(r);
    s >> VARINT(minerChainHeight);

    if (cblock->HasTransaction()) {
        validity = (Validity) ser_readdata8(s);
    }

    enum MilestoneStatus msFlag = (MilestoneStatus) ser_readdata8(s);
    isMilestone                 = msFlag == IS_TRUE_MILESTONE;

    if (msFlag > 0) {
        snapshot = std::make_shared<ChainState>(s);
    }
}

size_t NodeRecord::GetOptimalStorageSize() {
    if (optimalStorageSize > 0) {
        return optimalStorageSize;
    }
    optimalStorageSize = cblock->GetOptimalEncodingSize();
    optimalStorageSize += GetSizeOfVarInt(cumulativeReward.GetValue());
    optimalStorageSize += GetSizeOfVarInt(minerChainHeight);

    if (cblock->HasTransaction()) {
        optimalStorageSize += 1;
    }

    optimalStorageSize += 1;

    if (snapshot != nullptr) {
        optimalStorageSize += GetSizeOfVarInt(snapshot->height);
        optimalStorageSize += 4;
        optimalStorageSize += 8;
        optimalStorageSize += 4;
        optimalStorageSize += 4;
        optimalStorageSize += GetSizeOfVarInt(snapshot->hashRate);
    }

    return optimalStorageSize;
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

    static const std::string enumName[] = {"UNKNOWN", "VALID", "INVALID"};
    s += strprintf("   status: %s \n }", enumName[rec.validity]);
    return s;
}

NodeRecord NodeRecord::CreateGenesisRecord() {
    NodeRecord genesis(GENESIS);
    static auto genesisState = make_shared_ChainState();
    genesis.LinkChainState(genesisState);
    return genesis;
}

const NodeRecord GENESIS_RECORD = NodeRecord::CreateGenesisRecord();
