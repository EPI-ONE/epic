#include "mempool.h"

bool MemPool::Insert(ConstTxPtr value) {
    /* TODO: in future version we have to
     * check UTXO for incoming transaction */
    return std::get<1>(mempool_.emplace(std::move(value)));
}

bool MemPool::Contains(const ConstTxPtr& value) const {
    return mempool_.find(value) != mempool_.end();
}

bool MemPool::Erase(const ConstTxPtr& value) {
    if (value) {
        return mempool_.erase(value) > 0;
    }
    return false;
}

std::size_t MemPool::Size() const {
    return mempool_.size();
}

bool MemPool::IsEmpty() const {
    return this->Size() == 0;
}

/* for phase one linear search seems to be fine but later on
 * we have to improve this method; TODO optimize */
ConstTxPtr MemPool::GetTransaction(const uint256& blockHash, const arith_uint256& threshold) {
    arith_uint256 base_hash = UintToArith256(blockHash);

    for (const auto& tx : mempool_) {
        auto distance = base_hash ^ UintToArith256(tx->GetHash());
        if (distance <= threshold) {
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
