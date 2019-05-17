#include "consensus.h"

////////////////
// ChainState
//
ChainState::ChainState()
    : height(0), chainwork(arith_uint256(0)), lastUpdateTime(GENESIS.GetTime()), pprevious(nullptr) {
    milestoneTarget = params.initialMsTarget * 2 / arith_uint256(params.targetTimespan);
    blockTarget     = milestoneTarget * arith_uint256(params.targetTPS) * arith_uint256(params.timeInterval);
    hashRate        = GetMsDifficulty() / params.timeInterval;
}

ChainState::ChainState(RecordPtr pblock, std::shared_ptr<ChainState> previous) : pprevious(previous) {
    pblock->LinkChainState(*this);
    if (pprevious) {
        height    = pprevious->height + 1;
        chainwork = pprevious->chainwork + (params.maxTarget / UintToArith256(pblock->cBlock->GetHash()));

        lastUpdateTime  = pprevious->lastUpdateTime;
        milestoneTarget = pprevious->milestoneTarget;
        blockTarget     = pprevious->blockTarget;

        UpdateDifficulty(pblock->cBlock->GetTime());
    } else {
        // TODO: go somewhere to find the height
    }
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

////////////////
// NodeRecord
//
NodeRecord::NodeRecord() : minerChainHeight(0), validity(UNKNOWN), optimalStorageSize(0) {}

NodeRecord::NodeRecord(const ConstBlockPtr& blk)
    : cBlock(blk), minerChainHeight(0), validity(UNKNOWN), optimalStorageSize(0) {}

NodeRecord::NodeRecord(const BlockNet& blk) : minerChainHeight(0), validity(UNKNOWN), optimalStorageSize(0) {
    cBlock = std::make_shared<BlockNet>(blk);
}

NodeRecord::NodeRecord(VStream& s) {
    Deserialize(s);
}

void NodeRecord::InvalidateMilestone() {
    isMilestone = false;
    snapshot.reset();
}

void NodeRecord::LinkChainState(ChainState& ss) {
    snapshot = std::make_shared<ChainState>(ss);
    isMilestone = true;
}

void NodeRecord::Serialize(VStream& s) const {
    s << *cBlock;
    s << VARINT(cumulativeReward.GetValue());
    s << VARINT(minerChainHeight);

    if (cBlock->HasTransaction()) {
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
    BlockNet b;
    s >> b;
    cBlock = std::make_shared<BlockNet>(b);
    uint64_t r = 0;
    s >> VARINT(r);
    cumulativeReward = Coin(r);
    s >> VARINT(minerChainHeight);

    if (cBlock->HasTransaction()) {
        validity = (Validity) ser_readdata8(s);
    }

    enum MilestoneStatus msFlag = (MilestoneStatus) ser_readdata8(s);
    isMilestone                 = msFlag == IS_TRUE_MILESTONE;

    if (msFlag > 0) {
        snapshot = std::make_shared<ChainState>();
        s >> *snapshot;
    }
}

size_t NodeRecord::GetOptimalStorageSize() {
    if (optimalStorageSize > 0) {
        return optimalStorageSize;
    }
    optimalStorageSize = cBlock->GetOptimalEncodingSize();
    optimalStorageSize += GetSizeOfVarInt(cumulativeReward.GetValue());
    optimalStorageSize += GetSizeOfVarInt(minerChainHeight);

    if (cBlock->HasTransaction()) {
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

std::string std::to_string(const NodeRecord& rec) {
    std::string s = "NodeRecord {\n";
    s += strprintf("  contained%s \n", std::to_string(*(rec.cBlock)));
    s += strprintf("   miner chain height: %s \n", rec.minerChainHeight);
    s += strprintf("   cumulative reward: %s \n", rec.cumulativeReward.GetValue());

    if (rec.snapshot) {
        s += "   with chain state snapshot {\n";
        s += strprintf("     height: %s \n", rec.snapshot->height);
        s += strprintf("     chainwork: %s \n", rec.snapshot->chainwork.GetCompact());
        s += strprintf("     last update time: %s \n", rec.snapshot->lastUpdateTime);
        s += strprintf("     ms target: %s \n", rec.snapshot->milestoneTarget.GetCompact());
        s += strprintf("     block target: %s \n", rec.snapshot->blockTarget.GetCompact());
        s += strprintf("     hash rate: %s \n", rec.snapshot->hashRate);
        s += "   }\n";
    }

    static const std::string enumName[] = {"UNKNOWN", "VALID", "INVALID"};

    s += strprintf("   status: %s \n }", enumName[rec.validity]);

    return s;
}

NodeRecord NodeRecord::CreateGenesisRecord() {
    NodeRecord genesis(GENESIS);
    static ChainState genesisState;
    genesis.LinkChainState(genesisState);
    return genesis;
}

const NodeRecord GENESIS_RECORD = NodeRecord::CreateGenesisRecord();
