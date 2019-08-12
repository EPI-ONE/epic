#ifndef __SRC_MEMPOOL_H__
#define __SRC_MEMPOOL_H__

#include "arith_uint256.h"
#include "blocking_queue.h"
#include "transaction.h"

#include <shared_mutex>
#include <unordered_set>

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

    /**
     * basic operations for memory pool
     */
    bool Insert(ConstTxPtr value);
    bool Contains(const ConstTxPtr& value) const;
    bool Erase(const ConstTxPtr& value);
    bool IsEmpty() const;

    /**
     * processes transactions received from other nodes(memory pool)
     */
    bool ReceiveTx(const ConstTxPtr& tx);

    /**
     *  removes all conflicting transactions if this transaction is valid,
     *  otherwise simply remove it
     */
    void ReleaseTxFromConfirmed(const ConstTxPtr& tx, bool valid);

    std::size_t Size() const;

    /**
     * retrives the first transaction from the pool that has
     * a sortition distance less than the given threshold
     */
    ConstTxPtr GetTransaction(const uint256&, const arith_uint256& threshold);

    ConstTxPtr ExtractTransaction(const uint256&, const arith_uint256& threashold);

    void PushRedemptionTx(ConstTxPtr redemption);

    ConstTxPtr GetRedemptionTx(bool IsRegistration);

private:
    std::unordered_set<ConstTxPtr,
                       std::function<size_t(const ConstTxPtr&)>,
                       std::function<bool(const ConstTxPtr&, const ConstTxPtr&)>>
        mempool_;

    BlockingQueue<ConstTxPtr> redemptionTxQueue_;
    mutable std::shared_mutex mutex_;
};

extern std::unique_ptr<MemPool> MEMPOOL;

#endif // __SRC_MEMPOOL_H__
