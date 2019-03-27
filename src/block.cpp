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
    tx.SetParent(*this);
    transaction_.clear();
    transaction_.push_back(tx);
}

const uint256& Block::GetTxHash() {
    if (HasTransaction()) {
        return transaction_.front().GetHash();
    }
    return Hash::ZERO_HASH;
}

arith_uint256 Block::GetTargetAsInteger() const {
    arith_uint256 target = arith_uint256().SetCompact(diffTarget_);
    if (target <= 0 || target > params.maxTarget) {
        throw "Bad difficulty target: " + target.ToString();
    }
    return target;
}

bool Block::CheckPOW(bool throwException) {
    arith_uint256 target = GetTargetAsInteger();
    arith_uint256 hash   = UintToArith256(GetHash());
    if (hash > target) {
        if (throwException) {
            throw "Hash is higher than target: " + GetHash().ToString() +
                " vs " + target.ToString();
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
    if (HasTransaction()) {
    } else if (!transaction_.front().Verify()) {
        return false;
    }

    // check the conditions of the first registration block
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
        uint8_t v = ser_readdata8(s);
        transaction_.front().SetStatus((Transaction::Validity) v);
    }
    enum MilestoneStatus msFlag = (MilestoneStatus) ser_readdata8(s);
    isMilestone_                = msFlag == IS_TRUE_MILESTONE;
    if (msFlag > 0) {
        // TODO: store the milestone object in some kind of cache
        milestoneInstance_ = new Milestone();
        s >> *milestoneInstance_;
    }
}

std::string Block::ToString() {
    std::string s;
    s += " Block{ \n";
    s += strprintf("   hash: %s \n", GetHash().ToString());
    s += strprintf("   version: %s \n", version_);
    s += strprintf("   milestone block: %s \n", milestoneBlockHash_.ToString());
    s += strprintf("   previous block: %s \n", prevBlockHash_.ToString());
    s += strprintf("   tip block: %s \n", tipBlockHash_.ToString());
    s += strprintf("   time: %d \n", time_);
    s += strprintf("   difficulty target: %d \n", diffTarget_);
    s += strprintf("   nonce: %d \n ", nonce_);
    if (HasTransaction()) {
        s += "   with transaction:\n";
        s += transaction_.front().ToString();
    }
    return s;
}
