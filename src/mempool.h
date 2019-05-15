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

    /* insert a transaction into the mempool iff there
     * is no transaction in the pool with the same hash;
     * the return value indicates wether the value was
     * inserted */
    bool Insert(ConstTxPtr value);

    bool Contains(ConstTxPtr value) const;

    /* removes all transaction for which
     * std::equal_to<ConstTxPtr> is true
     * for the given value */
    bool Erase(ConstTxPtr value);

    /* retrives the first transaction from the pool that has
     * a sortition distance smaller or equal to the threshold given */
    std::optional<ConstTxPtr> GetTransaction(const uint256& BlockHash, const arith_uint256& threshold);

private:
    /* the set is locked into memory using the locked allocator;
     * this has the beneficial effect that it can never be swapped out */
    std::unordered_set<ConstTxPtr, std::hash<ConstTxPtr>, std::equal_to<ConstTxPtr>, locked_allocator<ConstTxPtr>>
        mempool_;
};

#endif
