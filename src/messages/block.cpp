// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "block.h"
#include "merkle.h"
#include "trimmer.h"

#include <unordered_set>

BlockHeader::BlockHeader(uint16_t version_,
                         uint256 milestoneBlockHash_,
                         uint256 prevBlockHash_,
                         uint256 tipBlockHash_,
                         uint256 merkle_,
                         uint32_t time_,
                         uint32_t diffTarget_,
                         uint32_t nonce_)
    : version(version_), milestoneBlockHash(milestoneBlockHash_), prevBlockHash(prevBlockHash_),
      tipBlockHash(tipBlockHash_), merkleRoot(merkle_), timestamp(time_), diffTarget(diffTarget_), nonce(nonce_) {}

BlockHeader::BlockHeader(const Block& b)
    : version(b.GetHeader().version), milestoneBlockHash(b.GetHeader().milestoneBlockHash),
      prevBlockHash(b.GetHeader().prevBlockHash), tipBlockHash(b.GetHeader().tipBlockHash),
      merkleRoot(b.GetHeader().merkleRoot), timestamp(b.GetHeader().timestamp), diffTarget(b.GetHeader().diffTarget),
      nonce(b.GetHeader().nonce) {}

BlockHeader::BlockHeader(VStream& payload) {
    payload >> *this;
}

void BlockHeader::SetNull() {
    milestoneBlockHash.SetNull();
    prevBlockHash.SetNull();
    tipBlockHash.SetNull();
    merkleRoot.SetNull();
    version    = 0;
    timestamp  = 0;
    diffTarget = 0;
    nonce      = 0;
}

std::string BlockHeader::to_string() const {
    std::string s;
    s += strprintf("      version: %s \n", version);
    s += strprintf("      milestone block: %s \n", std::to_string(milestoneBlockHash));
    s += strprintf("      previous block: %s \n", std::to_string(prevBlockHash));
    s += strprintf("      tip block: %s \n", std::to_string(tipBlockHash));
    s += strprintf("      merkle root: %s \n", std::to_string(merkleRoot));
    s += strprintf("      time: %d \n", std::to_string(timestamp));
    s += strprintf("      difficulty target: %d \n", std::to_string(diffTarget));
    s += strprintf("      nonce: %d \n", std::to_string(nonce));
    return s;
}

Block::Block() : NetMessage(BLOCK) {
    SetNull();
}

Block::Block(const Block& b)
    : NetMessage(BLOCK), hash_(b.hash_), header_(b.header_), proof_(b.proof_), transactions_(b.transactions_),
      optimalEncodingSize_(b.optimalEncodingSize_), source(b.source) {
    SetParents();
};

Block::Block(Block&& b) noexcept
    : NetMessage(BLOCK), hash_(b.hash_), header_(b.header_), proof_(std::move(b.proof_)),
      transactions_(std::move(b.transactions_)), optimalEncodingSize_(b.optimalEncodingSize_), source(b.source) {
    b.SetNull();
    SetParents();
}

Block::Block(uint16_t versionNum) : Block() {
    header_.version            = versionNum;
    header_.milestoneBlockHash = Hash::GetZeroHash();
    header_.prevBlockHash      = Hash::GetZeroHash();
    header_.tipBlockHash       = Hash::GetZeroHash();
    header_.merkleRoot         = uint256();
    header_.timestamp          = time(nullptr);
}

Block::Block(VStream& payload) : NetMessage(BLOCK) {
    payload >> *this;
}

void Block::SetNull() {
    header_.SetNull();
    proof_.clear();
    transactions_.clear();
    source = Source::UNKNOWN;
}

bool Block::IsNull() const {
    return header_.timestamp == 0;
}

BlockHeader Block::GetHeader() const {
    return header_;
}

uint16_t Block::GetVersion() const {
    return header_.version;
}

uint256 Block::GetMilestoneHash() const {
    return header_.milestoneBlockHash;
}

uint256 Block::GetPrevHash() const {
    return header_.prevBlockHash;
}

uint256 Block::GetTipHash() const {
    return header_.tipBlockHash;
}

uint256 Block::GetMerkleRoot() const {
    return header_.merkleRoot;
}

void Block::SetMilestoneHash(const uint256& h) {
    header_.milestoneBlockHash = h;
}

