#include "mempool.h"

void MemPool::Insert(ConstTxPtr value) {
    mempool_.insert(value);
}

bool MemPool::Contains(ConstTxPtr value) const {
    return mempool_.find(value) != mempool_.end();
}

bool MemPool::Erase(ConstTxPtr value) {
    return mempool_.erase(value) > 0;
}
