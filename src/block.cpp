#include "block.h"

#include <unordered_set>

Block::Block() {
    SetNull();
}

Block::Block(const Block& b)
    : hash_(b.hash_), version_(b.version_), milestoneBlockHash_(b.milestoneBlockHash_),
      prevBlockHash_(b.prevBlockHash_), tipBlockHash_(b.tipBlockHash_), time_(b.time_), diffTarget_(b.diffTarget_),
      nonce_(b.nonce_), transaction_(b.transaction_), optimalEncodingSize_(b.optimalEncodingSize_) {
    SetParents();
}

Block::Block(Block&& b) noexcept
    : hash_(b.hash_), version_(b.version_), milestoneBlockHash_(b.milestoneBlockHash_),
      prevBlockHash_(b.prevBlockHash_), tipBlockHash_(b.tipBlockHash_), time_(b.time_), diffTarget_(b.diffTarget_),
      nonce_(b.nonce_), transaction_(std::move(b.transaction_)), optimalEncodingSize_(b.optimalEncodingSize_) {
    b.SetNull();
    SetParents();
}

Block::Block(uint32_t versionNum) : Block() {
    version_            = versionNum;
    milestoneBlockHash_ = Hash::GetZeroHash();
    prevBlockHash_      = Hash::GetZeroHash();
    tipBlockHash_       = Hash::GetZeroHash();
    time_               = time(nullptr);
}

Block::Block(VStream& payload) {
    payload >> *this;
}

void Block::SetNull() {
    milestoneBlockHash_.SetNull();
    prevBlockHash_.SetNull();
    tipBlockHash_.SetNull();
    version_    = 0;
    time_       = 0;
    diffTarget_ = 0;
    nonce_      = 0;
    transaction_.reset();
}

bool Block::IsNull() const {
    return time_ == 0;
}

uint256 Block::GetMilestoneHash() const {
    return milestoneBlockHash_;
}

uint256 Block::GetPrevHash() const {
    return prevBlockHash_;
}

uint256 Block::GetTipHash() const {
    return tipBlockHash_;
}

void Block::SetMilestoneHash(const uint256& hash) {
    milestoneBlockHash_ = hash;
}

void Block::SetPrevHash(const uint256& hash) {
    prevBlockHash_ = hash;
}

void Block::SetTipHash(const uint256& hash) {
    tipBlockHash_ = hash;
}

void Block::UnCache() {
    optimalEncodingSize_ = 0;
    hash_.SetNull();
}

bool Block::Verify() const {
    // checks version
    spdlog::trace("Block::Verify version {}", hash_.to_substr());
    if (version_ != GetParams().version) {
        spdlog::info("Block with wrong version {} v.s. expected {} [{}]", version_, GetParams().version,
                     std::to_string(hash_));
        return false;
    }

    // checks pow
    spdlog::trace("Block::Verify pow {}", hash_.to_substr());
    if (!CheckPOW()) {
        return false;
    }

    // checks if the time of block is too far in the future
    spdlog::trace("Block::Verify allowed time {}", hash_.to_substr());
    time_t allowedTime = std::time(nullptr) + ALLOWED_TIME_DRIFT;
    if (time_ > allowedTime) {
        time_t t = time_;
        spdlog::info("Block too advanced in the future: {} ({}) v.s. allowed {} ({}) [{}]", std::string(ctime(&t)),
                     time_, std::string(ctime(&allowedTime)), allowedTime, std::to_string(hash_));
        return false;
    }

    // verify content of the block
    spdlog::trace("Block::Verify content {}", hash_.to_substr());
    if (GetOptimalEncodingSize() > MAX_BLOCK_SIZE) {
        spdlog::info("Block with size {} larger than MAX_BLOCK_SIZE [{}]", optimalEncodingSize_, std::to_string(hash_));
        return false;
    }

    if (HasTransaction()) {
        if (transaction_->GetInputs().empty() || transaction_->GetOutputs().empty()) {
            spdlog::info("Block contains empty inputs or outputs [{}]", std::to_string(hash_));
            return false;
        }

        // Make sure no duplicated TxInput
        spdlog::trace("Block::Verify duplicated input {}", hash_.to_substr());
        std::unordered_set<TxOutPoint> outpoints = {};
        for (const TxInput& input : transaction_->GetInputs()) {
            if (outpoints.find(input.outpoint) != outpoints.end()) {
                spdlog::info("Block contains duplicated outpoints [{}]", std::to_string(hash_));
                return false;
            }
            outpoints.insert(input.outpoint);
        }
        outpoints.clear();
    }

    // check the conditions of the first registration block
    spdlog::trace("Block::Verify first reg {}", hash_.to_substr());
    if (prevBlockHash_ == GENESIS.GetHash()) {
        // Must contain a tx
        if (!HasTransaction()) {
            spdlog::info("Block is the first registration but does not contain a tx [{}]", std::to_string(hash_));
            return false;
        }

        // ... with input from ZERO hash and index -1 and output value 0
        if (!transaction_->IsFirstRegistration()) {
            spdlog::info("Block is the first registration but conatains invalid tx [{}]", std::to_string(hash_));
            return false;
        }
    }

    return true;
}

void Block::AddTransaction(const Transaction& tx) {
    // Invalidate cached hash to force recomputation
    UnCache();
    transaction_.reset();
    transaction_ = tx;
    transaction_->SetParent(this);
    CalculateOptimalEncodingSize();
}

void Block::AddTransaction(ConstTxPtr&& tx) {
    if (!tx) {
        return;
    }

    UnCache();
    transaction_.reset();
    transaction_ = *tx;
    transaction_->SetParent(this);
    transaction_->SetParents();
    CalculateOptimalEncodingSize();
}

