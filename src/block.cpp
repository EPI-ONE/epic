#include "block.h"

BlockHeader::BlockHeader() {
    SetNull();
}

void BlockHeader::SetNull() {
    milestoneBlockHash_.SetNull();
    prevBlockHash_.SetNull();
    tipBlockHash_.SetNull();
    version_    = 0;
    time_       = 0;
    diffTarget_ = 0;
    nonce_      = 0;
}

bool BlockHeader::IsNull() const {
    return time_ == 0;
}

Block::Block() {
    SetNull();
}

Block::Block(uint32_t versionNum) {
    version_            = versionNum;
    diffTarget_         = 0x1d07fff8L;
    time_               = std::time(nullptr);
    milestoneBlockHash_ = Hash::ZERO_HASH;
    prevBlockHash_      = Hash::ZERO_HASH;
    tipBlockHash_       = Hash::ZERO_HASH;
}

Block::Block(const BlockHeader& header) : BlockHeader(header) {
    SetNull();
}

void Block::SetNull() {
    BlockHeader::SetNull();
    transaction_.reset();
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

    if (HasTransaction() && !transaction_->Verify()) {
        return false;
    }

    // check the conditions of the first registration block
    if (prevBlockHash_ == genesisBlockHash) {
        // Must contain a tx
        if (!HasTransaction()) {
            return false;
        }

        // ... with input from ZERO hash and index -1 and output value 0
        if (!transaction_->IsFirstRegistration()) {
            return false;
        }
    }

    return true;
}

void Block::AddTransaction(Transaction& tx) {
    // Invalidate cached hash to force recomputation
    hash_.SetNull();
    tx.SetParent(this);
    transaction_.reset();
    transaction_ = std::forward<Transaction>(tx);
}

bool Block::HasTransaction() const {
    return transaction_.has_value();
}

void Block::SetMinerChainHeight(uint32_t height) {
    minerChainHeight_ = height;
}

void Block::ResetReward() {
    cumulativeReward_ = ZERO_COIN;
}

void Block::SetDifficultyTarget(uint32_t target) {
    diffTarget_ = target;
}

void Block::SetTime(time_t time) {
    time_ = time;
}

const time_t Block::GetTime() const {
    return time_;
}

void Block::SetNonce(uint32_t nonce) {
    nonce_ = nonce;
}

void Block::InvalidateMilestone() {
    isMilestone_ = false;
    milestone_.reset();
}

void Block::SetMilestoneInstance(Milestone& ms) {
    if (milestone_.use_count() == 0) {
        milestone_ = std::make_shared<Milestone>(ms);
    } else {
        milestone_.reset(&ms);
    }

    isMilestone_ = true;
}

/*
 * TODO: serialize the header and compute the hash of the block
 * Currently is only a placeholder.
 */
const uint256& Block::GetHash() {
    if (hash_.IsNull()) {
        VStream s;
        SerializeToHash(s);
        hash_ = Hash<1>(s);
    }

    return hash_;
}

const uint256& Block::GetTxHash() {
    if (HasTransaction()) {
        transaction_->FinalizeHash();
        return transaction_->GetHash();
    }

    return Hash::ZERO_HASH;
}

// TODO
size_t Block::GetOptimalEncodingSize() const {
    return 0;
}

bool Block::IsRegistration() const {
    return HasTransaction() ? transaction_->IsRegistration() : false;
}

bool Block::IsFirstRegistration() const {
    return HasTransaction() ? transaction_->IsFirstRegistration() : false;
}

arith_uint256 Block::GetChainWork() const {
    arith_uint256 target = GetTargetAsInteger();
    return LARGEST_HASH / (target + 1);
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

void Block::SerializeToDB(VStream& s) const {
    s << *this;
    s << VARINT(cumulativeReward_);
    s << VARINT(minerChainHeight_);

    if (HasTransaction()) {
        s << (uint8_t) transaction_->GetStatus();
    }

    if (isMilestone_) {
        s << (uint8_t) IS_TRUE_MILESTONE;
    } else if (milestone_ != nullptr) {
        s << (uint8_t) IS_FAKE_MILESTONE;
    } else {
        s << (uint8_t) IS_NOT_MILESTONE;
    }

    if (milestone_ != nullptr) {
        SerializeMilestone(s, *milestone_);
    }
}

void Block::DeserializeFromDB(VStream& s) {
    s >> *this;
    s >> VARINT(cumulativeReward_);
    s >> VARINT(minerChainHeight_);

    if (HasTransaction()) {
        transaction_->SetStatus((Transaction::Validity) ser_readdata8(s));
    }

    enum MilestoneStatus msFlag = (MilestoneStatus) ser_readdata8(s);
    isMilestone_                = msFlag == IS_TRUE_MILESTONE;

    if (msFlag > 0) {
        // TODO: store the milestone object in some kind of cache
        milestone_ = std::make_shared<Milestone>();
        // s >> *milestone_;
        DeserializeMilestone(s, *milestone_);
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
        s += std::to_string(*(block.transaction_));
    }

    return s;
}

void Block::SerializeMilestone(VStream& s, Milestone& milestone) {
    ::Serialize(s, VARINT(milestone.height_));
    ::Serialize(s, milestone.chainwork_.GetCompact());
    ::Serialize(s, milestone.lastUpdateTime_);
    ::Serialize(s, milestone.milestoneTarget_.GetCompact());
    ::Serialize(s, milestone.blockTarget_.GetCompact());
    ::Serialize(s, VARINT(milestone.hashRate_));
}

void Block::DeserializeMilestone(VStream& s, Milestone& milestone) {
    ::Deserialize(s, VARINT(milestone.height_));
    milestone.chainwork_.SetCompact(ser_readdata32(s));
    milestone.lastUpdateTime_ = ser_readdata64(s);
    milestone.milestoneTarget_.SetCompact(ser_readdata32(s));
    milestone.blockTarget_.SetCompact(ser_readdata32(s));
    ::Deserialize(s, VARINT(milestone.hashRate_));
}

Block Block::CreateGenesis() {
    Block genesis(GENESIS_BLOCK_VERSION);
    Transaction tx;

    // Construct a script containing the difficulty bits
    // and the following message:
    std::string hexStr("04ffff001d0104454974206973206e6f772074656e2070617374207"
                       "4656e20696e20746865206576656e696e6720616e64207765206172"
                       "65207374696c6c20776f726b696e6721");

    // Convert the string to bytes
    auto vs = VStream(ParseHex(hexStr));

    // Add input and output
    tx.AddInput(TxInput(Script(vs)));
    std::optional<CKeyID> pubKeyID = DecodeAddress("JXA9kkybKZL848rxcvczu2pFGcodcHXrC");
    tx.AddOutput(TxOutput(66, Script(VStream(pubKeyID.value())))).FinalizeHash();

    genesis.AddTransaction(tx);
    genesis.SetMinerChainHeight(0);
    genesis.ResetReward();
    genesis.SetDifficultyTarget(EASY_DIFFICULTY_TARGET.GetCompact());
    genesis.SetTime(1548078136L);
    genesis.SetNonce(11882);
    assert(genesis.GetHash() == uint256S("3f9ed447274fd9050aef23f1efbb6845609c1858f0d17f04655a09503eb70115"));

    return genesis;
}
