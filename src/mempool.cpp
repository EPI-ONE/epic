#include "mempool.h"

bool MemPool::Insert(ConstTxPtr value) {
    return std::get<1>(mempool_.insert(value));
}

bool MemPool::Contains(ConstTxPtr value) const {
    return mempool_.find(value) != mempool_.end();
}

bool MemPool::Erase(ConstTxPtr value) {
    return mempool_.erase(value) > 0;
}
