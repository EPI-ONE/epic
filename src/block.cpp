#include "block.h"
#include "params.h"
#include "tinyformat.h"
#include <ctime>
static const Params& params = TestNetParams::GetParams();

Block::Block(uint32_t versionNum) {
    version_            = versionNum;
    diffTarget_         = 0x1d07fff8L;
    time_               = std::time(nullptr);
    milestoneBlockHash_ = Hash::ZERO_HASH;
    prevBlockHash_      = Hash::ZERO_HASH;
    tipBlockHash_       = Hash::ZERO_HASH;
}

bool Block::IsRegistration() const {
    if (transaction_.empty()) {
        return false;
    }
    return transaction_.front().IsRegistration();
}

bool Block::IsFirstRegistration() const {
    if (HasTransaction()) {
        return transaction_.front().IsFirstRegistration();
    }
    return false;
}

void Block::AddTransaction(Transaction& tx) {
    // Invalidate cached hash to force recomputation
    hash_.SetNull();
    tx.SetParent(this);
    transaction_.clear();
    transaction_.push_back(tx);
}

const uint256& Block::GetTxHash() {
    return HasTransaction() ? transaction_.front().GetHash() : Hash::ZERO_HASH;
}

arith_uint256 Block::GetTargetAsInteger() const {
    arith_uint256 target = arith_uint256().SetCompact(diffTarget_);
    if (target <= 0 || target > params.maxTarget) {
        throw "Bad difficulty target: " + std::to_string(target);
    }
    return target;
}

bool Block::CheckPOW(bool throwException) {
    arith_uint256 target = GetTargetAsInteger();
    arith_uint256 hash   = UintToArith256(GetHash());
    if (hash > target) {
        if (throwException) {
            throw "Hash is higher than target: " + std::to_string(GetHash()) + " vs " + std::to_string(target);
        } else {
            return false;
        }
    }
    return true;
}

bool Block::Verify() {
    // checks pow
    if (!CheckPOW(false)) {
        return false;
    }
    // checks if the time of block is too far in the future
    if (time_ > std::time(nullptr) + ALLOWED_TIME_DRIFT) {
        return false;
    }
    // verify content of the block
    if (GetOptimalEncodingSize() > MAX_BLOCK_SIZE) {
        return false;
    }
    if (HasTransaction() && !transaction_.front().Verify()) {
        return false;
    }

    // check the conditions of the first registration block
    if (prevBlockHash_ == params.GetGenesisBlockHash()) {
        // Must contain a tx
        if (!HasTransaction()) {
            return false;
        }
        // ... with input from ZERO hash and index -1 and output value 0
        if (!transaction_.front().IsFirstRegistration()) {
            return false;
        }
    }
    return true;
}

void Block::SerializeToDB(VStream& s) const {
    s << *this;
    s << VARINT(cumulativeReward_);
    s << VARINT(minerChainHeight_);
    if (HasTransaction()) {
        s << (uint8_t) transaction_.front().GetStatus();
    }
    if (isMilestone_) {
        s << (uint8_t) IS_TRUE_MILESTONE;
    } else if (milestoneInstance_ != nullptr) {
        s << (uint8_t) IS_FAKE_MILESTONE;
    } else {
        s << (uint8_t) IS_NOT_MILESTONE;
    }
    if (milestoneInstance_ != nullptr) {
        s << *milestoneInstance_;
    }
}

void Block::UnserializeFromDB(VStream& s) {
    s >> *this;
    s >> VARINT(cumulativeReward_);
    s >> VARINT(minerChainHeight_);
    if (HasTransaction()) {
        transaction_.front().SetStatus((Transaction::Validity) ser_readdata8(s));
    }
    enum MilestoneStatus msFlag = (MilestoneStatus) ser_readdata8(s);
    isMilestone_                = msFlag == IS_TRUE_MILESTONE;
    if (msFlag > 0) {
        // TODO: store the milestone object in some kind of cache
        milestoneInstance_ = std::make_shared<Milestone>();
        s >> *milestoneInstance_;
    }
}

std::string std::to_string(Block& block) {
    std::string s;
    s += " Block{ \n";
    s += strprintf("   hash: %s \n", std::to_string(block.GetHash()));
    s += strprintf("   version: %s \n", block.version_);
    s += strprintf("   milestone block: %s \n", std::to_string(block.milestoneBlockHash_));
    s += strprintf("   previous block: %s \n", std::to_string(block.prevBlockHash_));
    s += strprintf("   tip block: %s \n", std::to_string(block.tipBlockHash_));
    s += strprintf("   time: %d \n", std::to_string(block.time_));
    s += strprintf("   difficulty target: %d \n", std::to_string(block.diffTarget_));
    s += strprintf("   nonce: %d \n ", std::to_string(block.nonce_));
    if (block.HasTransaction()) {
        s += "   with transaction:\n";
        s += std::to_string(block.transaction_.front());
    }
    return s;
}
