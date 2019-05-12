#ifndef __SRC_MEMPOOL_H__
#define __SRC_MEMPOOL_H__

#include <optional>
#include <unordered_set>

#include "arith_uint256.h"
#include "locked.h"
#include "transaction.h"

class MemPool {
public:
    MemPool() = default;

    void Insert(ConstTxPtr value);

    bool Contains(ConstTxPtr value) const;

    std::optional<ConstTxPtr> GetTransaction(const uint256& BlockHash, const arith_uint256& distance);

    bool Erase(ConstTxPtr value);

private:
    std::unordered_set<ConstTxPtr, std::hash<ConstTxPtr>, std::equal_to<ConstTxPtr>, locked_allocator<ConstTxPtr>>
        mempool_;
};

#endif
