// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "block.h"
#include "merkle.h"
#include "trimmer.h"

#include <unordered_set>

Block::Header::Header(uint16_t version_,
                      uint256 milestoneBlockHash_,
                      uint256 prevBlockHash_,
                      uint256 tipBlockHash_,
                      uint256 merkle_,
                      uint32_t time_,
                      uint32_t diffTarget_,
                      uint32_t nonce_)
    : version(version_), milestoneBlockHash(milestoneBlockHash_), prevBlockHash(prevBlockHash_),
      tipBlockHash(tipBlockHash_), merkleRoot(merkle_), timestamp(time_), diffTarget(diffTarget_), nonce(nonce_) {}

Block::Header::Header(const Block& b)
    : version(b.header_.version), milestoneBlockHash(b.header_.milestoneBlockHash),
      prevBlockHash(b.header_.prevBlockHash), tipBlockHash(b.header_.tipBlockHash), merkleRoot(b.header_.merkleRoot),
      timestamp(b.header_.timestamp), diffTarget(b.header_.diffTarget), nonce(b.header_.nonce) {}


void Block::Header::SetNull() {
    milestoneBlockHash.SetNull();
    prevBlockHash.SetNull();
    tipBlockHash.SetNull();
    merkleRoot.SetNull();
    version    = 0;
    timestamp  = 0;
    diffTarget = 0;
    nonce      = 0;
}

Block::Block() : NetMessage(BLOCK) {
    SetNull();
}

Block::Block(const Block& b)
    : NetMessage(BLOCK), hash_(b.hash_), header_(b.header_), transactions_(b.transactions_),
      optimalEncodingSize_(b.optimalEncodingSize_), source(b.source) {
    SetProof(b.proof_);
    SetParents();
};

Block::Block(Block&& b) noexcept
    : NetMessage(BLOCK), hash_(b.hash_), header_(b.header_), transactions_(std::move(b.transactions_)),
      optimalEncodingSize_(b.optimalEncodingSize_), source(b.source) {
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
    ResetProof();
    transactions_.clear();
    source = Source::UNKNOWN;
}

bool Block::IsNull() const {
    return header_.timestamp == 0;
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

    if (!HasTransaction()) {
        assert(header_.merkleRoot.IsNull());
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
    if (header_.prevBlockHash == GENESIS.GetHash()) {
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
    transactions_.emplace_back(std::move(tx));
    CalculateOptimalEncodingSize();
}

void Block::AddTransactions(std::vector<ConstTxPtr>&& txns) {
    UnCache();
    for (const auto& tx : txns) {
        assert(!tx->GetHash().IsNull());
        tx->SetParent(this);
    }
    transactions_.insert(transactions_.end(), std::make_move_iterator(txns.begin()),
                         std::make_move_iterator(txns.end()));
    CalculateOptimalEncodingSize();
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

void Block::SetProof(const Proof& p) {
    UnCache();
    memcpy(proof_, p, sizeof(Proof));
}

void Block::SetProof(word_t* begin) {
    UnCache();
    memcpy(proof_, begin, sizeof(Proof));
}

const word_t* Block::GetProof() const {
    return &proof_[0];
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
    if (HasTransaction() && header_.merkleRoot.IsNull()) {
        header_.merkleRoot = ComputeMerkleRoot();
    }

    VStream s;
    s << header_ << proof_;

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
    return HasTransaction() ? transactions_[0]->IsFirstRegistration() && header_.prevBlockHash == GENESIS.GetHash() :
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
    if (hash_.IsNull()) {
        spdlog::info("No hash in this block!");
        return false;
    }

    VStream vs(header_);
    siphash_keys sipkeys;
    SetHeader(vs.data(), vs.size(), &sipkeys);

    auto status = VerifyProof(proof_, sipkeys);
    if (status != POW_OK) {
        spdlog::info("Invalid proof of edges: {}", ErrStr[status]);
        return false;
    }

    arith_uint256 target = GetTargetAsInteger();
    if (target <= 0 || target > GetParams().maxTarget) {
        spdlog::info("Bad difficulty target: " + std::to_string(target));
        return false;
    }

    auto proofHash = HashBLAKE2<256>(proof_, sizeof(Proof));
    if (UintToArith256(proofHash) > target) {
        spdlog::info("Proof hash {} is higher than target {} [{}]", std::to_string(proofHash), std::to_string(target),
                     std::to_string(hash_));
        return false;
    }

    return true;
}

void Block::Solve() {
    arith_uint256 target = GetTargetAsInteger();

    CalculateHash();
    while (UintToArith256(hash_) > target) {
        if (header_.nonce == UINT_LEAST32_MAX) {
            header_.timestamp = time(nullptr);
        }
        header_.nonce++;
        CalculateHash();
    }
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
    s += strprintf("      version: %s \n", block.header_.version);
    s += strprintf("      milestone block: %s \n", std::to_string(block.header_.milestoneBlockHash));
    s += strprintf("      previous block: %s \n", std::to_string(block.header_.prevBlockHash));
    s += strprintf("      tip block: %s \n", std::to_string(block.header_.tipBlockHash));
    s += strprintf("      merkle root: %s \n", std::to_string(block.header_.merkleRoot));
    s += strprintf("      time: %d \n", std::to_string(block.header_.timestamp));
    s += strprintf("      difficulty target: %d \n", std::to_string(block.header_.diffTarget));
    s += strprintf("      nonce: %d \n", std::to_string(block.header_.nonce));
    s += strprintf("      proof: [ ");
    for (int i = 0; i < PROOFSIZE; ++i) {
        s += strprintf("%d ", block.proof_[i]);
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
