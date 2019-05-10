#include "block.h"

Block::Block() {
    SetNull();
}

Block::Block(uint32_t versionNum) {
    version_            = versionNum;
    milestoneBlockHash_ = Hash::GetZeroHash();
    prevBlockHash_      = Hash::GetZeroHash();
    tipBlockHash_       = Hash::GetZeroHash();
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

void Block::SetTIPHash(const uint256& hash) {
    tipBlockHash_ = hash;
}

void Block::UnCache() {
    optimalEncodingSize_ = 0;
    hash_.SetNull();
}

bool Block::Verify() {
    // checks pow
    if (!CheckPOW()) {
        return false;
    }

    // checks if the time of block is too far in the future
    uint64_t allowedTime = std::time(nullptr) + ALLOWED_TIME_DRIFT;
    if (time_ > allowedTime) {
        spdlog::info(strprintf("Block %s too advanced in the future: %s (%s) v.s. allowed %s (%s)",
            std::to_string(hash_), ctime((time_t*) &time_), time_, ctime((time_t*) &allowedTime), allowedTime));
        return false;
    }

    // verify content of the block
    if (GetOptimalEncodingSize() > MAX_BLOCK_SIZE) {
        spdlog::info(
            strprintf("Block %s with size %s larger than MAX_BLOCK_SIZE", std::to_string(hash_), optimalEncodingSize_));
        return false;
    }

    if (HasTransaction()) {
        if (transaction_->GetInputs().empty() || transaction_->GetOutputs().empty()) {
            spdlog::info(strprintf("Block %s contains empty inputs or outputs", std::to_string(hash_)));
            return false;
        }

        // Make sure no duplicated TxInput
        std::unordered_set<TxOutPoint> outpoints = {};
        for (const TxInput& input : transaction_->GetInputs()) {
            if (outpoints.find(input.outpoint) != outpoints.end()) {
                spdlog::info(strprintf("Block %s contains duplicated outpoints", std::to_string(hash_)));
                return false;
            }
            outpoints.insert(input.outpoint);
        }
        outpoints.clear();
    }

    // check the conditions of the first registration block
    if (prevBlockHash_ == GENESIS.GetHash()) {
        // Must contain a tx
        if (!HasTransaction()) {
            spdlog::info(
                strprintf("Block %s is the first registration but does not contain a tx", std::to_string(hash_)));
            return false;
        }

        // ... with input from ZERO hash and index -1 and output value 0
        if (!transaction_->IsFirstRegistration()) {
            spdlog::info(
                strprintf("Block %s is the first registration but conatains invalid tx", std::to_string(hash_)));
            return false;
        }
    }

    return true;
}

void Block::AddTransaction(Transaction tx) {
    // Invalidate cached hash to force recomputation
    UnCache();
    transaction_.reset();
    transaction_ = tx;
    transaction_->SetParent(this);
    CalculateOptimalEncodingSize();
}

bool Block::HasTransaction() const {
    return transaction_.has_value();
}

std::optional<Transaction>& Block::GetTransaction() {
    return transaction_;
}

void Block::SetDifficultyTarget(uint32_t target) {
    diffTarget_ = target;
}

void Block::SetTime(uint64_t time) {
    time_ = time;
}

const uint64_t Block::GetTime() const {
    return time_;
}

void Block::SetNonce(uint32_t nonce) {
    hash_.SetNull();
    nonce_ = nonce;
}

const uint32_t Block::GetNonce() const {
    return nonce_;
}

const uint256& Block::GetHash() const {
    return hash_;
}

void Block::FinalizeHash() {
    if (hash_.IsNull())
        CalculateHash();
}

void Block::CalculateHash() {
    VStream s;
    Block::Serialize(s);
    GetTxHash().Serialize(s);
    hash_ = Hash<1>(s);
}

const uint256& Block::GetTxHash() {
    if (!HasTransaction()) {
        return Hash::GetZeroHash();
    }
    transaction_->FinalizeHash();
    return transaction_->GetHash();
}

size_t Block::CalculateOptimalEncodingSize() {
    if (optimalEncodingSize_ > 0) {
        return optimalEncodingSize_;
    }

    optimalEncodingSize_ = HEADER_SIZE;

    return optimalEncodingSize_;
}

size_t Block::GetOptimalEncodingSize() const {
    return optimalEncodingSize_;
}

bool Block::IsRegistration() const {
    return HasTransaction() ? transaction_->IsRegistration() : false;
}

bool Block::IsFirstRegistration() const {
    return HasTransaction() ? transaction_->IsFirstRegistration() : false;
}

arith_uint256 Block::GetChainWork() const {
    arith_uint256 target = GetTargetAsInteger();
    return params.maxTarget / (target + 1);
}

arith_uint256 Block::GetTargetAsInteger() const {
    arith_uint256 target = arith_uint256().SetCompact(diffTarget_);
    if (target <= 0 || target > params.maxTarget) {
        throw "Bad difficulty target: " + std::to_string(target);
    }

    return target;
}

bool Block::CheckPOW() {
    arith_uint256 target;
    try {
        target = GetTargetAsInteger();
    } catch (const std::string& s) {
        spdlog::info(s);
        return false;
    }
    FinalizeHash();
    arith_uint256 blkHash = UintToArith256(hash_);

    if (blkHash > target) {
        spdlog::info("Hash is higher than target: " + std::to_string(GetHash()) + " vs " + std::to_string(target));
        return false;
    }

    return true;
}

void Block::RandomizeHash() {
    hash_.randomize();
}

void Block::Solve() {
    arith_uint256 target = GetTargetAsInteger();

    FinalizeHash();
    for (;;) {
        if (UintToArith256(hash_) <= target) {
            return;
        }

        if (nonce_ == UINT_LEAST32_MAX)
            time_ = time(nullptr);

        nonce_++;
        CalculateHash();
    }
}

void Block::Solve(ThreadPool& solverPool) {
    arith_uint256 target = GetTargetAsInteger();
    std::size_t nthreads = solverPool.size();
    uint64_t final_time  = time_;

    uint32_t default_nonce            = 0;
    std::atomic<uint32_t> final_nonce = default_nonce;

    for (std::size_t i = 1; i <= nthreads; ++i) {
        solverPool.Execute([this, &final_nonce, &final_time, &default_nonce, i, target, nthreads]() {
            Block blk(*this);
            blk.nonce_ = i;
            blk.FinalizeHash();

            while (final_nonce.load() == 0) {
                if (UintToArith256(blk.hash_) <= target) {
                    if (final_nonce.compare_exchange_strong(default_nonce, blk.nonce_)) {
                        final_time = blk.time_;
                    }

                    break;
                }

                if (blk.nonce_ >= UINT_LEAST32_MAX - nthreads) {
                    blk.time_  = time(nullptr);
                    blk.nonce_ = i;
                } else {
                    blk.nonce_ += nthreads;
                }

                blk.CalculateHash();
            }
        });
    }

    // Block the main thread until a nonce is solved
    while (final_nonce.load() == 0) {
        std::this_thread::yield();
    }

    SetNonce(final_nonce.load());
    SetTime(final_time);
    FinalizeHash();
}

void Block::SetParents() {
    transaction_->SetParent(this);
    for (TxInput& input : transaction_->GetInputs()) {
        input.SetParent(&*transaction_);
    }

    for (TxOutput& output : transaction_->GetOutputs()) {
        output.SetParent(&*transaction_);
    }
}

std::string std::to_string(Block& block) {
    std::string s;
    s += " Block { \n";
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

Block Block::CreateGenesis() {
    Block genesisBlock(GENESIS_BLOCK_VERSION);
    Transaction tx;

    // Construct a script containing the difficulty bits
    // and the following message:
    std::string hexStr("04ffff001d0104454974206973206e6f772074656e2070617374207"
                       "4656e20696e20746865206576656e696e6720616e64207765206172"
                       "65207374696c6c20776f726b696e6721");

    // Convert the string to bytes
    auto vs = VStream(ParseHex(hexStr));

    // Add input and output
    tx.AddInput(TxInput(Tasm::Listing(vs)));

    std::optional<CKeyID> pubKeyID = DecodeAddress("14u6LvvWpReA4H2GwMMtm663P2KJGEkt77");
    tx.AddOutput(TxOutput(66, Tasm::Listing(VStream(pubKeyID.value())))).FinalizeHash();

    genesisBlock.AddTransaction(tx);
    genesisBlock.SetDifficultyTarget(0x1d00ffffL);
    genesisBlock.SetTime(1548078136L);
    genesisBlock.SetNonce(2081807681);
    genesisBlock.FinalizeHash();
    genesisBlock.CalculateOptimalEncodingSize();

    // The following commented lines were used for mining a genesis block
    // int numThreads = 44;
    // ThreadPool solverPool(numThreads);
    // solverPool.Start();
    // genesisBlock.Solve(numThreads, solverPool);
    // solverPool.Stop();
    // std::cout << std::to_string(genesisBlock) << std::endl;

    return genesisBlock;
}

BlockNet::BlockNet(const Block& b) : Block(b) {}

size_t BlockNet::CalculateOptimalEncodingSize() {
    if (optimalEncodingSize_ > 0) {
        return optimalEncodingSize_;
    }

    optimalEncodingSize_ = Block::CalculateOptimalEncodingSize() + 1;
    if (!HasTransaction())
        return optimalEncodingSize_;

    optimalEncodingSize_ += ::GetSizeOfCompactSize(transaction_->GetInputs().size());
    for (const TxInput& input : transaction_->GetInputs()) {
        size_t listingDataSize    = input.listingContent.data.size();
        size_t listingProgramSize = input.listingContent.program.size();
        optimalEncodingSize_ += (32 + 4 + ::GetSizeOfCompactSize(listingDataSize) + listingDataSize +
                                 ::GetSizeOfCompactSize(listingProgramSize) + listingProgramSize);
    }

    optimalEncodingSize_ += ::GetSizeOfCompactSize(transaction_->GetOutputs().size());
    for (const TxOutput& output : transaction_->GetOutputs()) {
        size_t listingDataSize    = output.listingContent.data.size();
        size_t listingProgramSize = output.listingContent.program.size();
        optimalEncodingSize_ += (8 + ::GetSizeOfCompactSize(listingDataSize) + listingDataSize +
                                 ::GetSizeOfCompactSize(listingProgramSize) + listingProgramSize);
    }

    return optimalEncodingSize_;
}

const Block GENESIS = Block::CreateGenesis();