void Block::SetPrevHash(const uint256& h) {
    header_.prevBlockHash = h;
}

void Block::SetTipHash(const uint256& h) {
    header_.tipBlockHash = h;
}

void Block::SetMerkle(const uint256& h) {
    if (h.IsNull()) {
        header_.merkleRoot = ComputeMerkleRoot();
    } else {
        header_.merkleRoot = h;
    }
}

void Block::UnCache() {
    optimalEncodingSize_ = 0;
    hash_.SetNull();
    header_.merkleRoot.SetNull();
}

bool Block::Verify() const {
    // check version
    spdlog::trace("Block::Verify version {}", hash_.to_substr());
    if (header_.version != GetParams().version) {
        spdlog::info("Block with wrong version {} v.s. expected {} [{}]", header_.version, GetParams().version,
                     std::to_string(hash_));
        return false;
    }

    // check pow
    spdlog::trace("Block::Verify pow {}", hash_.to_substr());
    if (!CheckPOW()) {
        return false;
    }

    // check merkle
    spdlog::trace("Block::Verify merkle {}", hash_.to_substr());
    bool mutated;
    auto root = ComputeMerkleRoot(&mutated);
    if (mutated) {
        spdlog::info("Block contains duplicated transactions in a merkle tree branch. [{}]", std::to_string(hash_));
        return false;
    }
    if (root != header_.merkleRoot) {
        spdlog::info("Block contains invalid merkle root. [{}]", std::to_string(hash_));
        return false;
    }

    // check if the timestamp is too far in the future
    spdlog::trace("Block::Verify allowed time {}", hash_.to_substr());
    time_t allowedTime = std::time(nullptr) + ALLOWED_TIME_DRIFT;
    if (header_.timestamp > allowedTime) {
        time_t t = header_.timestamp;
        spdlog::info("Block too advanced in the future: {} ({}) v.s. allowed {} ({}) [{}]", std::string(ctime(&t)),
                     header_.timestamp, std::string(ctime(&allowedTime)), allowedTime, std::to_string(hash_));
        return false;
    }

    // verify content of the block
    spdlog::trace("Block::Verify number of txns {}", hash_.to_substr());
    if (transactions_.size() > GetParams().blockCapacity) {
        spdlog::info("Block with {} transactions larger than its capacity ({}]) [{}]", transactions_.size(),
                     GetParams().blockCapacity, std::to_string(hash_));
    }

    spdlog::trace("Block::Verify content {}", hash_.to_substr());
    if (GetOptimalEncodingSize() > MAX_BLOCK_SIZE) {
        spdlog::info("Block with size {} larger than MAX_BLOCK_SIZE [{}]", optimalEncodingSize_, std::to_string(hash_));
        return false;
    }

    if (HasTransaction()) {
        std::unordered_set<uint256> txhashes;
        for (const auto& tx : transactions_) {
            if (!tx->Verify()) {
                return false;
            }
            txhashes.insert(tx->GetHash());
        }

        if (txhashes.size() != transactions_.size()) {
            spdlog::info("Block contains duplicated transactions. [{}]", std::to_string(hash_));
            return false;
        }
    }

    // check the conditions of the first registration block
    spdlog::trace("Block::Verify first reg {}", hash_.to_substr());
    if (header_.prevBlockHash == GENESIS->GetHash()) {
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
}

void Block::AddTransaction(ConstTxPtr tx) {
    if (!tx) {
        return;
    }
    assert(!tx->GetHash().IsNull());

    UnCache();
    tx->SetParent(this);
    transactions_.emplace_back(std::move(tx));
}

void Block::AddTransactions(std::vector<ConstTxPtr>&& txns) {
    UnCache();
    for (const auto& tx : txns) {
        assert(!tx->GetHash().IsNull());
        tx->SetParent(this);
    }
    transactions_.insert(transactions_.end(), std::make_move_iterator(txns.begin()),
                         std::make_move_iterator(txns.end()));
}

bool Block::HasTransaction() const {
    return !transactions_.empty();
}

const std::vector<ConstTxPtr>& Block::GetTransactions() const {
    return transactions_;
};

std::vector<ConstTxPtr> Block::GetTransactions() {
    return transactions_;
}

size_t Block::GetTransactionSize() const {
    return transactions_.size();
}

void Block::SetDifficultyTarget(uint32_t target) {
    header_.diffTarget = target;
}

uint32_t Block::GetDifficultyTarget() const {
    return header_.diffTarget;
}

void Block::SetTime(uint32_t t) {
    header_.timestamp = t;
}

uint32_t Block::GetTime() const {
    return header_.timestamp;
}

void Block::SetNonce(uint32_t nonce) {
    hash_.SetNull();
    header_.merkleRoot.SetNull();
    header_.nonce = nonce;
}

uint32_t Block::GetNonce() const {
    return header_.nonce;
}

const std::vector<uint32_t>& Block::GetProof() const {
    return proof_;
}

void Block::SetProof(std::vector<word_t>&& p) {
    UnCache();
    proof_ = p;
}

void Block::InitProofSize(size_t s) {
    hash_.SetNull();
    proof_.clear();
    proof_.resize(s);
}

uint256 Block::ComputeMerkleRoot(bool* mutated) const {
    return ::ComputeMerkleRoot(GetTxHashes(), mutated);
}

const uint256& Block::GetHash() const {
    return hash_;
}

const uint256& Block::GetProofHash() const {
    return proofHash_;
}

void Block::FinalizeHash() {
    if (hash_.IsNull()) {
        CalculateHash();
    }
}

void Block::CalculateHash() {
    if (HasTransaction() && header_.merkleRoot.IsNull()) {
        header_.merkleRoot = ComputeMerkleRoot();
    }

    VStream s;
    s << header_ << proof_;

    hash_      = HashSHA2<1>(s);
    proofHash_ = HashBLAKE2<256>(proof_.data(), proof_.size() * sizeof(word_t));
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
    optimalEncodingSize_ =
        HEADER_SIZE + (sizeof(word_t) * proof_.size()) + ::GetSizeOfCompactSize(transactions_.size());

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
    return HasTransaction() ? transactions_[0]->IsFirstRegistration() && header_.prevBlockHash == GENESIS->GetHash() :
                              false;
}

arith_uint256 Block::GetChainWork() const {
    arith_uint256 target = GetTargetAsInteger();
    return GetParams().maxTarget / target;
}

arith_uint256 Block::GetTargetAsInteger() const {
    arith_uint256 target{};
    target.SetCompact(header_.diffTarget);
    return target;
}

bool Block::CheckPOW() const {
    assert(!hash_.IsNull());

    if (proof_.size() != CYCLELEN) {
        spdlog::info("Bad proof size: {} [{}]", proof_.size(), std::to_string(hash_));
        return false;
    }

    // Initilize siphash keys by block header
    VStream vs(header_);
    if (CYCLELEN) {
        siphash_keys sipkeys;
        SetHeader(vs.data(), vs.size(), &sipkeys);

        // Verify cuckaroo pow
        auto status = VerifyProof(proof_.data(), sipkeys);
        if (status != POW_OK) {
            spdlog::info("Invalid proof of edges: {}", ErrStr[status]);
            return false;
        }
    }

    // Verify target validity
    arith_uint256 target = GetTargetAsInteger();
    if (target <= 0 || target > GetParams().maxTarget) {
        spdlog::info("Bad difficulty target: " + std::to_string(target));
        return false;
    }

    // Verify proof target
    if (UintToArith256(CYCLELEN ? proofHash_ : (uint256) HashBLAKE2<256>(vs)) > target) {
        spdlog::info("Proof hash {} is higher than target {} [{}]", std::to_string(proofHash_), std::to_string(target),
                     std::to_string(hash_));
        return false;
    }

    return true;
}

void Block::SetParents() {
    for (const auto& tx : transactions_) {
        tx->SetParent(this);
        tx->SetParents();
    }
}

std::string std::to_string(const Block& block, bool showtx, std::vector<uint8_t> validity) {
    std::string s;
    s += " Block { \n";
    s += strprintf("      hash: %s \n", std::to_string(block.GetHash()));
    s += block.header_.to_string();
    s += strprintf("      proof: [ ");
    for (int i = 0; i < block.proof_.size(); ++i) {
        s += strprintf("%d%s ", block.proof_[i], i == block.proof_.size() - 1 ? "" : ",");
    }
    s += strprintf("] \n");

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
