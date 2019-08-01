#include "mempool.h"

#define READER_LOCK(mu) std::shared_lock<std::shared_mutex> reader(mu);
#define WRITER_LOCK(mu) std::unique_lock<std::shared_mutex> writer(mu);

bool MemPool::Insert(ConstTxPtr value) {
    WRITER_LOCK(mutex_)
    return std::get<1>(mempool_.emplace(std::move(value)));
}

bool MemPool::Contains(const ConstTxPtr& value) const {
    READER_LOCK(mutex_)
    return mempool_.find(value) != mempool_.end();
}

bool MemPool::Erase(const ConstTxPtr& value) {
    if (value) {
        WRITER_LOCK(mutex_)
        return mempool_.erase(value) > 0;
    }
    return false;
}

std::size_t MemPool::Size() const {
    READER_LOCK(mutex_)
    return mempool_.size();
}

bool MemPool::IsEmpty() const {
    return this->Size() == 0;
}

ConstTxPtr MemPool::GetTransaction(const uint256& blockHash, const arith_uint256& threshold) {
    arith_uint256 base_hash = UintToArith256(blockHash);
    READER_LOCK(mutex_)

    for (const auto& tx : mempool_) {
        auto distance = base_hash ^ UintToArith256(tx->GetHash());
        if (PartitionCmp(distance, threshold)) {
            return tx;
        }
    }

    return nullptr;
}

ConstTxPtr MemPool::ExtractTransaction(const uint256& blkHash, const arith_uint256& threashold) {
    auto txptr = GetTransaction(blkHash, threashold);
    Erase(txptr);
    return txptr;
}

void MemPool::PushRedemptionTx(ConstTxPtr redemption) {
    READER_LOCK(mutex_)
    redemptionTxQueue_.Put(redemption);
}

ConstTxPtr MemPool::GetRedemptionTx(bool IsFirstReg) {
    READER_LOCK(mutex_)
    if (!redemptionTxQueue_.Empty()) {
        ConstTxPtr ptr;
        redemptionTxQueue_.Take(ptr);
        if (IsFirstReg && !ptr->IsFirstRegistration()) {
            return nullptr;
        }
        return ptr;
    }
    return nullptr;
}
