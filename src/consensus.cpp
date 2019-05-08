#include "consensus.h"

ChainState::ChainState() : height(0), lastUpdateTime(GENESIS.GetTime()), chainwork(arith_uint256(0)), pprevious(nullptr) {
    milestoneTarget = params.initialMsTarget * 2 / arith_uint256(params.targetTimespan);
    blockTarget     = milestoneTarget * arith_uint256(params.targetTPS) * arith_uint256(params.timeInterval);
    hashRate        = GetMsDifficulty() / params.timeInterval;
}

ChainState::ChainState(RecordPtr pblock, std::shared_ptr<ChainState> previous) : pprevious(previous) {
    pblock->LinkChainState(*this);
    if (pprevious != nullptr) {
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

void NodeRecord::InvalidateMilestone() {
    isMilestone = false;
    snapshot.reset();
}

void NodeRecord::LinkChainState(ChainState& ss) {
    if (snapshot.use_count() == 0) {
        snapshot = std::make_shared<ChainState>(ss);
    } else {
        snapshot.reset(&ss);
    }

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
    uint64_t r;
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

NodeRecord NodeRecord::CreateGenesisRecord() {
    return NodeRecord(GENESIS);
}

std::shared_ptr<ChainState> make_shared_ChainState() {
    return std::make_shared<ChainState>();
}

std::shared_ptr<ChainState> make_shared_ChainState(RecordPtr pblock, std::shared_ptr<ChainState> previous, bool inMainChain) {
    std::shared_ptr<ChainState> pms = std::make_shared<ChainState>(pblock, previous);
    if (inMainChain) 
        pms->pprevious->pnext = pms;
    return pms;
}

const NodeRecord GENESIS_RECORD = NodeRecord::CreateGenesisRecord();
