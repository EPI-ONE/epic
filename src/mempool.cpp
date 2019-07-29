#include "mempool.h"

bool MemPool::Insert(ConstTxPtr value) {
    /* TODO: in future version we have to
     * check UTXO for incoming transaction */
    WRITER_LOCK(mutex_)
    auto result = mempool_.insert(std::move(value));
    return std::get<1>(result);
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

/* for phase one linear search seems to be fine but later on
 * we have to improve this method; TODO optimize */
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
    if (!redemptionTxQueue.Empty()) {
        ConstTxPtr ptr;
        redemptionTxQueue.Take(ptr);
        return ptr;
    }
    auto txptr = GetTransaction(blkHash, threashold);
    Erase(txptr);
    return txptr;
}

void MemPool::PushRedemptionTx(ConstTxPtr redemption) {
    READER_LOCK(mutex_)
    redemptionTxQueue.Put(redemption);
}

ConstTxPtr MemPool::GetRedemptionTx(bool IsRegistration) {
    READER_LOCK(mutex_)
    if (!redemptionTxQueue.Empty()) {
        ConstTxPtr ptr;
        redemptionTxQueue.Take(ptr);
        if (IsRegistration && !ptr->IsFirstRegistration()) {
            return nullptr;
        }
        return ptr;
    }
    return nullptr;
}
