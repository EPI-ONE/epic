#include "block.h"

#include <unordered_set>

Block::Block() : NetMessage(BLOCK) {
    SetNull();
}

Block::Block(const Block& b)
    : NetMessage(BLOCK), hash_(b.hash_), version_(b.version_), milestoneBlockHash_(b.milestoneBlockHash_),
      prevBlockHash_(b.prevBlockHash_), tipBlockHash_(b.tipBlockHash_), merkleRoot_(b.merkleRoot_), time_(b.time_),
      diffTarget_(b.diffTarget_), nonce_(b.nonce_), transactions_(b.transactions_),
      optimalEncodingSize_(b.optimalEncodingSize_), source(b.source) {
    SetParents();
}

Block::Block(Block&& b) noexcept
    : NetMessage(BLOCK), hash_(b.hash_), version_(b.version_), milestoneBlockHash_(b.milestoneBlockHash_),
      prevBlockHash_(b.prevBlockHash_), tipBlockHash_(b.tipBlockHash_), merkleRoot_(b.merkleRoot_), time_(b.time_),
      diffTarget_(b.diffTarget_), nonce_(b.nonce_), transactions_(std::move(b.transactions_)),
      optimalEncodingSize_(b.optimalEncodingSize_), source(b.source) {
    b.SetNull();
    SetParents();
}

Block::Block(uint32_t versionNum) : Block() {
    version_            = versionNum;
    milestoneBlockHash_ = Hash::GetZeroHash();
    prevBlockHash_      = Hash::GetZeroHash();
    tipBlockHash_       = Hash::GetZeroHash();
    merkleRoot_         = uint256();
    time_               = time(nullptr);
}

Block::Block(VStream& payload) : NetMessage(BLOCK) {
    payload >> *this;
}

