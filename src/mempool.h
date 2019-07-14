#ifndef __SRC_MEMPOOL_H__
#define __SRC_MEMPOOL_H__

#include <optional>
#include <unordered_set>

#include "arith_uint256.h"
#include "transaction.h"

class MemPool {
public:
    MemPool()
        : mempool_(
              16,
              [](const ConstTxPtr& t) {
                  assert(t);
                  return t->GetHash().GetCheapHash();
              },
              [](const ConstTxPtr& a, const ConstTxPtr& b) {
                  if (!a || !b) {
                      return false;
                  }
                  assert(!a->GetHash().IsNull() && !b->GetHash().IsNull());
                  return *a == *b;
              }) {}

    /* insert a transaction into the mempool iff there
     * is no transaction in the pool with the same hash;
     * the return value indicates wether the value was
     * inserted */
    bool Insert(ConstTxPtr value);

    bool Contains(const ConstTxPtr& value) const;

    /* removes all transaction for which
     * std::equal_to<ConstConstTxPtr> is true
     * for the given value */
    bool Erase(const ConstTxPtr& value);

    std::size_t Size() const;

    bool IsEmpty() const;

    /* retrives the first transaction from the pool that has
     * a sortition distance smaller or equal to the threshold given */
    ConstTxPtr GetTransaction(const uint256&, const arith_uint256& threshold);

    ConstTxPtr ExtractTransaction(const uint256&, const arith_uint256& threashold);

private:
    std::unordered_set<ConstTxPtr,
                       std::function<size_t(const ConstTxPtr&)>,
                       std::function<bool(const ConstTxPtr&, const ConstTxPtr&)>>
        mempool_;
};

extern std::unique_ptr<MemPool> MEMPOOL;

#endif