bool Block::HasTransaction() const {
    return transaction_.has_value();
}

const std::optional<Transaction>& Block::GetTransaction() const {
    return transaction_;
}

void Block::SetDifficultyTarget(uint32_t target) {
    diffTarget_ = target;
}

uint32_t Block::GetDifficultyTarget() const {
    return diffTarget_;
}

void Block::SetTime(uint32_t time) {
    time_ = time;
}

uint32_t Block::GetTime() const {
    return time_;
}

void Block::SetNonce(uint32_t nonce) {
    hash_.SetNull();
    nonce_ = nonce;
}

uint32_t Block::GetNonce() const {
    return nonce_;
}

const uint256& Block::GetHash() const {
    return hash_;
}

void Block::FinalizeHash() {
    if (hash_.IsNull()) {
        CalculateHash();
    }
}

void Block::CalculateHash() {
    VStream s;
    s << version_ << milestoneBlockHash_ << prevBlockHash_ << tipBlockHash_ << time_ << diffTarget_ << nonce_
      << FinalizeTxHash();
    hash_ = Hash<1>(s);
}

const uint256& Block::FinalizeTxHash() {
    if (!HasTransaction()) {
        return Hash::GetZeroHash();
    }

    transaction_->FinalizeHash();
    return transaction_->GetHash();
}

const uint256& Block::GetTxHash() const {
    return transaction_->GetHash();
}

size_t Block::CalculateOptimalEncodingSize() {
    optimalEncodingSize_ = HEADER_SIZE + 1; // 1 is the flag for whether there is a transaction
    if (!HasTransaction()) {
        return optimalEncodingSize_;
    }

    optimalEncodingSize_ += ::GetSizeOfCompactSize(transaction_->GetInputs().size());
    for (const TxInput& input : transaction_->GetInputs()) {
        size_t listingDataSize    = input.listingContent.data.size();
        size_t listingProgramSize = input.listingContent.program.size();
        optimalEncodingSize_ += (Hash::SIZE + 4                                                    // outpoint
                                 + ::GetSizeOfCompactSize(listingDataSize) + listingDataSize       // listing data
                                 + ::GetSizeOfCompactSize(listingProgramSize) + listingProgramSize // listing program
        );
    }

    optimalEncodingSize_ += ::GetSizeOfCompactSize(transaction_->GetOutputs().size());
    for (const TxOutput& output : transaction_->GetOutputs()) {
        size_t listingDataSize    = output.listingContent.data.size();
        size_t listingProgramSize = output.listingContent.program.size();
        optimalEncodingSize_ += (GetSizeOfVarInt(output.value.GetValue())                            // output value
                                 + ::GetSizeOfCompactSize(listingDataSize) + listingDataSize         // listing data
                                 + ::GetSizeOfCompactSize(listingProgramSize) + listingProgramSize); // listing program
    }

    return optimalEncodingSize_;
}

size_t Block::GetOptimalEncodingSize() const {
    return optimalEncodingSize_;
}

bool Block::IsRegistration() const {
    return HasTransaction() ? transaction_->IsRegistration() : false;
}

bool Block::IsFirstRegistration() const {
    return HasTransaction() ? transaction_->IsFirstRegistration() && prevBlockHash_ == GENESIS.GetHash() : false;
}

arith_uint256 Block::GetChainWork() const {
    arith_uint256 target = GetTargetAsInteger();
    return GetParams().maxTarget / target;
}

arith_uint256 Block::GetTargetAsInteger() const {
    arith_uint256 target{};
    target.SetCompact(diffTarget_);

    if (target <= 0 || target > GetParams().maxTarget) {
        throw "Bad difficulty target: " + std::to_string(target);
    }

    return target;
}

bool Block::CheckPOW() const {
    if (hash_.IsNull()) {
        spdlog::info("No hash in this block!");
        return false;
    }

    arith_uint256 target;
    try {
        target = GetTargetAsInteger();
    } catch (const std::string& s) {
        spdlog::info(s);
        return false;
    }

    if (UintToArith256(hash_) > target) {
        spdlog::info("Hash {} is higher than target {}", std::to_string(GetHash()), std::to_string(target));
        return false;
    }

    return true;
}

void Block::Solve() {
    arith_uint256 target = GetTargetAsInteger();

    CalculateHash();
    while (UintToArith256(hash_) > target) {
        if (nonce_ == UINT_LEAST32_MAX) {
            time_ = time(nullptr);
        }
        nonce_++;
        CalculateHash();
    }
}

void Block::SetParents() {
    if (!HasTransaction()) {
        return;
    }
    transaction_->SetParent(this);
    transaction_->SetParents();
}

std::string std::to_string(const Block& block, bool showtx) {
    std::string s;
    s += " Block { \n";
    s += strprintf("      hash: %s \n", std::to_string(block.GetHash()));
    s += strprintf("      version: %s \n", block.version_);
    s += strprintf("      milestone block: %s \n", std::to_string(block.milestoneBlockHash_));
    s += strprintf("      previous block: %s \n", std::to_string(block.prevBlockHash_));
    s += strprintf("      tip block: %s \n", std::to_string(block.tipBlockHash_));
    s += strprintf("      time: %d \n", std::to_string(block.time_));
    s += strprintf("      difficulty target: %d \n", std::to_string(block.diffTarget_));
    s += strprintf("      nonce: %d \n ", std::to_string(block.nonce_));

    if (block.HasTransaction() && showtx) {
        s += strprintf("  with %s\n", std::to_string(*(block.transaction_)));
    }

    s += "  }";

    return s;
}