void Block::SetNull() {
    milestoneBlockHash_.SetNull();
    prevBlockHash_.SetNull();
    tipBlockHash_.SetNull();
    merkleRoot_.SetNull();
    version_    = 0;
    time_       = 0;
    diffTarget_ = 0;
    nonce_      = 0;
    transactions_.clear();
    source = Source::UNKNOWN;
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

uint256 Block::GetMerkleRoot() const {
    return merkleRoot_;
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
    // check version
    spdlog::trace("Block::Verify version {}", hash_.to_substr());
    if (version_ != GetParams().version) {
        spdlog::info("Block with wrong version {} v.s. expected {} [{}]", version_, GetParams().version,
                     std::to_string(hash_));
        return false;
    }

    // check pow
    spdlog::trace("Block::Verify pow {}", hash_.to_substr());
    if (!CheckPOW()) {
        return false;
    }

    // check if the timestamp is too far in the future
    spdlog::trace("Block::Verify allowed time {}", hash_.to_substr());
    time_t allowedTime = std::time(nullptr) + ALLOWED_TIME_DRIFT;
    if (time_ > allowedTime) {
        time_t t = time_;
        spdlog::info("Block too advanced in the future: {} ({}) v.s. allowed {} ({}) [{}]", std::string(ctime(&t)),
                     time_, std::string(ctime(&allowedTime)), allowedTime, std::to_string(hash_));
        return false;
    }

    // check for merkle root
    bool mutated;
    uint256 root = ComputeMerkleRoot(&mutated);
    if (merkleRoot_ != root) {
        spdlog::info("Block contains invalid merkle root. [{}]", std::to_string(hash_));
        return false;
    }
    if (mutated) {
        spdlog::info("Block contains duplicated transactions in a merkle tree branch. [{}]", std::to_string(hash_));
        return false;
    }

    // verify content of the block
    spdlog::trace("Block::Verify content {}", hash_.to_substr());
    if (GetOptimalEncodingSize() > MAX_BLOCK_SIZE) {
        spdlog::info("Block with size {} larger than MAX_BLOCK_SIZE [{}]", optimalEncodingSize_, std::to_string(hash_));
        return false;
    }

    if (HasTransaction()) {
        for (const auto& tx : transactions_) {
            if (!tx->Verify()) {
                return false;
            }
        }
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
        if (!transactions_[0]->IsFirstRegistration()) {
            spdlog::info("Block is the first registration but conatains invalid tx [{}]", std::to_string(hash_));
            return false;
        }
    }

    return true;
}

void Block::AddTransaction(const Transaction& tx) {
    assert(!tx.GetHash().IsNull());
    // Invalidate cached hash to force recomputation
    UnCache();
    auto tx_ptr = std::make_shared<Transaction>(tx);
    tx_ptr->SetParent(this);
    transactions_.emplace_back(std::move(tx_ptr));
    CalculateOptimalEncodingSize();
}

void Block::AddTransaction(ConstTxPtr tx) {
    if (!tx) {
        return;
    }
    assert(!tx->GetHash().IsNull());

    UnCache();
    tx->SetParent(this);
    tx->SetParents();
    transactions_.emplace_back(std::move(tx));
    CalculateOptimalEncodingSize();
}

bool Block::HasTransaction() const {
    return !transactions_.empty();
}

const std::vector<ConstTxPtr>& Block::GetTransactions() const {
    return transactions_;
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

uint256 Block::ComputeMerkleRoot(bool* mutated) const {
    return ::ComputeMerkleRoot(GetTxHashes(), mutated);
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
    if (HasTransaction() && merkleRoot_.IsNull()) {
        merkleRoot_ = ComputeMerkleRoot();
    }

    VStream s;
    s << version_ << milestoneBlockHash_ << prevBlockHash_ << tipBlockHash_ << merkleRoot_ << time_ << diffTarget_
      << nonce_;

    hash_ = HashSHA2<1>(s);
}

std::vector<uint256> Block::GetTxHashes() const {
    std::vector<uint256> leaves;
    leaves.reserve(transactions_.size());

    for (const auto& tx : transactions_) {
        leaves.emplace_back(tx->GetHash());
    }

    return leaves;
}

size_t Block::CalculateOptimalEncodingSize() {
    optimalEncodingSize_ = HEADER_SIZE + ::GetSizeOfCompactSize(transactions_.size());

    for (const auto& tx : transactions_) {
        optimalEncodingSize_ += ::GetSizeOfCompactSize(tx->GetInputs().size());
        for (const TxInput& input : tx->GetInputs()) {
            size_t listingDataSize    = input.listingContent.data.size();
            size_t listingProgramSize = input.listingContent.program.size();
            optimalEncodingSize_ +=
                (Hash::SIZE + 4 + 4                                                // outpoint
                 + ::GetSizeOfCompactSize(listingDataSize) + listingDataSize       // listing data
                 + ::GetSizeOfCompactSize(listingProgramSize) + listingProgramSize // listing program
                );
        }

        optimalEncodingSize_ += ::GetSizeOfCompactSize(tx->GetOutputs().size());
        for (const TxOutput& output : tx->GetOutputs()) {
            size_t listingDataSize    = output.listingContent.data.size();
            size_t listingProgramSize = output.listingContent.program.size();
            optimalEncodingSize_ +=
                (GetSizeOfVarInt(output.value.GetValue())                            // output value
                 + ::GetSizeOfCompactSize(listingDataSize) + listingDataSize         // listing data
                 + ::GetSizeOfCompactSize(listingProgramSize) + listingProgramSize); // listing program
        }
    }

    return optimalEncodingSize_;
}

size_t Block::GetOptimalEncodingSize() const {
    assert(optimalEncodingSize_ > 0);
    return optimalEncodingSize_;
}

bool Block::IsRegistration() const {
    return HasTransaction() ? transactions_[0]->IsRegistration() : false;
}

bool Block::IsFirstRegistration() const {
    return HasTransaction() ? transactions_[0]->IsFirstRegistration() && prevBlockHash_ == GENESIS.GetHash() : false;
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
        std::cout << nonce_ << " " << std::to_string(hash_) << std::endl;
    }
}

void Block::SetParents() {
    for (const auto& tx : transactions_) {
        tx->SetParent(this);
        tx->SetParents();
    }
}

uint256 ComputeMerkleRoot(std::vector<uint256> hashes, bool* mutated) {
    bool mutation = false;
    while (hashes.size() > 1) {
        if (mutated) {
            for (size_t pos = 0; pos + 1 < hashes.size(); pos += 2) {
                if (hashes[pos] == hashes[pos + 1])
                    mutation = true;
            }
        }

        if (hashes.size() & 1) {
            hashes.push_back(hashes.back());
        }

        SHA256D64(hashes[0].begin(), hashes[0].begin(), hashes.size() / 2);
        hashes.resize(hashes.size() / 2);
    }

    if (mutated) {
        *mutated = mutation;
    }

    if (hashes.empty()) {
        return uint256();
    }

    return hashes[0];
}

std::string std::to_string(const Block& block, bool showtx, std::vector<uint8_t> validity) {
    std::string s;
    s += " Block { \n";
    s += strprintf("      hash: %s \n", std::to_string(block.GetHash()));
    s += strprintf("      version: %s \n", block.version_);
    s += strprintf("      milestone block: %s \n", std::to_string(block.milestoneBlockHash_));
    s += strprintf("      previous block: %s \n", std::to_string(block.prevBlockHash_));
    s += strprintf("      tip block: %s \n", std::to_string(block.tipBlockHash_));
    s += strprintf("      merkle root: %s \n", std::to_string(block.merkleRoot_));
    s += strprintf("      time: %d \n", std::to_string(block.time_));
    s += strprintf("      difficulty target: %d \n", std::to_string(block.diffTarget_));
    s += strprintf("      nonce: %d \n ", std::to_string(block.nonce_));

    const std::array<std::string, 3> enumName{"UNKNOWN", "VALID", "INVALID"};
    if (block.HasTransaction() && showtx) {
        s += strprintf("  with transactions:\n");
        for (size_t i = 0; i < block.transactions_.size(); ++i) {
            s += strprintf("   [%s] %s %s\n", i, std::to_string(*block.transactions_[i]),
                           validity.empty() ? "" : ": " + enumName[validity[i]]);
        }
    }

    s += "  }";

    return s;
}
