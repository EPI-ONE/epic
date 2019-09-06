// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_MEMPOOL_H
#define EPIC_MEMPOOL_H

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

    MemPool(const MemPool& m) : mempool_(m.mempool_) {}

    /**
     * basic operations for memory pool
     */
    bool Insert(ConstTxPtr);
    bool Contains(const ConstTxPtr&) const;
    bool Erase(const ConstTxPtr&);
    void Erase(const std::vector<ConstTxPtr>&);
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
     * retrives the transactions from the pool that has
     * sortition distances less than the given threshold
     */
    std::vector<ConstTxPtr> ExtractTransactions(const uint256&, const arith_uint256& threshold, size_t limit = -1);

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

#endif // EPIC_MEMPOOL_H
